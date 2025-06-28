#include "Managers/CollisionManager.h"

/*
* CollisionManager.cpp 负责解决在C++代码生成过程中可能出现的命名冲突问题。
*
* 在虚幻引擎中，一个类可以包含与父类同名的成员，或者函数名与成员变量名相同。
* 这些在UObject系统中是允许的，但在C++中会导致编译错误。此类通过检测这些
* 冲突，并智能地重命名变量、函数或参数（例如，通过添加后缀 "_Func" 或数字）
* 来确保生成的代码是有效且无冲突的。
*
* 它通过为每个结构体（包括其继承链）维护一个名称列表来实现这一点，并检查
* 新添加的名称是否与现有名称、父类名称或保留关键字冲突。
*/


NameInfo::NameInfo(HashStringTableIndex NameIdx, ECollisionType CurrentType)
	: Name(NameIdx), CollisionData(0x0)
{
	// 成员初始化列表的顺序不被保证，因此在 "CollisionData" 初始化之后再初始化 "OwnType"。
	OwnType = static_cast<uint8>(CurrentType);
}

void NameInfo::InitCollisionData(const NameInfo& Existing, ECollisionType CurrentType, bool bIsSuper)
{
	CollisionData = Existing.CollisionData;
	OwnType = static_cast<uint8>(CurrentType);

	switch (CurrentType)
	{
	case ECollisionType::MemberName:
		if (bIsSuper)
		{
			SuperMemberNameCollisionCount++;
			return;
		}
		MemberNameCollisionCount++;
		break;
	case ECollisionType::FunctionName:
		if (bIsSuper)
		{
			SuperFuncNameCollisionCount++;
			return;
		}
		FunctionNameCollisionCount++;
		break;
	case ECollisionType::ParameterName:
		ParamNameCollisionCount++;
		break;
	default:
		break;
	}
}

bool NameInfo::HasCollisions() const
{
	ECollisionType OwnCollisionType = static_cast<ECollisionType>(OwnType);

	if (OwnCollisionType == ECollisionType::MemberName)
	{
		return SuperMemberNameCollisionCount > 0x0 || MemberNameCollisionCount > 0x0;
	}
	else if (OwnCollisionType == ECollisionType::FunctionName)
	{
		return MemberNameCollisionCount > 0x0 || SuperMemberNameCollisionCount > 0x0 || FunctionNameCollisionCount > 0x0;
	}
	else if (OwnCollisionType == ECollisionType::ParameterName)
	{
		return MemberNameCollisionCount > 0x0 || SuperMemberNameCollisionCount > 0x0 || FunctionNameCollisionCount > 0x0 || SuperFuncNameCollisionCount > 0x0 || ParamNameCollisionCount > 0x0;
	}

	return false;
}

std::string NameInfo::DebugStringify() const
{
	return std::format(R"(
OwnType: {};
MemberNameCollisionCount: {};
SuperMemberNameCollisionCount: {};
FunctionNameCollisionCount: {};
SuperFuncNameCollisionCount: {};
ParamNameCollisionCount: {};
)", StringifyCollisionType(static_cast<ECollisionType>(OwnType)), MemberNameCollisionCount, SuperMemberNameCollisionCount, FunctionNameCollisionCount, SuperFuncNameCollisionCount, ParamNameCollisionCount);
}

uint64 KeyFunctions::GetKeyForCollisionInfo(UEStruct Super, UEProperty Member)
{
	uint64 Key = 0x0;

	FName Name = Member.GetFName();
	Key += Name.GetCompIdx();
	Key += Name.GetNumber();

	Key <<= 32;
	Key |= (static_cast<uint64>(Member.GetOffset()) << 24);
	Key |= (static_cast<uint64>(Member.GetSize()) << 16);
	Key |= Super.GetIndex();

	return reinterpret_cast<uint64>(Member.GetAddress());
}

uint64 KeyFunctions::GetKeyForCollisionInfo([[maybe_unused]] UEStruct Super, UEFunction Member)
{
	uint64 Key = 0x0;

	FName Name = Member.GetFName();
	Key += Name.GetCompIdx();
	Key += Name.GetNumber();

	Key <<= 32;
	Key |= Member.GetIndex();

	return Key;
}

