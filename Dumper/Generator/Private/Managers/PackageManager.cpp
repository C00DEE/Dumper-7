#include "Unreal/ObjectArray.h"

#include "Managers/PackageManager.h"

/*
* PackageManager.cpp 负责管理游戏内所有"包"（Packages）的依赖关系。
* 在代码生成中，每个包通常对应一个或多个文件（如 "PackageName_Classes.h"）。
*
* 这个管理器的核心职责是：
* 1. 依赖发现：分析每个类和结构体，找出它们依赖的其他类型，从而构建出包与包之间的依赖图。
* 2. 循环依赖处理：这是最关键的功能。当两个或多个包互相#include时，会导致编译错误。
*    此管理器能检测到这种循环，并智能地选择其中一个包使用"前向声明"（Forward Declaration）
*    来代替完整的#include，从而打破编译循环。
* 3. 命名冲突解决：处理游戏内可能存在的同名包，确保生成的文件名唯一。
* 4. 提供遍历接口：在处理完所有依赖关系后，提供一个有序的遍历方式，供代码生成器使用。
*/

/* 用于StructManager中标记循环头文件 */
#include "Managers/StructManager.h"


// 辅助函数，将b1与b2进行逻辑或运算并赋值给b1
inline void BooleanOrEqual(bool& b1, bool b2)
{
	b1 = b1 || b2;
}

PackageInfoHandle::PackageInfoHandle(std::nullptr_t Nullptr)
	: Info(nullptr)
{
}

PackageInfoHandle::PackageInfoHandle(const PackageInfo& InInfo)
	: Info(&InInfo)
{
}

// 获取包名称的字符串条目。
const StringEntry& PackageInfoHandle::GetNameEntry() const
{
	return PackageManager::GetPackageName(*Info);
}

// 获取包的索引。
int32 PackageInfoHandle::GetIndex() const
{
	return Info->PackageIndex;
}

// 获取包的最终名称，如果存在命名冲突，则附加后缀。
std::string PackageInfoHandle::GetName() const
{
	const StringEntry& Name = GetNameEntry();

	if (Info->CollisionCount <= 0) [[likely]]
		return Name.GetName();

	return Name.GetName() + "_" + std::to_string(Info->CollisionCount - 1);
}
// 获取包名的原始名称和碰撞计数。
std::pair<std::string, uint8> PackageInfoHandle::GetNameCollisionPair() const
{
	const StringEntry& Name = GetNameEntry();

	if (Name.IsUniqueInTable()) [[likely]]
		return { Name.GetName(), 0 };

	return { Name.GetName(), Info->CollisionCount };
}

// 检查包中是否包含类。
bool PackageInfoHandle::HasClasses() const
{
	return Info->ClassesSorted.GetNumEntries() > 0x0;
}

// 检查包中是否包含结构体。
bool PackageInfoHandle::HasStructs() const
{
	return Info->StructsSorted.GetNumEntries() > 0x0;
}

// 检查包中是否包含函数。
bool PackageInfoHandle::HasFunctions() const
{
	return !Info->Functions.empty();
}

// 检查包中是否包含参数结构体。
bool PackageInfoHandle::HasParameterStructs() const
{
	return Info->bHasParams;
}

// 检查包中是否包含枚举。
bool PackageInfoHandle::HasEnums() const
{
	return !Info->Enums.empty();
}

// 检查包是否为空（不包含任何可生成的类型）。
bool PackageInfoHandle::IsEmpty() const
{
	return !HasClasses() && !HasStructs() && !HasEnums() && !HasParameterStructs() && !HasFunctions();
}

// 获取包内经过拓扑排序的结构体依赖管理器。
const DependencyManager& PackageInfoHandle::GetSortedStructs() const
{
	return Info->StructsSorted;
}

// 获取包内经过拓扑排序的类依赖管理器。
const DependencyManager& PackageInfoHandle::GetSortedClasses() const
{
	return Info->ClassesSorted;
}

// 获取包内所有函数的索引列表。
const std::vector<int32>& PackageInfoHandle::GetFunctions() const
{
	return Info->Functions;
}

// 获取包内所有枚举的索引列表。
const std::vector<int32>& PackageInfoHandle::GetEnums() const
{
	return Info->Enums;
}

