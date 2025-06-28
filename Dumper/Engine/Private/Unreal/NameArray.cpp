#include <format>

#include "Unreal/ObjectArray.h"
#include "Unreal/NameArray.h"

// 名称数组的全局指针
uint8* NameArray::GNames = nullptr;

// FNameEntry构造函数，初始化地址
FNameEntry::FNameEntry(void* Ptr)
	: Address((uint8*)Ptr)
{
}

// 获取名称的宽字符串表示
std::wstring FNameEntry::GetWString()
{
	if (!Address)
		return L"";

	return GetStr(Address);
}

// 获取名称的标准字符串表示（将宽字符串转换为标准字符串）
std::string FNameEntry::GetString()
{
	if (!Address)
		return "";

	return UtfN::WStringToString(GetWString());
}

// 获取名称条目的地址
void* FNameEntry::GetAddress()
{
	return Address;
}

// 初始化FNameEntry，设置名称条目的偏移量和字符串获取函数
void FNameEntry::Init(const uint8_t* FirstChunkPtr, int64 NameEntryStringOffset)
{
	if (Settings::Internal::bUseNamePool)
	{
		// 定义常量字符串长度
		constexpr int64 NoneStrLen = 0x4;
		constexpr uint16 BytePropertyStrLen = 0xC;

		// "ByteProperty"的"Byte"部分作为uint32
		constexpr uint32 BytePropertyStartAsUint32 = 'etyB'; // "Byte" part of "ByteProperty"

		// 设置名称池的字符串偏移量和头部偏移量
		Off::FNameEntry::NamePool::StringOffset = NameEntryStringOffset;
		Off::FNameEntry::NamePool::HeaderOffset = NameEntryStringOffset == 6 ? 4 : 0;

		// 尝试找到ByteProperty条目
		uint8* AssumedBytePropertyEntry = *reinterpret_cast<uint8* const*>(FirstChunkPtr) + NameEntryStringOffset + NoneStrLen;

		/* 检查FNameEntry后是否有填充。最多检查0x4字节的填充。 */
		for (int i = 0; i < 0x4; i++)
		{
			const uint32 FirstPartOfByteProperty = *reinterpret_cast<const uint32*>(AssumedBytePropertyEntry + NameEntryStringOffset);

			if (FirstPartOfByteProperty == BytePropertyStartAsUint32)
				break;

			AssumedBytePropertyEntry += 0x1;
		}

		// 获取ByteProperty的头信息
		uint16 BytePropertyHeader = *reinterpret_cast<const uint16*>(AssumedBytePropertyEntry + Off::FNameEntry::NamePool::HeaderOffset);

		/* 移位超过头部大小是不允许的，所以这里限制移位计数 */
		constexpr int32 MaxAllowedShiftCount = sizeof(BytePropertyHeader) * 0x8;

		// 计算FNameEntry长度的移位计数
		while (BytePropertyHeader != BytePropertyStrLen && FNameEntryLengthShiftCount < MaxAllowedShiftCount)
		{			
			FNameEntryLengthShiftCount++;
			BytePropertyHeader >>= 1;
		}

		// 如果无法获取移位计数，返回错误
		if (FNameEntryLengthShiftCount == MaxAllowedShiftCount)
		{
			std::cerr << "\nDumper-7: Error, couldn't get FNameEntryLengthShiftCount!\n" << std::endl;
			GetStr = [](uint8* NameEntry) -> std::wstring { return L"Invalid FNameEntryLengthShiftCount!"; };
			return;
		}

		// 设置获取字符串的lambda函数
		GetStr = [](uint8* NameEntry) -> std::wstring
		{
			// 从条目头部获取不带编号的头信息
			const uint16 HeaderWithoutNumber = *reinterpret_cast<uint16*>(NameEntry + Off::FNameEntry::NamePool::HeaderOffset);
			// 计算名称长度
			const int32 NameLen = HeaderWithoutNumber >> FNameEntry::FNameEntryLengthShiftCount;

			if (NameLen == 0)
			{
				// 如果名称长度为0，可能是引用了其他条目
				const int32 EntryIdOffset = Off::FNameEntry::NamePool::StringOffset + ((Off::FNameEntry::NamePool::StringOffset == 6) * 2);

				const int32 NextEntryIndex = *reinterpret_cast<int32*>(NameEntry + EntryIdOffset);
				const int32 Number = *reinterpret_cast<int32*>(NameEntry + EntryIdOffset + sizeof(int32));

				// 如果有编号，附加到名称后
				if (Number > 0)
					return NameArray::GetNameEntry(NextEntryIndex).GetWString() + L'_' + std::to_wstring(Number - 1);

				return NameArray::GetNameEntry(NextEntryIndex).GetWString();
			}

			// 根据名称是否使用宽字符返回适当的字符串
			if (HeaderWithoutNumber & NameWideMask)
				return std::wstring(reinterpret_cast<const wchar_t*>(NameEntry + Off::FNameEntry::NamePool::StringOffset), NameLen);

			return UtfN::StringToWString(std::string(reinterpret_cast<const char*>(NameEntry + Off::FNameEntry::NamePool::StringOffset), NameLen));
		};
	}
	else
	{
		// 获取几个关键的FNameEntry来确定偏移量
		uint8_t* FNameEntryNone = (uint8_t*)NameArray::GetNameEntry(0x0).GetAddress();
		uint8_t* FNameEntryIdxThree = (uint8_t*)NameArray::GetNameEntry(0x3).GetAddress();
		uint8_t* FNameEntryIdxEight = (uint8_t*)NameArray::GetNameEntry(0x8).GetAddress();

		// 寻找"None"字符串偏移量
		for (int i = 0; i < 0x20; i++)
		{
			if (*reinterpret_cast<uint32*>(FNameEntryNone + i) == 'enoN') // None
			{
				Off::FNameEntry::NameArray::StringOffset = i;
				break;
			}
		}

		// 寻找索引偏移量
		for (int i = 0; i < 0x20; i++)
		{
			// 最低位是bIsWide掩码，右移1位得到索引
			if ((*reinterpret_cast<uint32*>(FNameEntryIdxThree + i) >> 1) == 0x3 &&
				(*reinterpret_cast<uint32*>(FNameEntryIdxEight + i) >> 1) == 0x8)
			{
				Off::FNameEntry::NameArray::IndexOffset = i;
				break;
			}
		}

		// 设置获取字符串的lambda函数
		GetStr = [](uint8* NameEntry) -> std::wstring
		{
			const int32 NameIdx = *reinterpret_cast<int32*>(NameEntry + Off::FNameEntry::NameArray::IndexOffset);
			const void* NameString = reinterpret_cast<void*>(NameEntry + Off::FNameEntry::NameArray::StringOffset);

			// 根据是否使用宽字符返回适当的字符串
			if (NameIdx & NameWideMask)
				return std::wstring(reinterpret_cast<const wchar_t*>(NameString));

			return UtfN::StringToWString<std::string>(reinterpret_cast<const char*>(NameString));
		};
	}
}

