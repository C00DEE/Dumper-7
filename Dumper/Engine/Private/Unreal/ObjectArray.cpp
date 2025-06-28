#include <iostream>
#include <fstream>
#include <format>
#include <filesystem>

#include "Unreal/ObjectArray.h"
#include "OffsetFinder/Offsets.h"
#include "Utils.h"


namespace fs = std::filesystem;

// 定义不同版本UE4的固定UObject数组布局
constexpr inline std::array FFixedUObjectArrayLayouts =
{
	FFixedUObjectArrayLayout // Default UE4.11 - UE4.20
	{
		.ObjectsOffset = 0x0,
		.MaxObjectsOffset = 0x8,
		.NumObjectsOffset = 0xC
	}
};

// 定义不同版本UE4的分块固定UObject数组布局
constexpr inline std::array FChunkedFixedUObjectArrayLayouts =
{
	FChunkedFixedUObjectArrayLayout // Default UE4.21 and above
	{
		.ObjectsOffset = 0x00,
		.MaxElementsOffset = 0x10,
		.NumElementsOffset = 0x14,
		.MaxChunksOffset = 0x18,
		.NumChunksOffset = 0x1C,
	},
	FChunkedFixedUObjectArrayLayout // Back4Blood
	{
		.ObjectsOffset = 0x10, // last
		.MaxElementsOffset = 0x00,
		.NumElementsOffset = 0x04,
		.MaxChunksOffset = 0x08,
		.NumChunksOffset = 0x0C,
	},
	FChunkedFixedUObjectArrayLayout // DeltaForce
	{
		.ObjectsOffset = 0x20,
		.MaxElementsOffset = 0x10,
		.NumElementsOffset = 0x4,
		.MaxChunksOffset = 0x0,
		.NumChunksOffset = 0x14,
	},
	FChunkedFixedUObjectArrayLayout // Mutliversus
	{
		.ObjectsOffset = 0x18,
		.MaxElementsOffset = 0x10,
		.NumElementsOffset = 0x00, // first
		.MaxChunksOffset = 0x14,
		.NumChunksOffset = 0x20,
	}
};

// 验证地址是否是有效的GObjects（固定布局）
bool IsAddressValidGObjects(const uintptr_t Address, const FFixedUObjectArrayLayout& Layout)
{
	/* 假设在所有使用FFixedUObjectArray的游戏中FUObjectItem布局是常量的。 */
	struct FUObjectItem
	{
		void* Object;
		uint8_t Pad[0x10];
	};


	void* Objects = *reinterpret_cast<void**>(Address + Layout.ObjectsOffset);
	const int32 MaxElements = *reinterpret_cast<const int32*>(Address + Layout.MaxObjectsOffset);
	const int32 NumElements = *reinterpret_cast<const int32*>(Address + Layout.NumObjectsOffset);


	FUObjectItem* ObjectsButDecrypted = (FUObjectItem*)ObjectArray::DecryptPtr(Objects);

	if (NumElements > MaxElements)
		return false;

	if (MaxElements > 0x400000)
		return false;

	if (NumElements < 0x1000)
		return false;

	if (IsBadReadPtr(ObjectsButDecrypted))
		return false;

	if (IsBadReadPtr(ObjectsButDecrypted[5].Object))
		return false;

	const uintptr_t FithObject = reinterpret_cast<uintptr_t>(ObjectsButDecrypted[0x5].Object);
	const int32 IndexOfFithobject = *reinterpret_cast<int32_t*>(FithObject + 0xC);

	if (IndexOfFithobject != 0x5)
		return false;

	return true;
}

