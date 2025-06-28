#pragma once
#include "Unreal/Enums.h"
#include "../Settings.h"


// 固定大小的UObject数组布局
struct FFixedUObjectArrayLayout
{
	int32 ObjectsOffset = -1;
	int32 MaxObjectsOffset = -1;
	int32 NumObjectsOffset = -1;

	// 检查布局是否有效
	inline bool IsValid() const
	{
		return ObjectsOffset != -1 && MaxObjectsOffset != -1 && NumObjectsOffset != -1;
	}
};

// 分块的固定UObject数组布局
struct FChunkedFixedUObjectArrayLayout
{
	int32 ObjectsOffset = -1;
	int32 MaxElementsOffset = -1;
	int32 NumElementsOffset = -1;
	int32 MaxChunksOffset = -1;
	int32 NumChunksOffset = -1;

	// 检查布局是否有效
	inline bool IsValid() const
	{
		return ObjectsOffset != -1 && MaxElementsOffset != -1 && NumElementsOffset != -1 && MaxChunksOffset != -1 && NumChunksOffset != -1;
	}
};

namespace Off
{
	// 初始化所有偏移量
	void Init();

	// 这些偏移量不在生成期间使用，而是在生成的SDK内部使用
	namespace InSDK
	{
		// ProcessEvent相关偏移量
		namespace ProcessEvent
		{
			inline int32 PEIndex;
			inline int32 PEOffset;

			void InitPE();
			void InitPE(int32 Index, const char* const ModuleName = nullptr);
		}

		// GWorld相关偏移量
		namespace World
		{
			inline int32 GWorld = 0x0;

			void InitGWorld();
		}

		// 对象数组相关偏移量
		namespace ObjArray
		{
			inline int32 GObjects;
			inline int32 ChunkSize;
			inline int32 FUObjectItemSize;
			inline int32 FUObjectItemInitialOffset;
		}

		// 名称(FName)相关偏移量
		namespace Name
		{
			/* Whether we're using FName::AppendString or, in an edge case, FName::ToString */
			// 我们是使用 FName::AppendString 还是在极端情况下使用 FName::ToString
			inline bool bIsUsingAppendStringOverToString = true;
			inline int32 AppendNameToString;
			inline int32 FNameSize;
		}

		// 名称数组(FNameArray)相关偏移量
		namespace NameArray
		{
			inline int32 GNames = 0x0;
			inline int32 FNamePoolBlockOffsetBits = 0x0;
			inline int32 FNameEntryStride = 0x0;
		}

		// 属性相关偏移量
		namespace Properties
		{
			inline int32 PropertySize;
		}

		// FText相关偏移量
		namespace Text
		{
			inline int32 TextDatOffset = 0x0;

			inline int32 InTextDataStringOffset = 0x0;

			inline int32 TextSize = 0x0;

			void InitTextOffsets();
		}

		// ULevel相关偏移量
		namespace ULevel
		{
			inline int32 Actors;
		}

		// UDataTable相关偏移量
		namespace UDataTable
		{
			inline int32 RowMap;
		}
	}

	// FUObjectArray相关偏移量
	namespace FUObjectArray
	{
		inline FFixedUObjectArrayLayout FixedLayout;
		inline FChunkedFixedUObjectArrayLayout ChunkedFixedLayout;

		inline bool bIsChunked = false;

		inline int32 GetObjectsOffset() { return  bIsChunked ? ChunkedFixedLayout.ObjectsOffset : FixedLayout.ObjectsOffset; }
		inline int32 GetNumElementsOffset() { return  bIsChunked ? ChunkedFixedLayout.NumElementsOffset : FixedLayout.NumObjectsOffset; }
	}

	// 名称数组(GNames)相关偏移量
	namespace NameArray
	{
		inline int32 ChunksStart;
		inline int32 MaxChunkIndex;
		inline int32 NumElements;
		inline int32 ByteCursor;
	}

	// FField相关偏移量
	namespace FField
	{
		// Fixed for CasePreserving FNames by OffsetFinder::FixupHardcodedOffsets();
		// 通过 OffsetFinder::FixupHardcodedOffsets() 修正，以支持保留大小写的 FName
		inline int32 Vft = 0x00;
		inline int32 Class = 0x08;
		inline int32 Owner = 0x10;
		inline int32 Next = 0x20;
		inline int32 Name = 0x28;
		inline int32 Flags = 0x30;
	}

