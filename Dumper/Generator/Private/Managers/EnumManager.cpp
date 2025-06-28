#include "Managers/EnumManager.h"

/*
* EnumManager.cpp 负责管理和处理游戏中的所有UEnum（虚幻引擎枚举）类型。
*
* 它的主要任务是在代码生成之前，收集所有枚举的信息，包括它们的名称、底层
* 类型大小以及所有成员（值和名称）。
*
* 此管理器特别处理了几个关键问题：
* 1. 确定枚举的正确底层大小（如uint8, uint16等），这通过分析使用枚举的
*    属性或枚举值的最大范围来实现。
* 2. 解决枚举成员之间的命名冲突。如果同一个枚举中有两个成员具有相同的名称
*    （在去掉命名空间前缀后），管理器会通过添加数字后缀来确保其唯一性。
* 3. 过滤掉C++中的非法标识符（如关键字 "DELETE" 或常见宏 "TRUE"），
*    避免生成的代码出现编译错误。
*/

namespace EnumInitHelper
{
	// 模板函数，获取指定类型T的最大值。
	template<typename T>
	constexpr inline uint64 GetMaxOfType()
	{
		return (1ull << (sizeof(T) * 0x8ull)) - 1;
	}

	// 根据枚举值的大小，更新枚举底层类型的大小。
	void SetEnumSizeForValue(uint8& Size, uint64 EnumValue)
	{
		if (EnumValue > GetMaxOfType<uint32>()) {
			Size = 0x8;
		}
		else if (EnumValue > GetMaxOfType<uint16>()) {
			Size = max(Size, 0x4);
		}
		else if (EnumValue > GetMaxOfType<uint8>()) {
			Size = max(Size, 0x2);
		}
		else {
			Size = max(Size, 0x1);
		}
	}
}

// 获取枚举成员的唯一名称。如果存在冲突，则附加一个数字后缀。
std::string EnumCollisionInfo::GetUniqueName() const
{
	const std::string Name = EnumManager::GetValueName(*this).GetName();

	if (CollisionCount > 0)
		return Name + "_" + std::to_string(CollisionCount - 1);

	return Name;
}

// 获取枚举成员的原始（未处理冲突的）名称。
std::string EnumCollisionInfo::GetRawName() const
{
	return EnumManager::GetValueName(*this).GetName();
}

// 获取枚举成员的整数值。
uint64 EnumCollisionInfo::GetValue() const
{
	return MemberValue;
}

// 获取此名称的冲突次数。
uint8 EnumCollisionInfo::GetCollisionCount() const
{
	return CollisionCount;
}

EnumInfoHandle::EnumInfoHandle(const EnumInfo& InInfo)
	: Info(&InInfo)
{
}

// 获取枚举的底层类型大小（以字节为单位）。
uint8 EnumInfoHandle::GetUnderlyingTypeSize() const
{
	return Info->UnderlyingTypeSize;
}

// 获取枚举的名称。
const StringEntry& EnumInfoHandle::GetName() const
{
	return EnumManager::GetEnumName(*Info);
}

// 获取枚举的成员数量。
int32 EnumInfoHandle::GetNumMembers() const
{
	return Info->MemberInfos.size();
}

// 获取用于遍历所有成员碰撞信息的迭代器。
CollisionInfoIterator EnumInfoHandle::GetMemberCollisionInfoIterator() const
{
	return CollisionInfoIterator(Info->MemberInfos);
}

