#include <vector>
#include <array>

#include "Unreal/ObjectArray.h"
#include "Generators/CppGenerator.h"
#include "Wrappers/MemberWrappers.h"
#include "Managers/MemberManager.h"

#include "../Settings.h"

/*
* CppGenerator.cpp 是代码生成器的核心部分，专门负责将解析出的游戏引擎数据结构
*（如类、结构体、枚举和函数）转换成格式化、可编译的 C++ 代码。它处理复杂的
* 内存布局问题，如成员对齐、位域、继承关系等，以确保生成的代码与原始引擎
* 中的定义完全一致。此外，它还负责生成函数实现，包括处理UFunction的特定
* 调用约定和参数传递。
*/

// 根据成员大小获取其在C++中的基本类型名称（如 uint8, uint16 等）。
// 主要用于位域填充，确保使用正确大小的底层类型来表示位。
constexpr std::string GetTypeFromSize(uint8 Size)
{
	switch (Size)
	{
	case 1:
		return "uint8";
	case 2:
		return "uint16";
	case 4:
		return "uint32";
	case 8:
		return "uint64";
	default:
		return "INVALID_TYPE_SIZE_FOR_BIT_PADDING";
	}
}

// 格式化生成一个成员变量的字符串。
// 为了代码美观和可读性，它会自动调整类型和名称之间的间距，并将注释对齐。
// 最终格式: [tab] Type      MemberName;      // Comment
std::string CppGenerator::MakeMemberString(const std::string& Type, const std::string& Name, std::string&& Comment)
{
	//<tab><--45 chars--><-------50 chars----->
	//     Type          MemberName;           // Comment
	int NumSpacesToComment;

	if (Type.length() < 45)
	{
		NumSpacesToComment = 50;
	}
	else if ((Type.length() + Name.length()) > 95)
	{
		NumSpacesToComment = 1;
	}
	else
	{
		NumSpacesToComment = 50 - (Type.length() - 45);
	}

	return std::format("\t{:{}} {:{}} // {}\n", Type, 45, Name + ";", NumSpacesToComment, std::move(Comment));
}

// 生成一个没有名称的成员字符串，通常用于 using 声明等特殊情况。
std::string CppGenerator::MakeMemberStringWithoutName(const std::string& Type)
{
	return '\t' + Type + ";\n";
}

// 生成用于填充内存空隙的字节数组。
// 当两个成员变量之间存在未定义的空间时，需要用这个来保证后续成员的偏移正确。
std::string CppGenerator::GenerateBytePadding(const int32 Offset, const int32 PadSize, std::string&& Reason)
{
	return MakeMemberString("uint8", std::format("Pad_{:X}[0x{:X}]", Offset, PadSize), std::format("0x{:04X}(0x{:04X})({})", Offset, PadSize, std::move(Reason)));
}

// 生成用于填充位域空隙的位域成员。
// 当一个字节内的位没有被完全使用时，这个函数会填充剩余的位，以确保下一个成员的对齐和偏移正确。
std::string CppGenerator::GenerateBitPadding(uint8 UnderlayingSizeBytes, const uint8 PrevBitPropertyEndBit, const int32 Offset, const int32 PadSize, std::string&& Reason)
{
	return MakeMemberString(GetTypeFromSize(UnderlayingSizeBytes), std::format("BitPad_{:X}_{:X} : {:X}", Offset, PrevBitPropertyEndBit, PadSize), std::format("0x{:04X}(0x{:04X})({})", Offset, UnderlayingSizeBytes, std::move(Reason)));
}

