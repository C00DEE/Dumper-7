#include <format>

#include "Utils.h"

#include "OffsetFinder/Offsets.h"
#include "OffsetFinder/OffsetFinder.h"

#include "Unreal/ObjectArray.h"
#include "Unreal/NameArray.h"


/**
 * @brief 初始化 ProcessEvent 的偏移量和索引。
 * @details ProcessEvent 是 Unreal Engine 中用于处理蓝图事件和 RPC 的核心函数。
 *          此函数通过扫描 UObject 虚函数表（VTable）来定位 ProcessEvent。
 *          它使用特定的字节模式来识别正确的函数。如果主要方法失败，
 *          它会尝试使用一个备用方法，即查找包含 "Accessed None" 字符串的函数附近。
 */
void Off::InSDK::ProcessEvent::InitPE()
{
	void** Vft = *(void***)ObjectArray::GetByIndex(0).GetAddress();

	/* Primary, and more reliable, check for ProcessEvent */
	auto IsProcessEvent = [](const uint8_t* FuncAddress, [[maybe_unused]] int32_t Index) -> bool
	{
		return FindPatternInRange({ 0xF7, -0x1, Off::UFunction::FunctionFlags, 0x0, 0x0, 0x0, 0x0, 0x04, 0x0, 0x0 }, FuncAddress, 0x400)
			&& FindPatternInRange({ 0xF7, -0x1, Off::UFunction::FunctionFlags, 0x0, 0x0, 0x0, 0x0, 0x0, 0x40, 0x0 }, FuncAddress, 0xF00);
	};

	const void* ProcessEventAddr = nullptr;
	int32_t ProcessEventIdx = 0;

	auto [FuncPtr, FuncIdx] = IterateVTableFunctions(Vft, IsProcessEvent);

	ProcessEventAddr = FuncPtr;
	ProcessEventIdx = FuncIdx;

	if (!FuncPtr)
	{
		/* ProcessEvent is sometimes located right after a func with the string L"Accessed None. Might as well check for it, because else we're going to crash anyways. */
		void* PossiblePEAddr = (void*)FindByWStringInAllSections(L"Accessed None").FindNextFunctionStart();

		auto IsSameAddr = [PossiblePEAddr](const uint8_t* FuncAddress, [[maybe_unused]] int32_t Index) -> bool
		{
			return FuncAddress == PossiblePEAddr;
		};

		auto [FuncPtr2, FuncIdx2] = IterateVTableFunctions(Vft, IsSameAddr);
		ProcessEventAddr = FuncPtr2;
		ProcessEventIdx = FuncIdx2;
	}

	if (ProcessEventAddr)
	{
		Off::InSDK::ProcessEvent::PEIndex = ProcessEventIdx;
		Off::InSDK::ProcessEvent::PEOffset = GetOffset(ProcessEventAddr);

		std::cerr << std::format("PE-Offset: 0x{:X}\n", Off::InSDK::ProcessEvent::PEOffset);
		std::cerr << std::format("PE-Index: 0x{:X}\n\n", ProcessEventIdx);
		return;
	}

	std::cerr << "\nCouldn't find ProcessEvent!\n\n" << std::endl;
}

/**
 * @brief 通过索引和模块名初始化 ProcessEvent 的偏移量。
 * @details 这是一个备用初始化函数，当 ProcessEvent 的索引已知时使用。
 *          它直接从虚函数表中获取函数地址，并根据模块基地址计算偏移量。
 * @param Index ProcessEvent 在虚函数表中的索引。
 * @param ModuleName 包含 ProcessEvent 的模块名称。
 */
void Off::InSDK::ProcessEvent::InitPE(int32 Index, const char* const ModuleName)
{
	Off::InSDK::ProcessEvent::PEIndex = Index;

	void** VFT = *reinterpret_cast<void***>(ObjectArray::GetByIndex(0).GetAddress());

	Off::InSDK::ProcessEvent::PEOffset = reinterpret_cast<uintptr_t>(VFT[Off::InSDK::ProcessEvent::PEIndex]) - GetModuleBase(ModuleName);

	std::cerr << std::format("PE-Offset: 0x{:X}\n", Off::InSDK::ProcessEvent::PEOffset);
}