// 内部初始化函数，遍历所有UObject以收集和处理枚举信息。
void EnumManager::InitInternal()
{
	for (auto Obj : ObjectArray())
	{
		if (Obj.HasAnyFlags(EObjectFlags::ClassDefaultObject))
			continue;

		if (Obj.IsA(EClassCastFlags::Struct))
		{
			UEStruct ObjAsStruct = Obj.Cast<UEStruct>();
			
			// 遍历结构体的属性，寻找枚举属性（EnumProperty）和字节属性（ByteProperty），
			// 以此来确定枚举的实际底层大小。
			for (UEProperty Property : ObjAsStruct.GetProperties())
			{
				if (!Property.IsA(EClassCastFlags::EnumProperty) && !Property.IsA(EClassCastFlags::ByteProperty))
					continue;

				UEEnum Enum = nullptr;
				UEProperty UnderlayingProperty = nullptr;

				if (Property.IsA(EClassCastFlags::EnumProperty))
				{
					Enum = Property.Cast<UEEnumProperty>().GetEnum();
					UnderlayingProperty = Property.Cast<UEEnumProperty>().GetUnderlayingProperty();

					if (!UnderlayingProperty)
						continue;
				}
				else /* ByteProperty */
				{
					Enum = Property.Cast<UEByteProperty>().GetEnum();
					UnderlayingProperty = Property;
				}

				if (!Enum)
					continue;

				EnumInfo& Info = EnumInfoOverrides[Enum.GetIndex()];

				Info.bWasInstanceFound = true;
				Info.UnderlyingTypeSize = 0x1;

				/* 检查此枚举的底层类型大小是否大于默认大小（1字节） */
				if (Enum)
				{
					Info.UnderlyingTypeSize = Property.GetSize();
					continue;
				}

				if (UnderlayingProperty)
				{
					Info.UnderlyingTypeSize = UnderlayingProperty.GetSize();
					continue;
				}
			}
		}
		else if (Obj.IsA(EClassCastFlags::Enum))
		{
			UEEnum ObjAsEnum = Obj.Cast<UEEnum>();

			/* 将枚举名称添加到覆盖信息中 */
			EnumInfo& NewOrExistingInfo = EnumInfoOverrides[Obj.GetIndex()];
			NewOrExistingInfo.Name = UniqueEnumNameTable.FindOrAdd(ObjAsEnum.GetEnumPrefixedName()).first;

			uint64 EnumMaxValue = 0x0;

			/* 初始化枚举成员的名称及其碰撞信息 */
			std::vector<std::pair<FName, int64>> NameValuePairs = ObjAsEnum.GetNameValuePairs();
			for (int i = 0; i < NameValuePairs.size(); i++)
			{
				auto& [Name, Value] = NameValuePairs[i];

				std::wstring NameWitPrefix = Name.ToWString();
				
				// 记录除"_MAX"之外的最大枚举值，用于推断大小。
				if (!NameWitPrefix.ends_with(L"_MAX"))
					EnumMaxValue = max(EnumMaxValue, Value);

				auto [NameIndex, bWasInserted] = UniqueEnumValueNames.FindOrAdd(MakeNameValid(NameWitPrefix.substr(NameWitPrefix.find_last_of(L"::") + 1)));

				EnumCollisionInfo CurrentEnumValueInfo;
				CurrentEnumValueInfo.MemberName = NameIndex;
				CurrentEnumValueInfo.MemberValue = Value;

				if (bWasInserted) [[likely]]
				{
					NewOrExistingInfo.MemberInfos.push_back(CurrentEnumValueInfo);
					continue;
				}

				/* 全局存在具有此名称的值，现在检查它是否也在本地（即在此枚举中）重复 */
				for (int j = 0; j < i; j++)
				{
					EnumCollisionInfo& CrosscheckedInfo = NewOrExistingInfo.MemberInfos[j];

					if (CrosscheckedInfo.MemberName != NameIndex) [[likely]]
						continue;

					/* 发现重复项 */
					CurrentEnumValueInfo.CollisionCount = CrosscheckedInfo.CollisionCount + 1;
					break;
				}

				/* 检查此名称是否为非法名称 */
				for (HashStringTableIndex IllegalIndex : IllegalNames)
				{
					if (NameIndex == IllegalIndex) [[unlikely]]
					{
						CurrentEnumValueInfo.CollisionCount++;
						break;
					}
				}

				NewOrExistingInfo.MemberInfos.push_back(CurrentEnumValueInfo);
			}

			/* 如果没有通过实例属性确定大小，则根据枚举包含的最大值来初始化大小 */
			if (!NewOrExistingInfo.bWasEnumSizeInitialized && !NewOrExistingInfo.bWasInstanceFound)
			{
				EnumInitHelper::SetEnumSizeForValue(NewOrExistingInfo.UnderlyingTypeSize, EnumMaxValue);
				NewOrExistingInfo.bWasEnumSizeInitialized = true;
			}
		}
	}
}

// 初始化一个包含非法枚举成员名称的列表。
// 这些名称通常是C++关键字或常用宏，不能用作标识符。
void EnumManager::InitIllegalNames()
{
	IllegalNames.push_back(UniqueEnumValueNames.FindOrAdd("IN").first);
	IllegalNames.push_back(UniqueEnumValueNames.FindOrAdd("OUT").first);
	IllegalNames.push_back(UniqueEnumValueNames.FindOrAdd("TRUE").first);
	IllegalNames.push_back(UniqueEnumValueNames.FindOrAdd("FALSE").first);
	IllegalNames.push_back(UniqueEnumValueNames.FindOrAdd("DELETE").first);
	IllegalNames.push_back(UniqueEnumValueNames.FindOrAdd("PF_MAX").first);
	IllegalNames.push_back(UniqueEnumValueNames.FindOrAdd("SW_MAX").first);
	IllegalNames.push_back(UniqueEnumValueNames.FindOrAdd("MM_MAX").first);
	IllegalNames.push_back(UniqueEnumValueNames.FindOrAdd("SIZE_MAX").first);
	IllegalNames.push_back(UniqueEnumValueNames.FindOrAdd("RELATIVE").first);
	IllegalNames.push_back(UniqueEnumValueNames.FindOrAdd("TRANSPARENT").first);
}

// 公开的初始化函数，确保初始化过程只执行一次。
void EnumManager::Init()
{
	if (bIsInitialized)
		return;

	bIsInitialized = true;

	EnumInfoOverrides.reserve(0x1000);

	InitIllegalNames(); // 必须先调用此函数
	InitInternal();
}