// 为给定的结构体（或类）生成所有成员变量的C++代码。
// 这是生成器中最复杂的部分之一，它需要处理：
// - 继承自父类的成员
// - 字节填充和位域填充
// - 静态成员和普通成员的分离
// - 联合体（Union）的特殊处理
// - 确保最终的内存布局和UE原始定义一致
std::string CppGenerator::GenerateMembers(const StructWrapper& Struct, const MemberManager& Members, int32 SuperSize, int32 SuperLastMemberEnd, int32 SuperAlign, int32 PackageIndex)
{
	constexpr uint64 EstimatedCharactersPerLine = 0xF0;

	const bool bIsUnion = Struct.IsUnion();

	std::string OutMembers;
	OutMembers.reserve(Members.GetNumMembers() * EstimatedCharactersPerLine);

	bool bEncounteredZeroSizedVariable = false;
	bool bEncounteredStaticVariable = false;
	bool bAddedSpaceZeroSized = false;
	bool bAddedSpaceStatic = false;
	// 此Lambda函数用于在静态成员、零大小成员和普通成员之间添加空行，以提高代码的可读性。
	auto AddSpaceBetweenSticAndNormalMembers = [&](const PropertyWrapper& Member)
	{
		if (!bAddedSpaceStatic && bEncounteredZeroSizedVariable && !Member.IsZeroSizedMember()) [[unlikely]]
		{
			OutMembers += '\n';
			bAddedSpaceZeroSized = true;
		}

		if (!bAddedSpaceStatic && bEncounteredStaticVariable && !Member.IsStatic()) [[unlikely]]
		{
			OutMembers += '\n';
			bAddedSpaceStatic = true;
		}

		bEncounteredZeroSizedVariable = Member.IsZeroSizedMember();
		bEncounteredStaticVariable = Member.IsStatic() && !Member.IsZeroSizedMember();
	};

	bool bLastPropertyWasBitField = false;

	// 初始化上一个属性的结束位置为父类的大小。
	int32 PrevPropertyEnd = SuperSize;
	int32 PrevBitPropertyEnd = 0;
	int32 PrevBitPropertyEndBit = 1;

	uint8 PrevBitPropertySize = 0x1;
	int32 PrevBitPropertyOffset = 0x0;
	uint64 PrevNumBitsInUnderlayingType = 0x8;

	const int32 SuperTrailingPaddingSize = SuperSize - SuperLastMemberEnd;
	bool bIsFirstSizedMember = true;
	// 遍历结构体的所有成员
	for (const PropertyWrapper& Member : Members.IterateMembers())
	{
		AddSpaceBetweenSticAndNormalMembers(Member);

		const int32 MemberOffset = Member.GetOffset();
		const int32 MemberSize = Member.GetSize();

		const int32 CurrentPropertyEnd = MemberOffset + MemberSize;

		// 生成成员注释，包含偏移、大小和属性标志。
		std::string Comment = std::format("0x{:04X}(0x{:04X})({})", MemberOffset, MemberSize, Member.GetFlagsOrCustomComment());

		const bool bIsBitField = Member.IsBitField();

		/* 在不同字节偏移处的两个位域之间添加填充 */
		// 如果当前位域和上一个位域不在同一个字节，并且上一个位域没有填充满，则需要补齐。
		if (CurrentPropertyEnd > PrevPropertyEnd && bLastPropertyWasBitField && bIsBitField && PrevBitPropertyEndBit < PrevNumBitsInUnderlayingType && !bIsUnion)
		{
			OutMembers += GenerateBitPadding(PrevBitPropertySize, PrevBitPropertyEndBit, PrevBitPropertyOffset, PrevNumBitsInUnderlayingType - PrevBitPropertyEndBit, "Fixing Bit-Field Size For New Byte [ Dumper-7 ]");
			PrevBitPropertyEndBit = 0;
		}

		// 如果当前成员的偏移大于上一个属性的结束位置，说明中间有空隙，需要填充。
		// 联合体（union）不需要填充，因为其成员共享内存。
		if (MemberOffset > PrevPropertyEnd && !bIsUnion)
			OutMembers += GenerateBytePadding(PrevPropertyEnd, MemberOffset - PrevPropertyEnd, "Fixing Size After Last Property [ Dumper-7 ]");

		bIsFirstSizedMember = Member.IsZeroSizedMember() || Member.IsStatic();

		// 处理位域属性
		if (bIsBitField)
		{
			uint8 BitFieldIndex = Member.GetBitIndex();
			uint8 BitSize = Member.GetBitCount();

			if (CurrentPropertyEnd > PrevPropertyEnd)
				PrevBitPropertyEndBit = 0x0;

			std::string BitfieldInfoComment = std::format("BitIndex: 0x{:02X}, PropSize: 0x{:04X} ({})", BitFieldIndex, MemberSize, Member.GetFlagsOrCustomComment());
			Comment = std::format("0x{:04X}(0x{:04X})({})", MemberOffset, MemberSize, BitfieldInfoComment);

			if (PrevBitPropertyEnd < MemberOffset)
				PrevBitPropertyEndBit = 0;
			// 如果两个位域之间有空位，需要填充。
			if (PrevBitPropertyEndBit < BitFieldIndex && !bIsUnion)
				OutMembers += GenerateBitPadding(MemberSize, PrevBitPropertyEndBit, MemberOffset, BitFieldIndex - PrevBitPropertyEndBit, "Fixing Bit-Field Size Between Bits [ Dumper-7 ]");

			PrevBitPropertyEndBit = BitFieldIndex + BitSize;
			PrevBitPropertyEnd = MemberOffset  + MemberSize;

			PrevBitPropertySize = MemberSize;
			PrevBitPropertyOffset = MemberOffset;

			PrevNumBitsInUnderlayingType = (MemberSize * 0x8);
		}

		bLastPropertyWasBitField = bIsBitField;

		if (!Member.IsStatic()) [[likely]]
			PrevPropertyEnd = MemberOffset + (MemberSize * Member.GetArrayDim());

		std::string MemberName = Member.GetName();
		// 处理数组类型成员
		if (Member.GetArrayDim() > 1)
		{
			MemberName += std::format("[0x{:X}]", Member.GetArrayDim());
		}
		// 处理位域类型成员
		else if (bIsBitField)
		{
			MemberName += (" : " + std::to_string(Member.GetBitCount()));
		}
		// 处理带有默认值的成员
		if (Member.HasDefaultValue()) [[unlikely]]
			MemberName += (" = " + Member.GetDefaultValue());

		const bool bAllowForConstPtrMembers = Struct.IsFunction();

		/* using 指令 */
		// 处理零大小成员（通常是空的基类或 using 声明）
		if (Member.IsZeroSizedMember()) [[unlikely]]
		{
			OutMembers += MakeMemberStringWithoutName(GetMemberTypeString(Member, PackageIndex, bAllowForConstPtrMembers));
		}
		else [[likely]]
		{
			OutMembers += MakeMemberString(GetMemberTypeString(Member, PackageIndex, bAllowForConstPtrMembers), MemberName, std::move(Comment));
		}
	}

	// 计算结构体末尾缺失的字节数，并进行填充，以保证结构体总大小正确。
	const int32 MissingByteCount = Struct.GetUnalignedSize() - PrevPropertyEnd;
	if (MissingByteCount > 0x0 /* >=Struct.GetAlignment()*/)
		OutMembers += GenerateBytePadding(PrevPropertyEnd, MissingByteCount, "Fixing Struct Size After Last Property [ Dumper-7 ]");

	return OutMembers;
}

