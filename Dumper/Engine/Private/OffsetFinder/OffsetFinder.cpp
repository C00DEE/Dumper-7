#include <vector>

#include "OffsetFinder/OffsetFinder.h"
#include "Unreal/ObjectArray.h"

/* UObject */

/**
 * @brief 查找 UObject::Flags 的偏移量。
 * @details 该函数通过在对象中搜索一个常见的枚举标志值来确定 UObject::Flags 成员的偏移量。
 *			它假设某个特定的标志值（此处为 0x43）在大量对象中普遍存在。
 * @return int32_t UObject::Flags 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindUObjectFlagsOffset()
{
	constexpr auto EnumFlagValueToSearch = 0x43;

	/* We're looking for a commonly occuring flag and this number basically defines the minimum number that counts ad "commonly occuring". */
	constexpr auto MinNumFlagValuesRequiredAtOffset = 0xA0;

	for (int i = 0; i < 0x20; i++)
	{
		int Offset = 0x0;
		while (Offset != OffsetNotFound)
		{
			// Look for 0x43 in this object, as it is a really common value for UObject::Flags
			Offset = FindOffset(std::vector{ std::pair{ ObjectArray::GetByIndex(i).GetAddress(), EnumFlagValueToSearch } }, Offset, 0x40);

			if (Offset == OffsetNotFound)
				break; // Early exit

			/* We're looking for a common flag. To check if the flag  is common we're checking the first 0x100 objects to see how often the flag occures at this offset. */
			int32 NumObjectsWithFlagAtOffset = 0x0;

			int Counter = 0;
			for (UEObject Obj : ObjectArray())
			{
				// Only check the (possible) flags of the first 0x100 objects
				if (Counter++ == 0x100)
					break;

				const int32 TypedValueAtOffset = *reinterpret_cast<int32*>(reinterpret_cast<uintptr_t>(Obj.GetAddress()) + Offset);

				if (TypedValueAtOffset == EnumFlagValueToSearch)
					NumObjectsWithFlagAtOffset++;
			}

			if (NumObjectsWithFlagAtOffset > MinNumFlagValuesRequiredAtOffset)
				return Offset;
		}
	}

	return OffsetNotFound;
}

/**
 * @brief 查找 UObject::Index 的偏移量。
 * @details 通过比较几个已知索引的对象，找到存储其索引的成员的偏移量。
 * @return int32_t UObject::Index 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindUObjectIndexOffset()
{
	std::vector<std::pair<void*, int32_t>> Infos;

	Infos.emplace_back(ObjectArray::GetByIndex(0x055).GetAddress(), 0x055);
	Infos.emplace_back(ObjectArray::GetByIndex(0x123).GetAddress(), 0x123);

	return FindOffset<4>(Infos, sizeof(void*)); // Skip VTable
}

/**
 * @brief 查找 UObject::Class 的偏移量。
 * @details UObject::Class 指针最终会指向自身（对于 "Class CoreUObject.Class" 对象）。
 *			此函数利用这个特性，寻找一个指针成员，该成员经过数次解引用后会指向自身。
 * @return int32_t UObject::Class 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindUObjectClassOffset()
{
	/* Checks for a pointer that points to itself in the end. The UObject::Class pointer of "Class CoreUObject.Class" will point to "Class CoreUObject.Class". */
	auto IsValidCyclicUClassPtrOffset = [](const uint8_t* ObjA, const uint8_t* ObjB, int32_t ClassPtrOffset)
	{
		/* Will be advanced before they are used. */
		const uint8_t* NextClassA = ObjA;
		const uint8_t* NextClassB = ObjB;

		for (int MaxLoopCount = 0; MaxLoopCount < 0x10; MaxLoopCount++)
		{
			const uint8_t* CurrentClassA = NextClassA;
			const uint8_t* CurrentClassB = NextClassB;

			NextClassA = *reinterpret_cast<const uint8_t* const*>(NextClassA + ClassPtrOffset);
			NextClassB = *reinterpret_cast<const uint8_t* const*>(NextClassB + ClassPtrOffset);

			/* If this was UObject::Class it would never be invalid. The pointer would simply point to itself.*/
			if (!NextClassA || !NextClassB || IsBadReadPtr(NextClassA) || IsBadReadPtr(NextClassB))
				return false;

			if (CurrentClassA == NextClassA && CurrentClassB == NextClassB)
				return true;
		}
	};

	const uint8_t* const ObjA = static_cast<const uint8_t*>(ObjectArray::GetByIndex(0x055).GetAddress());
	const uint8_t* const ObjB = static_cast<const uint8_t*>(ObjectArray::GetByIndex(0x123).GetAddress());

	int32_t Offset = 0;
	while (Offset != OffsetNotFound)
	{
		Offset = GetValidPointerOffset<true>(ObjA, ObjB, Offset + sizeof(void*), 0x50);

		if (IsValidCyclicUClassPtrOffset(ObjA, ObjB, Offset))
			return Offset;
	}

	return OffsetNotFound;
}

