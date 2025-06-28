#include "Generators/DumpspaceGenerator.h"

// 获取结构体带前缀的名称
std::string DumpspaceGenerator::GetStructPrefixedName(const StructWrapper& Struct)
{
	// 如果是函数，则返回 "外部类名_函数名"
	if (Struct.IsFunction())
		return Struct.GetUnrealStruct().GetOuter().GetValidName() + "_" + Struct.GetName();
	// 获取唯一的名称
	auto [ValidName, bIsUnique] = Struct.GetUniqueName();
	// 如果名称是唯一的，则直接返回
	if (bIsUnique) [[likely]]
		return ValidName;

	/* 格式：Package::FStructName */
	// 否则返回 "包名::结构体名"
	return PackageManager::GetName(Struct.GetUnrealStruct().GetPackageIndex()) + "::" + ValidName;
}

// 获取枚举带前缀的名称
std::string DumpspaceGenerator::GetEnumPrefixedName(const EnumWrapper& Enum)
{
	// 获取唯一的名称
	auto [ValidName, bIsUnique] = Enum.GetUniqueName();
	// 如果名称是唯一的，则直接返回
	if (bIsUnique) [[likely]]
		return ValidName;

	/* 格式：Package::ESomeEnum */
	// 否则返回 "包名::枚举名"
	return PackageManager::GetName(Enum.GetUnrealEnum().GetPackageIndex()) + "::" + ValidName;
}

// 将枚举大小转换为类型字符串
std::string DumpspaceGenerator::EnumSizeToType(const int32 Size)
{
	// 根据大小定义底层类型数组
	static constexpr std::array<const char*, 8> UnderlayingTypesBySize = {
		"uint8",
		"uint16",
		"InvalidEnumSize",
		"uint32",
		"InvalidEnumSize",
		"InvalidEnumSize",
		"InvalidEnumSize",
		"uint64"
	};
	// 根据大小返回对应的类型字符串
	return Size <= 0x8 ? UnderlayingTypesBySize[static_cast<size_t>(Size) - 1] : "uint8";
}

// 获取成员的EType
DSGen::EType DumpspaceGenerator::GetMemberEType(const PropertyWrapper& Property)
{
	/* 预定义成员当前不受DumpspaceGenerator支持 */
	if (!Property.IsUnrealProperty())
		return DSGen::ET_Default;

	return GetMemberEType(Property.GetUnrealProperty());
}

// 获取成员的EType
DSGen::EType DumpspaceGenerator::GetMemberEType(UEProperty Prop)
{
	// 如果是枚举属性
	if (Prop.IsA(EClassCastFlags::EnumProperty))
	{
		return DSGen::ET_Enum;
	}
	// 如果是字节属性
	else if (Prop.IsA(EClassCastFlags::ByteProperty))
	{
		// 如果字节属性有关联的枚举
		if (Prop.Cast<UEByteProperty>().GetEnum())
			return DSGen::ET_Enum;
	}
	//else if (Prop.IsA(EClassCastFlags::ClassProperty))
	//{
	//	/* 检查这是否是UClass*，而不是TSubclassof<UObject> */
	//	if (!Prop.Cast<UEClassProperty>().HasPropertyFlags(EPropertyFlags::UObjectWrapper))
	//		return DSGen::ET_Class; 
	//}
	// 如果是对象属性
	else if (Prop.IsA(EClassCastFlags::ObjectProperty))
	{
		return DSGen::ET_Class;
	}
	// 如果是结构体属性
	else if (Prop.IsA(EClassCastFlags::StructProperty))
	{
		return DSGen::ET_Struct;
	}
	// 如果是数组、映射或集合属性
	else if (Prop.IsType(EClassCastFlags::ArrayProperty | EClassCastFlags::MapProperty | EClassCastFlags::SetProperty))
	{
		return DSGen::ET_Class;
	}

	return DSGen::ET_Default;
}