// 从一个FunctionWrapper中提取并生成函数的返回类型、名称和参数列表。
// 这个函数区分预定义函数和引擎中的UFunction。
// 对于UFunction，它会遍历其所有参数属性，正确处理输入、输出(Out)、引用(&)和返回值。
CppGenerator::FunctionInfo CppGenerator::GenerateFunctionInfo(const FunctionWrapper& Func)
{
	FunctionInfo RetFuncInfo;

	// 如果是预定义的函数（硬编码在Dumper中的），直接使用预设信息。
	if (Func.IsPredefined())
	{
		RetFuncInfo.RetType = Func.GetPredefFuncReturnType();
		RetFuncInfo.FuncNameWithParams = Func.GetPredefFuncNameWithParams();

		return RetFuncInfo;
	}

	MemberManager FuncParams = Func.GetMembers();

	RetFuncInfo.FuncFlags = Func.GetFunctionFlags();
	RetFuncInfo.bIsReturningVoid = true;
	RetFuncInfo.RetType = "void";
	RetFuncInfo.FuncNameWithParams = Func.GetName() + "(";

	bool bIsFirstParam = true;

	RetFuncInfo.UnrealFuncParams.reserve(5);
	// 遍历函数的所有成员，以找出参数
	for (const PropertyWrapper& Param : FuncParams.IterateMembers())
	{
		// 忽略非参数的属性
		if (!Param.HasPropertyFlags(EPropertyFlags::Parm))
			continue;

		std::string Type = GetMemberTypeString(Param);

		const bool bIsConst = Param.HasPropertyFlags(EPropertyFlags::ConstParm);

		ParamInfo PInfo;

		const bool bIsRef = Param.HasPropertyFlags(EPropertyFlags::ReferenceParm);
		const bool bIsOut = bIsRef || Param.HasPropertyFlags(EPropertyFlags::OutParm);
		const bool bIsRet = Param.IsReturnParam();

		if (bIsConst && (!bIsOut || bIsRef || bIsRet))
			Type = "const " + Type;

		// 如果是函数的返回值
		if (Param.IsReturnParam())
		{
			RetFuncInfo.RetType = Type;
			RetFuncInfo.bIsReturningVoid = false;

			PInfo.PropFlags = Param.GetPropertyFlags();
			PInfo.bIsConst = false;
			PInfo.Name = Param.GetName();
			PInfo.Type = GetMemberTypeString(Param, -1, true);

			RetFuncInfo.UnrealFuncParams.insert(RetFuncInfo.UnrealFuncParams.begin(), PInfo);

			continue;
		}
		// 如果不是第一个参数
		if (!bIsFirstParam)
			RetFuncInfo.FuncNameWithParams += ", ";
		// 如果是输出或引用参数
		if (bIsOut)
			Type += "&";

		RetFuncInfo.FuncNameWithParams += Type + " " + Param.GetName();

		bIsFirstParam = false;
		// 获取参数信息
		PInfo.PropFlags = Param.GetPropertyFlags();
		PInfo.bIsConst = bIsConst;
		PInfo.Name = Param.GetName();
		PInfo.Type = GetMemberTypeString(Param, -1, true);
		// 添加到参数列表
		RetFuncInfo.UnrealFuncParams.push_back(PInfo);
	}

	RetFuncInfo.FuncNameWithParams += ")";

	if (Func.IsConst())
		RetFuncInfo.FuncNameWithParams += " const";

	return RetFuncInfo;
}

