#pragma once
#include "Unreal/ObjectArray.h"
#include "HashStringTable.h"

// 碰撞类型枚举
enum class ECollisionType : uint8
{
	MemberName, // 成员名
	SuperMemberName, // 父类成员名
	FunctionName, // 函数名
	SuperFunctionName, // 父类函数名
	ParameterName, // 参数名
	None, // 无
};

// 将碰撞类型字符串化
inline std::string StringifyCollisionType(ECollisionType Type)
{
	switch (Type)
	{
	case ECollisionType::MemberName:
		return "ECollisionType::MemberName";
		break;
	case ECollisionType::SuperMemberName:
		return "ECollisionType::SuperMemberName";
		break;
	case ECollisionType::FunctionName:
		return "ECollisionType::FunctionName";
		break;
	case ECollisionType::SuperFunctionName:
		return "ECollisionType::SuperFunctionName";
		break;
	case ECollisionType::ParameterName:
		return "ECollisionType::ParameterName";
		break;
	case ECollisionType::None:
		return "ECollisionType::None";
		break;
	default:
		return "ECollisionType::Invalid";
		break;
	}
}

constexpr int32 OwnTypeBitCount = 0x7; // 自身类型位数
constexpr int32 PerCountBitCount = 0x5; // 每个计数位数

// 名称信息
struct NameInfo
{
public:
	HashStringTableIndex Name; // 名称

	union
	{
		struct
		{
			uint32 OwnType : OwnTypeBitCount; // 自身类型

			// Order must match ECollisionType
			// 顺序必须与ECollisionType匹配
			uint32 MemberNameCollisionCount : PerCountBitCount; // 成员名碰撞计数
			uint32 SuperMemberNameCollisionCount : PerCountBitCount; // 父类成员名碰撞计数
			uint32 FunctionNameCollisionCount : PerCountBitCount; // 函数名碰撞计数
			uint32 SuperFuncNameCollisionCount : PerCountBitCount; // 父类函数名碰撞计数
			uint32 ParamNameCollisionCount : PerCountBitCount; // 参数名碰撞计数
		};

		uint32 CollisionData; // 碰撞数据
	};

	static_assert(sizeof(CollisionData) >= (OwnTypeBitCount + (5 * PerCountBitCount)) / 32, "Too many bits to fit into uint32, recude the number of bits!"); // 位太多，无法容纳到uint32中，请减少位数！

public:
	inline NameInfo()
		: Name(HashStringTableIndex::FromInt(-1)), CollisionData(0x0)
	{
	}

	NameInfo(HashStringTableIndex NameIdx, ECollisionType CurrentType);

public:
	// 初始化碰撞数据
	void InitCollisionData(const NameInfo& Existing, ECollisionType CurrentType, bool bIsSuper);

	// 是否存在碰撞
	bool HasCollisions() const;

public:
	// 是否有效
	inline bool IsValid() const
	{
		return Name != -1;
	}

public:
	// 调试字符串化
	std::string DebugStringify() const;
};

namespace KeyFunctions
{
	/* Make a unique key from UEProperty/UEFunction for NameTranslation */
	/* 为NameTranslation从UEProperty/UEFunction创建唯一键 */
	uint64 GetKeyForCollisionInfo(UEStruct Super, UEProperty Member);
	uint64 GetKeyForCollisionInfo(UEStruct Super, UEFunction Function);
}

// 碰撞管理器
class CollisionManager
{
private:
	friend class CollisionManagerTest;
	friend class MemberManagerTest;

public:
	using NameContainer = std::vector<NameInfo>; // 名称容器

	using NameInfoMapType = std::unordered_map<uint64, NameContainer>; // 名称信息映射类型
	using TranslationMapType = std::unordered_map<uint64, uint64>; // 翻译映射类型

private:
	/* Nametable used for storing the string-names of member-/function-names contained by NameInfos */
	/* 用于存储NameInfos中包含的成员/函数名称的字符串名称的名称表 */
	HashStringTable MemberNames;

	/* Member-names and name-collision info*/
	/* 成员名称和名称冲突信息 */
	CollisionManager::NameInfoMapType NameInfos;

	/* Map to translation from UEProperty/UEFunction to Index in NameContainer */
	/* 从UEProperty/UEFunction到NameContainer中索引的转换映射 */
	CollisionManager::TranslationMapType TranslationMap;

	/* Names reserved for predefined members or local variables in function-bodies. Eg. "Class", "Parms", etc. */
	/* 为预定义成员或函数体中的局部变量保留的名称。例如"Class"、"Parms"等。 */
	NameContainer ClassReservedNames;

	/* Names reserved for all members/parameters. Eg. "float", "operator", "return", ... */
	/* 为所有成员/参数保留的名称。例如"float"、"operator"、"return"等。 */
	NameContainer ReservedNames;

private:
	/* Returns index of NameInfo inside of the NameContainer it was added to */
	/* 返回添加到NameContainer中的NameInfo的索引 */
	uint64 AddNameToContainer(NameContainer& StructNames, UEStruct Struct, std::pair<HashStringTableIndex, bool>&& NamePair, ECollisionType CurrentType, bool bIsStruct, UEFunction Func = nullptr);

public:
	/* For external use by 'MemberManager::InitReservedNames()' */
	/* 供 'MemberManager::InitReservedNames()' 外部使用 */
	void AddReservedClassName(const std::string& Name, bool bIsParameterOrLocalVariable);
	void AddReservedName(const std::string& Name);
	void AddStructToNameContainer(UEStruct ObjAsStruct, bool bIsStruct);

	// 字符串化名称
	std::string StringifyName(UEStruct Struct, NameInfo Info);

public:
	// 获取未检查的名称冲突信息
	template<typename UEType>
	inline NameInfo GetNameCollisionInfoUnchecked(UEStruct Struct, UEType Member)
	{
		CollisionManager::NameContainer& InfosForStruct = NameInfos.at(Struct.GetIndex());
		uint64 NameInfoIndex = TranslationMap[KeyFunctions::GetKeyForCollisionInfo(Struct, Member)];

		return InfosForStruct.at(NameInfoIndex);
	}
};

