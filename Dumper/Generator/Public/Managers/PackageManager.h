#pragma once

#include "Unreal/Enums.h"
#include "Unreal/UnrealObjects.h"

#include "Managers/DependencyManager.h"
#include "HashStringTable.h"


namespace PackageManagerUtils
{
	// 获取依赖项
	std::unordered_set<int32> GetDependencies(UEStruct Struct, int32 StructIndex);
}

class PackageInfoHandle;
class PackageManager;

// 需求信息
struct RequirementInfo
{
	int32 PackageIdx; // 包索引
	bool bShouldIncludeStructs; // 是否应包含结构体
	bool bShouldIncludeClasses; // 是否应包含类
};

// 已访问节点信息
struct VisitedNodeInformation
{
	int32 PackageIdx; // 包索引
	mutable uint64 StructsIterationHitCount = 0x0; // 结构体迭代命中计数
	mutable uint64 ClassesIterationHitCount = 0x0; // 类迭代命中计数
};

using DependencyListType = std::unordered_map<int32, RequirementInfo>; // 依赖列表类型


// 依赖信息
struct DependencyInfo
{
	/* Counter incremented every time this element is hit during iteration, **if** the counter is less than the CurrentIterationIndex */
	/* 每次迭代期间命中此元素时递增的计数器，**如果**计数器小于CurrentIterationIndex */
	mutable uint64 StructsIterationHitCount = 0x0; // 结构体迭代命中计数
	mutable uint64 ClassesIterationHitCount = 0x0; // 类迭代命中计数

	/* List of packages required by "ThisPackage_structs.h" */
	/* "ThisPackage_structs.h"所需的包列表 */
	DependencyListType StructsDependencies;

	/* List of packages required by "ThisPackage_classes.h" */
	/* "ThisPackage_classes.h"所需的包列表 */
	DependencyListType ClassesDependencies;

	/* List of packages required by "ThisPackage_parameters.h" */
	/* "ThisPackage_parameters.h"所需的包列表 */
	DependencyListType ParametersDependencies;
};

// 包含数据
struct IncludeData
{
	bool bIncludedStructs = false; // 是否已包含结构体
	bool bIncludedClasses = false; // 是否已包含类
};

using VisitedNodeContainerType = std::unordered_map<int32, IncludeData>; // 已访问节点容器类型


// 包信息
struct PackageInfo
{
private:
	friend class PackageInfoHandle;
	friend class PackageManager;
	friend class PackageManagerTest;

private:
	int32 PackageIndex; // 包索引

	/* Name of this Package*/
	/* 此包的名称 */
	HashStringTableIndex Name = HashStringTableIndex::FromInt(-1);

	/* Count to track how many packages with this name already exists at the point this PackageInfos' initialization */
	/* 跟踪此PackageInfos初始化时已存在多少个同名包的计数 */
	uint64 CollisionCount = 0x0; // 冲突计数

	bool bHasParams; // 是否有参数

	DependencyManager StructsSorted; // 已排序的结构体
	DependencyManager ClassesSorted; // 已排序的类

	std::vector<int32> Functions; // 函数
	std::vector<int32> Enums; // 枚举

	/* Pair<Index, bIsClass>. Forward declarations for enums, mostly for enums from packages with cyclic dependencies */
	/* Pair<Index, bIsClass>。枚举的前向声明，主要用于具有循环依赖关系的包中的枚举 */
	std::vector<std::pair<int32, bool>> EnumForwardDeclarations;

	/* mutable to allow PackageManager to erase cyclic dependencies */
	/* 可变的，以允许PackageManager擦除循环依赖 */
	mutable DependencyInfo PackageDependencies;
};

// 包信息句柄
class PackageInfoHandle
{
private:
	const PackageInfo* Info;

public:
	PackageInfoHandle() = default;
	PackageInfoHandle(std::nullptr_t Nullptr);
	PackageInfoHandle(const PackageInfo& InInfo);

public:
	// 句柄是否有效
	inline bool IsValidHandle() { return Info != nullptr; }

public:
	// 获取索引
	int32 GetIndex() const;

	/* Returns a pair of name and CollisionCount */
	/* 返回名称和冲突计数的配对 */
	std::string GetName() const;
	const StringEntry& GetNameEntry() const;
	std::pair<std::string, uint8> GetNameCollisionPair() const;