// 获取需要前向声明的枚举列表。
const std::vector<std::pair<int32, bool>>& PackageInfoHandle::GetEnumForwardDeclarations() const
{
	return Info->EnumForwardDeclarations;
}

// 获取包的依赖信息。
const DependencyInfo& PackageInfoHandle::GetPackageDependencies() const
{
	return Info->PackageDependencies;
}

// 从结构体依赖中移除指定的包。
void PackageInfoHandle::ErasePackageDependencyFromStructs(int32 Package) const
{
	Info->PackageDependencies.StructsDependencies.erase(Package);
}

// 从类依赖中移除指定的包。
void PackageInfoHandle::ErasePackageDependencyFromClasses(int32 Package) const
{
	Info->PackageDependencies.ClassesDependencies.erase(Package);
}


namespace PackageManagerUtils
{
	// 递归地获取一个属性的所有依赖项。
	// 例如，一个TArray<FMyStruct>依赖于FMyStruct，一个TMap<EEnum, UMyClass*>依赖于EEnum和UMyClass。
	void GetPropertyDependency(UEProperty Prop, std::unordered_set<int32>& Store)
	{
		if (Prop.IsA(EClassCastFlags::StructProperty))
		{
			Store.insert(Prop.Cast<UEStructProperty>().GetUnderlayingStruct().GetIndex());
		}
		else if (Prop.IsA(EClassCastFlags::EnumProperty))
		{
			if (UEObject Enum = Prop.Cast<UEEnumProperty>().GetEnum())
				Store.insert(Enum.GetIndex());
		}
		else if (Prop.IsA(EClassCastFlags::ByteProperty))
		{
			if (UEObject Enum = Prop.Cast<UEByteProperty>().GetEnum())
				Store.insert(Enum.GetIndex());
		}
		else if (Prop.IsA(EClassCastFlags::ArrayProperty))
		{
			GetPropertyDependency(Prop.Cast<UEArrayProperty>().GetInnerProperty(), Store);
		}
		else if (Prop.IsA(EClassCastFlags::SetProperty))
		{
			GetPropertyDependency(Prop.Cast<UESetProperty>().GetElementProperty(), Store);
		}
		else if (Prop.IsA(EClassCastFlags::MapProperty))
		{
			GetPropertyDependency(Prop.Cast<UEMapProperty>().GetKeyProperty(), Store);
			GetPropertyDependency(Prop.Cast<UEMapProperty>().GetValueProperty(), Store);
		}
		else if (Prop.IsA(EClassCastFlags::OptionalProperty) && !Prop.IsA(EClassCastFlags::ObjectPropertyBase))
		{
			GetPropertyDependency(Prop.Cast<UEOptionalProperty>().GetValueProperty(), Store);
		}
		else if (Prop.IsA(EClassCastFlags::DelegateProperty) || Prop.IsA(EClassCastFlags::MulticastInlineDelegateProperty))
		{
			const bool bIsNormalDeleage = !Prop.IsA(EClassCastFlags::MulticastInlineDelegateProperty);
			UEFunction SignatureFunction = bIsNormalDeleage ? Prop.Cast<UEDelegateProperty>().GetSignatureFunction() : Prop.Cast<UEMulticastInlineDelegateProperty>().GetSignatureFunction();

			if (!SignatureFunction)
				return;
			
			// 委托的依赖项是其签名函数的所有参数。
			for (UEProperty DelegateParam : SignatureFunction.GetProperties())
			{
				GetPropertyDependency(DelegateParam, Store);
			}
		}
	}

	// 获取一个结构体的所有依赖项（不包括自身）。
	std::unordered_set<int32> GetDependencies(UEStruct Struct, int32 StructIndex)
	{
		std::unordered_set<int32> Dependencies;

		const int32 StructIdx = Struct.GetIndex();

		for (UEProperty Property : Struct.GetProperties())
		{
			GetPropertyDependency(Property, Dependencies);
		}

		Dependencies.erase(StructIdx);

		return Dependencies;
	}