/**
 * @brief 查找 UObject::Name 的偏移量。
 * @details 此函数根据 FName 的内部结构（特别是 ComparisonIndex）的特性来查找 UObject::Name 的偏移量。
 *			它分析了多个对象中可能为 Name 的成员的值分布，以确定最有可能的偏移量。
 * @return int32_t UObject::Name 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindUObjectNameOffset()
{
	/*
	* Requirements:
	*	- CmpIdx > 0x10 && CmpIdx < 0xF0000000
	*	- AverageValue >= 0x100 && AverageValue <= 0xFF00000;
	*	- Offset != { OtherOffsets }
	*/

	/* A struct describing the value*/
	struct ValueInfo
	{
		int32 Offset;					   // Offset from the UObject start to this value
		int32 NumNamesWithLowCmpIdx = 0x0; // The number of names where the comparison index is in the range [0, 16]. Usually this should be far less than 0x20 names.
		uint64 TotalValue = 0x0;		   // The total value of the int32 data at this offset over all objects in GObjects
		bool bIsValidCmpIdxRange = true;   // Whether this value could be a valid FName::ComparisonIndex
	};

	std::array<ValueInfo, 0xC> PossibleOffset;

	constexpr auto MaxAllowedComparisonIndexValue = 0x4000000; // Somewhat arbitrary limit. Make sure this isn't too low for games on FNamePool with lots of names and 0x14 block-size bits

	constexpr auto MaxAllowedAverageComparisonIndexValue = MaxAllowedComparisonIndexValue / 4; // Also somewhat arbitrary limit, but the average value shouldn't be as high as the max allowed one
	constexpr auto MinAllowedAverageComparisonIndexValue = 0x100; // If the average name is below 0x100 it is either the smallest UE application ever, or not the right offset

	constexpr auto LowComparisonIndexUpperCap = 0x10; // The upper limit of what is considered a "low" comparison index
	constexpr auto MaxLlowedNamesWithLowCmpIdx = 0x40;


	int ArrayLength = 0x0;
	for (int i = sizeof(void*); i <= 0x40; i += 0x4)
	{
		// Skip Flags and Index offsets
		if (i == Off::UObject::Flags || i == Off::UObject::Index)
			continue;

		// Skip Class and Outer offsets
		if (i == Off::UObject::Class || i == Off::UObject::Outer)
		{
			if (sizeof(void*) > 0x4)
				i += 0x4;

			continue;
		}

		PossibleOffset[ArrayLength++].Offset = i;
	}

	auto GetDataAtOffsetAsInt = [](void* Ptr, int32 Offset) -> uint32 { return *reinterpret_cast<int32*>(reinterpret_cast<uintptr_t>(Ptr) + Offset); };


	int NumObjectsConsidered = 0;

	for (UEObject Object : ObjectArray())
	{
		constexpr auto X86SmallPageSize = 0x1000;
		constexpr auto MaxAccessedSizeInUObject = 0x44;

		/*
		* Purpose: Make sure all offsets in the UObject::Name finder can be accessed
		* Reasoning: Objects are allocated in Blocks, these allocations are page-aligned in both size and base. If an object + MaxAccessedSizeInUObject goes past the page-bounds
		*            it might also go past the extends of an allocation. There's no reliable way of getting the size of UObject without knowing it's offsets first.
		*/
		const bool bIsGoingPastPageBounds = (reinterpret_cast<uintptr_t>(Object.GetAddress()) & (X86SmallPageSize - 1)) > (X86SmallPageSize - MaxAccessedSizeInUObject);
		if (bIsGoingPastPageBounds)
			continue;

		NumObjectsConsidered++;

		for (int i = 0x0; i < ArrayLength; i++)
		{
			ValueInfo& Info = PossibleOffset[i];

			const uint32 ValueAtOffset = GetDataAtOffsetAsInt(Object.GetAddress(), Info.Offset);

			Info.TotalValue += ValueAtOffset;
			Info.bIsValidCmpIdxRange = Info.bIsValidCmpIdxRange && ValueAtOffset < MaxAllowedComparisonIndexValue;
			Info.NumNamesWithLowCmpIdx += (ValueAtOffset <= LowComparisonIndexUpperCap);
		}
	}

	int32 FirstValidOffset = -1;
	for (int i = 0x0; i < ArrayLength; i++)
	{
		ValueInfo& Info = PossibleOffset[i];

		const auto AverageValue = (Info.TotalValue / NumObjectsConsidered);

		if (Info.bIsValidCmpIdxRange && Info.NumNamesWithLowCmpIdx <= MaxLlowedNamesWithLowCmpIdx
			&& AverageValue >= MinAllowedAverageComparisonIndexValue && AverageValue <= MaxAllowedAverageComparisonIndexValue)
		{
			if (FirstValidOffset == -1)
			{
				FirstValidOffset = Info.Offset;
				continue;
			}

			/* This shouldn't be the case, so log it as an info but continue, as the first offset is still likely the right one. */
			std::cerr << std::format("Dumper-7: Another UObject::Name offset (0x{:04X}) is also considered valid\n", Info.Offset);
		}
	}

	return FirstValidOffset;
}

/**
 * @brief 查找 UObject::Outer 的偏移量。
 * @details Outer 指针指向包含此对象的对象。此函数通过在对象中查找一个有效的指针来确定 Outer 的偏移量，
 *			同时排除已知的 Class 和 Index 成员。
 * @return int32_t UObject::Outer 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindUObjectOuterOffset()
{
	int32_t LowestFoundOffset = 0xFFFF;

	// loop a few times in case we accidentally choose a UPackage (which doesn't have an Outer) to find Outer
	for (int i = 0; i < 0x10; i++)
	{
		int32_t Offset = 0;

		const uint8_t* ObjA = static_cast<uint8*>(ObjectArray::GetByIndex(rand() % 0x400).GetAddress());
		const uint8_t* ObjB = static_cast<uint8*>(ObjectArray::GetByIndex(rand() % 0x400).GetAddress());

		while (Offset != OffsetNotFound)
		{
			Offset = GetValidPointerOffset(ObjA, ObjB, Offset + sizeof(void*), 0x50);

			// Make sure we didn't re-find the Class offset or Index (if the Index filed is a valid pionter for some ungodly reason). 
			if (Offset != Off::UObject::Class && Offset != Off::UObject::Index)
				break;
		}

		if (Offset != OffsetNotFound && Offset < LowestFoundOffset)
			LowestFoundOffset = Offset;
	}

	return LowestFoundOffset == 0xFFFF ? OffsetNotFound : LowestFoundOffset;
}

/**
 * @brief 修正硬编码的偏移量。
 * @details 根据引擎版本或特定设置（如 bUseCasePreservingName、bUseFProperty），
 *			对一些硬编码的偏移量进行调整，以适应不同版本的内存布局。
 */
