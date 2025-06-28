#include "DSGen.h"

#include <fstream>

// 构造函数
DSGen::DSGen()
{
}

// 设置生成文件的目录
void DSGen::setDirectory(const std::filesystem::path& directory)
{
	DSGen::directory = directory;

	dumpTimeStamp = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

// 添加偏移量
void DSGen::addOffset(const std::string& name, uintptr_t offset)
{
	offsets.push_back(std::pair(name, offset));
}

// 创建一个结构体或类的持有者
DSGen::ClassHolder DSGen::createStructOrClass(const std::string& name, bool isClass, int size,
                                              const std::vector<std::string>& inheritedClasses)
{
	ClassHolder c;
	c.className = name;
	c.classType = isClass ? ET_Class : ET_Struct;
	c.classSize = size;
	// 如果当前生成的类型是类/结构体，则所有继承的类型也都是类/结构体
	if (!inheritedClasses.empty())
		c.interitedTypes.insert(c.interitedTypes.begin(), inheritedClasses.begin(), inheritedClasses.end());

	return c;
}

// 创建成员类型
DSGen::MemberType DSGen::createMemberType(EType type, const std::string& typeName, const std::string& extendedType,
	const std::vector<MemberType>& subTypes, bool isReference)
{
	MemberType t;
	t.type = type;
	t.typeName = typeName;
	t.extendedType = extendedType;
	t.reference = isReference;
	if (!subTypes.empty())
		t.subTypes.insert(t.subTypes.begin(), subTypes.begin(), subTypes.end());
	return t;
}

// 向结构体或类中添加成员
void DSGen::addMemberToStructOrClass(ClassHolder& classHolder, const std::string& memberName, EType type,
	const std::string& typeName, const std::string& extendedType, int offset, int size, int arrayDim, int bitOffset)
{
	MemberType t;
	t.type = type;
	t.typeName = typeName;
	t.extendedType = extendedType;

	MemberDefinition m;
	m.memberType = t;
	m.memberName = memberName;
	m.offset = offset;
	m.size = size;
	m.arrayDim = arrayDim;
	m.bitOffset = bitOffset;

	classHolder.members.push_back(m);
}

// 向结构体或类中添加成员
void DSGen::addMemberToStructOrClass(ClassHolder& classHolder, const std::string& memberName, const MemberType& memberType,
	int offset, int size, int arrayDim, int bitOffset)
{
	MemberDefinition m;
	m.memberType = memberType;
	m.memberName = memberName;
	m.offset = offset;
	m.size = size;
	m.arrayDim = arrayDim;
	m.bitOffset = bitOffset;

	classHolder.members.push_back(m);
}

// 创建枚举持有者
DSGen::EnumHolder DSGen::createEnum(const std::string& enumName, const std::string& enumType,
	const std::vector<std::pair<std::string, int>>& enumMembers)
{
	EnumHolder e;
	e.enumName = enumName;
	e.enumType = enumType;
	if (!enumMembers.empty())
		e.enumMembers.insert(e.enumMembers.begin(), enumMembers.begin(), enumMembers.end());

	return e;
}


// 创建函数并将其与所属类关联
void DSGen::createFunction(ClassHolder& owningClass, const std::string& functionName,
                                            const std::string& functionFlags, uintptr_t functionOffset, const MemberType& returnType,
                                            const std::vector<std::pair<MemberType, std::string>>& functionParams)
{
	FunctionHolder f;
	f.functionName = functionName;
	f.functionFlags = functionFlags;
	f.functionOffset = functionOffset;
	f.returnType = returnType;
	if (!functionParams.empty())
		f.functionParams.insert(f.functionParams.begin(), functionParams.begin(), functionParams.end());

	owningClass.functions.push_back(f);
}

// "烘焙"结构体或类，将其转换为JSON格式以便序列化
void DSGen::bakeStructOrClass(ClassHolder& classHolder)
{
	nlohmann::json jClass;

	nlohmann::json membersArray = nlohmann::json::array();

	nlohmann::json inheritInfo = nlohmann::json::array();
	for (auto super : classHolder.interitedTypes)
		inheritInfo.push_back(super);

	nlohmann::json InheritInfo;
	InheritInfo["__InheritInfo"] = inheritInfo;
	membersArray.push_back(InheritInfo);

	nlohmann::json gSize;
	gSize["__MDKClassSize"] = classHolder.classSize;
	membersArray.push_back(gSize);

	for(auto& member : classHolder.members)
	{
		nlohmann::json jmember;


		if (member.bitOffset > -1)
			jmember[member.memberName] = std::make_tuple(member.memberType.jsonify(), member.offset, member.size, member.arrayDim, member.bitOffset);
		else
			jmember[member.memberName] = std::make_tuple(member.memberType.jsonify(), member.offset, member.size, member.arrayDim);
		membersArray.push_back(jmember);
	}
	nlohmann::json j;

	j[classHolder.className] = membersArray;

	if (classHolder.classType == ET_Class)
		classes.push_back(j);
	else
		structs.push_back(j);

	if(!classHolder.functions.empty())
	{
		nlohmann::json classFunctions = nlohmann::json::array();
		for (auto& func : classHolder.functions)
		{
			nlohmann::json functionParams = nlohmann::json::array();

			for (const auto& param : func.functionParams)
				functionParams.push_back(std::make_tuple(param.first.jsonify(), param.first.reference ? "&" : "", param.second));

			nlohmann::json j1;
			j1[func.functionName] = std::make_tuple(func.returnType.jsonify(), functionParams, func.functionOffset, func.functionFlags);
			classFunctions.push_back(j1);
		}
		nlohmann::json f;

		f[classHolder.className] = classFunctions;

		functions.push_back(f);
	}

}

// "烘焙"枚举，将其转换为JSON格式以便序列化
void DSGen::bakeEnum(EnumHolder& enumHolder)
{
	nlohmann::json members = nlohmann::json::array();
	for(const auto& member : enumHolder.enumMembers)
	{
		nlohmann::json m;
		m[member.first] = member.second;
		members.push_back(m);
	}
	nlohmann::json j;
	j[enumHolder.enumName] = std::make_tuple(members, enumHolder.enumType);
	enums.push_back(j);
}

// 将所有收集和烘焙过的信息转储到磁盘上的JSON文件中
void DSGen::dump()
{
	if (directory.empty())
		throw std::exception("Please initialize a directory first!");

	constexpr auto version = 10202; // 定义使用的版本号

	// lambda函数，用于将JSON对象保存到磁盘
	auto saveToDisk = [&](const nlohmann::json& json, const std::string& fileName, bool offsetFile = false)
	{
		nlohmann::json j;
		j["updated_at"] = dumpTimeStamp; // 添加时间戳
		j["data"] = json; // 添加主要数据
		j["version"] = version; // 添加版本号

		if(offsetFile){
			nlohmann::json credit;
			credit["dumper_used"] = "Dumper-7"; // 使用的工具名称
			credit["dumper_link"] = "https://github.com/Encryqed/Dumper-7"; // 工具的链接
			j["credit"] = credit; // 添加致谢信息
		}

		std::ofstream file(directory / fileName);
		file << j.dump(-1, ' ', false, nlohmann::detail::error_handler_t::replace); // 将JSON写入文件
	};

	saveToDisk(nlohmann::json(nlohmann::json(offsets)), "OffsetsInfo.json", true); // 保存偏移信息
	saveToDisk(nlohmann::json(classes), "ClassesInfo.json"); // 保存类信息
	saveToDisk(nlohmann::json(functions), "FunctionsInfo.json"); // 保存函数信息
	saveToDisk(nlohmann::json(structs), "StructsInfo.json"); // 保存结构体信息
	saveToDisk(nlohmann::json(enums), "EnumsInfo.json"); // 保存枚举信息


}