	// 将依赖项的包信息设置到包依赖追踪器中。
	inline void SetPackageDependencies(DependencyListType& DependencyTracker, const std::unordered_set<int32>& Dependencies, int32 StructPackageIdx, bool bAllowToIncludeOwnPackage = false)
	{
		for (int32 Dependency : Dependencies)
		{
			const int32 PackageIdx = ObjectArray::GetByIndex(Dependency).GetPackageIndex();


			if (bAllowToIncludeOwnPackage || PackageIdx != StructPackageIdx)
			{
				RequirementInfo& ReqInfo = DependencyTracker[PackageIdx];
				ReqInfo.PackageIdx = PackageIdx;
				ReqInfo.bShouldIncludeStructs = true; // 依赖项只包含在 "PackageName_structs.hpp" 文件中的结构体/枚举。
			}
		}
	}

	// 专门为枚举添加包依赖，因为枚举总是在 _structs.hpp 文件中。
	inline void AddEnumPackageDependencies(DependencyListType& DependencyTracker, const std::unordered_set<int32>& Dependencies, int32 StructPackageIdx, bool bAllowToIncludeOwnPackage = false)
	{
		for (int32 Dependency : Dependencies)
		{
			UEObject DependencyObject = ObjectArray::GetByIndex(Dependency);

			if (!DependencyObject.IsA(EClassCastFlags::Enum))
				continue;

			const int32 PackageIdx = DependencyObject.GetPackageIndex();

			if (bAllowToIncludeOwnPackage || PackageIdx != StructPackageIdx)
			{
				RequirementInfo& ReqInfo = DependencyTracker[PackageIdx];
				ReqInfo.PackageIdx = PackageIdx;
				ReqInfo.bShouldIncludeStructs = true; // 依赖项只包含在 "PackageName_structs.hpp" 文件中的枚举。
			}
		}
	}

	// 添加结构体之间的包内依赖关系，用于拓扑排序。
	inline void AddStructDependencies(DependencyManager& StructDependencies, const std::unordered_set<int32>& Dependenies, int32 StructIdx, int32 StructPackageIndex)
	{
		std::unordered_set<int32> TempSet;

		for (int32 DependencyStructIdx : Dependenies)
		{
			UEObject Obj = ObjectArray::GetByIndex(DependencyStructIdx);
			
			// 只处理同一包内的依赖关系。
			if (Obj.GetPackageIndex() == StructPackageIndex && !Obj.IsA(EClassCastFlags::Enum))
				TempSet.insert(DependencyStructIdx);
		}

		StructDependencies.SetDependencies(StructIdx, std::move(TempSet));
	}
}

// 初始化依赖关系。遍历所有对象，为每个包构建其内部和外部的依赖列表。
void PackageManager::InitDependencies()
{
	// 收集编译此文件所需的所有包

	for (auto Obj : ObjectArray())
	{
		if (Obj.HasAnyFlags(EObjectFlags::ClassDefaultObject))
			continue;

		int32 CurrentPackageIdx = Obj.GetPackageIndex();

		const bool bIsStruct = Obj.IsA(EClassCastFlags::Struct);
		const bool bIsClass = Obj.IsA(EClassCastFlags::Class);

		const bool bIsFunction = Obj.IsA(EClassCastFlags::Function);
		const bool bIsEnum = Obj.IsA(EClassCastFlags::Enum);

		if (bIsStruct && !bIsFunction)
		{
			PackageInfo& Info = PackageInfos[CurrentPackageIdx];
			Info.PackageIndex = CurrentPackageIdx;

			UEStruct ObjAsStruct = Obj.Cast<UEStruct>();

			const int32 StructIdx = ObjAsStruct.GetIndex();
			const int32 StructPackageIdx = ObjAsStruct.GetPackageIndex();

			DependencyListType& PackageDependencyList = bIsClass ? Info.PackageDependencies.ClassesDependencies : Info.PackageDependencies.StructsDependencies;
			DependencyManager& ClassOrStructDependencyList = bIsClass ? Info.ClassesSorted : Info.StructsSorted;

			std::unordered_set<int32> Dependencies = PackageManagerUtils::GetDependencies(ObjAsStruct, StructIdx);

			ClassOrStructDependencyList.SetExists(StructIdx);

			PackageManagerUtils::SetPackageDependencies(PackageDependencyList, Dependencies, StructPackageIdx, bIsClass);

			if (!bIsClass)
				PackageManagerUtils::AddStructDependencies(ClassOrStructDependencyList, Dependencies, StructIdx, StructPackageIdx);

			/* 对结构体和类都进行处理 */
			if (UEStruct Super = ObjAsStruct.GetSuper())
			{
				const int32 SuperPackageIdx = Super.GetPackageIndex();

				if (SuperPackageIdx == StructPackageIdx)
				{
					/* 只有当父类在同一个包内时，才需要在文件内进行排序 */
					ClassOrStructDependencyList.AddDependency(Obj.GetIndex(), Super.GetIndex());
				}
				else
				{
					/* 一个包不能依赖于自身，结构体的父类总是在_structs文件中，类也是如此 */
					RequirementInfo& ReqInfo = PackageDependencyList[SuperPackageIdx];
					BooleanOrEqual(ReqInfo.bShouldIncludeStructs, !bIsClass);
					BooleanOrEqual(ReqInfo.bShouldIncludeClasses, bIsClass);
				}
			}

			if (!bIsClass)
				continue;
			
			/* 将类的函数添加到包中 */
			for (UEFunction Func : ObjAsStruct.GetFunctions())
			{
				Info.Functions.push_back(Func.GetIndex());

				std::unordered_set<int32> ParamDependencies = PackageManagerUtils::GetDependencies(Func, Func.GetIndex());

				BooleanOrEqual(Info.bHasParams, Func.HasMembers());

				const int32 FuncPackageIndex = Func.GetPackageIndex();

				/* 将依赖项添加到参数依赖中，并且只将枚举添加到类依赖中（枚举类的前向声明默认为int） */
				PackageManagerUtils::SetPackageDependencies(Info.PackageDependencies.ParametersDependencies, ParamDependencies, FuncPackageIndex, true);
				PackageManagerUtils::AddEnumPackageDependencies(Info.PackageDependencies.ClassesDependencies, ParamDependencies, FuncPackageIndex, true);
			}
		}
		else if (bIsEnum)
		{
			PackageInfo& Info = PackageInfos[CurrentPackageIdx];
			Info.PackageIndex = CurrentPackageIdx;

			Info.Enums.push_back(Obj.GetIndex());
		}
	}
}