CppGenerator::FunctionInfo CppGenerator::GenerateFunctionInfo(const FunctionWrapper& Func)
{
	FunctionInfo RetFuncInfo;

	if (Func.IsPredefined())
	{
		RetFuncInfo.RetType = Func.GetPredefFuncReturnType();
		RetFuncInfo.FuncNameWithParams = Func.GetPredefFuncNameWithParams();

		return RetFuncInfo;
	}

	MemberManager FuncParams = Func.GetMembers();

	RetFuncInfo.FuncFlags = Func.GetFunctionFlags();
	RetFuncInfo.bIsReturningVoid = true;
	RetFuncInfo.RetType = "void";
	RetFuncInfo.FuncNameWithParams = Func.GetName() + "(";

	bool bIsFirstParam = true;

	RetFuncInfo.UnrealFuncParams.reserve(5);

	for (const PropertyWrapper& Param : FuncParams.IterateMembers())
	{
		if (!Param.HasPropertyFlags(EPropertyFlags::Parm))
			continue;

		std::string Type = GetMemberTypeString(Param);

		const bool bIsConst = Param.HasPropertyFlags(EPropertyFlags::ConstParm);

		ParamInfo PInfo;

		const bool bIsRef = Param.HasPropertyFlags(EPropertyFlags::ReferenceParm);
		const bool bIsOut = bIsRef || Param.HasPropertyFlags(EPropertyFlags::OutParm);
		const bool bIsRet = Param.IsReturnParam();

		if (bIsConst && (!bIsOut || bIsRef || bIsRet))
			Type = "const " + Type;

		if (Param.IsReturnParam())
		{
			RetFuncInfo.RetType = Type;
			RetFuncInfo.bIsReturningVoid = false;

			PInfo.PropFlags = Param.GetPropertyFlags();
			PInfo.bIsConst = false;
			PInfo.Name = Param.GetName();
			PInfo.Type = Type;
			PInfo.bIsRetParam = true;
			RetFuncInfo.UnrealFuncParams.push_back(PInfo);
			continue;
		}

		// 某些类型（如结构体、数组、字符串）应通过引用传递以提高效率。
		const bool bIsMoveType = Param.IsType(EClassCastFlags::StructProperty | EClassCastFlags::ArrayProperty | EClassCastFlags::StrProperty | EClassCastFlags::TextProperty | EClassCastFlags::MapProperty | EClassCastFlags::SetProperty);
		
		// 处理输出参数（通过指针或引用传递）
		if (bIsOut)
			Type += bIsRef ? '&' : '*';

		// 处理移动语义和const引用
		if (!bIsOut && !bIsRef && bIsMoveType)
		{
			Type += "&";

			if (!bIsConst)
				Type = "const " + Type;
		}

		std::string ParamName = Param.GetName();

		PInfo.bIsOutPtr = bIsOut && !bIsRef;
		PInfo.bIsOutRef = bIsOut && bIsRef;
		PInfo.bIsMoveParam = bIsMoveType;
		PInfo.bIsRetParam = false;
		PInfo.bIsConst = bIsConst;
		PInfo.PropFlags = Param.GetPropertyFlags();
		PInfo.Name = ParamName;
		PInfo.Type = Type;
		RetFuncInfo.UnrealFuncParams.push_back(PInfo);

		// 在参数之间添加逗号
		if (!bIsFirstParam)
			RetFuncInfo.FuncNameWithParams += ", ";

		RetFuncInfo.FuncNameWithParams += Type + " " + ParamName;

		bIsFirstParam = false;
	}

	RetFuncInfo.FuncNameWithParams += ")";

	return RetFuncInfo;
}