void OffsetFinder::FixupHardcodedOffsets()
{
	if (Settings::Internal::bUseCasePreservingName)
	{
		Off::FField::Flags += 0x8;

		Off::FFieldClass::Id += 0x08;
		Off::FFieldClass::CastFlags += 0x08;
		Off::FFieldClass::ClassFlags += 0x08;
		Off::FFieldClass::SuperClass += 0x08;
	}

	if (Settings::Internal::bUseFProperty)
	{
		/*
		* On versions below 5.1.1: class FFieldVariant { void*, bool } -> extends to { void*, bool, uint8[0x7] }
		* ON versions since 5.1.1: class FFieldVariant { void* }
		*
		* Check:
		* if FFieldVariant contains a bool, the memory at the bools offset will not be a valid pointer
		* if FFieldVariant doesn't contain a bool, the memory at the bools offset will be the next member of FField, the Next ptr [valid]
		*/

		const int32 OffsetToCheck = Off::FField::Owner + 0x8;
		void* PossibleNextPtrOrBool0 = *(void**)((uint8*)ObjectArray::FindClassFast("Actor").GetChildProperties().GetAddress() + OffsetToCheck);
		void* PossibleNextPtrOrBool1 = *(void**)((uint8*)ObjectArray::FindClassFast("ActorComponent").GetChildProperties().GetAddress() + OffsetToCheck);
		void* PossibleNextPtrOrBool2 = *(void**)((uint8*)ObjectArray::FindClassFast("Pawn").GetChildProperties().GetAddress() + OffsetToCheck);

		auto IsValidPtr = [](void* a) -> bool
		{
			return !IsBadReadPtr(a) && (uintptr_t(a) & 0x1) == 0; // realistically, there wont be any pointers to unaligned memory
		};

		if (IsValidPtr(PossibleNextPtrOrBool0) && IsValidPtr(PossibleNextPtrOrBool1) && IsValidPtr(PossibleNextPtrOrBool2))
		{
			std::cerr << "Applaying fix to hardcoded offsets \n" << std::endl;

			Settings::Internal::bUseMaskForFieldOwner = true;

			Off::FField::Next -= 0x08;
			Off::FField::Name -= 0x08;
			Off::FField::Flags -= 0x08;
		}
	}
}

/**
 * @brief 初始化 FName 相关的设置。
 * @details FName 的内存在不同引擎版本中有所不同。此函数通过分析第一个对象的 Name 成员
 *			来推断 FName 的结构（例如，是否包含大小写保留、是否有大纲编号等），
 *			并相应地设置全局配置。
 */
void OffsetFinder::InitFNameSettings()
{
	UEObject FirstObject = ObjectArray::GetByIndex(0);

	const uint8* NameAddress = static_cast<const uint8*>(FirstObject.GetFName().GetAddress());

	const int32 FNameFirstInt /* ComparisonIndex */ = *reinterpret_cast<const int32*>(NameAddress);
	const int32 FNameSecondInt /* [Number/DisplayIndex] */ = *reinterpret_cast<const int32*>(NameAddress + 0x4);

	/* Some games move 'Name' before 'Class'. Just substract the offset of 'Name' with the offset of the member that follows right after it, to get an estimate of sizeof(FName). */
	const int32 FNameSize = !Settings::Internal::bIsObjectNameBeforeClass ? (Off::UObject::Outer - Off::UObject::Name) : (Off::UObject::Class - Off::UObject::Name);

	Off::FName::CompIdx = 0x0;
	Off::FName::Number = 0x4; // defaults for check

	 // FNames for which FName::Number == [1...4]
	auto GetNumNamesWithNumberOneToFour = []() -> int32
	{
		int32 NamesWithNumberOneToFour = 0x0;

		for (UEObject Obj : ObjectArray())
		{
			const uint32 Number = Obj.GetFName().GetNumber();

			if (Number > 0x0 && Number < 0x5)
				NamesWithNumberOneToFour++;
		}

		return NamesWithNumberOneToFour;
	};

	/*
	* Games without FNAME_OUTLINE_NUMBER have a min. percentage of 6% of all object-names for which FName::Number is in a [1...4] range
	* On games with FNAME_OUTLINE_NUMBER the (random) integer after FName::ComparisonIndex is in the range from [1...4] about 2% (or less) of times.
	*
	* The minimum percentage of names is set to 3% to give both normal names, as well as outline-numer names a buffer-zone.
	*
	* This doesn't work on some very small UE template games, which is why PostInitFNameSettings() was added to fix the incorrect behavior of this function
	*/
	constexpr float MinPercentage = 0.03f;

	/* Minimum required ammount of names for which FName::Number is in a [1...4] range */
	const int32 FNameNumberThreashold = (ObjectArray::Num() * MinPercentage);

	Off::FName::CompIdx = 0x0;

	if (FNameSize == 0x8 && FNameFirstInt == FNameSecondInt) /* WITH_CASE_PRESERVING_NAME + FNAME_OUTLINE_NUMBER */
	{
		Settings::Internal::bUseCasePreservingName = true;
		Settings::Internal::bUseOutlineNumberName = true;

		Off::FName::Number = -0x1;
		Off::InSDK::Name::FNameSize = 0x8;
	}
	else if (FNameSize == 0x10) /* WITH_CASE_PRESERVING_NAME */
	{
		Settings::Internal::bUseCasePreservingName = true;

		Off::FName::Number = FNameFirstInt == FNameSecondInt ? 0x8 : 0x4;

		Off::InSDK::Name::FNameSize = 0xC;
	}
	else if (GetNumNamesWithNumberOneToFour() < FNameNumberThreashold) /* FNAME_OUTLINE_NUMBER */
	{
		Settings::Internal::bUseOutlineNumberName = true;

		Off::FName::Number = -0x1;

		Off::InSDK::Name::FNameSize = 0x4;
	}
	else /* Default */
	{
		Off::FName::Number = 0x4;

		Off::InSDK::Name::FNameSize = 0x8;
	}
}

/**
 * @brief 后续初始化 FName 设置。
 * @details 在初步初始化后，此函数使用一个已知类成员（如 PlayerStart::PlayerStartTag）的大小来验证和修正 FName 的大小。
 *			这提供了一种更可靠的方式来确定 FName 的确切大小，并据此调整相关偏移量。
 */