// 初始化包的名称，处理潜在的命名冲突。
void PackageManager::InitNames()
{
	for (auto& [PackageIdx, Info] : PackageInfos)
	{
		std::string PackageName = ObjectArray::GetByIndex(PackageIdx).GetValidName();

		auto [Name, bWasInserted] = UniquePackageNameTable.FindOrAdd(PackageName);
		Info.Name = Name;

		if (!bWasInserted) [[unlikely]]
			Info.CollisionCount = UniquePackageNameTable[Name].GetCollisionCount().CollisionCount;
	}
}

// 辅助函数，当检测到循环依赖时，标记一个包中的结构体依赖为需要循环修复。
void PackageManager::HelperMarkStructDependenciesOfPackage(UEStruct Struct, int32 OwnPackageIdx, int32 RequiredPackageIdx, bool bIsClass)
{
	if (UEStruct Super = Struct.GetSuper())
	{
		if (Super.GetPackageIndex() == RequiredPackageIdx)
			StructManager::PackageManagerSetCycleForStruct(Super.GetIndex(), OwnPackageIdx);
	}

	if (bIsClass)
		return;

	for (UEProperty Child : Struct.GetProperties())
	{
		if (!Child.IsA(EClassCastFlags::StructProperty))
			continue;

		const UEStruct UnderlayingStruct = Child.Cast<UEStructProperty>().GetUnderlayingStruct();

		if (UnderlayingStruct.GetPackageIndex() == RequiredPackageIdx)
			StructManager::PackageManagerSetCycleForStruct(UnderlayingStruct.GetIndex(), OwnPackageIdx);
	}
}
// 辅助函数，计算一个结构体对另一个包的依赖数量。
int32 PackageManager::HelperCountStructDependenciesOfPackage(UEStruct Struct, int32 RequiredPackageIdx, bool bIsClass)
{
	int32 RetCount = 0x0;

	if (UEStruct Super = Struct.GetSuper())
	{
		if (Super.GetPackageIndex() == RequiredPackageIdx)
			RetCount++;
	}

	if (bIsClass)
		return RetCount;

	for (UEProperty Child : Struct.GetProperties())
	{
		if (!Child.IsA(EClassCastFlags::StructProperty))
			continue;

		const int32 UnderlayingStructPackageIdx = Child.Cast<UEStructProperty>().GetUnderlayingStruct().GetPackageIndex();

		if (UnderlayingStructPackageIdx == RequiredPackageIdx)
			RetCount++;
	}

	return RetCount;
}