// 获取成员类型字符串
std::string DumpspaceGenerator::GetMemberTypeStr(UEProperty Property, std::string& OutExtendedType, std::vector<DSGen::MemberType>& OutSubtypes)
{
	UEProperty Member = Property;

	auto [Class, FieldClass] = Member.GetClass();

	EClassCastFlags Flags = Class ? Class.GetCastFlags() : FieldClass.GetCastFlags();
	// 字节属性
	if (Flags & EClassCastFlags::ByteProperty)
	{
		if (UEEnum Enum = Member.Cast<UEByteProperty>().GetEnum())
			return GetEnumPrefixedName(Enum);

		return "uint8";
	}
	// 16位无符号整数属性
	else if (Flags & EClassCastFlags::UInt16Property)
	{
		return "uint16";
	}
	// 32位无符号整数属性
	else if (Flags & EClassCastFlags::UInt32Property)
	{
		return "uint32";
	}
	// 64位无符号整数属性
	else if (Flags & EClassCastFlags::UInt64Property)
	{
		return "uint64";
	}
	// 8位有符号整数属性
	else if (Flags & EClassCastFlags::Int8Property)
	{
		return "int8";
	}
	// 16位有符号整数属性
	else if (Flags & EClassCastFlags::Int16Property)
	{
		return "int16";
	}
	// 32位有符号整数属性
	else if (Flags & EClassCastFlags::IntProperty)
	{
		return "int32";
	}
	// 64位有符号整数属性
	else if (Flags & EClassCastFlags::Int64Property)
	{
		return "int64";
	}
	// 浮点数属性
	else if (Flags & EClassCastFlags::FloatProperty)
	{
		return "float";
	}
	// 双精度浮点数属性
	else if (Flags & EClassCastFlags::DoubleProperty)
	{
		return "double";
	}
	// 类属性
	else if (Flags & EClassCastFlags::ClassProperty)
	{
		if (Member.HasPropertyFlags(EPropertyFlags::UObjectWrapper))
		{
			OutSubtypes.emplace_back(GetMemberType(Member.Cast<UEClassProperty>().GetMetaClass()));

			return "TSubclassOf";
		}

		OutExtendedType = "*";

		return "UClass";
	}
	// 名称属性
	else if (Flags & EClassCastFlags::NameProperty)
	{
		return "FName";
	}
	// 字符串属性
	else if (Flags & EClassCastFlags::StrProperty)
	{
		return "FString";
	}
	// 文本属性
	else if (Flags & EClassCastFlags::TextProperty)
	{
		return "FText";
	}
	// 布尔属性
	else if (Flags & EClassCastFlags::BoolProperty)
	{
		return Member.Cast<UEBoolProperty>().IsNativeBool() ? "bool" : "uint8";
	}
	// 结构体属性
	else if (Flags & EClassCastFlags::StructProperty)
	{
		const StructWrapper& UnderlayingStruct = Member.Cast<UEStructProperty>().GetUnderlayingStruct();

		return GetStructPrefixedName(UnderlayingStruct);
	}
	// 数组属性
	else if (Flags & EClassCastFlags::ArrayProperty)
	{
		OutSubtypes.push_back(GetMemberType(Member.Cast<UEArrayProperty>().GetInnerProperty()));

		return "TArray";
	}
	// 弱对象属性
	else if (Flags & EClassCastFlags::WeakObjectProperty)
	{
		if (UEClass PropertyClass = Member.Cast<UEWeakObjectProperty>().GetPropertyClass()) 
		{
			OutSubtypes.push_back(GetMemberType(PropertyClass));
		}
		else
		{
			OutSubtypes.push_back(ManualCreateMemberType(DSGen::ET_Class, "UObject"));
		}

		return "TWeakObjectPtr";
	}
	// 懒加载对象属性
	else if (Flags & EClassCastFlags::LazyObjectProperty)
	{
		if (UEClass PropertyClass = Member.Cast<UELazyObjectProperty>().GetPropertyClass())
		{
			OutSubtypes.push_back(GetMemberType(PropertyClass));
		}
		else
		{
			OutSubtypes.push_back(ManualCreateMemberType(DSGen::ET_Class, "UObject"));
		}

		return "TLazyObjectPtr";
	}
	// 软类属性
	else if (Flags & EClassCastFlags::SoftClassProperty)
	{
		if (UEClass PropertyClass = Member.Cast<UESoftClassProperty>().GetPropertyClass())
		{
			OutSubtypes.push_back(GetMemberType(PropertyClass));
		}
		else
		{
			OutSubtypes.push_back(ManualCreateMemberType(DSGen::ET_Class, "UClass"));
		}

		return "TSoftClassPtr";
	}
	// 软对象属性
	else if (Flags & EClassCastFlags::SoftObjectProperty)
	{
		if (UEClass PropertyClass = Member.Cast<UESoftObjectProperty>().GetPropertyClass())
		{
			OutSubtypes.push_back(GetMemberType(PropertyClass));
		}
		else
		{
			OutSubtypes.push_back(ManualCreateMemberType(DSGen::ET_Class, "UObject"));
		}

		return "TSoftObjectPtr";
	}
	// 对象属性
	else if (Flags & EClassCastFlags::ObjectProperty)
	{
		OutExtendedType = "*";

		if (UEClass PropertyClass = Member.Cast<UEObjectProperty>().GetPropertyClass())
			return GetStructPrefixedName(PropertyClass);
		
		return "UObject";
	}
	// 映射属性
	else if (Flags & EClassCastFlags::MapProperty)
	{
		UEMapProperty MemberAsMapProperty = Member.Cast<UEMapProperty>();

		OutSubtypes.emplace_back(GetMemberType(Member.Cast<UEMapProperty>().GetKeyProperty()));
		OutSubtypes.emplace_back(GetMemberType(Member.Cast<UEMapProperty>().GetValueProperty()));

		return "TMap";
	}
	// 集合属性
	else if (Flags & EClassCastFlags::SetProperty)
	{
		OutSubtypes.emplace_back(GetMemberType(Member.Cast<UESetProperty>().GetElementProperty()));

		return "TSet";
	}
	// 接口属性
	else if (Flags & EClassCastFlags::InterfaceProperty)
	{
		if (UEClass PropertyClass = Member.Cast<UEInterfaceProperty>().GetInterfaceClass())
			return GetStructPrefixedName(PropertyClass);

		return "TScriptInterface";
	}
	// 字段路径属性
	else if (Flags & EClassCastFlags::FieldPathProperty)
	{
		if (FNameConst PropertyClass = Member.Cast<UEFieldPathProperty>().GetPropertyFName())
		{
			OutSubtypes.push_back(ManualCreateMemberType(DSGen::ET_Class, PropertyClass.ToString()));
		}
		else
		{
			OutSubtypes.push_back(ManualCreateMemberType(DSGen::ET_Class, "FField"));
		}

		return "TFieldPath";
	}
	// 枚举属性
	else if (Flags & EClassCastFlags::EnumProperty)
	{
		if (UEEnum Enum = Member.Cast<UEEnumProperty>().GetEnum())
			return GetEnumPrefixedName(Enum);

		return "enum";
	}
	// 默认返回 "void"
	return "void";
}