// 初始化名称数组
bool NameArray::InitializeNameArray(uint8_t* NameArray)
{
	int32 ValidPtrCount = 0x0;
	int32 ZeroQWordCount = 0x0;

	int32 PerChunk = 0x0;

	if (!NameArray || IsBadReadPtr(NameArray))
		return false;

	// 遍历名称数组查找有效指针
	for (int i = 0; i < 0x800; i += 0x8)
	{
		uint8_t* SomePtr = *reinterpret_cast<uint8_t**>(NameArray + i);

		if (SomePtr == 0)
		{
			ZeroQWordCount++;
		}
		else if (ZeroQWordCount == 0x0 && SomePtr != nullptr)
		{
			ValidPtrCount++;
		}
		else if (ZeroQWordCount > 0 && SomePtr != 0)
		{
			// 找到数组元素数量和区块数量
			int32 NumElements = *reinterpret_cast<int32_t*>(NameArray + i);
			int32 NumChunks = *reinterpret_cast<int32_t*>(NameArray + i + 4);

			if (NumChunks == ValidPtrCount)
			{
				// 设置数组元素和最大区块索引的偏移
				Off::NameArray::NumElements = i;
				Off::NameArray::MaxChunkIndex = i + 4;

				// 设置通过索引访问元素的lambda函数
				ByIndex = [](void* NamesArray, int32 ComparisonIndex, int32 NamePoolBlockOffsetBits) -> void*
				{
					const int32 ChunkIdx = ComparisonIndex / 0x4000;
					const int32 InChunk = ComparisonIndex % 0x4000;

					if (ComparisonIndex > NameArray::GetNumElements())
						return nullptr;

					return reinterpret_cast<void***>(NamesArray)[ChunkIdx][InChunk];
				};

				return true;
			}
		}
	}

	return false;
}