// 为单个函数生成C++代码，包括头文件中的声明和.cpp文件中的实现。
// - 对于内联函数，直接在头文件中生成函数体。
// - 对于预定义函数，使用预设的函数体。
// - 对于UFunction，生成调用UObject::ProcessEvent所需的代码。
std::string CppGenerator::GenerateSingleFunction(const FunctionWrapper& Func, const std::string& StructName, StreamType& FunctionFile, StreamType& ParamFile)
{
	namespace CppSettings = Settings::CppGenerator;

	std::string InHeaderFunctionText;

	FunctionInfo FuncInfo = GenerateFunctionInfo(Func);

	const bool bHasInlineBody = Func.HasInlineBody();
	// 处理模板函数的特殊模板文本
	const std::string TemplateText = (bHasInlineBody && Func.HasCustomTemplateText() ? (Func.GetPredefFunctionCustomTemplateText() + "\n\t") : "");

	const bool bIsConstFunc = Func.IsConst() && !Func.IsStatic();

	// 生成函数声明，如果是内联函数，则同时生成函数体。
	InHeaderFunctionText += std::format("\t{}{}{} {}{}", TemplateText, (Func.IsStatic() ? "static " : ""), FuncInfo.RetType, FuncInfo.FuncNameWithParams, bIsConstFunc ? " const" : "");
	InHeaderFunctionText += (bHasInlineBody ? ("\n\t" + Func.GetPredefFunctionInlineBody()) : ";") + "\n";

	if (bHasInlineBody)
		return InHeaderFunctionText;

	// 处理预定义函数，直接写入预设的实现代码。
	if (Func.IsPredefined())
	{
		std::string CustomComment = Func.GetPredefFunctionCustomComment();

		FunctionFile << std::format(R"(
// Predefined Function
{}
{} {}::{}{}
{}

)"
, !CustomComment.empty() ? ("// " + CustomComment + '\n') : ""
, Func.GetPredefFuncReturnType()
, StructName
, Func.GetPredefFuncNameWithParamsForCppFile()
, bIsConstFunc ? " const" : ""
, Func.GetPredefFunctionBody());

		return InHeaderFunctionText;
	}

	std::string ParamStructName = Func.GetParamStructName();

	// 如果函数有参数，则为其生成一个参数结构体。
	if (!Func.IsPredefined() && Func.GetParamStructSize() > 0x0)
		GenerateStruct(Func.AsStruct(), ParamFile, FunctionFile, ParamFile, -1, ParamStructName);


	// 创建参数结构体变量的字符串。
	std::string ParamVarCreationString = std::format(R"(
	{}{} Parms{{}};
)", CppSettings::ParamNamespaceName ? std::format("{}::", CppSettings::ParamNamespaceName) : "", ParamStructName);

	// 对于Native函数，在调用ProcessEvent前后需要保存和恢复函数标志。
	constexpr const char* StoreFunctionFlagsString = R"(
	auto Flgs = Func->FunctionFlags;
	Func->FunctionFlags |= 0x400;
)";

	std::string ParamDescriptionCommentString = "// Parameters:\n";
	std::string ParamAssignments;
	std::string OutPtrAssignments;
	std::string OutRefAssignments;

	const bool bHasParams = !FuncInfo.UnrealFuncParams.empty();
	bool bHasParamsToInit = false;
	bool bHasOutPtrParamsToInit = false;
	bool bHasOutRefParamsToInit = false;

	// 遍历所有参数，生成参数描述注释和参数赋值代码。
	for (const ParamInfo& PInfo : FuncInfo.UnrealFuncParams)
	{
		ParamDescriptionCommentString += std::format("// {:{}}{:{}}({})\n", PInfo.Type, 40, PInfo.Name, 55, StringifyPropertyFlags(PInfo.PropFlags));

		if (PInfo.bIsRetParam)
			continue;

		// 处理出参（指针形式）的赋值
		if (PInfo.bIsOutPtr)
		{
			OutPtrAssignments += !PInfo.bIsMoveParam ? std::format(R"(

	if ({0} != nullptr)
		*{0} = Parms.{0};)", PInfo.Name) : std::format(R"(

	if ({0} != nullptr)
		*{0} = std::move(Parms.{0});)", PInfo.Name);
			bHasOutPtrParamsToInit = true;
		}
		else
		{
			// 处理入参的赋值
			ParamAssignments += PInfo.bIsMoveParam ? std::format("\tParms.{0} = std::move({0});\n", PInfo.Name) : std::format("\tParms.{0} = {0};\n", PInfo.Name);
			bHasParamsToInit = true;
		}

		// 处理出参（引用形式）的赋值
		if (PInfo.bIsOutRef && !PInfo.bIsConst)
		{
			OutRefAssignments += PInfo.bIsMoveParam ? std::format("\n\t{0} = std::move(Parms.{0});", PInfo.Name) : std::format("\n\t{0} = Parms.{0};", PInfo.Name);
			bHasOutRefParamsToInit = true;
		}
	}

	ParamAssignments = '\n' + ParamAssignments;
	OutRefAssignments = '\n' + OutRefAssignments;

	constexpr const char* RestoreFunctionFlagsString = R"(

	Func->FunctionFlags = Flgs;)";

	// 处理返回值的代码片段。
	constexpr const char* ReturnValueString = R"(

	return Parms.ReturnValue;)";

	UEFunction UnrealFunc = Func.GetUnrealFunction();

	const bool bIsNativeFunc = Func.HasFunctionFlag(EFunctionFlags::Native);

	// 一个辅助lambda，用于在字符串中的引号前添加反斜杠，以防止C++字符串字面量出错。
	static auto PrefixQuotsWithBackslash = [](std::string&& Str) -> std::string
	{
		for (int i = 0; i < Str.size(); i++)
		{
			if (Str[i] == '"')
			{
				Str.insert(i, "\\");
				i++;
			}
		}

		return Str;
	};

	std::string FixedOuterName = PrefixQuotsWithBackslash(UnrealFunc.GetOuter().GetName());
	std::string FixedFunctionName = PrefixQuotsWithBackslash(UnrealFunc.GetName());

	// 生成完整的函数实现代码。
	std::string FunctionImplementation = std::format(R"(
// {}
// ({})
{}
{} {}::{}{}
{{
	static class UFunction* Func = nullptr;

	if (Func == nullptr)
		Func = {}->GetFunction("{}", "{}");
{}{}{}
	{}ProcessEvent(Func, {});{}{}{}{}
}}

)", UnrealFunc.GetFullName()
, StringifyFunctionFlags(FuncInfo.FuncFlags)
, bHasParams ? ParamDescriptionCommentString : ""
, FuncInfo.RetType
, StructName
, FuncInfo.FuncNameWithParams
, bIsConstFunc ? " const" : ""
, Func.IsStatic() ? "StaticClass()" : Func.IsInInterface() ? "AsUObject()->Class" : "Class"
, FixedOuterName
, FixedFunctionName
, bHasParams ? ParamVarCreationString : ""
, bHasParamsToInit ? ParamAssignments : ""
, bIsNativeFunc ? StoreFunctionFlagsString : ""
, Func.IsStatic() ? "GetDefaultObj()->" : Func.IsInInterface() ? "AsUObject()->" : "UObject::"
, bHasParams ? "&Parms" : "nullptr"
, bIsNativeFunc ? RestoreFunctionFlagsString : ""
, bHasOutRefParamsToInit ? OutRefAssignments : ""
, bHasOutPtrParamsToInit ? OutPtrAssignments : ""
, !FuncInfo.bIsReturningVoid ? ReturnValueString : "");

	FunctionFile << FunctionImplementation;

	return InHeaderFunctionText;
}