// 验证地址是否是有效的GObjects（分块布局）
bool IsAddressValidGObjects(const uintptr_t Address, const FChunkedFixedUObjectArrayLayout& Layout)
{
	void* Objects = *reinterpret_cast<void**>(Address + Layout.ObjectsOffset);
	const int32 MaxElements = *reinterpret_cast<const int32*>(Address + Layout.MaxElementsOffset);
	const int32 NumElements = *reinterpret_cast<const int32*>(Address + Layout.NumElementsOffset);
	const int32 MaxChunks   = *reinterpret_cast<const int32*>(Address + Layout.MaxChunksOffset);
	const int32 NumChunks   = *reinterpret_cast<const int32*>(Address + Layout.NumChunksOffset);

	void** ObjectsPtrButDecrypted = reinterpret_cast<void**>(ObjectArray::DecryptPtr(Objects));

	if (NumChunks > 0x14 || NumChunks < 0x1)
		return false;

	if (MaxChunks > 0x5FF || MaxChunks < 0x6)
		return false;

	if (NumElements > MaxElements || NumChunks > MaxChunks)
		return false;

	/* 区块数量对于所有元素来说永远不会太多或太少。不同的UE版本上出现两种不同的区块大小（0x10000, 0x10400）并进行检查。 */
	const bool bNumChunksFitsNumElements = ((NumElements / 0x10000) + 1) == NumChunks || ((NumElements / 0x10400) + 1) == NumChunks;

	if (!bNumChunksFitsNumElements)
		return false;

	/* 与上述相同，但是针对最大元素数量/区块。 */
	const bool bMaxChunksFitsMaxElements = (MaxElements / 0x10000) == MaxChunks || (MaxElements / 0x10400) == MaxChunks;

	if (!bMaxChunksFitsMaxElements)
		return false;

	/* 区块指针必须始终有效（特别是因为它已经被解密[如果它曾被加密的话]） */
	if (!ObjectsPtrButDecrypted || IsBadReadPtr(ObjectsPtrButDecrypted))
		return false;

	/* 检查每个区块指针是否有效。 */
	for (int i = 0; i < NumChunks; i++)
	{
		if (!ObjectsPtrButDecrypted[i] || IsBadReadPtr(ObjectsPtrButDecrypted[i]))
			return false;
	}
	
	return true;
}


// 初始化FUObjectItem，确定偏移量和大小
void ObjectArray::InitializeFUObjectItem(uint8_t* FirstItemPtr)
{
	for (int i = 0x0; i < 0x10; i += 4)
	{
		if (!IsBadReadPtr(*reinterpret_cast<uint8_t**>(FirstItemPtr + i)))
		{
			FUObjectItemInitialOffset = i;
			break;
		}
	}

	for (int i = FUObjectItemInitialOffset + 0x8; i <= 0x38; i += 4)
	{
		void* SecondObject = *reinterpret_cast<uint8**>(FirstItemPtr + i);
		void* ThirdObject  = *reinterpret_cast<uint8**>(FirstItemPtr + (i * 2) - FUObjectItemInitialOffset);

		if (!IsBadReadPtr(SecondObject) && !IsBadReadPtr(*reinterpret_cast<void**>(SecondObject)) && !IsBadReadPtr(ThirdObject) && !IsBadReadPtr(*reinterpret_cast<void**>(ThirdObject)))
		{
			SizeOfFUObjectItem = i - FUObjectItemInitialOffset;
			break;
		}
	}

	Off::InSDK::ObjArray::FUObjectItemInitialOffset = FUObjectItemInitialOffset;
	Off::InSDK::ObjArray::FUObjectItemSize = SizeOfFUObjectItem;
}

// 初始化解密功能
void ObjectArray::InitDecryption(uint8_t* (*DecryptionFunction)(void* ObjPtr), const char* DecryptionLambdaAsStr)
{
	DecryptPtr = DecryptionFunction;
	DecryptionLambdaStr = DecryptionLambdaAsStr;
}

// 初始化区块大小
void ObjectArray::InitializeChunkSize(uint8_t* ChunksPtr)
{
	int IndexOffset = 0x0;
	uint8* ObjAtIdx374 = (uint8*)ByIndex(ChunksPtr, 0x374, SizeOfFUObjectItem, FUObjectItemInitialOffset, 0x10000);
	uint8* ObjAtIdx106 = (uint8*)ByIndex(ChunksPtr, 0x106, SizeOfFUObjectItem, FUObjectItemInitialOffset, 0x10000);

	for (int i = 0x8; i < 0x20; i++)
	{
		if (*reinterpret_cast<int32*>(ObjAtIdx374 + i) == 0x374 && *reinterpret_cast<int32*>(ObjAtIdx106 + i) == 0x106)
		{
			IndexOffset = i;
			break;
		}
	}

	int IndexToCheck = 0x10400;
	while (ObjectArray::Num() > IndexToCheck)
	{
		if (void* Obj = ByIndex(ChunksPtr, IndexToCheck, SizeOfFUObjectItem, FUObjectItemInitialOffset, 0x10000))
		{
			const bool bHasBiggerChunkSize = (*reinterpret_cast<int32*>((uint8*)Obj + IndexOffset) != IndexToCheck);
			NumElementsPerChunk = bHasBiggerChunkSize ? 0x10400 : 0x10000;
			break;
		}
		IndexToCheck += 0x10400;
	}

	Off::InSDK::ObjArray::ChunkSize = NumElementsPerChunk;
}