/* UWorld */
/**
 * @brief 初始化 GWorld 的偏移量。
 * @details GWorld 是一个指向当前 UWorld 实例的全局指针。
 *          此函数通过在内存中搜索指向 UWorld 对象的指针来找到 GWorld 的地址，
 *          并计算其偏移量。
 */
void Off::InSDK::World::InitGWorld()
{
	UEClass UWorld = ObjectArray::FindClassFast("World");

	for (UEObject Obj : ObjectArray())
	{
		if (Obj.HasAnyFlags(EObjectFlags::ClassDefaultObject) || !Obj.IsA(UWorld))
			continue;

		/* Try to find a pointer to the word, aka UWorld** GWorld */
		void* Result = FindAlignedValueInProcess(Obj.GetAddress());

		/* Pointer to UWorld* couldn't be found */
		if (Result)
		{
			Off::InSDK::World::GWorld = GetOffset(Result);
			std::cerr << std::format("GWorld-Offset: 0x{:X}\n\n", Off::InSDK::World::GWorld);
			break;
		}
	}

	if (Off::InSDK::World::GWorld == 0x0)
		std::cerr << std::format("\nGWorld WAS NOT FOUND!!!!!!!!!\n\n");
}


/* FText */
/**
 * @brief 初始化 FText 相关偏移量。
 * @details FText 的内存布局比较复杂。此函数通过调用一个已知的从 FString 到 FText 的转换函数，
 *          然后在返回的 FText 对象内部查找特定的数据模式，来动态确定 FText 内部成员
 *          （如 TextData 指针和字符串数据）的偏移量。
 */