void OffsetFinder::PostInitFNameSettings()
{
	UEClass PlayerStart = ObjectArray::FindClassFast("PlayerStart");

	const int32 FNameSize = PlayerStart.FindMember("PlayerStartTag").GetSize();

	/* Nothing to do for us, everything is fine! */
	if (Off::InSDK::Name::FNameSize == FNameSize)
		return;

	/* We've used the wrong FNameSize to determine the offset of FField::Flags. Substract the old, wrong, size and add the new one.*/
	Off::FField::Flags = (Off::FField::Flags - Off::InSDK::Name::FNameSize) + FNameSize;

	const uint8* NameAddress = static_cast<const uint8*>(PlayerStart.GetFName().GetAddress());

	const int32 FNameFirstInt /* ComparisonIndex */ = *reinterpret_cast<const int32*>(NameAddress);
	const int32 FNameSecondInt /* [Number/DisplayIndex] */ = *reinterpret_cast<const int32*>(NameAddress + 0x4);

	if (FNameSize == 0x8 && FNameFirstInt == FNameSecondInt) /* WITH_CASE_PRESERVING_NAME + FNAME_OUTLINE_NUMBER */
	{
		Settings::Internal::bUseCasePreservingName = true;
		Settings::Internal::bUseOutlineNumberName = true;

		Off::FName::Number = -0x1;
		Off::InSDK::Name::FNameSize = 0x8;
	}
	else if (FNameSize > 0x8) /* WITH_CASE_PRESERVING_NAME */
	{
		Settings::Internal::bUseOutlineNumberName = false;
		Settings::Internal::bUseCasePreservingName = true;

		Off::FName::Number = FNameFirstInt == FNameSecondInt ? 0x8 : 0x4;

		Off::InSDK::Name::FNameSize = 0xC;
	}
	else if (FNameSize == 0x4) /* FNAME_OUTLINE_NUMBER */
	{
		Settings::Internal::bUseOutlineNumberName = true;
		Settings::Internal::bUseCasePreservingName = false;

		Off::FName::Number = -0x1;

		Off::InSDK::Name::FNameSize = 0x4;
	}
	else /* Default */
	{
		Settings::Internal::bUseOutlineNumberName = false;
		Settings::Internal::bUseCasePreservingName = false;

		Off::FName::Number = 0x4;
		Off::InSDK::Name::FNameSize = 0x8;
	}
}

/* UField */

/**
 * @brief 查找 UField::Next 的偏移量。
 * @details UField 对象形成一个单向链表。此函数通过比较两个已知 UField 对象的子对象的地址，
 *			找到指向下一个 UField 的 Next 指针的偏移量。
 * @return int32_t UField::Next 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindUFieldNextOffset()
{
	const uint8_t* KismetSystemLibraryChild = reinterpret_cast<uint8_t*>(ObjectArray::FindObjectFast<UEStruct>("KismetSystemLibrary").GetChild().GetAddress());
	const uint8_t* KismetStringLibraryChild = reinterpret_cast<uint8_t*>(ObjectArray::FindObjectFast<UEStruct>("KismetStringLibrary").GetChild().GetAddress());

#undef max
	const auto HighestUObjectOffset = std::max({ Off::UObject::Index, Off::UObject::Name, Off::UObject::Flags, Off::UObject::Outer, Off::UObject::Class });
#define max(a,b)            (((a) > (b)) ? (a) : (b))

	return GetValidPointerOffset(KismetSystemLibraryChild, KismetStringLibraryChild, Align(HighestUObjectOffset + 0x4, 0x8), 0x60);
}

/* FField */

/**
 * @brief 查找 FField::Next 的偏移量 (当使用 FProperty 系统时)。
 * @details 与 UField::Next 类似，但用于 FProperty 系统中的 FField 链表。
 * @return int32_t FField::Next 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindFFieldNextOffset()
{
	const uint8_t* GuidChildren = reinterpret_cast<uint8_t*>(ObjectArray::FindStructFast("Guid").GetChildProperties().GetAddress());
	const uint8_t* VectorChildren = reinterpret_cast<uint8_t*>(ObjectArray::FindStructFast("Vector").GetChildProperties().GetAddress());

	return GetValidPointerOffset(GuidChildren, VectorChildren, Off::FField::Owner + 0x8, 0x48);
}

/**
 * @brief 查找 FField::Name 的偏移量 (当使用 FProperty 系统时)。
 * @details 通过检查已知结构体（如 Guid 和 Vector）的第一个属性的名称，来动态确定 FField::Name 的偏移量。
 * @return int32_t FField::Name 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindFFieldNameOffset()
{
	UEFField GuidChild = ObjectArray::FindStructFast("Guid").GetChildProperties();
	UEFField VectorChild = ObjectArray::FindStructFast("Vector").GetChildProperties();

	std::string GuidChildName = GuidChild.GetName();
	std::string VectorChildName = VectorChild.GetName();

	if ((GuidChildName == "A" || GuidChildName == "D") && (VectorChildName == "X" || VectorChildName == "Z"))
		return Off::FField::Name;

	for (Off::FField::Name = Off::FField::Owner; Off::FField::Name < 0x40; Off::FField::Name += 4)
	{
		GuidChildName = GuidChild.GetName();
		VectorChildName = VectorChild.GetName();

		if ((GuidChildName == "A" || GuidChildName == "D") && (VectorChildName == "X" || VectorChildName == "Z"))
			return Off::FField::Name;
	}

	return OffsetNotFound;
}

/* UEnum */