/* 我们不讨论这个函数... */
// 初始化对象数组，扫描内存找到GObjects
void ObjectArray::Init(bool bScanAllMemory, const char* const ModuleName)
{
	if (!bScanAllMemory)
		std::cerr << "\nDumper-7 by me, you & him\n\n\n";

	const auto [ImageBase, ImageSize] = GetImageBaseAndSize(ModuleName);

	uintptr_t SearchBase = ImageBase;
	DWORD SearchRange = ImageSize;

	if (!bScanAllMemory)
	{
		const auto [DataSection, DataSize] = GetSectionByName(ImageBase, ".data");

		if (DataSection != 0x0 && DataSize != 0x0)
		{
			SearchBase = DataSection;
			SearchRange = DataSize;
		}
		else
		{
			bScanAllMemory = true;
		}
	}

	/* 减去0x50，这样我们在检查FixedArray->IsValid()或ChunkedArray->IsValid()时就不会尝试读取越界内存 */
	SearchRange -= 0x50;

	if (!bScanAllMemory)
		std::cerr << "Searching for GObjects...\n\n";

	// Lambda函数，检查地址是否符合任何数组布局
	auto MatchesAnyLayout = []<typename ArrayLayoutType, size_t Size>(const std::array<ArrayLayoutType, Size>& ObjectArrayLayouts, uintptr_t Address)
	{
		for (const ArrayLayoutType& Layout : ObjectArrayLayouts)
		{
			if (!IsAddressValidGObjects(Address, Layout))
				continue;

			if constexpr (std::is_same_v<ArrayLayoutType, FFixedUObjectArrayLayout>)
			{
				Off::FUObjectArray::bIsChunked = false;
				Off::FUObjectArray::FixedLayout = Layout;
			}
			else
			{
				Off::FUObjectArray::bIsChunked = true;
				Off::FUObjectArray::ChunkedFixedLayout = Layout;
			}

			return true;
		}
		
		return false;
	};

	// 扫描内存寻找GObjects
	for (int i = 0; i < SearchRange; i += 0x4)
	{
		const uintptr_t CurrentAddress = SearchBase + i;

		if (MatchesAnyLayout(FFixedUObjectArrayLayouts, CurrentAddress))
		{
			GObjects = reinterpret_cast<uint8_t*>(SearchBase + i);
			NumElementsPerChunk = -1;

			Off::InSDK::ObjArray::GObjects = (SearchBase + i) - ImageBase;

			std::cerr << "Found FFixedUObjectArray GObjects at offset 0x" << std::hex << Off::InSDK::ObjArray::GObjects << std::dec << "\n\n";

			ByIndex = [](void* ObjectsArray, int32 Index, uint32 FUObjectItemSize, uint32 FUObjectItemOffset, uint32 PerChunk) -> void*
			{
				if (Index < 0 || Index > Num())
					return nullptr;

				uint8_t* ChunkPtr = DecryptPtr(*reinterpret_cast<uint8_t**>(ObjectsArray));

				return *reinterpret_cast<void**>(ChunkPtr + FUObjectItemOffset + (Index * FUObjectItemSize));
			};

			uint8_t* ChunksPtr = DecryptPtr(*reinterpret_cast<uint8_t**>(GObjects + Off::FUObjectArray::GetObjectsOffset()));

			ObjectArray::InitializeFUObjectItem(*reinterpret_cast<uint8_t**>(ChunksPtr));

			return;
		}
		else if (MatchesAnyLayout(FChunkedFixedUObjectArrayLayouts, CurrentAddress))
		{
			GObjects = reinterpret_cast<uint8_t*>(SearchBase + i);
			NumElementsPerChunk = 0x10000;
			SizeOfFUObjectItem = 0x18;
			FUObjectItemInitialOffset = 0x0;

			Off::InSDK::ObjArray::GObjects = (SearchBase + i) - ImageBase;

			std::cerr << "Found FChunkedFixedUObjectArray GObjects at offset 0x" << std::hex << Off::InSDK::ObjArray::GObjects << std::dec << "\n\n";

			ByIndex = [](void* ObjectsArray, int32 Index, uint32 FUObjectItemSize, uint32 FUObjectItemOffset, uint32 PerChunk) -> void*
			{
				if (Index < 0 || Index > Num())
					return nullptr;

				const int32 ChunkIndex = Index / PerChunk;
				const int32 InChunkIdx = Index % PerChunk;

				uint8_t* ChunkPtr = DecryptPtr(*reinterpret_cast<uint8_t**>(ObjectsArray));

				uint8_t* Chunk = reinterpret_cast<uint8_t**>(ChunkPtr)[ChunkIndex];
				uint8_t* ItemPtr = Chunk + (InChunkIdx * FUObjectItemSize);

				return *reinterpret_cast<void**>(ItemPtr + FUObjectItemOffset);
			};
			
			uint8_t* ChunksPtr = DecryptPtr(*reinterpret_cast<uint8_t**>(GObjects + Off::FUObjectArray::GetObjectsOffset()));

			ObjectArray::InitializeFUObjectItem(*reinterpret_cast<uint8_t**>(ChunksPtr));

			ObjectArray::InitializeChunkSize(GObjects + Off::FUObjectArray::GetObjectsOffset());

			return;
		}
	}

	if (!bScanAllMemory)
	{
		ObjectArray::Init(true);
		return;
	}

	if (GObjects == nullptr)
	{
		std::cerr << "\nGObjects couldn't be found!\n\n\n";
		Sleep(3000);
		exit(1);
	}
}