// 辅助函数，将被依赖包中的枚举添加到前向声明列表。
void PackageManager::HelperAddEnumsFromPacakageToFwdDeclarations(UEStruct Struct, std::vector<std::pair<int32, bool>>& EnumsToForwardDeclare, int32 RequiredPackageIdx, bool bMarkAsClass)
{
	for (UEProperty Child : Struct.GetProperties())
	{
		const bool bIsEnumPrperty = Child.IsA(EClassCastFlags::EnumProperty);
		const bool bIsBytePrperty = Child.IsA(EClassCastFlags::ByteProperty);

		if (!bIsEnumPrperty && !bIsBytePrperty)
			continue;

		const UEObject UnderlayingEnum = bIsEnumPrperty ? Child.Cast<UEEnumProperty>().GetEnum() : Child.Cast<UEByteProperty>().GetEnum();

		if (UnderlayingEnum && UnderlayingEnum.GetPackageIndex() == RequiredPackageIdx)
			EnumsToForwardDeclare.emplace_back(UnderlayingEnum.GetIndex(), bMarkAsClass);
	}
}

// 辅助函数，为解决循环依赖的包初始化其所有需要的枚举前向声明。
void PackageManager::HelperInitEnumFwdDeclarationsForPackage(int32 PackageForFwdDeclarations, int32 RequiredPackage, bool bIsClass)
{
	PackageInfo& Info = PackageInfos.at(PackageForFwdDeclarations);

	std::vector<std::pair<int32, bool>>& EnumsToForwardDeclare = Info.EnumForwardDeclarations;

	DependencyManager::OnVisitCallbackType CheckForEnumsToForwardDeclareCallback = [&EnumsToForwardDeclare, RequiredPackage, bIsClass](int32 Index) -> void
	{
		HelperAddEnumsFromPacakageToFwdDeclarations(ObjectArray::GetByIndex<UEStruct>(Index), EnumsToForwardDeclare, RequiredPackage, bIsClass);
	};

	DependencyManager& Manager = bIsClass ? Info.ClassesSorted : Info.StructsSorted;
	Manager.VisitAllNodesWithCallback(CheckForEnumsToForwardDeclareCallback);

	/* 函数中使用的枚举也需要由类来声明，因为函数声明在类的头文件中 */
	for (const int32 FuncIdx : Info.Functions)
	{
		HelperAddEnumsFromPacakageToFwdDeclarations(ObjectArray::GetByIndex<UEFunction>(FuncIdx), EnumsToForwardDeclare, RequiredPackage, true);
	}
	
	// 排序并去重
	std::sort(EnumsToForwardDeclare.begin(), EnumsToForwardDeclare.end());
	EnumsToForwardDeclare.erase(std::unique(EnumsToForwardDeclare.begin(), EnumsToForwardDeclare.end()), EnumsToForwardDeclare.end());
}