/**
 * @brief 查找 UEnum::Names 数组的偏移量。
 * @details UEnum::Names 是一个存储枚举名和值的数组。此函数通过在已知的 UEnum 对象中搜索
 *			一个特定的枚举条目数量，来定位这个数组的偏移量。
 * @return int32_t UEnum::Names 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindEnumNamesOffset()
{
	std::vector<std::pair<void*, int32_t>> Infos;

	Infos.push_back({ ObjectArray::FindObjectFast("ENetRole", EClassCastFlags::Enum).GetAddress(), 0x5 });
	Infos.push_back({ ObjectArray::FindObjectFast("ETraceTypeQuery", EClassCastFlags::Enum).GetAddress(), 0x22 });

	int Ret = FindOffset(Infos) - 0x8;
	if (Ret == (OffsetNotFound - 0x8))
	{
		Infos[0] = { ObjectArray::FindObjectFast("EAlphaBlendOption", EClassCastFlags::Enum).GetAddress(), 0x10 };
		Infos[1] = { ObjectArray::FindObjectFast("EUpdateRateShiftBucket", EClassCastFlags::Enum).GetAddress(), 0x8 };

		Ret = FindOffset(Infos) - 0x8;
	}

	struct Name08Byte { uint8 Pad[0x08]; };
	struct Name16Byte { uint8 Pad[0x10]; };

	uint8* ArrayAddress = static_cast<uint8*>(Infos[0].first) + Ret;

	if (Settings::Internal::bUseCasePreservingName)
	{
		auto& ArrayOfNameValuePairs = *reinterpret_cast<TArray<TPair<Name16Byte, int64>>*>(ArrayAddress);

		if (ArrayOfNameValuePairs[1].Second == 1)
			return Ret;

		if constexpr (Settings::EngineCore::bCheckEnumNamesInUEnum)
		{
			if (static_cast<uint8_t>(ArrayOfNameValuePairs[1].Second) == 1 && static_cast<uint8_t>(ArrayOfNameValuePairs[2].Second) == 2)
			{

				Settings::Internal::bIsSmallEnumValue = true;
				return Ret;
			}
		}

		Settings::Internal::bIsEnumNameOnly = true;
	}
	else
	{
		const auto& Array = *reinterpret_cast<TArray<TPair<Name08Byte, int64>>*>(static_cast<uint8*>(Infos[0].first) + Ret);

		if (Array[1].Second == 1)
			return Ret;

		if constexpr (Settings::EngineCore::bCheckEnumNamesInUEnum)
		{
			if (static_cast<uint8_t>(Array[1].Second) == 1 && static_cast<uint8_t>(Array[2].Second) == 2)
			{
				Settings::Internal::bIsSmallEnumValue = true;
				return Ret;
			}
		}

		Settings::Internal::bIsEnumNameOnly = true;
	}

	return Ret;
}

/* UStruct */

/**
 * @brief 查找 UStruct::SuperStruct 的偏移量。
 * @details SuperStruct 指向父结构体。此函数通过已知的继承关系（如 Class -> Struct -> Field）
 *			来查找指向父结构体的 SuperStruct 指针的偏移量。
 * @return int32_t UStruct::SuperStruct 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindSuperOffset()
{
	std::vector<std::pair<void*, void*>> Infos;

	Infos.push_back({ ObjectArray::FindObjectFast("Struct").GetAddress(), ObjectArray::FindObjectFast("Field").GetAddress() });
	Infos.push_back({ ObjectArray::FindObjectFast("Class").GetAddress(), ObjectArray::FindObjectFast("Struct").GetAddress() });

	// Thanks to the ue4 dev who decided UStruct should be spelled Ustruct
	if (Infos[0].first == nullptr)
		Infos[0].first = Infos[1].second = ObjectArray::FindObjectFast("struct").GetAddress();

	return FindOffset(Infos);
}

/**
 * @brief 查找 UStruct::Children 的偏移量。
 * @details Children 指向该结构体的第一个子成员（UField）。此函数通过在已知结构体中
 *			查找其已知成员的地址来确定 Children 指针的偏移量。
 * @return int32_t UStruct::Children 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindChildOffset()
{
	std::vector<std::pair<void*, void*>> Infos;

	Infos.push_back({ ObjectArray::FindObjectFast("PlayerController").GetAddress(), ObjectArray::FindObjectFastInOuter("WasInputKeyJustReleased", "PlayerController").GetAddress() });
	Infos.push_back({ ObjectArray::FindObjectFast("Controller").GetAddress(), ObjectArray::FindObjectFastInOuter("UnPossess", "Controller").GetAddress() });

	if (FindOffset(Infos) == OffsetNotFound)
	{
		Infos.clear();

		Infos.push_back({ ObjectArray::FindObjectFast("Vector").GetAddress(), ObjectArray::FindObjectFastInOuter("X", "Vector").GetAddress() });
		Infos.push_back({ ObjectArray::FindObjectFast("Vector4").GetAddress(), ObjectArray::FindObjectFastInOuter("X", "Vector4").GetAddress() });
		Infos.push_back({ ObjectArray::FindObjectFast("Vector2D").GetAddress(), ObjectArray::FindObjectFastInOuter("X", "Vector2D").GetAddress() });
		Infos.push_back({ ObjectArray::FindObjectFast("Guid").GetAddress(), ObjectArray::FindObjectFastInOuter("A","Guid").GetAddress() });

		return FindOffset(Infos);
	}

	Settings::Internal::bUseFProperty = true;

	return FindOffset(Infos);
}

/**
 * @brief 查找 UStruct::ChildProperties 的偏移量 (当使用 FProperty 系统时)。
 * @details ChildProperties 是 FProperty 系统中用于替代 Children 的成员。
 *			此函数在一个已知结构体（如 Color 和 Guid）中查找有效的指针，以确定其偏移量。
 * @return int32_t UStruct::ChildProperties 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindChildPropertiesOffset()
{
	const uint8* ObjA = reinterpret_cast<const uint8*>(ObjectArray::FindStructFast("Color").GetAddress());
	const uint8* ObjB = reinterpret_cast<const uint8*>(ObjectArray::FindStructFast("Guid").GetAddress());

	return GetValidPointerOffset(ObjA, ObjB, Off::UStruct::Children + 0x08, 0x80);
}

/**
 * @brief 查找 UStruct::PropertiesSize 的偏移量。
 * @details PropertiesSize 存储了该结构体的大小。此函数通过比较已知大小的结构体
 *			（如 Color 和 Guid）来查找存储大小的成员的偏移量。
 * @return int32_t UStruct::PropertiesSize 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindStructSizeOffset()
{
	std::vector<std::pair<void*, int32_t>> Infos;

	Infos.push_back({ ObjectArray::FindObjectFast("Color").GetAddress(), 0x04 });
	Infos.push_back({ ObjectArray::FindObjectFast("Guid").GetAddress(), 0x10 });

	return FindOffset(Infos);
}

/**
 * @brief 查找 UStruct::MinAlignment 的偏移量。
 * @details MinAlignment 存储了该结构体的最小对齐要求。此函数通过比较已知对齐方式的结构体
 *			来查找存储最小对齐的成员的偏移量。
 * @return int32_t UStruct::MinAlignment 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindMinAlignmentOffset()
{
	std::vector<std::pair<void*, int32_t>> Infos;

	Infos.push_back({ ObjectArray::FindObjectFast("Transform").GetAddress(), 0x10 });
	Infos.push_back({ ObjectArray::FindObjectFast("PlayerController").GetAddress(), 0x8 });

	return FindOffset(Infos);
}

/* UFunction */