// 初始化名称池
bool NameArray::InitializeNamePool(uint8_t* NamePool)
{
	// 设置初始偏移量
	Off::NameArray::MaxChunkIndex = 0x0;
	Off::NameArray::ByteCursor = 0x4;

	Off::NameArray::ChunksStart = 0x10;

	bool bWasMaxChunkIndexFound = false;

	// 查找最大区块索引
	for (int i = 0x0; i < 0x20; i += 4)
	{
		const int32 PossibleMaxChunkIdx = *reinterpret_cast<int32*>(NamePool + i);

		if (PossibleMaxChunkIdx <= 0 || PossibleMaxChunkIdx > 0x10000)
			continue;

		int32 NotNullptrCount = 0x0;
		bool bFoundFirstPtr = false;

		/* 在假定没有更多有效指针之前，我们可能遇到的无效指针的数量。 */
		constexpr int32 MaxAllowedNumInvalidPtrs = 0x500;
		int32 NumPtrsSinceLastValid = 0x0;

		// 遍历寻找有效指针
		for (int j = 0x0; j < 0x10000; j += 8)
		{
			const int32 ChunkOffset = i + 8 + j + (i % 8);

			if ((*reinterpret_cast<uint8_t**>(NamePool + ChunkOffset)) != nullptr)
			{
				NotNullptrCount++;
				NumPtrsSinceLastValid = 0;

				if (!bFoundFirstPtr)
				{
					bFoundFirstPtr = true;
					Off::NameArray::ChunksStart = i + 8 + j + (i % 8);
				}
			}
			else
			{
				NumPtrsSinceLastValid++;

				/* 上次看到非空值是0x500次迭代之前。可以安全地说我们不会找到更多的。 */
				if (NumPtrsSinceLastValid == MaxAllowedNumInvalidPtrs)
					break;
			}
		}

		// 验证找到的最大区块索引
		if (PossibleMaxChunkIdx == (NotNullptrCount - 1))
		{
			Off::NameArray::MaxChunkIndex = i;
			Off::NameArray::ByteCursor = i + 4;
			bWasMaxChunkIndexFound = true;
			break;
		}
	}

	if (!bWasMaxChunkIndexFound)
		return false;

	// 定义特殊字符串的uint值，用于识别
	constexpr uint64 CoreUObjAsUint64 = 0x6A624F5565726F43; // little endian "jbOUeroC" ["/Script/CoreUObject"]
	constexpr uint32 NoneAsUint32 = 0x656E6F4E; // little endian "None"

	uint8_t** ChunkPtr = reinterpret_cast<uint8_t**>(NamePool + Off::NameArray::ChunksStart);

	// 寻找"/Script/CoreUObject"字符串
	bool bFoundCoreUObjectString = false;
	int64 FNameEntryHeaderSize = 0x0;

	constexpr int32 LoopLimit = 0x1000;

	for (int i = 0; i < LoopLimit; i++)
	{
		if (*reinterpret_cast<uint32*>(*ChunkPtr + i) == NoneAsUint32 && FNameEntryHeaderSize == 0)
		{
			FNameEntryHeaderSize = i;
		}
		else if (*reinterpret_cast<uint64*>(*ChunkPtr + i) == CoreUObjAsUint64)
		{
			bFoundCoreUObjectString = true;
			break;
		}
	}

	if (!bFoundCoreUObjectString)
		return false;

	// 设置名称条目步长和SDK内部偏移
	NameEntryStride = FNameEntryHeaderSize == 2 ? 2 : 4;
	Off::InSDK::NameArray::FNameEntryStride = NameEntryStride;

	// 设置通过索引访问元素的lambda函数
	ByIndex = [](void* NamesArray, int32 ComparisonIndex, int32 NamePoolBlockOffsetBits) -> void*
	{
		const int32 ChunkIdx = ComparisonIndex >> NamePoolBlockOffsetBits;
		const int32 InChunkOffset = (ComparisonIndex & ((1 << NamePoolBlockOffsetBits) - 1)) * NameEntryStride;

		const bool bIsBeyondLastChunk = ChunkIdx == NameArray::GetNumChunks() && InChunkOffset > NameArray::GetByteCursor();

		if (ChunkIdx < 0 || ChunkIdx > GetNumChunks() || bIsBeyondLastChunk)
			return nullptr;

		uint8_t* ChunkPtr = reinterpret_cast<uint8_t*>(NamesArray) + 0x10;

		return reinterpret_cast<uint8_t**>(ChunkPtr)[ChunkIdx] + InChunkOffset;
	};

	// 设置使用名称池标志并初始化FNameEntry
	Settings::Internal::bUseNamePool = true;
	FNameEntry::Init(reinterpret_cast<uint8*>(ChunkPtr), FNameEntryHeaderSize);

	return true;
}


