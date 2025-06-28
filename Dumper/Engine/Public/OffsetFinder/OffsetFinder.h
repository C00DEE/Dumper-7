#pragma once

#include <vector>

#include "Unreal/ObjectArray.h"

// 偏移量查找器命名空间
namespace OffsetFinder
{
	// 表示未找到偏移量的常量
	constexpr int32 OffsetNotFound = -1;

	// 模板函数，用于在给定的对象-值对中查找偏移量
	template<int Alignement = 4, typename T>
	inline int32_t FindOffset(const std::vector<std::pair<void*, T>>& ObjectValuePair, int MinOffset = 0x28, int MaxOffset = 0x1A0)
	{
		int32_t HighestFoundOffset = MinOffset;

		for (int i = 0; i < ObjectValuePair.size(); i++)
		{
			if (ObjectValuePair[i].first == nullptr)
			{
				std::cerr << "Dumper-7 ERROR: FindOffset is skipping ObjectValuePair[" << i << "] because .first is nullptr." << std::endl;
				continue;
			}

			for (int j = HighestFoundOffset; j < MaxOffset; j += Alignement)
			{
				const T TypedValueAtOffset = *reinterpret_cast<T*>(static_cast<uint8_t*>(ObjectValuePair[i].first) + j);

				if (TypedValueAtOffset == ObjectValuePair[i].second && j >= HighestFoundOffset)
				{
					if (j > HighestFoundOffset)
					{
						HighestFoundOffset = j;
						i = 0;
					}
					j = MaxOffset;
				}
			}
		}

		return HighestFoundOffset != MinOffset ? HighestFoundOffset : OffsetNotFound;
	}

	// 模板函数，用于获取有效的指针偏移量
	template<bool bCheckForVft = true>
	inline int32_t GetValidPointerOffset(const uint8_t* ObjA, const uint8_t* ObjB, int32_t StartingOffset, int32_t MaxOffset)
	{
		if (IsBadReadPtr(ObjA) || IsBadReadPtr(ObjB))
			return OffsetNotFound;

		for (int j = StartingOffset; j <= MaxOffset; j += sizeof(void*))
		{
			const bool bIsAValid = !IsBadReadPtr(*reinterpret_cast<void* const*>(ObjA + j)) && (bCheckForVft ? !IsBadReadPtr(**reinterpret_cast<void** const*>(ObjA + j)) : true);
			const bool bIsBValid = !IsBadReadPtr(*reinterpret_cast<void* const*>(ObjB + j)) && (bCheckForVft ? !IsBadReadPtr(**reinterpret_cast<void** const*>(ObjB + j)) : true);

			if (bIsAValid && bIsBValid)
				return j;
		}

		return OffsetNotFound;
	};

	/* UObject */
	// 查找 UObject::Flags 的偏移量
	int32_t FindUObjectFlagsOffset();
	// 查找 UObject::Index 的偏移量
	int32_t FindUObjectIndexOffset();
	// 查找 UObject::Class 的偏移量
	int32_t FindUObjectClassOffset();
	// 查找 UObject::Name 的偏移量
	int32_t FindUObjectNameOffset();
	// 查找 UObject::Outer 的偏移量
	int32_t FindUObjectOuterOffset();

	// 修正硬编码的偏移量
	void FixupHardcodedOffsets();
	// 初始化 FName 相关的设置
	void InitFNameSettings();
	// FName 设置的后初始化
	void PostInitFNameSettings();

	/* UField */
	// 查找 UField::Next 的偏移量
	int32_t FindUFieldNextOffset();

	/* FField */
	// 查找 FField::Next 的偏移量
	int32_t FindFFieldNextOffset();

	// 查找 FField::Name 的偏移量
	int32_t FindFFieldNameOffset();

	/* UEnum */
	// 查找 UEnum::Names 的偏移量
	int32_t FindEnumNamesOffset();

	/* UStruct */
	// 查找 UStruct::SuperStruct 的偏移量
	int32_t FindSuperOffset();
	// 查找 UStruct::Children 的偏移量
	int32_t FindChildOffset();
	// 查找 UStruct::ChildProperties 的偏移量
	int32_t FindChildPropertiesOffset();
	// 查找 UStruct::Size 的偏移量
	int32_t FindStructSizeOffset();
	// 查找 UStruct::MinAlignment 的偏移量
	int32_t FindMinAlignmentOffset();

	/* UFunction */
	// 查找 UFunction::FunctionFlags 的偏移量
	int32_t FindFunctionFlagsOffset();
	// 查找 UFunction::ExecFunction 的偏移量
	int32_t FindFunctionNativeFuncOffset();

	/* UClass */
	// 查找 UClass::CastFlags 的偏移量
	int32_t FindCastFlagsOffset();
	// 查找 UClass::ClassDefaultObject 的偏移量
	int32_t FindDefaultObjectOffset();
	// 查找 UClass::ImplementedInterfaces 的偏移量
	int32_t FindImplementedInterfacesOffset();

	/* Property */
	// 查找 Property::ElementSize 的偏移量
	int32_t FindElementSizeOffset();
	// 查找 Property::ArrayDim 的偏移量
	int32_t FindArrayDimOffset();
	// 查找 Property::PropertyFlags 的偏移量
	int32_t FindPropertyFlagsOffset();
	// 查找 Property::Offset_Internal 的偏移量
	int32_t FindOffsetInternalOffset();

	/* BoolProperty */
	// 查找 BoolProperty 基础结构的偏移量
	int32_t FindBoolPropertyBaseOffset();

	/* ArrayProperty */
	// 查找 ArrayProperty::Inner 的偏移量
	int32_t FindInnerTypeOffset(const int32 PropertySize);

	/* SetProperty */
	// 查找 SetProperty 基础结构的偏移量
	int32_t FindSetPropertyBaseOffset(const int32 PropertySize);

	/* MapProperty */
	// 查找 MapProperty 基础结构的偏移量
	int32_t FindMapPropertyBaseOffset(const int32 PropertySize);

	/* InSDK -> ULevel */
	// 查找 ULevel::Actors 的偏移量
	int32_t FindLevelActorsOffset();

	/* InSDK -> UDataTable */
	// 查找 UDataTable::RowMap 的偏移量
	int32_t FindDatatableRowMapOffset();
}