/**
 * @brief 查找 UFunction::FunctionFlags 的偏移量。
 * @details FunctionFlags 是一组描述函数行为的标志。此函数通过在几个已知函数中
 *			搜索它们已知的标志组合来确定 FunctionFlags 的偏移量。
 * @return int32_t UFunction::FunctionFlags 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindFunctionFlagsOffset()
{
	std::vector<std::pair<void*, EFunctionFlags>> Infos;

	Infos.push_back({ ObjectArray::FindObjectFast("WasInputKeyJustPressed", EClassCastFlags::Function).GetAddress(), EFunctionFlags::Final | EFunctionFlags::Native | EFunctionFlags::Public | EFunctionFlags::BlueprintCallable | EFunctionFlags::BlueprintPure | EFunctionFlags::Const });
	Infos.push_back({ ObjectArray::FindObjectFast("ToggleSpeaking", EClassCastFlags::Function).GetAddress(), EFunctionFlags::Exec | EFunctionFlags::Native | EFunctionFlags::Public });
	Infos.push_back({ ObjectArray::FindObjectFast("SwitchLevel", EClassCastFlags::Function).GetAddress(), EFunctionFlags::Exec | EFunctionFlags::Native | EFunctionFlags::Public });

	// Some games don't have APlayerController::SwitchLevel(), so we replace it with APlayerController::FOV() which has the same FunctionFlags
	if (Infos[2].first == nullptr)
		Infos[2].first = ObjectArray::FindObjectFast("FOV", EClassCastFlags::Function).GetAddress();

	const int32 Ret = FindOffset(Infos);

	if (Ret != OffsetNotFound)
		return Ret;

	for (auto& [_, Flags] : Infos)
		Flags |= EFunctionFlags::RequiredAPI;

	return FindOffset(Infos);
}

/**
 * @brief 查找 UFunction::Func (原生函数指针) 的偏移量。
 * @details Func 是指向函数具体实现的指针。此函数通过在几个已知原生函数中搜索
 *			有效的进程内地址来确定 Func 指针的偏移量。
 * @return int32_t UFunction::Func 的偏移量，如果未找到则返回 0x0。
 */
int32_t OffsetFinder::FindFunctionNativeFuncOffset()
{
	std::vector<std::pair<void*, EFunctionFlags>> Infos;

	uintptr_t WasInputKeyJustPressed = reinterpret_cast<uintptr_t>(ObjectArray::FindObjectFast("WasInputKeyJustPressed", EClassCastFlags::Function).GetAddress());
	uintptr_t ToggleSpeaking = reinterpret_cast<uintptr_t>(ObjectArray::FindObjectFast("ToggleSpeaking", EClassCastFlags::Function).GetAddress());
	uintptr_t SwitchLevel_Or_FOV = reinterpret_cast<uintptr_t>(ObjectArray::FindObjectFast("SwitchLevel", EClassCastFlags::Function).GetAddress());

	// Some games don't have APlayerController::SwitchLevel(), so we replace it with APlayerController::FOV() which has the same FunctionFlags
	if (SwitchLevel_Or_FOV == NULL)
		SwitchLevel_Or_FOV = reinterpret_cast<uintptr_t>(ObjectArray::FindObjectFast("FOV", EClassCastFlags::Function).GetAddress());

	for (int i = 0x40; i < 0x140; i += 8)
	{
		if (IsInProcessRange(*reinterpret_cast<uintptr_t*>(WasInputKeyJustPressed + i)) && IsInProcessRange(*reinterpret_cast<uintptr_t*>(ToggleSpeaking + i)) && IsInProcessRange(*reinterpret_cast<uintptr_t*>(SwitchLevel_Or_FOV + i)))
			return i;
	}

	return 0x0;
}

/* UClass */

/**
 * @brief 查找 UClass::ClassFlags 的偏移量。
 * @details ClassFlags 是一组描述类行为的标志。此函数通过在几个已知类中
 *			搜索它们已知的标志组合来确定 ClassFlags 的偏移量。
 * @return int32_t UClass::ClassFlags 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindCastFlagsOffset()
{
	std::vector<std::pair<void*, EClassCastFlags>> Infos;

	Infos.push_back({ ObjectArray::FindObjectFast("Actor").GetAddress(), EClassCastFlags::Actor });
	Infos.push_back({ ObjectArray::FindObjectFast("Class").GetAddress(), EClassCastFlags::Field | EClassCastFlags::Struct | EClassCastFlags::Class });

	return FindOffset(Infos);
}

/**
 * @brief 查找 UClass::ClassDefaultObject 的偏移量。
 * @details ClassDefaultObject 指向该类的默认对象（CDO）。此函数通过查找已知类的
 *			默认对象的地址来确定 ClassDefaultObject 指针的偏移量。
 * @return int32_t UClass::ClassDefaultObject 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindDefaultObjectOffset()
{
	std::vector<std::pair<void*, void*>> Infos;

	Infos.push_back({ ObjectArray::FindClassFast("Object").GetAddress(), ObjectArray::FindObjectFast("Default__Object").GetAddress() });
	Infos.push_back({ ObjectArray::FindClassFast("Field").GetAddress(), ObjectArray::FindObjectFast("Default__Field").GetAddress() });

	return FindOffset(Infos, 0x28, 0x200);
}

/**
 * @brief 查找 UClass::Interfaces 数组的偏移量。
 * @details Interfaces 是一个包含该类实现的所有接口的数组。此函数通过在一个已知类（ActorComponent）
 *			中查找一个已知的实现接口（Interface_AssetUserData）来定位此数组的偏移量。
 * @return int32_t UClass::Interfaces 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindImplementedInterfacesOffset()
{
	UEClass Interface_AssetUserDataClass = ObjectArray::FindClassFast("Interface_AssetUserData");

	const uint8_t* ActorComponentClassPtr = reinterpret_cast<const uint8_t*>(ObjectArray::FindClassFast("ActorComponent").GetAddress());

	for (int i = Off::UClass::ClassDefaultObject; i <= (0x350 - 0x10); i += sizeof(void*))
	{
		const auto& ActorArray = *reinterpret_cast<const TArray<FImplementedInterface>*>(ActorComponentClassPtr + i);

		if (ActorArray.IsValid() && !IsBadReadPtr(ActorArray.GetDataPtr()))
		{
			if (ActorArray[0].InterfaceClass == Interface_AssetUserDataClass)
				return i;
		}
	}

	return OffsetNotFound;
}

/* Property */