// 获取成员类型
DSGen::MemberType DumpspaceGenerator::GetMemberType(const PropertyWrapper& Property, bool bIsReference)
{
	std::string Dummy;
	std::vector<DSGen::MemberType> Dummys;
	// 预定义成员
	if (!Property.IsUnrealProperty())
		return { DSGen::ET_Default, Property.GetType(), "", Property.IsBitField(), {} };
	// 虚幻属性
	return GetMemberType(Property.GetUnrealProperty(), bIsReference);
}

// 获取成员类型
DSGen::MemberType DumpspaceGenerator::GetMemberType(UEProperty Property, bool bIsReference)
{
	std::string ExtendedType = "";
	std::vector<DSGen::MemberType> Subtypes;

	std::string TypeName = GetMemberTypeStr(Property, ExtendedType, Subtypes);

	DSGen::EType EType = GetMemberEType(Property);

	return { EType, TypeName, ExtendedType, Property.IsBitField(), Subtypes };
}

// 手动创建成员类型
DSGen::MemberType DumpspaceGenerator::ManualCreateMemberType(DSGen::EType Type, const std::string& TypeName, const std::string& ExtendedType)
{
	return { Type, TypeName, ExtendedType, false, {} };
}

// 生成枚举
void DumpspaceGenerator::GenerateEnum(const EnumWrapper& Enum)
{
	DSGen::Enum EnumDef;
	EnumDef.Name = GetEnumPrefixedName(Enum);
	EnumDef.Size = Enum.GetSize();
	EnumDef.Type = EnumSizeToType(EnumDef.Size);

	for (auto& Name : Enum.GetMembers())
	{
		EnumDef.Members.push_back({ Name.first, Name.second });
	}

	Generator::Get().AddEnum(EnumDef);
}