/* 
 * 查找对FName::GetNames的调用，或者直接引用GNames（如果调用已内联）
 * 
 * 返回 { GetNames/GNames, bIsGNamesDirectly };
*/
inline std::pair<uintptr_t, bool> FindFNameGetNamesOrGNames(uintptr_t EnterCriticalSectionAddress, uintptr_t StartAddress)
{
	/* 2字节操作 + 4字节相对偏移 */
	constexpr int32 ASMRelativeCallSizeBytes = 0x6;

	/* 从"ByteProperty"向上查找"GetNames"调用的范围 */
	constexpr int32 GetNamesCallSearchRange = 0x150;

	/* 在'FName::StaticInit'中查找对字符串"ByteProperty"的引用 */
	const uint8* BytePropertyStringAddress = static_cast<uint8*>(FindByStringInAllSections(L"ByteProperty", StartAddress));

	/* 重要：防止无限递归 */
	if (!BytePropertyStringAddress)
		return { 0x0, false };

	for (int i = 0; i < GetNamesCallSearchRange; i++)
	{
		/* 向上检查（是的，负索引）相对调用操作码 */
		if (BytePropertyStringAddress[-i] != 0xFF)
			continue;

		uintptr_t CallTarget = ASMUtils::Resolve32BitSectionRelativeCall(reinterpret_cast<uintptr_t>(BytePropertyStringAddress - i));

		if (CallTarget != EnterCriticalSectionAddress)
			continue;
		
		uintptr_t InstructionAfterCall = reinterpret_cast<uintptr_t>(BytePropertyStringAddress - (i - ASMRelativeCallSizeBytes));

		/* 检查我们是否在处理'call'操作码 */
		if (*reinterpret_cast<const uint8*>(InstructionAfterCall) == 0xE8)
			return { ASMUtils::Resolve32BitRelativeCall(InstructionAfterCall), false };

		return { ASMUtils::Resolve32BitRelativeMove(InstructionAfterCall), true };
	}

	/* 继续搜索"ByteProperty"的另一个引用，安全，因为我们正在检查是否找到了另一个字符串引用 */
	return FindFNameGetNamesOrGNames(EnterCriticalSectionAddress, reinterpret_cast<uintptr_t>(BytePropertyStringAddress));
};

// 尝试查找名称数组
bool NameArray::TryFindNameArray()
{
	/* 'static TNameEntryArray& FName::GetNames()'的类型 */
	using GetNameType = void* (*)();

	/* 从'FName::GetNames'开始，我们想要向下搜索'mov register, GNames'的范围 */
	constexpr int32 GetNamesCallSearchRange = 0x100;

	void* EnterCriticalSectionAddress = GetImportAddress(nullptr, "kernel32.dll", "EnterCriticalSection");

	auto [Address, bIsGNamesDirectly] = FindFNameGetNamesOrGNames(reinterpret_cast<uintptr_t>(EnterCriticalSectionAddress), GetModuleBase());

	if (Address == 0x0)
		return false;

	if (bIsGNamesDirectly)
	{
		if (!IsInProcessRange(Address) || IsBadReadPtr(*reinterpret_cast<void**>(Address)))
			return false;

		Off::InSDK::NameArray::GNames = GetOffset(Address);
		return true;
	}

	/* 调用GetNames以获取名称表分配的指针，用于后续比较 */
	void* Names = reinterpret_cast<GetNameType>(Address)();

	for (int i = 0; i < GetNamesCallSearchRange; i++)
	{
		/* 向上检查（是的，负索引）相对调用操作码 */
		if (*reinterpret_cast<const uint16*>(Address + i) != 0x8B48)
			continue;

		uintptr_t MoveTarget = ASMUtils::Resolve32BitRelativeMove(Address + i);

		if (!IsInProcessRange(MoveTarget))
			continue;

		void* ValueOfMoveTargetAsPtr = *reinterpret_cast<void**>(MoveTarget);

		if (IsBadReadPtr(ValueOfMoveTargetAsPtr) || ValueOfMoveTargetAsPtr != Names)
			continue;

		Off::InSDK::NameArray::GNames = GetOffset(MoveTarget);
		return true;
	}
	
	return false;
}