	// 是否有类
	bool HasClasses() const;
	// 是否有结构体
	bool HasStructs() const;
	// 是否有函数
	bool HasFunctions() const;
	// 是否有参数结构体
	bool HasParameterStructs() const;
	// 是否有枚举
	bool HasEnums() const;

	// 是否为空
	bool IsEmpty() const;

	// 获取已排序的结构体
	const DependencyManager& GetSortedStructs() const;
	// 获取已排序的类
	const DependencyManager& GetSortedClasses() const;

	// 获取函数
	const std::vector<int32>& GetFunctions() const;
	// 获取枚举
	const std::vector<int32>& GetEnums() const;

	// 获取枚举的前向声明
	const std::vector<std::pair<int32, bool>>& GetEnumForwardDeclarations() const;

	// 获取包依赖项
	const DependencyInfo& GetPackageDependencies() const;

	// 从结构体中擦除包依赖项
	void ErasePackageDependencyFromStructs(int32 Package) const;
	// 从类中擦除包依赖项
	void ErasePackageDependencyFromClasses(int32 Package) const;
};

using PackageManagerOverrideMapType = std::unordered_map<int32 /* PackageIndex */, PackageInfo>; // 包管理器覆写映射类型

// 包信息迭代器
struct PackageInfoIterator
{
private:
	friend class PackageManager;

private:
	using MapType = PackageManagerOverrideMapType;
	using IteratorType = PackageManagerOverrideMapType::const_iterator;

private:
	const MapType& PackageInfos;
	uint8 CurrentIterationHitCount;
	IteratorType It;

private:
	explicit PackageInfoIterator(const MapType& Infos, uint8 IterationHitCount, IteratorType ItPos)
		: PackageInfos(Infos), CurrentIterationHitCount(IterationHitCount), It(ItPos)
	{
	}

	explicit PackageInfoIterator(const MapType& Infos, uint8 IterationHitCount)
		: PackageInfos(Infos), CurrentIterationHitCount(IterationHitCount), It(Infos.cbegin())
	{
	}

public:
	inline PackageInfoIterator& operator++() { ++It; return *this; }
	inline PackageInfoHandle operator*() const { return { PackageInfoHandle(It->second) }; }

	inline bool operator==(const PackageInfoIterator& Other) const { return It == Other.It; }
	inline bool operator!=(const PackageInfoIterator& Other) const { return It != Other.It; }

public:
	PackageInfoIterator begin() const { return PackageInfoIterator(PackageInfos, CurrentIterationHitCount, PackageInfos.cbegin()); }
	PackageInfoIterator end() const   { return PackageInfoIterator(PackageInfos, CurrentIterationHitCount, PackageInfos.cend());   }
};

// 包管理器迭代参数
struct PackageManagerIterationParams
{
	int32 PrevPackage; // 上一个包
	int32 RequiredPackage; // 必需的包

	bool bWasPrevNodeStructs; // 上一个节点是结构体吗
	bool bRequiresClasses; // 需要类吗
	bool bRequiresStructs; // 需要结构体吗

	VisitedNodeContainerType& VisitedNodes; // 已访问的节点
};

// 包管理器
class PackageManager
{
private:
	friend class PackageInfoHandle;
	friend class PackageManagerTest;

public:
	using OverrideMaptType = PackageManagerOverrideMapType; // 覆写映射类型

	using IteratePackagesCallbackType = std::function<void(const PackageManagerIterationParams& OldParams, const PackageManagerIterationParams& NewParams, bool bIsStruct)>; // 迭代包回调类型
	using FindCycleCallbackType = std::function<void(const PackageManagerIterationParams& OldParams, const PackageManagerIterationParams& NewParams, bool bIsStruct)>; // 查找循环回调类型

private:
	// 单个依赖项迭代内部参数
	struct SingleDependencyIterationParamsInternal
	{
		const IteratePackagesCallbackType& CallbackForEachPackage; // 每个包的回调
		const FindCycleCallbackType& OnFoundCycle; // 找到循环时的回调

		PackageManagerIterationParams& NewParams; // 新参数
		const PackageManagerIterationParams& OldParams; // 旧参数
		const DependencyListType& Dependencies; // 依赖项
		VisitedNodeContainerType& VisitedNodes; // 已访问的节点