// 为一个结构体或类生成其所有的函数。
// 这包括遍历成员中的所有函数，并为每个函数调用GenerateSingleFunction。
// 此外，它还为所有UClass派生类自动添加 'StaticClass' 和 'GetDefaultObj' 两个重要的静态函数。
std::string CppGenerator::GenerateFunctions(const StructWrapper& Struct, const MemberManager& Members, const std::string& StructName, StreamType& FunctionFile, StreamType& ParamFile)
{
	namespace CppSettings = Settings::CppGenerator;
	// 定义 'StaticClass' 和 'GetDefaultObj' 两个预定义函数。
	// 这些是UE C++中非常常见的函数，用于类型识别和获取默认对象实例。
	static PredefinedFunction StaticClass;
	static PredefinedFunction GetDefaultObj;

	// 为接口类型（UInterface）定义AsUObject函数，用于将其转换为UObject*。
	static PredefinedFunction Interface_AsObject;
	static PredefinedFunction Interface_AsObject_Const;

	if (StaticClass.NameWithParams.empty())
		StaticClass = {
		.CustomComment = "Used with 'IsA' to check if an object is of a certain type",
		.ReturnType = "class UClass*",
		.NameWithParams = "StaticClass()",

		.bIsStatic = true,
		.bIsConst = false,
		.bIsBodyInline = true,
	};

	if (GetDefaultObj.NameWithParams.empty())
		GetDefaultObj = {
		.CustomComment = "Only use the default object to call \"static\" functions",
		.NameWithParams = "GetDefaultObj()",

		.bIsStatic = true,
		.bIsConst = false,
		.bIsBodyInline = true,
	};
	if (Interface_AsObject.NameWithParams.empty())
	{
		Interface_AsObject = {
		.CustomComment = "UObject inheritance was removed from interfaces to avoid virtual inheritance in the SDK.",
		.ReturnType = "class UObject*",
		.NameWithParams = "AsUObject()",

		.bIsStatic = false,
		.bIsConst = false,
		.bIsBodyInline = true,
		};

		Interface_AsObject.Body = "{\n\treturn reinterpret_cast<UObject*>(this);\n}";
	}

	if (Interface_AsObject_Const.NameWithParams.empty())
	{
		Interface_AsObject_Const = {
		.CustomComment = "UObject inheritance was removed from interfaces to avoid virtual inheritance in the SDK.",
		.ReturnType = "const class UObject*",
		.NameWithParams = "AsUObject()",

		.bIsStatic = false,
		.bIsConst = true,
		.bIsBodyInline = true,
		};

		Interface_AsObject_Const.Body = "{\n\treturn reinterpret_cast<const UObject*>(this);\n}";
	}

	std::string InHeaderFunctionText;

	bool bIsFirstIteration = true;
	bool bDidSwitch = false;
	bool bWasLastFuncStatic = false;
	bool bWasLastFuncInline = false;
	bool bWaslastFuncConst = false;

	const bool bIsInterface = Struct.IsInterface();

	for (const FunctionWrapper& Func : Members.IterateFunctions())
	{
		/* 委托（Delegate）类型的函数签名不应该生成为可调用函数 */
		if (Func.GetFunctionFlags() & EFunctionFlags::Delegate)
			continue;

		// 处理静态/非静态、内联/非内联、const/非const函数之间的空行，以提高代码可读性。
		if (bWasLastFuncInline != Func.HasInlineBody() && !bIsFirstIteration)
		{
			InHeaderFunctionText += "\npublic:\n";
			bDidSwitch = true;
		}

		if ((bWasLastFuncStatic != Func.IsStatic() || bWaslastFuncConst != Func.IsConst()) && !bIsFirstIteration && !bDidSwitch)
			InHeaderFunctionText += '\n';

		bWasLastFuncStatic = Func.IsStatic();
		bWasLastFuncInline = Func.HasInlineBody();
		bWaslastFuncConst = Func.IsConst();
		bIsFirstIteration = false;
		bDidSwitch = false;

		InHeaderFunctionText += GenerateSingleFunction(Func, StructName, FunctionFile, ParamFile);
	}

	/* 跳过预定义类、所有结构体和不继承自UObject的类（非常罕见） */
	if (!Struct.IsUnrealStruct() || !Struct.IsClass() || !Struct.GetSuper().IsValid())
		return InHeaderFunctionText;

	/* 为UClass特有的 'StaticClass' 和 'GetDefaultObj' 函数添加特殊间距 */
	if (bWasLastFuncInline != StaticClass.bIsBodyInline && !bIsFirstIteration)
	{
		InHeaderFunctionText += "\npublic:\n";
		bDidSwitch = true;
	}

	if ((bWasLastFuncStatic != StaticClass.bIsStatic || bWaslastFuncConst != StaticClass.bIsConst) && !bIsFirstIteration && !bDidSwitch)
		InHeaderFunctionText += '\n';

	const bool bIsNameUnique = Struct.GetUniqueName().second;
	
	// 根据设置决定是否使用XOR加密字符串字面量，以增加反编译难度。
	std::string Name = !bIsNameUnique ? Struct.GetFullName() : Struct.GetRawName();
	std::string NameText = CppSettings::XORString ? std::format("{}(\"{}\")", CppSettings::XORString, Name) : std::format("\"{}\"", Name);
	

	static UEClass BPGeneratedClass = nullptr;
	
	if (BPGeneratedClass == nullptr)
		BPGeneratedClass = ObjectArray::FindClassFast("BlueprintGeneratedClass");


	const char* StaticClassImplFunctionName = "StaticClassImpl";

	std::string NonFullName;

	const bool bIsBPStaticClass = Struct.IsAClassWithType(BPGeneratedClass);

	/* 蓝图生成的类是动态加载/卸载的，因此指向UClass的静态指针最终会失效。需要使用不同的实现。 */
	if (bIsBPStaticClass)
	{
		StaticClassImplFunctionName = "StaticBPGeneratedClassImpl";

		if (!bIsNameUnique)
			NonFullName = CppSettings::XORString ? std::format("{}(\"{}\")", CppSettings::XORString, Struct.GetRawName()) : std::format("\"{}\"", Struct.GetRawName());
	}
	

	/* 使用 'StaticClass' 的默认实现 */
	StaticClass.Body = std::format(
			R"({{
	return {}<{}{}{}>();
}})", StaticClassImplFunctionName, NameText, (!bIsNameUnique ? ", true" : ""), (bIsBPStaticClass && !bIsNameUnique ? ", " + NonFullName : ""));

	/* 设置 'GetDefaultObj' 的类特定部分 */
	GetDefaultObj.ReturnType = std::format("class {}*", StructName);
	GetDefaultObj.Body = std::format(
R"({{
	return GetDefaultObjImpl<{}>();
}})",StructName);


	std::shared_ptr<StructWrapper> CurrentStructPtr = std::make_shared<StructWrapper>(Struct);
	InHeaderFunctionText += GenerateSingleFunction(FunctionWrapper(CurrentStructPtr, &StaticClass), StructName, FunctionFile, ParamFile);
	InHeaderFunctionText += GenerateSingleFunction(FunctionWrapper(CurrentStructPtr, &GetDefaultObj), StructName, FunctionFile, ParamFile);

	// 如果是接口，则添加AsUObject函数
	if (bIsInterface)
	{
		InHeaderFunctionText += '\n';

		InHeaderFunctionText += GenerateSingleFunction(FunctionWrapper(CurrentStructPtr, &Interface_AsObject), StructName, FunctionFile, ParamFile);
		InHeaderFunctionText += GenerateSingleFunction(FunctionWrapper(CurrentStructPtr, &Interface_AsObject_Const), StructName, FunctionFile, ParamFile);
	}

	return InHeaderFunctionText;
}