// 使用指定偏移量初始化固定对象数组
void ObjectArray::Init(int32 GObjectsOffset, const FFixedUObjectArrayLayout& ObjectArrayLayout, const char* const ModuleName)
{
	GObjects = reinterpret_cast<uint8_t*>(GetModuleBase(ModuleName) + GObjectsOffset);
	Off::InSDK::ObjArray::GObjects = GObjectsOffset;

	std::cerr << "GObjects: 0x" << (void*)GObjects << "\n" << std::endl;

	Off::FUObjectArray::bIsChunked = false;
	Off::FUObjectArray::FixedLayout = ObjectArrayLayout.IsValid() ? ObjectArrayLayout : FFixedUObjectArrayLayouts[0];

	ByIndex = [](void* ObjectsArray, int32 Index, uint32 FUObjectItemSize, uint32 FUObjectItemOffset, uint32 PerChunk) -> void*
	{
		if (Index < 0 || Index > Num())
			return nullptr;

		uint8_t* ItemPtr = *reinterpret_cast<uint8_t**>(ObjectsArray) + (Index * FUObjectItemSize);

		return *reinterpret_cast<void**>(ItemPtr + FUObjectItemOffset);
	};

	uint8_t* ChunksPtr = DecryptPtr(*reinterpret_cast<uint8_t**>(GObjects + Off::FUObjectArray::GetObjectsOffset()));

	ObjectArray::InitializeFUObjectItem(*reinterpret_cast<uint8_t**>(ChunksPtr));
}