void Off::InSDK::Text::InitTextOffsets()
{
	if (!Off::InSDK::ProcessEvent::PEIndex)
	{
		std::cerr << std::format("\nDumper-7: Error, 'InitInSDKTextOffsets' was called before ProcessEvent was initialized!\n") << std::endl;
		return;
	}

	auto IsValidPtr = [](void* a) -> bool
	{
		return !IsBadReadPtr(a) && (uintptr_t(a) & 0x1) == 0; // realistically, there wont be any pointers to unaligned memory
	};


	UEFunction Conv_StringToText = ObjectArray::FindObjectFast<UEFunction>("Conv_StringToText", EClassCastFlags::Function);

	UEProperty InStringProp = nullptr;
	UEProperty ReturnProp = nullptr;

	for (UEProperty Prop : Conv_StringToText.GetProperties())
	{
		/* Func has 2 params, if the param is the return value assign to ReturnProp, else InStringProp*/
		(Prop.HasPropertyFlags(EPropertyFlags::ReturnParm) ? ReturnProp : InStringProp) = Prop;
	}

	const int32 ParamSize = Conv_StringToText.GetStructSize();

	const int32 FTextSize = ReturnProp.GetSize();

	const int32 StringOffset = InStringProp.GetOffset();
	const int32 ReturnValueOffset = ReturnProp.GetOffset();

	Off::InSDK::Text::TextSize = FTextSize;


	/* Allocate and zero-initialize ParamStruct */
#pragma warning(disable: 6255)
	uint8_t* ParamPtr = static_cast<uint8_t*>(alloca(ParamSize));
	memset(ParamPtr, 0, ParamSize);

	/* Choose a, fairly random, string to later search for in FTextData */
	constexpr const wchar_t* StringText = L"ThisIsAGoodString!";
	constexpr int32 StringLength = (sizeof(L"ThisIsAGoodString!") / sizeof(wchar_t));
	constexpr int32 StringLengthBytes = (sizeof(L"ThisIsAGoodString!"));

	/* Initialize 'InString' in the ParamStruct */
	*reinterpret_cast<FString*>(ParamPtr + StringOffset) = StringText;

	/* This function is 'static' so the object on which we call it doesn't matter */
	ObjectArray::GetByIndex(0).ProcessEvent(Conv_StringToText, ParamPtr);

	uint8_t* FTextDataPtr = nullptr;

	/* Search for the first valid pointer inside of the FText and make the offset our 'TextDatOffset' */
	for (int32 i = 0; i < (FTextSize - sizeof(void*)); i += sizeof(void*))
	{
		void* PossibleTextDataPtr = *reinterpret_cast<void**>(ParamPtr + ReturnValueOffset + i);

		if (IsValidPtr(PossibleTextDataPtr))
		{
			FTextDataPtr = static_cast<uint8_t*>(PossibleTextDataPtr);

			Off::InSDK::Text::TextDatOffset = i;
			break;
		}
	}

	if (!FTextDataPtr)
	{
		std::cerr << std::format("\nDumper-7: Error, 'FTextDataPtr' could not be found!\n") << std::endl;
		return;
	}

	constexpr int32 MaxOffset = 0x50;
	constexpr int32 StartOffset = sizeof(void*); // FString::NumElements offset

	/* Search for a pointer pointing to a int32 Value (FString::NumElements) equal to StringLength */
	for (int32 i = StartOffset; i < MaxOffset; i += sizeof(int32))
	{
		wchar_t* PosibleStringPtr = *reinterpret_cast<wchar_t**>((FTextDataPtr + i) - 0x8);
		const int32 PossibleLength = *reinterpret_cast<int32*>(FTextDataPtr + i);

		/* Check if our length matches and see if the data before the length is a pointer to our StringText */
		if (PossibleLength == StringLength && PosibleStringPtr && IsValidPtr(PosibleStringPtr) && memcmp(StringText, PosibleStringPtr, StringLengthBytes) == 0)
		{
			Off::InSDK::Text::InTextDataStringOffset = (i - 0x8);
			break;
		}
	}

	std::cerr << std::format("Off::InSDK::Text::TextSize: 0x{:X}\n", Off::InSDK::Text::TextSize);
	std::cerr << std::format("Off::InSDK::Text::TextDatOffset: 0x{:X}\n", Off::InSDK::Text::TextDatOffset);
	std::cerr << std::format("Off::InSDK::Text::InTextDataStringOffset: 0x{:X}\n\n", Off::InSDK::Text::InTextDataStringOffset);
}

/**
 * @brief 初始化所有核心偏移量。
 * @details 这是偏移量查找的主函数。它按顺序调用 OffsetFinder 中的各个查找函数，
 *          以确定 Unreal Engine 核心数据结构（如 UObject, UClass, UStruct, UFunction, FProperty 等）
 *          的关键成员偏移量。对于未找到的偏移量，会设置一个默认值以保证程序继续运行。
 *          函数执行过程中会打印找到的每个偏移量。
 */