// 为单个结构体或类生成完整的C++代码。
// 这是将所有部分（成员、函数、继承等）组合在一起的顶层函数。
void CppGenerator::GenerateStruct(const StructWrapper& Struct, StreamType& StructFile, StreamType& FunctionFile, StreamType& ParamFile, int32 PackageIndex, const std::string& StructNameOverride)
{
	if (!Struct.IsValid())
		return;

	std::string UniqueName = StructNameOverride.empty() ? GetStructPrefixedName(Struct) : StructNameOverride;
	std::string UniqueSuperName;

	const int32 StructSize = Struct.GetSize();
	int32 SuperSize = 0x0;
	int32 UnalignedSuperSize = 0x0;
	int32 SuperAlignment = 0x0;
	int32 SuperLastMemberEnd = 0x0;
	bool bIsReusingTrailingPaddingFromSuper = false;

	StructWrapper Super = Struct.GetSuper();

	const bool bHasValidSuper = Super.IsValid() && !Struct.IsFunction() && !Struct.IsInterface();

	/* 忽略具有有效Super字段的UFunction，参数结构体不应相互继承。 */
	if (bHasValidSuper)
	{
		UniqueSuperName = GetStructPrefixedName(Super);
		SuperSize = Super.GetSize();
		UnalignedSuperSize = Super.GetUnalignedSize();
		SuperAlignment = Super.GetAlignment();
		SuperLastMemberEnd = Super.GetLastMemberEnd();

		bIsReusingTrailingPaddingFromSuper = Super.HasReusedTrailingPadding();

		if (Super.IsCyclicWithPackage(PackageIndex)) [[unlikely]]
			UniqueSuperName = GetCycleFixupType(Super, true);
	}

	const int32 StructSizeWithoutSuper = StructSize - SuperSize;

	const bool bIsClass = Struct.IsClass();
	const bool bIsUnion = Struct.IsUnion();

	const bool bHasReusedTrailingPadding = Struct.HasReusedTrailingPadding();

	// 生成结构体/类的头部，包括名称、继承和内存对齐指令。
	StructFile << std::format(R"(
// {}
// 0x{:04X} (0x{:04X} - 0x{:04X})
{}{}{} {}{}{}{}
{{
)", Struct.GetFullName()
  , StructSizeWithoutSuper
  , StructSize
  , SuperSize
  , bHasReusedTrailingPadding ? "#pragma pack(push, 0x1)\n" : ""
  , Struct.HasCustomTemplateText() ? (Struct.GetCustomTemplateText() + "\n") : ""
  , bIsClass ? "class" : (bIsUnion ? "union" : "struct")
  , Struct.ShouldUseExplicitAlignment() || bHasReusedTrailingPadding ? std::format("alignas(0x{:02X}) ", Struct.GetAlignment()) : ""
  , UniqueName
  , Struct.IsFinal() ? " final" : ""
  , bHasValidSuper ? (" : public " + UniqueSuperName) : "");

	MemberManager Members = Struct.GetMembers();

	const bool bHasStaticClass = (bIsClass && Struct.IsUnrealStruct());

	const bool bHasMembers = Members.HasMembers() || (StructSizeWithoutSuper >= Struct.GetAlignment());
	const bool bHasFunctions = (Members.HasFunctions() && !Struct.IsFunction()) || bHasStaticClass;

	if (bHasMembers || bHasFunctions)
		StructFile << "public:\n";

	if (bHasMembers)
	{
		StructFile << GenerateMembers(Struct, Members, bIsReusingTrailingPaddingFromSuper ? UnalignedSuperSize : SuperSize, SuperLastMemberEnd, SuperAlignment, PackageIndex);

		if (bHasFunctions)
			StructFile << "\npublic:\n";
	}

	if (bHasFunctions)
		StructFile << GenerateFunctions(Struct, Members, UniqueName, FunctionFile, ParamFile);

	StructFile << "};\n";

	if (bHasReusedTrailingPadding)
		StructFile << "#pragma pack(pop)\n";

	// 根据设置，生成用于调试的静态断言，以验证结构体的大小和成员偏移是否正确。
	if constexpr (Settings::Debug::bGenerateInlineAssertionsForStructSize)
	{
		if (Struct.HasCustomTemplateText())
			return;

		std::string UniquePrefixedName = StructNameOverride.empty() ? GetStructPrefixedName(Struct) : StructNameOverride;

		const int32 StructSize = Struct.GetSize();

		// 对齐断言
		StructFile << std::format("static_assert(alignof({}) == 0x{:06X}, \"Wrong alignment on {}\");\n", UniquePrefixedName, Struct.GetAlignment(), UniquePrefixedName);

		// 大小断言
		StructFile << std::format("static_assert(sizeof({}) == 0x{:06X}, \"Wrong size on {}\");\n", UniquePrefixedName, (StructSize > 0x0 ? StructSize : 0x1), UniquePrefixedName);
	}


	if constexpr (Settings::Debug::bGenerateInlineAssertionsForStructMembers)
	{
		std::string UniquePrefixedName = StructNameOverride.empty() ? GetStructPrefixedName(Struct) : StructNameOverride;

		for (const PropertyWrapper& Member : Members.IterateMembers())
		{
			if (Member.IsBitField() || Member.IsZeroSizedMember() || Member.IsStatic())
				continue;

			std::string MemberName = Member.GetName();
			// 成员偏移断言
			StructFile << std::format("static_assert(offsetof({0}, {1}) == 0x{2:06X}, \"Member '{0}::{1}' has a wrong offset!\");\n", UniquePrefixedName, Member.GetName(), Member.GetOffset());
		}
	}
}

// 为单个枚举生成C++的 'enum class' 代码。
void CppGenerator::GenerateEnum(const EnumWrapper& Enum, StreamType& StructFile)
{
	if (!Enum.IsValid())
		return;

	CollisionInfoIterator EnumValueIterator = Enum.GetMembers();

	int32 NumValues = 0x0;
	std::string MemberString;
	// 遍历枚举的所有值，生成 "Name = Value" 格式的字符串。
	for (const EnumCollisionInfo& Info : EnumValueIterator)
	{
		NumValues++;
		MemberString += std::format("\t{:{}} = {},\n", Info.GetUniqueName(), 40, Info.GetValue());
	}

	if (!MemberString.empty()) [[likely]]
		MemberString.pop_back();

	// 生成完整的 'enum class' 定义。
	StructFile << std::format(R"(
// {}
// NumValues: 0x{:04X}
enum class {} : {}
{{
{}
}};
)", Enum.GetFullName()
  , NumValues
  , GetEnumPrefixedName(Enum)
  , GetEnumUnderlayingType(Enum)
  , MemberString);
}