/**
 * @brief 查找 Property::ElementSize 的偏移量。
 * @details ElementSize 存储了属性的基本元素大小。此函数通过检查已知结构体（Guid）
 *			中成员的大小来确定 ElementSize 的偏移量。
 * @return int32_t Property::ElementSize 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindElementSizeOffset()
{
	std::vector<std::pair<void*, int32_t>> Infos;

	UEStruct Guid = ObjectArray::FindStructFast("Guid");

	Infos.push_back({ Guid.FindMember("A").GetAddress(), 0x04 });
	Infos.push_back({ Guid.FindMember("C").GetAddress(), 0x04 });
	Infos.push_back({ Guid.FindMember("D").GetAddress(), 0x04 });

	return FindOffset(Infos);
}

/**
 * @brief 查找 Property::ArrayDim 的偏移量。
 * @details ArrayDim 存储了属性的数组维度（对于非数组，通常为1）。此函数通过检查
 *			已知结构体（Guid）中成员的维度来确定 ArrayDim 的偏移量。
 * @return int32_t Property::ArrayDim 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindArrayDimOffset()
{
	std::vector<std::pair<void*, int32_t>> Infos;

	UEStruct Guid = ObjectArray::FindStructFast("Guid");

	Infos.push_back({ Guid.FindMember("A").GetAddress(), 0x01 });
	Infos.push_back({ Guid.FindMember("C").GetAddress(), 0x01 });
	Infos.push_back({ Guid.FindMember("D").GetAddress(), 0x01 });

	const int32_t MinOffset = Off::Property::ElementSize - 0x10;
	const int32_t MaxOffset = Off::Property::ElementSize + 0x10;

	return FindOffset(Infos, MinOffset, MaxOffset);
}

/**
 * @brief 查找 Property::PropertyFlags 的偏移量。
 * @details PropertyFlags 是一组描述属性行为的标志。此函数通过在已知属性中
 *			搜索已知的标志组合来确定 PropertyFlags 的偏移量。
 * @return int32_t Property::PropertyFlags 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindPropertyFlagsOffset()
{
	std::vector<std::pair<void*, EPropertyFlags>> Infos;


	UEStruct Guid = ObjectArray::FindStructFast("Guid");
	UEStruct Color = ObjectArray::FindStructFast("Color");

	constexpr EPropertyFlags GuidMemberFlags = EPropertyFlags::Edit | EPropertyFlags::ZeroConstructor | EPropertyFlags::SaveGame | EPropertyFlags::IsPlainOldData | EPropertyFlags::NoDestructor | EPropertyFlags::HasGetValueTypeHash;
	constexpr EPropertyFlags ColorMemberFlags = EPropertyFlags::Edit | EPropertyFlags::BlueprintVisible | EPropertyFlags::ZeroConstructor | EPropertyFlags::SaveGame | EPropertyFlags::IsPlainOldData | EPropertyFlags::NoDestructor | EPropertyFlags::HasGetValueTypeHash;

	Infos.push_back({ Guid.FindMember("A").GetAddress(), GuidMemberFlags });
	Infos.push_back({ Color.FindMember("R").GetAddress(), ColorMemberFlags });

	if (Infos[1].first == nullptr) [[unlikely]]
		Infos[1].first = Color.FindMember("r").GetAddress();

	int FlagsOffset = FindOffset(Infos);

	// Same flags without AccessSpecifier
	if (FlagsOffset == OffsetNotFound)
	{
		Infos[0].second |= EPropertyFlags::NativeAccessSpecifierPublic;
		Infos[1].second |= EPropertyFlags::NativeAccessSpecifierPublic;

		FlagsOffset = FindOffset(Infos);
	}

	return FlagsOffset;
}

/**
 * @brief 查找 Property::Offset_Internal 的偏移量。
 * @details Offset_Internal 存储了属性在所属结构体中的偏移量。此函数通过比较
 *			已知结构体（Color）中成员的偏移量来确定此成员的偏移量。
 * @return int32_t Property::Offset_Internal 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindOffsetInternalOffset()
{
	std::vector<std::pair<void*, int32_t>> Infos;

	UEStruct Color = ObjectArray::FindStructFast("Color");

	Infos.push_back({ Color.FindMember("B").GetAddress(), 0x00 });
	Infos.push_back({ Color.FindMember("G").GetAddress(), 0x01 });
	Infos.push_back({ Color.FindMember("R").GetAddress(), 0x02 });

	// Thanks to the ue5 dev who decided FColor::R should be spelled FColor::r
	if (Infos[2].first == nullptr) [[unlikely]]
		Infos[2].first = Color.FindMember("r").GetAddress();

	return FindOffset(Infos);
}

/* BoolProperty */

/**
 * @brief 查找 FBoolProperty 的基础偏移量。
 * @details FBoolProperty 比较特殊，它使用位域来存储布尔值。此函数通过分析几个已知的
 *			FBoolProperty 成员来找到与位域相关的基础偏移量。
 * @return int32_t FBoolProperty 的基础偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindBoolPropertyBaseOffset()
{
	std::vector<std::pair<void*, uint8_t>> Infos;

	UEClass Engine = ObjectArray::FindClassFast("Engine");
	Infos.push_back({ Engine.FindMember("bIsOverridingSelectedColor").GetAddress(), 0xFF });
	Infos.push_back({ Engine.FindMember("bEnableOnScreenDebugMessagesDisplay").GetAddress(), 0b00000010 });
	Infos.push_back({ ObjectArray::FindClassFast("PlayerController").FindMember("bAutoManageActiveCameraTarget").GetAddress(), 0xFF });

	return (FindOffset<1>(Infos, Off::Property::Offset_Internal) - 0x3);
}

/* ArrayProperty */