/* 可以安全使用StructManager，保证其已经完成初始化 */
// 处理包之间的循环依赖关系。
void PackageManager::HandleCycles()
{
	struct CycleInfo
	{
		int32 CurrentPackage;
		int32 PreviousPacakge;

		bool bAreStructsCyclic;
		bool bAreclassesCyclic;
	};

	std::vector<CycleInfo> HandledPackages;

	// 当找到循环依赖时的回调函数。
	FindCycleCallbackType CleanedUpOnCycleFoundCallback = [&HandledPackages](const PackageManagerIterationParams& OldParams, const PackageManagerIterationParams& NewParams, bool bIsStruct) -> void
	{
		const int32 CurrentPackageIndex = NewParams.RequiredPackage;
		const int32 PreviousPackageIndex = NewParams.PrevPackage;

		/* 检查此包是否已处理过，如果是则返回 */
		for (const CycleInfo& Cycle : HandledPackages)
		{
			if (((Cycle.CurrentPackage == CurrentPackageIndex && Cycle.PreviousPacakge == PreviousPackageIndex)
				|| (Cycle.CurrentPackage == PreviousPackageIndex && Cycle.PreviousPacakge == CurrentPackageIndex))
				&& (Cycle.bAreStructsCyclic == bIsStruct || Cycle.bAreclassesCyclic == !bIsStruct))
			{
				return;
			}
		}

		/* 当前的循环包将在该函数的后面部分被添加到 'HandledPackages' 中 */


		const PackageInfoHandle CurrentPackageInfo = GetInfo(CurrentPackageIndex);
		const PackageInfoHandle PreviousPackageInfo = GetInfo(PreviousPackageIndex);

		const DependencyManager& CurrentStructsOrClasses = bIsStruct ? CurrentPackageInfo.GetSortedStructs() : CurrentPackageInfo.GetSortedClasses();
		const DependencyManager& PreviousStructsOrClasses = bIsStruct ? PreviousPackageInfo.GetSortedStructs() : PreviousPackageInfo.GetSortedClasses();


		bool bIsMutualInclusion = false;

		if (bIsStruct)
		{
			bIsMutualInclusion = CurrentPackageInfo.GetPackageDependencies().StructsDependencies.contains(PreviousPackageIndex) 
				&& PreviousPackageInfo.GetPackageDependencies().StructsDependencies.contains(CurrentPackageIndex);
		}
		else
		{
			bIsMutualInclusion = CurrentPackageInfo.GetPackageDependencies().ClassesDependencies.contains(PreviousPackageIndex)
				&& PreviousPackageInfo.GetPackageDependencies().ClassesDependencies.contains(CurrentPackageIndex);
		}

		/* 使用包之间的依赖数量来决定哪一个需要标记为循环依赖 */
		if (bIsMutualInclusion)
		{
			/* CurrentPackage 需要的 PreviousPackage 中的结构体数量 */
			int32 NumStructsRequiredByCurrent = 0x0;

			DependencyManager::OnVisitCallbackType CountDependenciesForCurrent = [&NumStructsRequiredByCurrent, PreviousPackageIndex, bIsStruct](int32 Index) -> void
			{
				NumStructsRequiredByCurrent += HelperCountStructDependenciesOfPackage(ObjectArray::GetByIndex<UEStruct>(Index), PreviousPackageIndex, !bIsStruct);
			};
			CurrentStructsOrClasses.VisitAllNodesWithCallback(CountDependenciesForCurrent);


			/* PreviousPackage 需要的 CurrentPackage 中的结构体数量 */
			int32 NumStructsRequiredByPrevious = 0x0;

			DependencyManager::OnVisitCallbackType CountDependenciesForPrevious = [&NumStructsRequiredByPrevious, CurrentPackageIndex, bIsStruct](int32 Index) -> void
			{
				NumStructsRequiredByPrevious += HelperCountStructDependenciesOfPackage(ObjectArray::GetByIndex<UEStruct>(Index), CurrentPackageIndex, !bIsStruct);
			};
			PreviousStructsOrClasses.VisitAllNodesWithCallback(CountDependenciesForPrevious);


			/* 两个循环包中哪个需要对方的结构体更少 */
			const bool bCurrentHasMoreDependencies = NumStructsRequiredByCurrent > NumStructsRequiredByPrevious;

			const int32 PackageIndexWithLeastDependencies = bCurrentHasMoreDependencies && bIsStruct ? PreviousPackageIndex : CurrentPackageIndex;
			const int32 PackageIndexToMarkCyclicWith = bCurrentHasMoreDependencies && bIsStruct ? CurrentPackageIndex : PreviousPackageIndex;


			/* 将包添加到已处理列表 */
			HandledPackages.push_back({ PackageIndexWithLeastDependencies, PackageIndexToMarkCyclicWith, bIsStruct, !bIsStruct });


			DependencyManager::OnVisitCallbackType SetCycleCallback = [PackageIndexWithLeastDependencies, PackageIndexToMarkCyclicWith, bIsStruct](int32 Index) -> void
			{
				HelperMarkStructDependenciesOfPackage(ObjectArray::GetByIndex<UEStruct>(Index), PackageIndexToMarkCyclicWith, PackageIndexWithLeastDependencies, !bIsStruct);
			};

			PreviousStructsOrClasses.VisitAllNodesWithCallback(SetCycleCallback);
		}
		else /* 只需将前一个包的结构体/类标记为循环依赖 */
		{
			HandledPackages.push_back({ PreviousPackageIndex, CurrentPackageIndex, bIsStruct, !bIsStruct });

			DependencyManager::OnVisitCallbackType SetCycleCallback = [PreviousPackageIndex, CurrentPackageIndex, bIsStruct](int32 Index) -> void
			{
				HelperMarkStructDependenciesOfPackage(ObjectArray::GetByIndex<UEStruct>(Index), PreviousPackageIndex, CurrentPackageIndex, !bIsStruct);
			};

			PreviousStructsOrClasses.VisitAllNodesWithCallback(SetCycleCallback);
		}
	};

	FindCycle(CleanedUpOnCycleFoundCallback);


	/* 真正地从我们的依赖图中移除循环。之前不能这样做，因为它会使迭代器失效 */
	for (const CycleInfo& Cycle : HandledPackages)
	{
		const PackageInfoHandle CurrentPackageInfo = GetInfo(Cycle.CurrentPackage);
		const PackageInfoHandle PreviousPackageInfo = GetInfo(Cycle.PreviousPacakge);

		/* 向我们移除依赖的包中添加枚举前向声明，因为这些依赖不考虑枚举 */
		HelperInitEnumFwdDeclarationsForPackage(Cycle.CurrentPackage, Cycle.PreviousPacakge, Cycle.bAreStructsCyclic);

		if (Cycle.bAreStructsCyclic)
		{
			CurrentPackageInfo.ErasePackageDependencyFromStructs(Cycle.PreviousPacakge);
			continue;
		}

		const RequirementInfo& CurrentRequirements = CurrentPackageInfo.GetPackageDependencies().ClassesDependencies.at(Cycle.CurrentPackage);

		/* 当这个包是循环的但仍可能需要 _structs.hpp 时，将类标记为 '不包含' */
		if (CurrentRequirements.bShouldIncludeStructs)
		{
			const_cast<RequirementInfo&>(CurrentRequirements).bShouldIncludeClasses = false;
		}
		else
		{
			CurrentPackageInfo.ErasePackageDependencyFromClasses(Cycle.PreviousPacakge);
		}
	}
}