// 尝试查找名称池
bool NameArray::TryFindNamePool()
{
	/* 我们想要搜索InitializeSRWLock的间接调用的字节数 */
	constexpr int32 InitSRWLockSearchRange = 0x50;

	/* 我们想要搜索加载字符串"ByteProperty"的lea指令的字节数 */
	constexpr int32 BytePropertySearchRange = 0x2A0;

	/* FNamePool::FNamePool包含对InitializeSRWLock或RtlInitializeSRWLock的调用，我们稍后会检查 */
	//uintptr_t InitSRWLockAddress = reinterpret_cast<uintptr_t>(GetImportAddress(nullptr, "kernel32.dll", "InitializeSRWLock"));
	uintptr_t InitSRWLockAddress = reinterpret_cast<uintptr_t>(GetAddressOfImportedFunctionFromAnyModule("kernel32.dll", "InitializeSRWLock"));
	uintptr_t RtlInitSRWLockAddress = reinterpret_cast<uintptr_t>(GetAddressOfImportedFunctionFromAnyModule("ntdll.dll", "RtlInitializeSRWLock"));

	/* FNamePool的单例实例，作为参数传递给FNamePool::FNamePool */
	void* NamePoolIntance = nullptr;

	uintptr_t SigOccurrence = 0x0;;

	uintptr_t Counter = 0x0;

	while (!NamePoolIntance)
	{
		/* 添加0x1，这样我们就不会再次找到相同的出现并导致无限循环（为此调试了20分钟） */
		if (SigOccurrence > 0x0)
			SigOccurrence += 0x1;

		/* 查找此签名的下一个出现，看看它是否可能是对FNamePool构造函数的调用 */
		SigOccurrence = reinterpret_cast<uintptr_t>(FindPattern("48 8D 0D ? ? ? ? E8", 0x0, true, SigOccurrence));

		if (SigOccurrence == 0x0)
			break;

		constexpr int32 SizeOfMovInstructionBytes = 0x7;

		const uintptr_t PossibleConstructorAddress = ASMUtils::Resolve32BitRelativeCall(SigOccurrence + SizeOfMovInstructionBytes);

		if (!IsInProcessRange(PossibleConstructorAddress))
			continue;

		for (int i = 0; i < InitSRWLockSearchRange; i++)
		{
			/* 检查带有操作码FF 15 00 00 00 00的相对调用 */
			if (*reinterpret_cast<uint16*>(PossibleConstructorAddress + i) != 0x15FF)
				continue;

			const uintptr_t RelativeCallTarget = ASMUtils::Resolve32BitSectionRelativeCall(PossibleConstructorAddress + i);

			if (!IsInProcessRange(RelativeCallTarget))
				continue;

			const uintptr_t ValueOfCallTarget = *reinterpret_cast<uintptr_t*>(RelativeCallTarget);

			if (ValueOfCallTarget != InitSRWLockAddress && ValueOfCallTarget != RtlInitSRWLockAddress)
				continue;

			/* 尝试查找"ByteProperty"字符串，因为它总是在FNamePool::FNamePool中被引用，所以我们用它来验证我们得到了正确的函数 */
			MemAddress StringRef = FindByStringInAllSections(L"ByteProperty", PossibleConstructorAddress, BytePropertySearchRange);

			/* 我们找不到宽字符串L"ByteProperty"，现在看看我们是否可以找到字符串"ByteProperty" */
			if (!StringRef)
				StringRef = FindByStringInAllSections("ByteProperty", PossibleConstructorAddress, BytePropertySearchRange);

			if (StringRef)
			{
				NamePoolIntance = reinterpret_cast<void*>(ASMUtils::Resolve32BitRelativeMove(SigOccurrence));
				break;
			}
		}
	}

	if (NamePoolIntance)
	{
		Off::InSDK::NameArray::GNames = GetOffset(NamePoolIntance);
		return true;
	}

	return false;
}