// 使用指定偏移量和区块大小初始化分块对象数组
void ObjectArray::Init(int32 GObjectsOffset, int32 ElementsPerChunk, const FChunkedFixedUObjectArrayLayout& ObjectArrayLayout, const char* const ModuleName)
{
	GObjects = reinterpret_cast<uint8_t*>(GetModuleBase(ModuleName) + GObjectsOffset);
	Off::InSDK::ObjArray::GObjects = GObjectsOffset;

	Off::FUObjectArray::bIsChunked = true;
	Off::FUObjectArray::ChunkedFixedLayout = ObjectArrayLayout.IsValid() ? ObjectArrayLayout : FChunkedFixedUObjectArrayLayouts[0];

	NumElementsPerChunk = ElementsPerChunk;
	Off::InSDK::ObjArray::ChunkSize = ElementsPerChunk;

	ByIndex = [](void* ObjectsArray, int32 Index, uint32 FUObjectItemSize, uint32 FUObjectItemOffset, uint32 PerChunk) -> void*
	{
		if (Index < 0 || Index > Num())
			return nullptr;

		const int32 ChunkIndex = Index / PerChunk;
		const int32 InChunkIdx = Index % PerChunk;

		uint8_t* Chunk = (*reinterpret_cast<uint8_t***>(ObjectsArray))[ChunkIndex];
		uint8_t* ItemPtr = reinterpret_cast<uint8_t*>(Chunk) + (InChunkIdx * FUObjectItemSize);

		return *reinterpret_cast<void**>(ItemPtr + FUObjectItemOffset);
	};

	uint8_t* ChunksPtr = DecryptPtr(*reinterpret_cast<uint8_t**>(GObjects + Off::FUObjectArray::GetObjectsOffset()));

	ObjectArray::InitializeFUObjectItem(*reinterpret_cast<uint8_t**>(ChunksPtr));
}

// 输出GObjects到文件
void ObjectArray::DumpObjects(const fs::path& Path, bool bWithPathname)
{
	std::ofstream DumpStream(Path / "GObjects-Dump.txt");

	DumpStream << "Object dump by Dumper-7\n\n";
	DumpStream << (!Settings::Generator::GameVersion.empty() && !Settings::Generator::GameName.empty() ? (Settings::Generator::GameVersion + '-' + Settings::Generator::GameName) + "\n\n" : "");
	DumpStream << "Count: " << Num() << "\n\n\n";

	for (auto Object : ObjectArray())
	{
		if (!bWithPathname)
		{
			DumpStream << std::format("[{:08X}] {{{}}} {}\n", Object.GetIndex(), Object.GetAddress(), Object.GetFullName());
		}
		else
		{
			DumpStream << std::format("[{:08X}] {{{}}} {}\n", Object.GetIndex(), Object.GetAddress(), Object.GetPathName());
		}
	}

	DumpStream.close();
}

// 输出带属性的GObjects到文件
void ObjectArray::DumpObjectsWithProperties(const fs::path& Path, bool bWithPathname)
{
	std::ofstream DumpStream(Path / "GObjects-Dump-WithProperties.txt");

	DumpStream << "Object dump by Dumper-7\n\n";
	DumpStream << (!Settings::Generator::GameVersion.empty() && !Settings::Generator::GameName.empty() ? (Settings::Generator::GameVersion + '-' + Settings::Generator::GameName) + "\n\n" : "");
	DumpStream << "Count: " << Num() << "\n\n\n";

	for (auto Object : ObjectArray())
	{
		if (!bWithPathname)
		{
			DumpStream << std::format("[{:08X}] {{{}}} {}\n", Object.GetIndex(), Object.GetAddress(), Object.GetFullName());
		}
		else
		{
			DumpStream << std::format("[{:08X}] {{{}}} {}\n", Object.GetIndex(), Object.GetAddress(), Object.GetPathName());
		}

		if (Object.IsA(EClassCastFlags::Struct))
		{
			for (UEProperty Prop : Object.Cast<UEStruct>().GetProperties())
			{
				DumpStream << std::format("[{:08X}] {{{}}}\t{} {}\n", Prop.GetOffset(), Prop.GetAddress(), Prop.GetPropClassName(), Prop.GetName());
			}
		}
	}

	DumpStream.close();
}

// 获取对象数组中的元素数量
int32 ObjectArray::Num()
{
	return *reinterpret_cast<int32*>(GObjects + Off::FUObjectArray::GetNumElementsOffset());
}

// 通过索引获取特定类型的对象
template<typename UEType>
static UEType ObjectArray::GetByIndex(int32 Index)
{
	return UEType(ByIndex(GObjects + Off::FUObjectArray::GetObjectsOffset(), Index, SizeOfFUObjectItem, FUObjectItemInitialOffset, NumElementsPerChunk));
}