	// FFieldClass相关偏移量
	namespace FFieldClass
	{
		// Fixed for CasePreserving FNames by OffsetFinder::FixupHardcodedOffsets();
		// Fixed for OutlineNumber FNames by OffsetFinder::FixFNameSize();
		// 通过 OffsetFinder::FixupHardcodedOffsets() 修正，以支持保留大小写的 FName
		// 通过 OffsetFinder::FixFNameSize() 修正，以支持带数字后缀的 FName
		inline int32 Name = 0x00;
		inline int32 Id = 0x08;
		inline int32 CastFlags = 0x10;
		inline int32 ClassFlags = 0x18;
		inline int32 SuperClass = 0x20;
	}

	// FName相关偏移量
	namespace FName
	{
		// These values initialized by OffsetFinder::InitUObjectOffsets()
		// 这些值由 OffsetFinder::InitUObjectOffsets() 初始化
		inline int32 CompIdx = 0x0;
		inline int32 Number = 0x4;
	}

	// FNameEntry相关偏移量
	namespace FNameEntry
	{
		// These values are initialized by FNameEntry::Init()
		// 这些值由 FNameEntry::Init() 初始化
		namespace NameArray
		{
			inline int32 StringOffset;
			inline int32 IndexOffset;
		}

		// These values are initialized by FNameEntry::Init()
		// 这些值由 FNameEntry::Init() 初始化
		namespace NamePool
		{
			inline int32 HeaderOffset;
			inline int32 StringOffset;
		}
	}

	// UObject相关偏移量
	namespace UObject
	{
		inline int32 Vft;
		inline int32 Flags;
		inline int32 Index;
		inline int32 Class;
		inline int32 Name;
		inline int32 Outer;
	}

	// UField相关偏移量
	namespace UField
	{
		inline int32 Next;
	}
	// UEnum相关偏移量
	namespace UEnum
	{
		inline int32 Names;
	}

	// UStruct相关偏移量
	namespace UStruct
	{
		inline int32 SuperStruct;
		inline int32 Children;
		inline int32 ChildProperties;
		inline int32 Size;
		inline int32 MinAlignemnt;
	}

	// UFunction相关偏移量
	namespace UFunction
	{
		inline int32 FunctionFlags;
		inline int32 ExecFunction;
	}

	// UClass相关偏移量
	namespace UClass
	{
		inline int32 CastFlags;
		inline int32 ClassDefaultObject;
		inline int32 ImplementedInterfaces;
	}

	// Property相关偏移量
	namespace Property
	{
		inline int32 ArrayDim;
		inline int32 ElementSize;
		inline int32 PropertyFlags;
		inline int32 Offset_Internal;
	}

	// ByteProperty相关偏移量
	namespace ByteProperty
	{
		inline int32 Enum;
	}

	// BoolProperty相关偏移量
	namespace BoolProperty
	{
		struct UBoolPropertyBase
		{
			uint8 FieldSize;
			uint8 ByteOffset;
			uint8 ByteMask;
			uint8 FieldMask;
		};

		inline int32 Base;
	}

	// ObjectProperty相关偏移量
	namespace ObjectProperty
	{
		inline int32 PropertyClass;
	}

	// ClassProperty相关偏移量
	namespace ClassProperty
	{
		inline int32 MetaClass;
	}

	// StructProperty相关偏移量
	namespace StructProperty
	{
		inline int32 Struct;
	}

	// ArrayProperty相关偏移量
	namespace ArrayProperty
	{
		inline int32 Inner;
	}

	// DelegateProperty相关偏移量
	namespace DelegateProperty
	{
		inline int32 SignatureFunction;
	}

	// MapProperty相关偏移量
	namespace MapProperty
	{
		struct UMapPropertyBase
		{
			void* KeyProperty;
			void* ValueProperty;
		};

		inline int32 Base;
	}

	// SetProperty相关偏移量
	namespace SetProperty
	{
		inline int32 ElementProp;
	}

	// EnumProperty相关偏移量
	namespace EnumProperty
	{
		struct UEnumPropertyBase
		{
			void* UnderlayingProperty;
			class UEnum* Enum;
		};

		inline int32 Base;
	}

	// FieldPathProperty相关偏移量
	namespace FieldPathProperty
	{
		inline int32 FieldClass;
	}

	// OptionalProperty相关偏移量
	namespace OptionalProperty
	{
		inline int32 ValueProperty;
	}
}

// 各种属性的大小
namespace PropertySizes
{
	// 初始化属性大小
	void Init();

	/* These are properties for which their size might change depending on the UE version or compilerflags. */
	// 这些是其大小可能因UE版本或编译器标志而异的属性。
	inline int32 DelegateProperty = 0x10;
	void InitTDelegateSize();

	inline int32 FieldPathProperty = 0x20;
	void InitFFieldPathSize();
}