/**
 * @brief 查找 FArrayProperty::Inner 的偏移量。
 * @details Inner 指向数组元素的属性。在 FProperty 系统中，其偏移量可能会变化。
 *			此函数通过检查一个已知的 FArrayProperty 成员来确定 Inner 的偏移量。
 * @param PropertySize 属性的基本大小。
 * @return int32_t FArrayProperty::Inner 的偏移量。
 */
int32_t OffsetFinder::FindInnerTypeOffset(const int32 PropertySize)
{
	if (!Settings::Internal::bUseFProperty)
		return PropertySize;

	if (UEProperty Property = ObjectArray::FindClassFast("GameViewportClient").FindMember("DebugProperties", EClassCastFlags::ArrayProperty))
	{
		void* AddressToCheck = *reinterpret_cast<void**>(reinterpret_cast<uint8*>(Property.GetAddress()) + PropertySize);

		if (IsBadReadPtr(AddressToCheck))
			return PropertySize + 0x8;
	}

	return PropertySize;
}

/* SetProperty */

/**
 * @brief 查找 FSetProperty 的基础偏移量。
 * @details 用于确定 FSetProperty 中存储元素属性的成员的偏移量。
 * @param PropertySize 属性的基本大小。
 * @return int32_t FSetProperty 基础偏移量。
 */
int32_t OffsetFinder::FindSetPropertyBaseOffset(const int32 PropertySize)
{
	if (!Settings::Internal::bUseFProperty)
		return PropertySize;

	if (auto Object = ObjectArray::FindStructFast("LevelCollection").FindMember("Levels", EClassCastFlags::SetProperty))
	{
		void* AddressToCheck = *(void**)((uint8*)Object.GetAddress() + PropertySize);

		if (IsBadReadPtr(AddressToCheck))
			return PropertySize + 0x8;
	}

	return PropertySize;
}

/* MapProperty */

/**
 * @brief 查找 FMapProperty 的基础偏移量。
 * @details 用于确定 FMapProperty 中存储键和值属性的成员的偏移量。
 * @param PropertySize 属性的基本大小。
 * @return int32_t FMapProperty 基础偏移量。
 */
int32_t OffsetFinder::FindMapPropertyBaseOffset(const int32 PropertySize)
{
	if (!Settings::Internal::bUseFProperty)
		return PropertySize;

	if (auto Object = ObjectArray::FindClassFast("UserDefinedEnum").FindMember("DisplayNameMap", EClassCastFlags::MapProperty))
	{
		void* AddressToCheck = *(void**)((uint8*)Object.GetAddress() + PropertySize);

		if (IsBadReadPtr(AddressToCheck))
			return PropertySize + 0x8;
	}

	return PropertySize;
}

/* InSDK -> ULevel */

/**
 * @brief 查找 ULevel::Actors 数组的偏移量。
 * @details Actors 数组包含了关卡中所有的 Actor。此函数通过在一个有效的 ULevel 对象中
 *			搜索一个有效的 TArray<AActor*> 结构来定位 Actors 数组的偏移量。
 * @return int32_t ULevel::Actors 的偏移量，如果未找到则返回 OffsetNotFound。
 */
int32_t OffsetFinder::FindLevelActorsOffset()
{
	UEObject Level = nullptr;
	uintptr_t Lvl = 0x0;

	for (auto Obj : ObjectArray())
	{
		if (Obj.HasAnyFlags(EObjectFlags::ClassDefaultObject) || !Obj.IsA(EClassCastFlags::Level))
			continue;

		Level = Obj;
		Lvl = reinterpret_cast<uintptr_t>(Obj.GetAddress());
		break;
	}

	if (Lvl == 0x0)
		return OffsetNotFound;

	/*
	class ULevel : public UObject
	{
		FURL URL;
		TArray<AActor*> Actors;
		TArray<AActor*> GCActors;
	};

	SearchStart = sizeof(UObject) + sizeof(FURL)
	SearchEnd = offsetof(ULevel, OwningWorld)
	*/
	int32 SearchStart = ObjectArray::FindClassFast("Object").GetStructSize() + ObjectArray::FindObjectFast<UEStruct>("URL", EClassCastFlags::Struct).GetStructSize();
	int32 SearchEnd = Level.GetClass().FindMember("OwningWorld").GetOffset();

	for (int i = SearchStart; i <= (SearchEnd - 0x10); i += sizeof(void*))
	{
		const TArray<void*>& ActorArray = *reinterpret_cast<TArray<void*>*>(Lvl + i);

		if (ActorArray.IsValid() && !IsBadReadPtr(ActorArray.GetDataPtr()))
		{
			return i;
		}
	}

	return OffsetNotFound;
}


/* InSDK -> UDataTable */

/**
 * @brief 查找 UDataTable::RowMap 的偏移量。
 * @details RowMap 是一个存储数据表所有行的映射。此函数通过定位 RowStruct 属性，
 *			并假设 RowMap 紧随其后，来确定其偏移量。
 * @return int32_t UDataTable::RowMap 的偏移量。
 */
int32_t OffsetFinder::FindDatatableRowMapOffset()
{
	const UEClass DataTable = ObjectArray::FindClassFast("DataTable");

	constexpr int32 UObjectOuterSize = 0x8;
	constexpr int32 RowStructSize = 0x8;

	if (!DataTable)
	{
		std::cerr << "\nDumper-7: [DataTable] Couldn't find \"DataTable\" class, assuming default layout.\n" << std::endl;
		return (Off::UObject::Outer + UObjectOuterSize + RowStructSize);
	}

	UEProperty RowStructProp = DataTable.FindMember("RowStruct", EClassCastFlags::ObjectProperty);

	if (!RowStructProp)
	{
		std::cerr << "\nDumper-7: [DataTable] Couldn't find \"RowStruct\" property, assuming default layout.\n" << std::endl;
		return (Off::UObject::Outer + UObjectOuterSize + RowStructSize);
	}

	return RowStructProp.GetOffset() + RowStructProp.GetSize();
}