// 通过全名查找特定类型的对象
template<typename UEType>
UEType ObjectArray::FindObject(const std::string& FullName, EClassCastFlags RequiredType)
{
	for (UEObject Object : ObjectArray())
	{
		if (Object.IsA(RequiredType) && Object.GetFullName() == FullName)
		{
			return Object.Cast<UEType>();
		}
	}

	return UEType();
}

// 通过名称快速查找特定类型的对象
template<typename UEType>
UEType ObjectArray::FindObjectFast(const std::string& Name, EClassCastFlags RequiredType)
{
	auto ObjArray = ObjectArray();

	for (UEObject Object : ObjArray)
	{
		if (Object.IsA(RequiredType) && Object.GetName() == Name)
		{
			return Object.Cast<UEType>();
		}
	}

	return UEType();
}

// 在特定外部对象中通过名称快速查找对象
template<typename UEType>
static UEType ObjectArray::FindObjectFastInOuter(const std::string& Name, std::string Outer)
{
	auto ObjArray = ObjectArray();

	for (UEObject Object : ObjArray)
	{
		if (Object.GetName() == Name && Object.GetOuter().GetName() == Outer)
		{
			return Object.Cast<UEType>();
		}
	}

	return UEType();
}

// 通过名称查找结构体
UEStruct ObjectArray::FindStruct(const std::string& Name)
{
	return FindObjectFast<UEClass>(Name, EClassCastFlags::Struct);
}

// 通过名称快速查找结构体
UEStruct ObjectArray::FindStructFast(const std::string& Name)
{
	return FindObjectFast<UEClass>(Name, EClassCastFlags::Struct);
}

// 通过全名查找类
UEClass ObjectArray::FindClass(const std::string& FullName)
{
	return FindObject<UEClass>(FullName, EClassCastFlags::Class);
}

// 通过名称快速查找类
UEClass ObjectArray::FindClassFast(const std::string& Name)
{
	return FindObjectFast<UEClass>(Name, EClassCastFlags::Class);
}

// 获取对象数组的开始迭代器
ObjectArray::ObjectsIterator ObjectArray::begin()
{
	return ObjectsIterator();
}

// 获取对象数组的结束迭代器
ObjectArray::ObjectsIterator ObjectArray::end()
{
	return ObjectsIterator(Num());
}

// 对象数组迭代器构造函数
ObjectArray::ObjectsIterator::ObjectsIterator(int32 StartIndex)
	: CurrentIndex(StartIndex), CurrentObject(ObjectArray::GetByIndex(StartIndex))
{
}

// 对象数组迭代器解引用操作符
UEObject ObjectArray::ObjectsIterator::operator*()
{
	return CurrentObject;
}

// 对象数组迭代器前缀递增操作符
ObjectArray::ObjectsIterator& ObjectArray::ObjectsIterator::operator++()
{
	CurrentObject = ObjectArray::GetByIndex(++CurrentIndex);

	while (!CurrentObject && CurrentIndex < (ObjectArray::Num() - 1))
	{
		CurrentObject = ObjectArray::GetByIndex(++CurrentIndex);
	}

	if (!CurrentObject && CurrentIndex == (ObjectArray::Num() - 1)) [[unlikely]]
		CurrentIndex++;

	return *this;
}

// 对象数组迭代器不等于操作符
bool ObjectArray::ObjectsIterator::operator!=(const ObjectsIterator& Other)
{
	return CurrentIndex != Other.CurrentIndex;
}

// 获取当前迭代器索引
int32 ObjectArray::ObjectsIterator::GetIndex() const
{
	return CurrentIndex;
}