// 初始化PackageManager的主函数。
void PackageManager::Init()
{
	if (bIsInitialized)
		return;

	bIsInitialized = true;

	PackageInfos.reserve(0x800);

	InitDependencies();
	InitNames();
}

// 在所有管理器都初步初始化后调用，用于处理管理器之间的交叉逻辑，如循环依赖。
void PackageManager::PostInit()
{
	if (bIsPostInitialized)
		return;

	bIsPostInitialized = true;
	
	// 必须先初始化StructManager
	StructManager::Init();

	HandleCycles();
}
// 遍历单个依赖项的实现。
void PackageManager::IterateSingleDependencyImplementation(SingleDependencyIterationParamsInternal& Params, bool bCheckForCycle)
{
	if (!Params.bShouldHandlePackage)
		return;

	const bool bIsIncluded = Params.IterationHitCounterRef >= CurrentIterationHitCount;

	if (!bIsIncluded)
	{
		Params.IterationHitCounterRef = CurrentIterationHitCount;

		IncludeData& Include = Params.VisitedNodes[Params.CurrentIndex];
		Include.bIncludedStructs = (Include.bIncludedStructs || Params.bIsStruct);
		Include.bIncludedClasses = (Include.bIncludedClasses || !Params.bIsStruct);

		for (auto& [Index, Requirements] : Params.Dependencies)
		{
			Params.NewParams.bWasPrevNodeStructs = Params.bIsStruct;
			Params.NewParams.bRequiresClasses = Requirements.bShouldIncludeClasses;
			Params.NewParams.bRequiresStructs = Requirements.bShouldIncludeStructs;
			Params.NewParams.RequiredPackage = Requirements.PackageIdx;

			/* 递归遍历依赖项 */
			IterateDependenciesImplementation(Params.NewParams, Params.CallbackForEachPackage, Params.OnFoundCycle, bCheckForCycle);
		}

		Params.VisitedNodes.erase(Params.CurrentIndex);

		// 执行操作
		Params.CallbackForEachPackage(Params.NewParams, Params.OldParams, Params.bIsStruct);
		return;
	}

	if (bCheckForCycle)
	{
		auto It = Params.VisitedNodes.find(Params.CurrentIndex);
		if (It != Params.VisitedNodes.end())
		{
			if ((It->second.bIncludedStructs && Params.bIsStruct) || (It->second.bIncludedClasses && !Params.bIsStruct))
				Params.OnFoundCycle(Params.NewParams, Params.OldParams, Params.bIsStruct);
		}
	}
}
// 遍历依赖关系的递归实现。
void PackageManager::IterateDependenciesImplementation(const PackageManagerIterationParams& Params, const IteratePackagesCallbackType& CallbackForEachPackage, const FindCycleCallbackType& OnFoundCycle, bool bCheckForCycle)
{
	PackageManagerIterationParams NewParams = {
		.PrevPackage = Params.RequiredPackage,

		.VisitedNodes = Params.VisitedNodes,
	};

	DependencyInfo& Dependencies = PackageInfos.at(Params.RequiredPackage).PackageDependencies;

	SingleDependencyIterationParamsInternal StructsParams{
		.CallbackForEachPackage = CallbackForEachPackage,
		.OnFoundCycle = OnFoundCycle,

		.NewParams = NewParams,
		.OldParams = Params,
		.Dependencies = Dependencies.StructsDependencies,
		.VisitedNodes = Params.VisitedNodes,

		.CurrentIndex = Params.RequiredPackage,
		.PrevIndex = Params.PrevPackage,
		.IterationHitCounterRef = Dependencies.StructsIterationHitCount,

		.bShouldHandlePackage = Params.bRequiresStructs,
		.bIsStruct = true,
	};

	SingleDependencyIterationParamsInternal ClassesParams{
		.CallbackForEachPackage = CallbackForEachPackage,
		.OnFoundCycle = OnFoundCycle,

		.NewParams = NewParams,
		.OldParams = Params,
		.Dependencies = Dependencies.ClassesDependencies,
		.VisitedNodes = Params.VisitedNodes,

		.CurrentIndex = Params.RequiredPackage,
		.PrevIndex = Params.PrevPackage,
		.IterationHitCounterRef = Dependencies.ClassesIterationHitCount,

		.bShouldHandlePackage = Params.bRequiresClasses,
		.bIsStruct = false,
	};

	IterateSingleDependencyImplementation(StructsParams, bCheckForCycle);
	IterateSingleDependencyImplementation(ClassesParams, bCheckForCycle);
}