void Off::Init()
{
	auto OverwriteIfInvalidOffset = [](int32& Offset, int32 DefaultValue)
	{
		if (Offset == OffsetFinder::OffsetNotFound)
		{
			std::cerr << std::format("Defaulting to offset: 0x{:X}\n", DefaultValue);
			Offset = DefaultValue;
		}
	};

	Off::UObject::Flags = OffsetFinder::FindUObjectFlagsOffset();
	OverwriteIfInvalidOffset(Off::UObject::Flags, sizeof(void*)); // Default to right after VTable
	std::cerr << std::format("Off::UObject::Flags: 0x{:X}\n", Off::UObject::Flags);

	Off::UObject::Index = OffsetFinder::FindUObjectIndexOffset();
	OverwriteIfInvalidOffset(Off::UObject::Index, (Off::UObject::Flags + sizeof(int32))); // Default to right after Flags
	std::cerr << std::format("Off::UObject::Index: 0x{:X}\n", Off::UObject::Index);

	Off::UObject::Class = OffsetFinder::FindUObjectClassOffset();
	OverwriteIfInvalidOffset(Off::UObject::Class, (Off::UObject::Index + sizeof(int32))); // Default to right after Index
	std::cerr << std::format("Off::UObject::Class: 0x{:X}\n", Off::UObject::Class);

	Off::UObject::Outer = OffsetFinder::FindUObjectOuterOffset();
	std::cerr << std::format("Off::UObject::Outer: 0x{:X}\n", Off::UObject::Outer);

	Off::UObject::Name = OffsetFinder::FindUObjectNameOffset();
	OverwriteIfInvalidOffset(Off::UObject::Name, (Off::UObject::Class + sizeof(void*))); // Default to right after Class
	std::cerr << std::format("Off::UObject::Name: 0x{:X}\n\n", Off::UObject::Name);

	OverwriteIfInvalidOffset(Off::UObject::Outer, (Off::UObject::Name + sizeof(int32) + sizeof(int32)));  // Default to right after Name


	OffsetFinder::InitFNameSettings();

	::NameArray::PostInit();

	Off::UStruct::Children = OffsetFinder::FindChildOffset();
	std::cerr << std::format("Off::UStruct::Children: 0x{:X}\n", Off::UStruct::Children);

	Off::UField::Next = OffsetFinder::FindUFieldNextOffset();
	std::cerr << std::format("Off::UField::Next: 0x{:X}\n", Off::UField::Next);

	Off::UStruct::SuperStruct = OffsetFinder::FindSuperOffset();
	std::cerr << std::format("Off::UStruct::SuperStruct: 0x{:X}\n", Off::UStruct::SuperStruct);

	Off::UStruct::Size = OffsetFinder::FindStructSizeOffset();
	std::cerr << std::format("Off::UStruct::Size: 0x{:X}\n", Off::UStruct::Size);

	Off::UStruct::MinAlignemnt = OffsetFinder::FindMinAlignmentOffset();
	std::cerr << std::format("Off::UStruct::MinAlignemnts: 0x{:X}\n", Off::UStruct::MinAlignemnt);

	Off::UClass::CastFlags = OffsetFinder::FindCastFlagsOffset();
	std::cerr << std::format("Off::UClass::CastFlags: 0x{:X}\n", Off::UClass::CastFlags);

	// Castflags become available for use

	if (Settings::Internal::bUseFProperty)
	{
		std::cerr << std::format("\nGame uses FProperty system\n\n");

		Off::UStruct::ChildProperties = OffsetFinder::FindChildPropertiesOffset();
		std::cerr << std::format("Off::UStruct::ChildProperties: 0x{:X}\n", Off::UStruct::ChildProperties);

		OffsetFinder::FixupHardcodedOffsets(); // must be called after FindChildPropertiesOffset 

		Off::FField::Next = OffsetFinder::FindFFieldNextOffset();
		std::cerr << std::format("Off::FField::Next: 0x{:X}\n", Off::FField::Next);
		
		Off::FField::Name = OffsetFinder::FindFFieldNameOffset();
		std::cerr << std::format("Off::FField::Name: 0x{:X}\n", Off::FField::Name);

		/*
		* FNameSize might be wrong at this point of execution.
		* FField::Flags is not critical so a fix is only applied later in OffsetFinder::PostInitFNameSettings().
		*/
		Off::FField::Flags = Off::FField::Name + Off::InSDK::Name::FNameSize;
		std::cerr << std::format("Off::FField::Flags: 0x{:X}\n", Off::FField::Flags);
	}

	Off::UClass::ClassDefaultObject = OffsetFinder::FindDefaultObjectOffset();
	std::cerr << std::format("Off::UClass::ClassDefaultObject: 0x{:X}\n", Off::UClass::ClassDefaultObject);

	Off::UClass::ImplementedInterfaces = OffsetFinder::FindImplementedInterfacesOffset();
	std::cerr << std::format("Off::UClass::ImplementedInterfaces: 0x{:X}\n", Off::UClass::ImplementedInterfaces);

	Off::UEnum::Names = OffsetFinder::FindEnumNamesOffset();
	std::cerr << std::format("Off::UEnum::Names: 0x{:X}\n", Off::UEnum::Names) << std::endl;

	Off::UFunction::FunctionFlags = OffsetFinder::FindFunctionFlagsOffset();
	std::cerr << std::format("Off::UFunction::FunctionFlags: 0x{:X}\n", Off::UFunction::FunctionFlags);

	Off::UFunction::ExecFunction = OffsetFinder::FindFunctionNativeFuncOffset();
	std::cerr << std::format("Off::UFunction::ExecFunction: 0x{:X}\n", Off::UFunction::ExecFunction) << std::endl;

	Off::Property::ElementSize = OffsetFinder::FindElementSizeOffset();
	std::cerr << std::format("Off::Property::ElementSize: 0x{:X}\n", Off::Property::ElementSize);

	Off::Property::ArrayDim = OffsetFinder::FindArrayDimOffset();
	std::cerr << std::format("Off::Property::ArrayDim: 0x{:X}\n", Off::Property::ArrayDim);

	Off::Property::Offset_Internal = OffsetFinder::FindOffsetInternalOffset();
	std::cerr << std::format("Off::Property::Offset_Internal: 0x{:X}\n", Off::Property::Offset_Internal);

	Off::Property::PropertyFlags = OffsetFinder::FindPropertyFlagsOffset();
	std::cerr << std::format("Off::Property::PropertyFlags: 0x{:X}\n", Off::Property::PropertyFlags);

	Off::InSDK::Properties::PropertySize = OffsetFinder::FindBoolPropertyBaseOffset();
	std::cerr << std::format("UPropertySize: 0x{:X}\n", Off::InSDK::Properties::PropertySize) << std::endl;

	Off::ArrayProperty::Inner = OffsetFinder::FindInnerTypeOffset(Off::InSDK::Properties::PropertySize);
	std::cerr << std::format("Off::ArrayProperty::Inner: 0x{:X}\n", Off::ArrayProperty::Inner);

	Off::SetProperty::ElementProp = OffsetFinder::FindSetPropertyBaseOffset(Off::InSDK::Properties::PropertySize);
	std::cerr << std::format("Off::SetProperty::ElementProp: 0x{:X}\n", Off::SetProperty::ElementProp);

	Off::MapProperty::Base = OffsetFinder::FindMapPropertyBaseOffset(Off::InSDK::Properties::PropertySize);
	std::cerr << std::format("Off::MapProperty::Base: 0x{:X}\n", Off::MapProperty::Base) << std::endl;

	Off::InSDK::ULevel::Actors = OffsetFinder::FindLevelActorsOffset();
	std::cerr << std::format("Off::InSDK::ULevel::Actors: 0x{:X}\n", Off::InSDK::ULevel::Actors) << std::endl;

	Off::InSDK::UDataTable::RowMap = OffsetFinder::FindDatatableRowMapOffset();
	std::cerr << std::format("Off::InSDK::UDataTable::RowMap: 0x{:X}\n", Off::InSDK::UDataTable::RowMap) << std::endl;

	OffsetFinder::PostInitFNameSettings();

	std::cerr << std::endl;

	Off::ByteProperty::Enum = Off::InSDK::Properties::PropertySize;
	Off::BoolProperty::Base = Off::InSDK::Properties::PropertySize;
	Off::ObjectProperty::PropertyClass = Off::InSDK::Properties::PropertySize; 
	Off::StructProperty::Struct = Off::InSDK::Properties::PropertySize;
	Off::EnumProperty::Base = Off::InSDK::Properties::PropertySize;
	Off::DelegateProperty::SignatureFunction = Off::InSDK::Properties::PropertySize;
	Off::FieldPathProperty::FieldClass = Off::InSDK::Properties::PropertySize;
	Off::OptionalProperty::ValueProperty = Off::InSDK::Properties::PropertySize;

	Off::ClassProperty::MetaClass = Off::InSDK::Properties::PropertySize + 0x8; //0x8 inheritance from ObjectProperty
}