uint64 CollisionManager::AddNameToContainer(NameContainer& StructNames, UEStruct Struct, std::pair<HashStringTableIndex, bool>&& NamePair, ECollisionType CurrentType, bool bIsStruct, UEFunction Func)
{
	// 静态lambda，用于在给定的搜索列表（SearchNames）中查找冲突，并将结果添加到目标列表（OutTargetNames）。
	static auto AddCollidingName = [](const NameContainer& SearchNames, NameContainer* OutTargetNames, HashStringTableIndex NameIdx, ECollisionType CurrentType, bool bIsSuper) -> bool
	{
		assert(OutTargetNames && "Target container was nullptr!");

		NameInfo NewInfo(NameIdx, CurrentType);
		NewInfo.OwnType = static_cast<uint8>(CurrentType);

		for (auto RevIt = SearchNames.crbegin(); RevIt != SearchNames.crend(); ++RevIt)
		{
			const NameInfo& ExistingName = *RevIt;

			if (ExistingName.Name != NameIdx)
				continue;

			NewInfo.InitCollisionData(ExistingName, CurrentType, bIsSuper);
			OutTargetNames->push_back(NewInfo);

			return true;
		}

		return false;
	};

	const bool bIsParameter = CurrentType == ECollisionType::ParameterName;

	auto [NameIdx, bWasInserted] = NamePair;

	if (bWasInserted && !bIsParameter)
	{
		// 如果名称是新的且不是参数，直接创建一个空的NameInfo并添加。
		StructNames.emplace_back(NameIdx, CurrentType);
		return StructNames.size() - 1;
	}

	NameContainer* FuncParamNames = nullptr;

	if (Func)
	{
		FuncParamNames = &NameInfos[Func.GetIndex()];

		if (bWasInserted && bIsParameter)
		{
			// 如果名称是新的且是参数，添加到函数的参数名称列表中。
			FuncParamNames->emplace_back(NameIdx, CurrentType);
			return FuncParamNames->size() - 1;
		}
		// 检查与函数自身参数的冲突。
		if (AddCollidingName(*FuncParamNames, FuncParamNames, NameIdx, CurrentType, false))
			return FuncParamNames->size() - 1;

		if (bIsStruct)
		{
			/* 最后搜索保留名称，以防有属性也与保留名称冲突。 */
			if (AddCollidingName(ReservedNames, FuncParamNames, NameIdx, CurrentType, false))
				return FuncParamNames->size() - 1;
		}
	}

	NameContainer* TargetNameContainer = bIsParameter ? FuncParamNames : &StructNames;

	/* 检查当前结构体的所有成员名，看是否与其中之一冲突。 */
	if (AddCollidingName(StructNames, TargetNameContainer, NameIdx, CurrentType, false))
		return TargetNameContainer->size() - 1;

	/* 这个可能重复的名称没有出现在结构体自身的名称列表中，因此检查所有父类，看是否与父类的名称冲突。 */
	for (UEStruct Current = Struct.GetSuper(); Current; Current = Current.GetSuper())
	{
		NameContainer& SuperNames = NameInfos[Current.GetIndex()];

		if (AddCollidingName(SuperNames, TargetNameContainer, NameIdx, CurrentType, true))
			return TargetNameContainer->size() - 1;
	}

	if (!bIsStruct)
	{
		/* 最后搜索保留名称，以防父类中有预定义的成员或局部变量与之冲突。 */
		if (AddCollidingName(ClassReservedNames, TargetNameContainer, NameIdx, CurrentType, false))
			return TargetNameContainer->size() - 1;
	}

	/* 最后搜索保留名称，以防结构体或父结构体中的属性已经与保留名称冲突。 */
	if (AddCollidingName(ReservedNames, TargetNameContainer, NameIdx, CurrentType, false))
		return TargetNameContainer->size() - 1;

	/* 搜索当前结构体的名称列表、父类的名称列表以及保留名称列表都没有结果。此名称没有冲突，添加它！ */
	if (bIsParameter && FuncParamNames)
	{
		FuncParamNames->emplace_back(NameIdx, CurrentType);
		return FuncParamNames->size() - 1;
	}
	else
	{
		StructNames.emplace_back(NameIdx, CurrentType);
		return StructNames.size() - 1;
	}
}