// 公开接口，用于以拓扑顺序遍历所有包及其依赖项。
void PackageManager::IterateDependencies(const IteratePackagesCallbackType& CallbackForEachPackage)
{
	VisitedNodeContainerType VisitedNodes;

	PackageManagerIterationParams Params = {
		.PrevPackage = -1,

		.VisitedNodes = VisitedNodes,
	};

	FindCycleCallbackType OnCycleFoundCallback = [](const PackageManagerIterationParams& OldParams, const PackageManagerIterationParams& NewParams, bool bIsStruct) -> void { };

	/* 为新的迭代周期增加命中计数器 */
	CurrentIterationHitCount++;

	for (const auto& [PackageIndex, Info] : PackageInfos)
	{
		Params.RequiredPackage = PackageIndex;
		Params.bWasPrevNodeStructs = true;
		Params.bRequiresClasses = true;
		Params.bRequiresStructs = true;
		Params.VisitedNodes.clear();

		IterateDependenciesImplementation(Params, CallbackForEachPackage, OnCycleFoundCallback, false);
	}
}

// 公开接口，用于查找所有循环依赖。
void PackageManager::FindCycle(const FindCycleCallbackType& OnFoundCycle)
{
	VisitedNodeContainerType VisitedNodes;

	PackageManagerIterationParams Params = {
		.PrevPackage = -1,

		.VisitedNodes = VisitedNodes,
	};

	FindCycleCallbackType CallbackForEachPackage = [](const PackageManagerIterationParams& OldParams, const PackageManagerIterationParams& NewParams, bool bIsStruct) -> void {};

	/* 为新的迭代周期增加命中计数器 */
	CurrentIterationHitCount++;

	for (const auto& [PackageIndex, Info] : PackageInfos)
	{
		Params.RequiredPackage = PackageIndex;
		Params.bWasPrevNodeStructs = true;
		Params.bRequiresClasses = true;
		Params.bRequiresStructs = true;
		Params.VisitedNodes.clear();

		IterateDependenciesImplementation(Params, CallbackForEachPackage, OnFoundCycle, true);
	}
}