/**
 * @brief 初始化各种属性的大小。
 * @details 除了偏移量，某些属性类型的大小也需要动态确定。
 *          此命名空间下的函数负责初始化这些大小，例如 TDelegate 和 FFieldPath。
 */
void PropertySizes::Init()
{
	InitTDelegateSize();
	InitFFieldPathSize();
}

/**
 * @brief 初始化 TDelegate 的大小。
 * @details 通过查找一个已知的委托属性（如 AudioComponent::OnQueueSubtitles）
 *          来确定 TDelegate（在UProperty系统中为FDelegateProperty）的大小。
 *          如果找不到，则会遍历所有对象来寻找一个委托属性。
 */
void PropertySizes::InitTDelegateSize()
{
	/* If the AudioComponent class or the OnQueueSubtitles member weren't found, fallback to looping GObjects and looking for a Delegate. */
	auto OnPropertyNotFoudn = [&]() -> void
	{
		for (UEObject Obj : ObjectArray())
		{
			if (!Obj.IsA(EClassCastFlags::Struct))
				continue;

			for (UEProperty Prop : Obj.Cast<UEClass>().GetProperties())
			{
				if (Prop.IsA(EClassCastFlags::DelegateProperty))
				{
					PropertySizes::DelegateProperty = Prop.GetSize();
					return;
				}
			}
		}
	};

	const UEClass AudioComponentClass = ObjectArray::FindClassFast("AudioComponent");

	if (!AudioComponentClass)
		return OnPropertyNotFoudn();

	const UEProperty OnQueueSubtitlesProp = AudioComponentClass.FindMember("OnQueueSubtitles", EClassCastFlags::DelegateProperty);

	if (!OnQueueSubtitlesProp)
		return OnPropertyNotFoudn();

	PropertySizes::DelegateProperty = OnQueueSubtitlesProp.GetSize();
}

