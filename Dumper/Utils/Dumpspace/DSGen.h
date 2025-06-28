#pragma once

#include <string>
#include <filesystem>
#include "../Json/json.hpp"

class DSGen
{
public:

	// EType 枚举描述了类、结构体、枚举或函数参数的成员类型
	// 或类、结构体、函数、枚举本身。
	// 设置正确的类型对于 dumpspace 网站来说很重要，以便在点击类型时知道重定向到哪里。
	// 指针或引用在这里不起作用，例如，成员 UWorld* owningWorld 是一个 ET_Class。
	// 更多示例：
	// int itemCount -> ET_Default
	// EToastType type -> ET_Enum (如果我们能确认 EToastType 是一个枚举)
	// FVector location -> ET_Struct (如果我们能确认 FVector 是一个结构体)
	// AWeaponDef* weaponDefinition -> ET_Class (如果我们能确认 AWeaponDef 是一个类)
	// 模板示例：
	// TMap<int, FVector> -> ET_Class，我们忽略模板（int, FVector）是什么
	//
	// 请记住，如果你有一个成员，并且它的类型是未知的，即未定义的，请将该成员标记为 ET_Default。
	// 否则，网站将搜索该类型，导致缺少定义错误。
	enum EType
	{
		ET_Default, // 所有未定义或默认的类型（int, bool, char,..., FType（未定义））
		ET_Struct,  // 所有结构体类型（FVector, Fquat）
		ET_Class,   // 所有类类型（Uworld, UWorld*）
		ET_Enum,    // 所有枚举类型（EToastType）
		ET_Function // 不需要，仅用于函数定义


	};

	static std::string getTypeShort(EType type)
	{
		if (type == ET_Default)
			return "D";
		if (type == ET_Struct)
			return "S";
		if (type == ET_Class)
			return "C";
		if (type == ET_Enum)
			return "E";
		if (type == ET_Function)
			return "F";
		return {};
	}

	// MemberType 结构体保存有关成员类型的信息
	struct MemberType
	{
		EType type; // 成员类型的 EType
		std::string typeName; // 类型的名称，例如 UClass（不应包含任何 * 或 & ！）
		std::string extendedType; // 如果类型是 UClass*，请确保 * 在这里！如果不是，则可以留空
		bool reference = false; // 仅在函数参数中需要。否则忽略
		std::vector<MemberType> subTypes; // 大多数情况下为空，仅当 MemberType 是模板时才需要，例如 TArray<abc>

		/**
		 * \brief 创建一个包含有关 MemberType 和 SubTypes 所有信息的 JSON
		 * \return 返回一个包含有关 MemberType 和 SubTypes 所有信息的 JSON
		 */
		nlohmann::json jsonify() const
		{
			//为 memberType 创建一个数组
			nlohmann::json arr = nlohmann::json::array();
			arr.push_back(typeName); //首先是 typeName
			arr.push_back(getTypeShort(type)); //然后是短类型
			arr.push_back(extendedType); //然后是任何扩展类型
			nlohmann::json subTypeArr = nlohmann::json::array();
			for (auto& subType : subTypes)
				subTypeArr.push_back(subType.jsonify());
			
			arr.push_back(subTypeArr);

			return arr;
		}
	};

	// MemberDefinition 包含成员所需的所有信息
	struct MemberDefinition
	{
		MemberType memberType;
		std::string memberName;
		int offset;
		int bitOffset;
		int size;
		int arrayDim;

	};

	struct FunctionHolder
	{
		std::string functionName; // 函数的名称，例如 getWeapon
		std::string functionFlags; // 调用函数的标志，例如 Blueprint|Static|abc
		uintptr_t functionOffset; // 函数在二进制文件中的偏移量
		MemberType returnType; // 返回类型，如果没有，则传递 void
		std::vector<std::pair<MemberType, std::string>> functionParams; // 函数参数及其类型和名称
	};

	// Classholder 可以用于类和结构体，它们在理论上是相同的。该结构体保存有关类的所有信息
	struct ClassHolder
	{
		int classSize;
		EType classType;
		std::string className;
		std::vector<std::string> interitedTypes;
		std::vector<MemberDefinition> members;
		std::vector<FunctionHolder> functions;
	};

	struct EnumHolder
	{
		std::string enumType; // 枚举类型，uint32_t 或 int 或 uint64_t
		std::string enumName; // 枚举的名称
		std::vector<std::pair<std::string, int>> enumMembers; //枚举成员，它们的名称和代表数字（abc = 5）
	};



private:
	static inline std::string dumpTimeStamp{};

	static inline std::filesystem::path directory{};

	static inline std::vector<std::tuple<std::string, uintptr_t>> offsets{};

	static inline nlohmann::json classes = nlohmann::json::array();
	static inline nlohmann::json structs = nlohmann::json::array();
	static inline nlohmann::json functions = nlohmann::json::array();
	static inline nlohmann::json enums = nlohmann::json::array();

public:
	//多余的构造函数
	DSGen();

	// 设置目录路径。dumpspace 文件将位于 directory/dumpspace 下
	static void setDirectory(const std::filesystem::path& directory);

	// 添加偏移量
	static void addOffset(const std::string& name, uintptr_t offset);

	// 创建一个可以用来添加成员的类或结构体。如果大小或继承未知，可以稍后添加。
	static ClassHolder createStructOrClass(
		const std::string& name, 
		bool isClass = true, 
		int size = 0, 
		const std::vector<std::string>& inheritedClasses = {});

	// 创建一个 MemberType 结构体。仅当成员是模板类型且具有子类型或成员是子类型时，才真正需要创建 MemberType
	static MemberType createMemberType(
		EType type, 
		const std::string& typeName, 
		const std::string& extendedType = "", 
		const std::vector<MemberType>& subTypes = {},
		bool isReference = false
	);

	// 向类或结构体添加一个新成员
	static void addMemberToStructOrClass(
		ClassHolder& classHolder, 
		const std::string& memberName, 
		EType type, 
		const std::string& typeName, 
		const std::string& extendedType, 
		int offset, 
		int size,
		int arrayDim = 1,
		int bitOffset = -1
	);

	// 向类或结构体添加一个新成员
	static void addMemberToStructOrClass(
		ClassHolder& classHolder,
		const std::string& memberName,
		const MemberType& memberType,
		int offset,
		int size,
		int arrayDim = 1,
		int bitOffset = -1
	);

	// 创建一个 EnumHolder
	static EnumHolder createEnum(
		const std::string& enumName, 
		const std::string& enumType, 
		const std::vector<std::pair<std::string, int>>& 
		enumMembers
	);

	// 创建一个 FunctionHolder
	static void createFunction(
		ClassHolder& owningClass,
		const std::string& functionName,
		const std::string& functionFlags,
		uintptr_t functionOffset,
		const MemberType& returnType,
		const std::vector<std::pair<MemberType,
		std::string>>&functionParams = {}
	);

	// 烘焙一个稍后将被转储的 ClassHolder
	static void bakeStructOrClass(ClassHolder& classHolder);

	// 烘焙一个稍后将被转储的 EnumHolder
	static void bakeEnum(EnumHolder& enumHolder);


	// 将所有烘焙的信息转储到磁盘。这应该是最后一步
	static void dump();
};