		int32 CurrentIndex; // 当前索引
		int32 PrevIndex; // 上一个索引
		uint64& IterationHitCounterRef; // 迭代命中计数器引用

		bool bShouldHandlePackage; // 是否应处理包
		bool bIsStruct; // 是结构体吗
	};

private:
	/* NameTable containing names of all Packages as well as information on name-collisions */
	/* 包含所有包名称以及名称冲突信息的名称表 */
	static inline HashStringTable UniquePackageNameTable;

	/* Map containing infos on all Packages. Implemented due to information missing in the Unreal's reflection system (PackageSize). */
	/* 包含所有包信息的映射。由于虚幻引擎反射系统（PackageSize）中缺少信息而实现。 */
	static inline OverrideMaptType PackageInfos;

	/* Count to track how often the PackageInfos was iterated. Allows for up to 2^64 iterations of this list. */
	/* 跟踪PackageInfos被迭代次数的计数。允许此列表最多进行2^64次迭代。 */
	static inline uint64 CurrentIterationHitCount = 0x0;

	static inline bool bIsInitialized = false; // 是否已初始化
	static inline bool bIsPostInitialized = false; // 是否已进行后初始化

private:
	// 初始化依赖项
	static void InitDependencies();
	// 初始化名称
	static void InitNames();
	// 处理循环
	static void HandleCycles();

private:
	// 辅助函数：标记包的结构体依赖项
	static void HelperMarkStructDependenciesOfPackage(UEStruct Struct, int32 OwnPackageIdx, int32 RequiredPackageIdx, bool bIsClass);
	// 辅助函数：计算包的结构体依赖项数量
	static int32 HelperCountStructDependenciesOfPackage(UEStruct Struct, int32 OwnPackageIdx, bool bIsClass);

	// 辅助函数：将包中的枚举添加到前向声明
	static void HelperAddEnumsFromPacakageToFwdDeclarations(UEStruct Struct, std::vector<std::pair<int32, bool>>& EnumsToForwardDeclare, int32 RequiredPackageIdx, bool bMarkAsClass);

	// 辅助函数：为包初始化枚举前向声明
	static void HelperInitEnumFwdDeclarationsForPackage(int32 PackageForFwdDeclarations, int32 RequiredPackage, bool bIsClass);

public:
	// 初始化
	static void Init();
	// 后初始化
	static void PostInit();

private:
	// 获取包名称
	static inline const StringEntry& GetPackageName(const PackageInfo& Info)
	{
		return UniquePackageNameTable[Info.Name];
	}

private:
	// 迭代单个依赖项实现
	static void IterateSingleDependencyImplementation(SingleDependencyIterationParamsInternal& Params, bool bCheckForCycle);

	// 迭代依赖项实现
	static void IterateDependenciesImplementation(const PackageManagerIterationParams& Params, const IteratePackagesCallbackType& CallbackForEachPackage, const FindCycleCallbackType& OnFoundCycle, bool bCheckForCycle);

public:
	// 迭代依赖项
	static void IterateDependencies(const IteratePackagesCallbackType& CallbackForEachPackage);
	// 查找循环
	static void FindCycle(const FindCycleCallbackType& OnFoundCycle);

public:
	// 获取包信息
	static inline const OverrideMaptType& GetPackageInfos()
	{
		return PackageInfos;
	}

	// 获取名称
	static inline std::string GetName(int32 PackageIndex)
	{
		return GetInfo(PackageIndex).GetName();
	}

	// 包名称是否唯一
	static inline bool IsPackageNameUnique(const PackageInfo& Info)
	{
		return UniquePackageNameTable[Info.Name].IsUnique();
	}

	// 获取信息
	static inline PackageInfoHandle GetInfo(int32 PackageIndex)
	{
		return PackageInfos.at(PackageIndex);
	}

	// 获取信息
	static inline PackageInfoHandle GetInfo(const UEObject Package)
	{
		if (!Package)
			return {};

		return PackageInfos.at(Package.GetIndex());
	}

	// 迭代包信息
	static inline PackageInfoIterator IterateOverPackageInfos()
	{
		CurrentIterationHitCount++;

		return PackageInfoIterator(PackageInfos, CurrentIterationHitCount);
	}
};