void CollisionManager::AddReservedClassName(const std::string& Name, bool bIsParameterOrLocalVariable)
{
	NameInfo NewInfo;
	NewInfo.Name = MemberNames.FindOrAdd(Name).first;
	NewInfo.CollisionData = 0x0;
	NewInfo.OwnType = static_cast<uint8>(bIsParameterOrLocalVariable ? ECollisionType::ParameterName : ECollisionType::SuperMemberName);

	ClassReservedNames.push_back(NewInfo);
}

void CollisionManager::AddReservedName(const std::string& Name)
{
	NameInfo NewInfo;
	NewInfo.Name = MemberNames.FindOrAdd(Name).first;
	NewInfo.CollisionData = 0x0;
	NewInfo.OwnType = static_cast<uint8>(ECollisionType::MemberName);

	ReservedNames.push_back(NewInfo);
}

void CollisionManager::AddStructToNameContainer(UEStruct Struct, bool bIsStruct)
{
	if (UEStruct Super = Struct.GetSuper())
	{
		if (NameInfos.find(Super.GetIndex()) == NameInfos.end())
			AddStructToNameContainer(Super, bIsStruct);
	}

	NameContainer& StructNames = NameInfos[Struct.GetIndex()];

	if (!StructNames.empty())
		return;

	// Lambda函数，用于将成员添加到容器并更新翻译映射表。
	auto AddToContainerAndTranslationMap = [&](auto Member, ECollisionType CollisionType, bool bIsStruct, UEFunction Func = nullptr) -> void
	{
		const uint64 Index = AddNameToContainer(StructNames, Struct, MemberNames.FindOrAdd(Member.GetValidName()), CollisionType, bIsStruct, Func);

		const auto [It, bInserted] = TranslationMap.emplace(KeyFunctions::GetKeyForCollisionInfo(Struct, Member), Index);
		
		if (!bInserted)
			std::cerr << "Error, no insertion took place, key {0x" << std::hex << KeyFunctions::GetKeyForCollisionInfo(Struct, Member) << "} duplicated!" << std::endl;
	};
	// 遍历并添加所有属性（成员变量）。
	for (UEProperty Prop : Struct.GetProperties())
		AddToContainerAndTranslationMap(Prop, ECollisionType::MemberName, bIsStruct);
	// 遍历并添加所有函数及其参数。
	for (UEFunction Func : Struct.GetFunctions())
	{
		AddToContainerAndTranslationMap(Func, ECollisionType::FunctionName, bIsStruct);

		for (UEProperty Prop : Func.GetProperties())
			AddToContainerAndTranslationMap(Prop, ECollisionType::ParameterName, bIsStruct, Func);
	}
};

std::string CollisionManager::StringifyName(UEStruct Struct, NameInfo Info)
{
	ECollisionType OwnCollisionType = static_cast<ECollisionType>(Info.OwnType);

	std::string Name = MemberNames.GetStringEntry(Info.Name).GetName();

	//std::cerr << "Nm: " << Name << "\nInfo:" << Info.DebugStringify() << "\n";

	// 注意：内部if语句的顺序很重要。
	if (OwnCollisionType == ECollisionType::MemberName)
	{
		if (Info.SuperMemberNameCollisionCount > 0x0)
		{
			Name += ("_" + Struct.GetValidName());
		}
		if (Info.MemberNameCollisionCount > 0x0)
		{
			Name += ("_" + std::to_string(Info.MemberNameCollisionCount - 1));
		}
	}
	else if (OwnCollisionType == ECollisionType::FunctionName)
	{
		if (Info.MemberNameCollisionCount > 0x0 || Info.SuperMemberNameCollisionCount > 0x0)
		{
			Name = ("Func_" + Name);
		}
		if (Info.FunctionNameCollisionCount > 0x0)
		{
			Name += ("_" + std::to_string(Info.FunctionNameCollisionCount - 1));
		}
	}
	else if (OwnCollisionType == ECollisionType::ParameterName)
	{
		if (Info.MemberNameCollisionCount > 0x0 || Info.SuperMemberNameCollisionCount > 0x0 || Info.FunctionNameCollisionCount > 0x0 || Info.SuperFuncNameCollisionCount > 0x0)
		{
			Name = ("Param_" + Name);
		}
		if (Info.ParamNameCollisionCount > 0x0)
		{
			Name += ("_" + std::to_string(Info.ParamNameCollisionCount - 1));
		}
	}

	return Name;
}