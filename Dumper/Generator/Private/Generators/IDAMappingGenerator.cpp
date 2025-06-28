#include <fstream>

#include "Generators/IDAMappingGenerator.h"

// 对函数名进行修饰，以符合C++的命名规则
std::string IDAMappingGenerator::MangleFunctionName(const std::string& ClassName, const std::string& FunctionName)
{
	// 格式：_ZN + 类名长度 + 类名 + (函数名长度+4) + "exec" + 函数名 + "Ev"
	return "_ZN" + std::to_string(ClassName.length()) + ClassName + std::to_string(FunctionName.length() + 4) + "exec" + FunctionName + "Ev";
}

// 写入ReadMe文件内容
void IDAMappingGenerator::WriteReadMe(StreamType& ReadMe)
{
	ReadMe << R"(
/*
* 由 Dumper-7 生成的文件
*
* https://github.com/Encryqed/Dumper-7
*/

支持: IDA 7.7 及以上版本 (包括 IDA 8.3)

'.idmap' 文件可用于导入VFTables和Exec函数的名称，需要使用以下插件:

https://github.com/Fischsalat/IDAExecFunctionsImporter


文件格式:

一个'.idmap'文件就是一个标识符数组。插件所做的就是在某个偏移量处为变量/函数赋予一个特定的名称。

struct Identifier
{
    uint32 Offset; // 相对于镜像基址的偏移
    uint16 NameLength; // 名称长度
    const char Name[NameLength]; // 非空字符结尾
};
)";
}

// 生成虚函数表名称
void IDAMappingGenerator::GenerateVTableName(StreamType& IdmapFile, UEObject DefaultObject)
{
	UEClass Class = DefaultObject.GetClass();
	UEClass Super = Class.GetSuper().Cast<UEClass>();
	// 如果父类存在且虚函数表与父类相同，则不生成
	if (Super && DefaultObject.GetVft() == Super.GetDefaultObject().GetVft())
		return;
	// 名称格式：类C++名 + "_VFT"
	std::string Name = Class.GetCppName() + "_VFT";
	// 获取虚函数表偏移量
	uint32 Offset = static_cast<uint32>(GetOffset(DefaultObject.GetVft()));
	// 获取名称长度
	uint16 NameLen = static_cast<uint16>(Name.length());
	// 写入流
	WriteToStream(IdmapFile, Offset);
	WriteToStream(IdmapFile, NameLen);
	WriteToStream(IdmapFile, Name.c_str(), NameLen);
}

// 生成类函数
void IDAMappingGenerator::GenerateClassFunctions(StreamType& IdmapFile, UEClass Class)
{
	static std::unordered_map<uint32, std::string> Funcs;
	// 遍历类的所有函数
	for (UEFunction Func : Class.GetFunctions())
	{
		// 只处理原生函数
		if (!Func.HasFlags(EFunctionFlags::Native))
			continue;
		// 获取修饰后的函数名
		std::string MangledName = MangleFunctionName(Class.GetCppName(), Func.GetValidName());
		// 获取执行函数偏移量
		uint32 Offset = static_cast<uint32>(GetOffset(Func.GetExecFunction()));
		// 获取名称长度
		uint16 NameLen = static_cast<uint16>(MangledName.length());
		// 尝试插入函数，防止重复
		auto [It, bInseted] = Funcs.emplace(Offset, Func.GetFullName());
		// 如果插入失败（已存在），则跳过
		if (!bInseted)
		{
			//std::cerr << "Collision: \nOld: " << It->second << "\nNew: " << Func.GetFullName() << "\n" << std::endl;
			continue;
		}
		// 写入流
		WriteToStream(IdmapFile, Offset);
		WriteToStream(IdmapFile, NameLen);
		WriteToStream(IdmapFile, MangledName.c_str(), NameLen);
	}
}

// 生成IDAMap文件
void IDAMappingGenerator::Generate()
{
	// 定义文件名
	std::string IdaMappingFileName = (Settings::Generator::GameVersion + '-' + Settings::Generator::GameName + ".idmap");
	// 使文件名有效
	FileNameHelper::MakeValidFileName(IdaMappingFileName);

	/* 以二进制方式打开流，否则ofstream会在数字后添加\r，可能被解释为\n */
	std::ofstream IdmapFile(MainFolder / IdaMappingFileName, std::ios::binary);

	/* 创建一个ReadMe文件，描述'.idmap'是什么以及如何使用它 */
	std::ofstream ReadMe(MainFolder / "ReadMe.txt");

	/* 写入文件格式说明以及IDA插件的链接 */
	WriteReadMe(ReadMe);
	// 遍历所有对象
	for (UEObject Obj : ObjectArray())
	{
		// 如果是类默认对象
		if (Obj.HasAnyFlags(EObjectFlags::ClassDefaultObject))
		{
			/* 从默认对象获取VTable偏移量，并将 ClassName + "_VFT" 后缀写入文件 */
			GenerateVTableName(IdmapFile, Obj);
		}
		// 如果是类
		else if (Obj.IsA(EClassCastFlags::Class))
		{
			/* 遍历类的所有函数，并将其以 "exec" 前缀添加到流中 */
			GenerateClassFunctions(IdmapFile, Obj.Cast<UEClass>());
		}
	}
}