/**
 * @brief 初始化 FFieldPath 的大小。
 * @details 在 FProperty 系统中，通过查找一个已知的 FFieldPathProperty 属性
 *          （如 SetFieldPathPropertyByName 函数的 Value 参数）来确定其大小。
 *          如果找不到，则会遍历所有对象来寻找一个 FFieldPathProperty。
 */
void PropertySizes::InitFFieldPathSize()
{
	if (!Settings::Internal::bUseFProperty)
		return;

	/* If the SetFieldPathPropertyByName function or the Value parameter weren't found, fallback to looping GObjects and looking for a Delegate. */
	auto OnPropertyNotFoudn = [&]() -> void
	{
		for (UEObject Obj : ObjectArray())
		{
			if (!Obj.IsA(EClassCastFlags::Struct))
				continue;

			for (UEProperty Prop : Obj.Cast<UEClass>().GetProperties())
			{
				if (Prop.IsA(EClassCastFlags::FieldPathProperty))
				{
					PropertySizes::FieldPathProperty = Prop.GetSize();
					return;
				}
			}
		}
	};

	const UEFunction SetFieldPathPropertyByNameFunc = ObjectArray::FindObjectFast<UEFunction>("SetFieldPathPropertyByName", EClassCastFlags::Function);

	if (!SetFieldPathPropertyByNameFunc)
		return OnPropertyNotFoudn();

	const UEProperty ValueParamProp = SetFieldPathPropertyByNameFunc.FindMember("Value", EClassCastFlags::FieldPathProperty);

	if (!ValueParamProp)
		return OnPropertyNotFoudn();

	PropertySizes::FieldPathProperty = ValueParamProp.GetSize();
}