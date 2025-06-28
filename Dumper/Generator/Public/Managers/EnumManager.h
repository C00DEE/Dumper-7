#pragma once

#include "CollisionManager.h"


class EnumInfoHandle;

class EnumManager;

// 枚举冲突信息
struct EnumCollisionInfo
{
private:
	friend class EnumManager;

private:
	HashStringTableIndex MemberName; // 成员名
	uint64 MemberValue; // 成员值

	uint8 CollisionCount = 0; // 冲突计数

public:
	// 获取唯一名称
	std::string GetUniqueName() const;
	// 获取原始名称
	std::string GetRawName() const;
	// 获取值
	uint64 GetValue() const;

	// 获取冲突计数
	uint8 GetCollisionCount() const;
};

// 枚举信息
struct EnumInfo
{
private:
	friend class EnumInfoHandle;
	friend class EnumManager;

private:
	/* Name of this Enum*/
	/* 此枚举的名称 */
	HashStringTableIndex Name;

	/* sizeof(UnderlayingType) */
	/* 底层类型的大小 */
	uint8 UnderlyingTypeSize = 0x1;

	/* Wether an occurence of this enum was found, if not guess the type by the enums' max value */
	/* 是否找到了此枚举的实例，如果没有，则根据枚举的最大值猜测类型 */
	bool bWasInstanceFound = false;

	/* Whether this enums' size was initialized before */
	/* 此枚举的大小之前是否已初始化 */
	bool bWasEnumSizeInitialized = false;

	/* Infos on all members and if there are any collisions between member-names */
	/* 所有成员的信息以及成员名之间是否存在冲突 */
	std::vector<EnumCollisionInfo> MemberInfos;
};

// 冲突信息迭代器
struct CollisionInfoIterator
{
private:
	const std::vector<EnumCollisionInfo>& CollisionInfos;

public:
	CollisionInfoIterator(const std::vector<EnumCollisionInfo>& Infos)
		: CollisionInfos(Infos)
	{
	}

public:
	auto begin() const { return CollisionInfos.cbegin(); }
	auto end() const { return CollisionInfos.end(); }
};

// 枚举信息句柄
class EnumInfoHandle
{
private:
	const EnumInfo* Info;

public:
	EnumInfoHandle() = default;
	EnumInfoHandle(const EnumInfo& InInfo);

public:
	// 获取底层类型大小
	uint8 GetUnderlyingTypeSize() const;
	// 获取名称
	const StringEntry& GetName() const;

	// 获取成员数量
	int32 GetNumMembers() const;

	// 获取成员冲突信息迭代器
	CollisionInfoIterator GetMemberCollisionInfoIterator() const;
};


// 枚举管理器
class EnumManager
{
private:
	friend class EnumCollisionInfo;
	friend class EnumInfoHandle;
	friend class EnumManagerTest;

public:
	using OverrideMaptType = std::unordered_map<int32 /* EnumIndex */, EnumInfo>; // 覆写映射类型
	using IllegalNameContaierType = std::vector<HashStringTableIndex>; // 非法名称容器类型

private:
	/* NameTable containing names of all enums as well as information on name-collisions */
	/* 包含所有枚举名称以及名称冲突信息的名称表 */
	static inline HashStringTable UniqueEnumNameTable;

	/* Map containing infos on all enums. Implemented due to information missing in the Unreal's reflection system (EnumSize). */
	/* 包含所有枚举信息的映射。由于虚幻引擎的反射系统（EnumSize）中缺少信息而实现。 */
	static inline OverrideMaptType EnumInfoOverrides;

	/* NameTable containing names of all enum-values as well as information on name-collisions */
	/* 包含所有枚举值名称以及名称冲突信息的名称表 */
	static inline HashStringTable UniqueEnumValueNames;

	/* List containing names-indices which contain illegal enum names such as 'PF_MAX' */
	/* 包含非法枚举名称（如"PF_MAX"）的名称索引列表 */
	static inline IllegalNameContaierType IllegalNames;

	static inline bool bIsInitialized = false; // 是否已初始化

private:
	// 内部初始化
	static void InitInternal();
	// 初始化非法名称
	static void InitIllegalNames();

public:
	// 初始化
	static void Init();

private:
	// 获取枚举名称
	static inline const StringEntry& GetEnumName(const EnumInfo& Info)
	{
		return UniqueEnumNameTable[Info.Name];
	}

	// 获取值名称
	static inline const StringEntry& GetValueName(const EnumCollisionInfo& Info)
	{
		return UniqueEnumValueNames[Info.MemberName];
	}

public:
	// 获取枚举信息
	static inline const OverrideMaptType& GetEnumInfos()
	{
		return EnumInfoOverrides;
	}

	// 枚举名称是否唯一
	static inline bool IsEnumNameUnique(const EnumInfo& Info)
	{
		return UniqueEnumNameTable[Info.Name].IsUnique();
	}

	// 获取信息
	static inline EnumInfoHandle GetInfo(const UEEnum Enum)
	{
		if (!Enum)
			return {};

		return EnumInfoOverrides.at(Enum.GetIndex());
	}
};