// 尝试初始化名称数组系统
bool NameArray::TryInit(bool bIsTestOnly)
{
	uintptr_t ImageBase = GetModuleBase();

	uint8* GNamesAddress = nullptr;

	bool bFoundNameArray = false;
	bool bFoundnamePool = false;

	// 尝试查找名称数组
	if (NameArray::TryFindNameArray())
	{
		std::cerr << std::format("Found 'TNameEntryArray GNames' at offset 0x{:X}\n", Off::InSDK::NameArray::GNames) << std::endl;
		GNamesAddress = *reinterpret_cast<uint8**>(ImageBase + Off::InSDK::NameArray::GNames);// Derefernce
		Settings::Internal::bUseNamePool = false;
		bFoundNameArray = true;
	}
	// 尝试查找名称池
	else if (NameArray::TryFindNamePool())
	{
		std::cerr << std::format("Found 'FNamePool GNames' at offset 0x{:X}\n", Off::InSDK::NameArray::GNames) << std::endl;
		GNamesAddress = reinterpret_cast<uint8*>(ImageBase + Off::InSDK::NameArray::GNames); // No derefernce
		Settings::Internal::bUseNamePool = true;
		bFoundnamePool = true;
	}

	// 如果两者都没找到，返回失败
	if (!bFoundNameArray && !bFoundnamePool)
	{
		std::cerr << "\n\nCould not find GNames!\n\n" << std::endl;
		return false;
	}

	// 如果只是测试模式，返回
	if (bIsTestOnly)
		return false;

	// 根据找到的类型初始化
	if (bFoundNameArray && NameArray::InitializeNameArray(GNamesAddress))
	{
		GNames = GNamesAddress;
		Settings::Internal::bUseNamePool = false;
		FNameEntry::Init();
		return true;
	}
	else if (bFoundnamePool && NameArray::InitializeNamePool(reinterpret_cast<uint8_t*>(GNamesAddress)))
	{
		GNames = GNamesAddress;
		Settings::Internal::bUseNamePool = true;
		/* FNameEntry::Init()已移至NameArray::InitializeNamePool中，避免重复逻辑 */
		return true;
	}

	//GNames = nullptr;
	//Off::InSDK::NameArray::GNames = 0x0;
	//Settings::Internal::bUseNamePool = false;

	std::cerr << "The address that was found couldn't be used by the generator, this might be due to GNames-encryption.\n" << std::endl;

	return false;
}

// 使用偏移量覆盖初始化名称数组
bool NameArray::TryInit(int32 OffsetOverride, bool bIsNamePool, const char* const ModuleName)
{
	uintptr_t ImageBase = GetModuleBase(ModuleName);

	uint8* GNamesAddress = nullptr;

	const bool bIsNameArrayOverride = !bIsNamePool;
	const bool bIsNamePoolOverride = bIsNamePool;

	bool bFoundNameArray = false;
	bool bFoundnamePool = false;

	Off::InSDK::NameArray::GNames = OffsetOverride;

	// 使用名称数组覆盖
	if (bIsNameArrayOverride)
	{
		std::cerr << std::format("Overwrote offset: 'TNameEntryArray GNames' set as offset 0x{:X}\n", Off::InSDK::NameArray::GNames) << std::endl;
		GNamesAddress = *reinterpret_cast<uint8**>(ImageBase + Off::InSDK::NameArray::GNames);// Derefernce
		Settings::Internal::bUseNamePool = false;
		bFoundNameArray = true;
	}
	// 使用名称池覆盖
	else if (bIsNamePoolOverride)
	{
		std::cerr << std::format("Overwrote offset: 'FNamePool GNames' set as offset 0x{:X}\n", Off::InSDK::NameArray::GNames) << std::endl;
		GNamesAddress = reinterpret_cast<uint8*>(ImageBase + Off::InSDK::NameArray::GNames); // No derefernce
		Settings::Internal::bUseNamePool = true;
		bFoundnamePool = true;
	}

	// 如果没有找到，返回失败
	if (!bFoundNameArray && !bFoundnamePool)
	{
		std::cerr << "\n\nCould not find GNames!\n\n" << std::endl;
		return false;
	}

	// 根据找到的类型初始化
	if (bFoundNameArray && NameArray::InitializeNameArray(GNamesAddress))
	{
		GNames = GNamesAddress;
		Settings::Internal::bUseNamePool = false;
		FNameEntry::Init();
		return true;
	}
	else if (bFoundnamePool && NameArray::InitializeNamePool(reinterpret_cast<uint8_t*>(GNamesAddress)))
	{
		GNames = GNamesAddress;
		Settings::Internal::bUseNamePool = true;
		/* FNameEntry::Init()已移至NameArray::InitializeNamePool中，避免重复逻辑 */
		return true;
	}

	std::cerr << "The address was overwritten, but couldn't be used. This might be due to GNames-encryption.\n" << std::endl;

	return false;
}

