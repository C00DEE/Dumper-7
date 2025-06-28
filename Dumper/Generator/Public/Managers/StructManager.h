#pragma once

#include <unordered_map>
#include <unordered_set>

#include "Unreal/UnrealObjects.h"
#include "HashStringTable.h"


/*
struct alignas(0x08) Parent
{
	uint8 DeltaFlags;                              // Offset: 0x0, Size: 0x1
	//uint8 CompilerGeneratedTrailingPadding[0x7]; // 0x7 byte padding to achieve 0x8 alignment (size is always a multiple of alignment)
};

struct Child : Parent
{
	// This EXPLICITE padding replaces the implicite padding of the Parent class. However, this isn't done concistently by the compiler.
	uint8 Pad_idk[0x7];          // Offset: 0x1, Size: 0x7
};
// static_assert(sizeof(Parent) == 0x8); // true
// static_assert(sizeof(Child) == 0x8);  // true
*/

/*
struct alignas(0x08) Parent
{
	uint8 DeltaFlags;                              // 偏移量：0x0，大小：0x1
	//uint8 CompilerGeneratedTrailingPadding[0x7]; // 7字节填充以实现0x8对齐（大小始终是对齐的倍数）
};

struct Child : Parent
{
	// 这个显式填充替换了父类的隐式填充。然而，编译器并不总是一致地这样做。
	uint8 Pad_idk[0x7];          // 偏移量：0x1，大小：0x7
};
// static_assert(sizeof(Parent) == 0x8); // true
// static_assert(sizeof(Child) == 0x8);  // true
*/

// 结构体信息
struct StructInfo
{
	HashStringTableIndex Name; // 名称

	/* End of the last member-variable of this struct, used to calculate implicit trailing padding */
	/* 此结构体最后一个成员变量的末尾，用于计算隐式尾部填充 */
	int32 LastMemberEnd = 0x0;

	/* Unaligned size of this struct */
	/* 此结构体的未对齐大小 */
	int32 Size = INT_MAX;

	/* Alignment of this struct for alignas(Alignment), might be implicit */
	/* 此结构体的对齐方式，用于 alignas(Alignment)，可能是隐式的 */
	int32 Alignment = 0x1;


	/* Whether alignment should be explicitly specified with 'alignas(Alignment)' */
	/* 是否应使用 'alignas(Alignment)' 显式指定对齐 */
	bool bUseExplicitAlignment;

	/* Whether this struct has child-structs for which the compiler places members in this structs' trailing padding, see example above (line 9) */
	/* 此结构体是否具有子结构体，编译器会将其成员放置在此结构体的尾部填充中，请参见上面的示例（第9行） */
	bool bHasReusedTrailingPadding = false;

	/* Wheter this class is ever inherited from. Set to false when this struct is found to be another structs super */
	/* 此类是否曾被继承。当发现此结构体是另一个结构体的父类时，设置为false */
	bool bIsFinal = true;

	/* Whether this struct is in a package that has cyclic dependencies. Actual index of cyclic package is stored in StructManager::CyclicStructsAndPackages */
	/* 此结构体是否位于具有循环依赖的包中。循环包的实际索引存储在 StructManager::CyclicStructsAndPackages 中 */
	bool bIsPartOfCyclicPackage;
};

class StructManager;

// 结构体信息句柄
class StructInfoHandle
{
private:
	const StructInfo* Info;

public:
	StructInfoHandle() = default;
	StructInfoHandle(const StructInfo& InInfo);

public:
	// 获取最后一个成员的末尾
	int32 GetLastMemberEnd() const;
	// 获取大小
	int32 GetSize() const;
	// 获取未对齐的大小
	int32 GetUnalignedSize() const;
	// 获取对齐方式
	int32 GetAlignment() const;
	// 是否应使用显式对齐
	bool ShouldUseExplicitAlignment() const;
	// 获取名称
	const StringEntry& GetName() const;
	// 是否为最终
	bool IsFinal() const;
	// 是否重用了尾部填充
	bool HasReusedTrailingPadding() const;

	// 是否是循环包的一部分
	bool IsPartOfCyclicPackage() const;
};

// 结构体管理器
class StructManager
{
private:
	friend StructInfoHandle;
	friend class StructManagerTest;

public:
	using OverrideMapType = std::unordered_map<int32 /*StructIdx*/, StructInfo>; // 覆写映射类型
	using CycleInfoListType = std::unordered_map<int32 /*StructIdx*/, std::unordered_set<int32 /* Packages cyclic with this structs' package */>>; // 循环信息列表类型

private:
	/* NameTable containing names of all structs/classes as well as information on name-collisions */
	/* 包含所有结构体/类名称以及名称冲突信息的名称表 */
	static inline HashStringTable UniqueNameTable;

	/* Map containing infos on all structs/classes. Implemented due to bugs/inconsistencies in Unreal's reflection system */
	/* 包含所有结构体/类信息的映射。因虚幻引擎反射系统中的错误/不一致而实现 */
	static inline OverrideMapType StructInfoOverrides;

	/* Map containing infos on all structs/classes that are within a packages that has cyclic dependencies */
	/* 包含所有位于具有循环依赖关系的包中的结构体/类信息的映射 */
	static inline CycleInfoListType CyclicStructsAndPackages;

	static inline bool bIsInitialized = false; // 是否已初始化

private:
	// 初始化对齐和名称
	static void InitAlignmentsAndNames();
	// 初始化大小和bIsFinal标志
	static void InitSizesAndIsFinal();

public:
	// 初始化
	static void Init();

private:
	// 获取名称
	static inline const StringEntry& GetName(const StructInfo& Info)
	{
		return UniqueNameTable[Info.Name];
	}

public:
	// 获取结构体信息
	static inline const OverrideMapType& GetStructInfos()
	{
		return StructInfoOverrides;
	}

	// 结构体名称是否唯一
	static inline bool IsStructNameUnique(HashStringTableIndex NameIndex)
	{
		return UniqueNameTable[NameIndex].IsUnique();
	}

	// debug function
	// 调试函数
	static inline std::string GetName(HashStringTableIndex NameIndex)
	{
		return UniqueNameTable[NameIndex].GetName();
	}

	// 获取信息
	static inline StructInfoHandle GetInfo(const UEStruct Struct)
	{
		if (!Struct)
			return {};

		return StructInfoOverrides.at(Struct.GetIndex());
	}

	// 结构体是否与包存在循环依赖
	static inline bool IsStructCyclicWithPackage(int32 StructIndex, int32 PackageIndex)
	{
		auto It = CyclicStructsAndPackages.find(StructIndex);
		if (It != CyclicStructsAndPackages.end())
			return It->second.contains(PackageIndex);

		return false;
	}

	/* 
	* Utility function for PackageManager::PostInit to handle the initialization of our list of cyclic structs and their respective packages
	* 
	* Marks StructInfo as 'bIsPartOfCyclicPackage = true' and adds struct to 'CyclicStructsAndPackages'
	*/
	/* 
	* PackageManager::PostInit 的实用函数，用于处理循环结构体及其各自包列表的初始化
	* 
	* 将 StructInfo 标记为 'bIsPartOfCyclicPackage = true' 并将结构体添加到 'CyclicStructsAndPackages'
	*/
	static inline void PackageManagerSetCycleForStruct(int32 StructIndex, int32 PackageIndex)
	{
		StructInfo& Info = StructInfoOverrides.at(StructIndex);

		Info.bIsPartOfCyclicPackage = true;


		CyclicStructsAndPackages[StructIndex].insert(PackageIndex);
	}
};