// 生成结构体
void DumpspaceGenerator::GenerateStruct(const StructWrapper& Struct)
{
	DSGen::Struct StructDef;
	StructDef.Name = GetStructPrefixedName(Struct);
	StructDef.Size = Struct.GetSize();
	StructDef.Super = Struct.GetSuper() ? GetStructPrefixedName(Struct.GetSuper()) : "";

	for (const MemberProperty& Member : Struct.GetMembers())
	{
		DSGen::Member MemberDef;

		MemberDef.Name = Member.GetName();
		MemberDef.Type = GetMemberType(Member.GetProperty());
		MemberDef.Offset = Member.GetOffset();
		MemberDef.Size = Member.GetSize();

		StructDef.Members.push_back(MemberDef);
	}

	Generator::Get().AddStruct(StructDef);
}

// 生成函数
void DumpspaceGenerator::GenerateFunction(const StructWrapper& Function)
{
	DSGen::Struct FunctionDef;
	FunctionDef.Name = GetStructPrefixedName(Function);
	FunctionDef.Size = Function.GetSize();
	FunctionDef.IsFunction = true;
	FunctionDef.Super = Function.GetSuper() ? GetStructPrefixedName(Function.GetSuper()) : "";

	for (const MemberProperty& Member : Function.GetMembers())
	{
		DSGen::Member MemberDef;
		MemberDef.Name = Member.GetName();
		MemberDef.Type = GetMemberType(Member.GetProperty(), Member.IsReturnParam()); // in Dumpspace references are handled as ret-val pointers
		MemberDef.Offset = Member.GetOffset();
		MemberDef.Size = Member.GetSize();

		if (Member.IsReturnParam())
			FunctionDef.Return = MemberDef;
		else
			FunctionDef.Members.push_back(MemberDef);
	}

	Generator::Get().AddStruct(FunctionDef);
}

// 生成类
void DumpspaceGenerator::GenerateClass(const StructWrapper& Class)
{
	GenerateStruct(Class);
}

// 完成包的生成
void DumpspaceGenerator::FinalizePackage(const PackageWrapper& Package)
{
	DSGen::Package PackageDef;

	PackageDef.Name = Package.GetPath();
	PackageDef.NumStructs = Package.GetNumStructs();
	PackageDef.NumEnums = Package.GetNumEnums();

	Generator::Get().AddPackage(PackageDef);
}

// 最终确定生成器
void DumpspaceGenerator::FinalizeGenerator()
{
	DSGen::GeneratorOptions Options;
	Options.GameName = (Settings::Generator::GameVersion + '-' + Settings::Generator::GameName);
	Options.GameVersion = Settings::Generator::GameVersion;
	Options.GameNameAndVersion = Settings::Generator::GameName;
	Options.ProcessEventIndex = Off::InSDK::ProcessEvent::PEIndex;

	Generator::Get().AddOptions(Options);

	std::cout << "Writing Dumpspace file..." << std::endl;

	Generator::Get().WriteToFile(DumperFolder / (Options.GameName + ".ds"));
}

// 运行生成器
void DumpspaceGenerator::Generate()
{
	// 访问并生成所有枚举
	EnumManager::VisitAllEnums(GenerateEnum);
	// 准备要生成的所有结构体
	std::vector<StructWrapper> StructsToGenerate;
	StructManager::GetAllStructs(StructsToGenerate);
	// 定义一个生成类或结构体的回调函数
	DependencyManager::OnVisitCallbackType GenerateClassOrStructCallback = [&](int32 Index) -> void
	{
		StructWrapper Struct = StructManager::GetStructByIndex(Index);

		if (Struct.IsClass())
		{
			GenerateClass(Struct);
		}
		else
		{
			GenerateStruct(Struct);
		}
	};
	// 遍历并生成所有包
	PackageManager::VisitAllPackages([&](const PackageWrapper& Package) -> bool
	{
		// 最终确定包
		FinalizePackage(Package);
		// 获取包内的函数
		const auto& Functions = Package.GetFunctions();

		for (const auto& Func : Functions)
		{
			GenerateFunction(Func);
		}
		// 获取包的依赖管理器
		DependencyManager& Dependencies = Package.GetDependencyManager();
		// 根据依赖关系生成类和结构体
		Dependencies.Generate(GenerateClassOrStructCallback);

		return true;
	});
	// 最终确定生成器
	FinalizeGenerator();
}