// 设置GNames但不提交
bool NameArray::SetGNamesWithoutCommiting()
{
	/* GNames已经设置 */
	if (Off::InSDK::NameArray::GNames != 0x0)
		return false;

	// 尝试查找名称数组
	if (NameArray::TryFindNameArray())
	{
		std::cerr << std::format("Found 'TNameEntryArray GNames' at offset 0x{:X}\n", Off::InSDK::NameArray::GNames) << std::endl;
		Settings::Internal::bUseNamePool = false;
		return true;
	}
	// 尝试查找名称池
	else if (NameArray::TryFindNamePool())
	{
		std::cerr << std::format("Found 'FNamePool GNames' at offset 0x{:X}\n", Off::InSDK::NameArray::GNames) << std::endl;
		Settings::Internal::bUseNamePool = true;
		return true;
	}

	std::cerr << "\n\nCould not find GNames!\n\n" << std::endl;
	return false;
}

// 初始化后的额外设置
void NameArray::PostInit()
{
	if (GNames && Settings::Internal::bUseNamePool)
	{
		// 逆序迭代，因为较新的对象更可能具有等于NumChunks-1的块索引
		
		NameArray::FNameBlockOffsetBits = 0xE;

		int i = ObjectArray::Num();
		while (i >= 0)
		{
			const int32 CurrentBlock = NameArray::GetNumChunks();

			UEObject Obj = ObjectArray::GetByIndex(i);

			if (!Obj)
			{
				i--;
				continue;
			}

			const int32 ObjNameChunkIdx = Obj.GetFName().GetCompIdx() >> NameArray::FNameBlockOffsetBits;

			if (ObjNameChunkIdx == CurrentBlock)
				break;

			if (ObjNameChunkIdx > CurrentBlock)
			{
				NameArray::FNameBlockOffsetBits++;
				i = ObjectArray::Num();
			}

			i--;
		}
		Off::InSDK::NameArray::FNamePoolBlockOffsetBits = NameArray::FNameBlockOffsetBits;

		std::cerr << "NameArray::FNameBlockOffsetBits: 0x" << std::hex << NameArray::FNameBlockOffsetBits << "\n" << std::endl;
	}
}

// 获取分块数量
int32 NameArray::GetNumChunks()
{
	return *reinterpret_cast<int32*>(GNames + Off::NameArray::MaxChunkIndex);
}

// 获取元素数量
int32 NameArray::GetNumElements()
{
	return !Settings::Internal::bUseNamePool ? *reinterpret_cast<int32*>(GNames + Off::NameArray::NumElements) : 0;
}

// 获取字节游标
int32 NameArray::GetByteCursor()
{
	return Settings::Internal::bUseNamePool ? *reinterpret_cast<int32*>(GNames + Off::NameArray::ByteCursor) : 0;
}

// 通过名称获取名称条目
FNameEntry NameArray::GetNameEntry(const void* Name)
{
	return ByIndex(GNames, FName(Name).GetCompIdx(), FNameBlockOffsetBits);
}

// 通过索引获取名称条目
FNameEntry NameArray::GetNameEntry(int32 Idx)
{
	return ByIndex(GNames, Idx, FNameBlockOffsetBits);
}