/*
* 编译器不会为特定的模板类型生成函数，除非它在对应头文件声明的.cpp文件中使用。
*
* 参见 https://stackoverflow.com/questions/456713/why-do-i-get-unresolved-external-symbol-errors-when-using-templates
*/
[[maybe_unused]] void TemplateTypeCreationForObjectArray(void)
{
	ObjectArray::FindObject<UEObject>("");
	ObjectArray::FindObject<UEField>("");
	ObjectArray::FindObject<UEEnum>("");
	ObjectArray::FindObject<UEStruct>("");
	ObjectArray::FindObject<UEClass>("");
	ObjectArray::FindObject<UEFunction>("");
	ObjectArray::FindObject<UEProperty>("");
	ObjectArray::FindObject<UEByteProperty>("");
	ObjectArray::FindObject<UEBoolProperty>("");
	ObjectArray::FindObject<UEObjectProperty>("");
	ObjectArray::FindObject<UEClassProperty>("");
	ObjectArray::FindObject<UEStructProperty>("");
	ObjectArray::FindObject<UEArrayProperty>("");
	ObjectArray::FindObject<UEMapProperty>("");
	ObjectArray::FindObject<UESetProperty>("");
	ObjectArray::FindObject<UEEnumProperty>("");

	ObjectArray::FindObjectFast<UEObject>("");
	ObjectArray::FindObjectFast<UEField>("");
	ObjectArray::FindObjectFast<UEEnum>("");
	ObjectArray::FindObjectFast<UEStruct>("");
	ObjectArray::FindObjectFast<UEClass>("");
	ObjectArray::FindObjectFast<UEFunction>("");
	ObjectArray::FindObjectFast<UEProperty>("");
	ObjectArray::FindObjectFast<UEByteProperty>("");
	ObjectArray::FindObjectFast<UEBoolProperty>("");
	ObjectArray::FindObjectFast<UEObjectProperty>("");
	ObjectArray::FindObjectFast<UEClassProperty>("");
	ObjectArray::FindObjectFast<UEStructProperty>("");
	ObjectArray::FindObjectFast<UEArrayProperty>("");
	ObjectArray::FindObjectFast<UEMapProperty>("");
	ObjectArray::FindObjectFast<UESetProperty>("");
	ObjectArray::FindObjectFast<UEEnumProperty>("");

	ObjectArray::FindObjectFastInOuter<UEObject>("", "");
	ObjectArray::FindObjectFastInOuter<UEField>("", "");
	ObjectArray::FindObjectFastInOuter<UEEnum>("", "");
	ObjectArray::FindObjectFastInOuter<UEStruct>("", "");
	ObjectArray::FindObjectFastInOuter<UEClass>("", "");
	ObjectArray::FindObjectFastInOuter<UEFunction>("", "");
	ObjectArray::FindObjectFastInOuter<UEProperty>("", "");
	ObjectArray::FindObjectFastInOuter<UEByteProperty>("", "");
	ObjectArray::FindObjectFastInOuter<UEBoolProperty>("", "");
	ObjectArray::FindObjectFastInOuter<UEObjectProperty>("", "");
	ObjectArray::FindObjectFastInOuter<UEClassProperty>("", "");
	ObjectArray::FindObjectFastInOuter<UEStructProperty>("", "");
	ObjectArray::FindObjectFastInOuter<UEArrayProperty>("", "");
	ObjectArray::FindObjectFastInOuter<UEMapProperty>("", "");
	ObjectArray::FindObjectFastInOuter<UESetProperty>("", "");
	ObjectArray::FindObjectFastInOuter<UEEnumProperty>("", "");

	ObjectArray::GetByIndex<UEObject>(-1);
	ObjectArray::GetByIndex<UEField>(-1);
	ObjectArray::GetByIndex<UEEnum>(-1);
	ObjectArray::GetByIndex<UEStruct>(-1);
	ObjectArray::GetByIndex<UEClass>(-1);
	ObjectArray::GetByIndex<UEFunction>(-1);
	ObjectArray::GetByIndex<UEProperty>(-1);
	ObjectArray::GetByIndex<UEByteProperty>(-1);
	ObjectArray::GetByIndex<UEBoolProperty>(-1);
	ObjectArray::GetByIndex<UEObjectProperty>(-1);
	ObjectArray::GetByIndex<UEClassProperty>(-1);
	ObjectArray::GetByIndex<UEStructProperty>(-1);
	ObjectArray::GetByIndex<UEArrayProperty>(-1);
	ObjectArray::GetByIndex<UEMapProperty>(-1);
	ObjectArray::GetByIndex<UESetProperty>(-1);
	ObjectArray::GetByIndex<UEEnumProperty>(-1);
}
