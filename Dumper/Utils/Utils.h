#pragma once

#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <functional>


/* 来源: https://en.cppreference.com/w/cpp/string/byte/tolower */
// 将字符串转换为小写
inline std::string str_tolower(std::string S)
{
	std::transform(S.begin(), S.end(), S.begin(), [](unsigned char C) { return std::tolower(C); });
	return S;
}

// 字符串长度辅助函数，根据字符类型选择 strlen 或 wcslen
template<typename CharType>
inline int32_t StrlenHelper(const CharType* Str)
{
	if constexpr (std::is_same<CharType, char>())
	{
		return strlen(Str);
	}
	else
	{
		return wcslen(Str);
	}
}

// 字符串比较辅助函数，根据字符类型选择 strncmp 或 wcsncmp
template<typename CharType>
inline bool StrnCmpHelper(const CharType* Left, const CharType* Right, size_t NumCharsToCompare)
{
	if constexpr (std::is_same<CharType, char>())
	{
		return strncmp(Left, Right, NumCharsToCompare) == 0;
	}
	else
	{
		return wcsncmp(Left, Right, NumCharsToCompare) == 0;
	}
}

namespace ASMUtils
{
	/* 有关 jmp 操作码的参考，请参见 IDA 或 https://c9x.me/x86/html/file_module_x86_id_147.html */
	// 检查是否为32位RIP相对跳转指令
	inline bool Is32BitRIPRelativeJump(uintptr_t Address)
	{
		return Address && *reinterpret_cast<uint8_t*>(Address) == 0xE9; /* 48 表示 jmp, FF 表示 "RIP 相对" -- 小端序 */
	}

	// 解析32位RIP相对跳转的目标地址
	inline uintptr_t Resolve32BitRIPRelativeJumpTarget(uintptr_t Address)
	{
		constexpr int32_t InstructionSizeBytes = 0x5; // 指令大小（字节）
		constexpr int32_t InstructionImmediateDisplacementOffset = 0x1; // 立即数位移偏移

		const int32_t Offset = *reinterpret_cast<int32_t*>(Address + InstructionImmediateDisplacementOffset);

		/* 加上 InstructionSizeBytes 是因为偏移是相对于下一条指令的 */
		return Address + InstructionSizeBytes + Offset;
	}

	/* 参见 https://c9x.me/x86/html/file_module_x86_id_147.html */
	// 解析32位寄存器相对跳转
	inline uintptr_t Resolve32BitRegisterRelativeJump(uintptr_t Address)
	{
		/*
		* 48 FF 25 C1 10 06 00     jmp QWORD [rip+0x610c1]
		*
		* 48 FF 25 <-- 指令信息 [跳转，相对，rip]
		* C1 10 06 00 <-- 32位偏移，相对于这些指令之后的地址 (+ 7) [如果 48 的地址是 0x0，则偏移相对于地址 0x7]
		*/

		return ((Address + 7) + *reinterpret_cast<int32_t*>(Address + 3));
	}

	// 解析32位节相对调用
	inline uintptr_t Resolve32BitSectionRelativeCall(uintptr_t Address)
	{
		/* 与 Resolve32BitRIPRelativeJump 相同，但我们解析的是调用，指令少一个字节 */
		return ((Address + 6) + *reinterpret_cast<int32_t*>(Address + 2));
	}

	// 解析32位相对调用
	inline uintptr_t Resolve32BitRelativeCall(uintptr_t Address)
	{
		/* 与 Resolve32BitRIPRelativeJump 相同，但我们解析的是非相对调用，指令少两个字节 */
		return ((Address + 5) + *reinterpret_cast<int32_t*>(Address + 1));
	}

	// 解析32位相对移动
	inline uintptr_t Resolve32BitRelativeMove(uintptr_t Address)
	{
		/* 与 Resolve32BitRIPRelativeJump 相同，但我们解析的是相对 mov */
		return ((Address + 7) + *reinterpret_cast<int32_t*>(Address + 3));
	}

	// 解析32位相对lea指令
	inline uintptr_t Resolve32BitRelativeLea(uintptr_t Address)
	{
		/* 与 Resolve32BitRIPRelativeJump 相同，但我们解析的是相对 lea */
		return ((Address + 7) + *reinterpret_cast<int32_t*>(Address + 3));
	}
}


// 客户端ID结构体
struct CLIENT_ID
{
	HANDLE UniqueProcess;
	HANDLE UniqueThread;
};

// 线程环境块 (TEB)
struct TEB
{
	NT_TIB NtTib;
	PVOID EnvironmentPointer;
	CLIENT_ID ClientId;
	PVOID ActiveRpcHandle;
	PVOID ThreadLocalStoragePointer;
	struct PEB* ProcessEnvironmentBlock;
};

// PEB加载器数据
struct PEB_LDR_DATA
{
	ULONG Length;
	BOOLEAN Initialized;
	BYTE MoreFunnyPadding[0x3];
	HANDLE SsHandle;
	LIST_ENTRY InLoadOrderModuleList; // 按加载顺序排列的模块列表
	LIST_ENTRY InMemoryOrderModuleList; // 按内存顺序排列的模块列表
	LIST_ENTRY InInitializationOrderModuleList; // 按初始化顺序排列的模块列表
	PVOID EntryInProgress;
	BOOLEAN ShutdownInProgress;
	BYTE MoreFunnyPadding2[0x7];
	HANDLE ShutdownThreadId;
};

// 进程环境块 (PEB)
struct PEB
{
	BOOLEAN InheritedAddressSpace;
	BOOLEAN ReadImageFileExecOptions;
	BOOLEAN BeingDebugged;
	union
	{
		BOOLEAN BitField;
		struct
		{
			BOOLEAN ImageUsesLargePages : 1;
			BOOLEAN IsProtectedProcess : 1;
			BOOLEAN IsImageDynamicallyRelocated : 1;
			BOOLEAN SkipPatchingUser32Forwarders : 1;
			BOOLEAN IsPackagedProcess : 1;
			BOOLEAN IsAppContainer : 1;
			BOOLEAN IsProtectedProcessLight : 1;
			BOOLEAN SpareBits : 1;
		};
	};
	BYTE ManuallyAddedPaddingCauseTheCompilerIsStupid[0x4]; // 编译器太傻了，手动添加填充
	HANDLE Mutant;
	PVOID ImageBaseAddress;
	PEB_LDR_DATA* Ldr;
};

// UNICODE字符串
struct UNICODE_STRING
{
	USHORT Length;
	USHORT MaximumLength;
	BYTE MoreStupidCompilerPaddingYay[0x4]; // 又是愚蠢的编译器填充
	PWCH Buffer;
};

// 加载器数据表条目
struct LDR_DATA_TABLE_ENTRY
{
	LIST_ENTRY InLoadOrderLinks;
	LIST_ENTRY InMemoryOrderLinks;
	//union
	//{
	//	LIST_ENTRY InInitializationOrderLinks;
	//	LIST_ENTRY InProgressLinks;
	//};
	PVOID DllBase;
	PVOID EntryPoint;
	ULONG SizeOfImage;
	BYTE MoreStupidCompilerPaddingYay[0x4]; // 更多愚蠢的编译器填充
	UNICODE_STRING FullDllName;
	UNICODE_STRING BaseDllName;
}; 

// 获取当前线程的TEB
inline _TEB* _NtCurrentTeb()
{
	return reinterpret_cast<struct _TEB*>(__readgsqword(((LONG)__builtin_offsetof(NT_TIB, Self))));
}

// 获取PEB
inline PEB* GetPEB()
{
	return reinterpret_cast<TEB*>(_NtCurrentTeb())->ProcessEnvironmentBlock;
}

// 通过模块名获取加载器数据表条目
inline LDR_DATA_TABLE_ENTRY* GetModuleLdrTableEntry(const char* SearchModuleName)
{
	PEB* Peb = GetPEB();
	PEB_LDR_DATA* Ldr = Peb->Ldr;

	int NumEntriesLeft = Ldr->Length;

	for (LIST_ENTRY* P = Ldr->InMemoryOrderModuleList.Flink; P && NumEntriesLeft-- > 0; P = P->Flink)
	{
		LDR_DATA_TABLE_ENTRY* Entry = reinterpret_cast<LDR_DATA_TABLE_ENTRY*>(P);

		std::wstring WideModuleName(Entry->BaseDllName.Buffer, Entry->BaseDllName.Length >> 1);
		std::string ModuleName = std::string(WideModuleName.begin(), WideModuleName.end());

		if (str_tolower(ModuleName) == str_tolower(SearchModuleName))
			return Entry;
	}

	return nullptr;
}

// 获取模块基址
inline uintptr_t GetModuleBase(const char* const ModuleName = nullptr)
{
	if (ModuleName == nullptr)
		return reinterpret_cast<uintptr_t>(GetPEB()->ImageBaseAddress);

	return reinterpret_cast<uintptr_t>(GetModuleLdrTableEntry(ModuleName)->DllBase);
}

// 获取镜像基址和大小
inline std::pair<uintptr_t, uintptr_t> GetImageBaseAndSize(const char* const ModuleName = nullptr)
{
	uintptr_t ImageBase = GetModuleBase(ModuleName);
	PIMAGE_NT_HEADERS NtHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(ImageBase + reinterpret_cast<PIMAGE_DOS_HEADER>(ImageBase)->e_lfanew);

	return { ImageBase, NtHeader->OptionalHeader.SizeOfImage };
}

/* 返回节的基址和大小 */
inline std::pair<uintptr_t, DWORD> GetSectionByName(uintptr_t ImageBase, const std::string& ReqestedSectionName)
{
	if (ImageBase == 0)
		return { NULL, 0 };

	const PIMAGE_DOS_HEADER DosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(ImageBase);
	const PIMAGE_NT_HEADERS NtHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(ImageBase + DosHeader->e_lfanew);

	PIMAGE_SECTION_HEADER Sections = IMAGE_FIRST_SECTION(NtHeaders);

	DWORD TextSize = 0;

	for (int i = 0; i < NtHeaders->FileHeader.NumberOfSections; i++)
	{
		IMAGE_SECTION_HEADER& CurrentSection = Sections[i];

		std::string SectionName = reinterpret_cast<const char*>(CurrentSection.Name);

		if (SectionName == ReqestedSectionName)
			return { (ImageBase + CurrentSection.VirtualAddress), CurrentSection.Misc.VirtualSize };
	}

	return { NULL, 0 };
}

// 获取地址相对于模块基址的偏移
inline uintptr_t GetOffset(const uintptr_t Address)
{
	static uintptr_t ImageBase = 0x0;

	if (ImageBase == 0x0)
		ImageBase = GetModuleBase();

	return Address > ImageBase ? (Address - ImageBase) : 0x0;
}

// 获取地址相对于模块基址的偏移
inline uintptr_t GetOffset(const void* Address)
{
	return GetOffset(reinterpret_cast<const uintptr_t>(Address));
}

// 检查地址是否位于任何已加载模块的内存范围内
inline bool IsInAnyModules(const uintptr_t Address)
{
	PEB* Peb = GetPEB();
	PEB_LDR_DATA* Ldr = Peb->Ldr;

	int NumEntriesLeft = Ldr->Length;

	for (LIST_ENTRY* P = Ldr->InMemoryOrderModuleList.Flink; P && NumEntriesLeft-- > 0; P = P->Flink)
	{
		LDR_DATA_TABLE_ENTRY* Entry = reinterpret_cast<LDR_DATA_TABLE_ENTRY*>(P);

		if (reinterpret_cast<void*>(Address) > Entry->DllBase && reinterpret_cast<void*>(Address) < ((PCHAR)Entry->DllBase + Entry->SizeOfImage))
			return true;
	}

	return false;
}

// 处理器 (x86-64) 仅将虚拟地址的52位（或57位）转换为物理地址，未使用的位必须全为0或全为1。
// 检查给定的虚拟地址是否有效
inline bool IsValidVirtualAddress(const uintptr_t Address)
{
	constexpr uint64_t BitMask = 0b1111'1111ull << 56;

	return (Address & BitMask) == BitMask || (Address & BitMask) == 0x0;
}

// 检查地址是否在当前进程的内存范围内
inline bool IsInProcessRange(const uintptr_t Address)
{
	const auto [ImageBase, ImageSize] = GetImageBaseAndSize();

	if (Address >= ImageBase && Address < (ImageBase + ImageSize))
		return true;

	return IsInAnyModules(Address);
}

// 检查地址是否在当前进程的内存范围内
inline bool IsInProcessRange(const void* Address)
{
	return IsInProcessRange(reinterpret_cast<const uintptr_t>(Address));
}

// 检查指针是否为无效的读指针
inline bool IsBadReadPtr(const void* Ptr)
{
	if(!IsValidVirtualAddress(reinterpret_cast<const uintptr_t>(Ptr)))
		return true;

	MEMORY_BASIC_INFORMATION Mbi;

	if (VirtualQuery(Ptr, &Mbi, sizeof(Mbi)))
	{
		constexpr DWORD AccessibleMask = (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);
		constexpr DWORD InaccessibleMask = (PAGE_GUARD | PAGE_NOACCESS);

		return !(Mbi.Protect & AccessibleMask) || (Mbi.Protect & InaccessibleMask);
	}

	return true;
};

// 检查指针是否为无效的读指针
inline bool IsBadReadPtr(const uintptr_t Ptr)
{
	return IsBadReadPtr(reinterpret_cast<const void*>(Ptr));
}

// 通过模块名获取模块地址
inline void* GetModuleAddress(const char* SearchModuleName)
{
	LDR_DATA_TABLE_ENTRY* Entry = GetModuleLdrTableEntry(SearchModuleName);

	if (Entry)
		return Entry->DllBase;

	return nullptr;
}

/* 获取导入函数指针的存储地址 */
inline PIMAGE_THUNK_DATA GetImportAddress(uintptr_t ModuleBase, const char* ModuleToImportFrom, const char* SearchFunctionName)
{
	/* 获取导入函数的模块 */
	PIMAGE_DOS_HEADER DosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(ModuleBase);

	if (ModuleBase == 0x0 || DosHeader->e_magic != IMAGE_DOS_SIGNATURE)
		return nullptr;

	PIMAGE_NT_HEADERS NtHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(ModuleBase + reinterpret_cast<PIMAGE_DOS_HEADER>(ModuleBase)->e_lfanew);

	if (!NtHeader)
		return nullptr;

	PIMAGE_IMPORT_DESCRIPTOR ImportTable = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(ModuleBase + NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	/* 遍历所有模块，找到正确的模块后，遍历所有导入项以获取所需函数 */
	for (PIMAGE_IMPORT_DESCRIPTOR Import = ImportTable; Import && Import->Characteristics != 0x0; Import++)
	{
		if (Import->Name == 0xFFFF)
			continue;

		const char* Name = reinterpret_cast<const char*>(ModuleBase + Import->Name);

		if (str_tolower(Name) != str_tolower(ModuleToImportFrom))
			continue;

		PIMAGE_THUNK_DATA NameThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(ModuleBase + Import->OriginalFirstThunk);
		PIMAGE_THUNK_DATA FuncThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(ModuleBase + Import->FirstThunk);

		while (!IsBadReadPtr(NameThunk)
			&& !IsBadReadPtr(FuncThunk)
			&& !IsBadReadPtr(ModuleBase + NameThunk->u1.AddressOfData)
			&& !IsBadReadPtr(FuncThunk->u1.AddressOfData))
		{
			/*
			* 函数可能通过模块导出表中的序号(Ordinal)导入
			*
			* 名称可以通过在模块的导出名称表中查找此序号来检索
			*/
			if ((NameThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) != 0) // 无序号
			{
				NameThunk++;
				FuncThunk++;
				continue; // 将来可能会处理
			}

			/* 获取此函数的导入数据 */
			PIMAGE_IMPORT_BY_NAME NameData = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(ModuleBase + NameThunk->u1.ForwarderString);
			PIMAGE_IMPORT_BY_NAME FunctionData = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(FuncThunk->u1.AddressOfData);

			if (std::string(NameData->Name) == SearchFunctionName)
				return FuncThunk;

			NameThunk++;
			FuncThunk++;
		}
	}

	return nullptr;
}

/* 获取导入函数指针的存储地址 */
inline PIMAGE_THUNK_DATA GetImportAddress(const char* SearchModuleName, const char* ModuleToImportFrom, const char* SearchFunctionName)
{
	const uintptr_t SearchModule = SearchModuleName ? reinterpret_cast<uintptr_t>(GetModuleAddress(SearchModuleName)) : GetModuleBase();

	return GetImportAddress(SearchModule, ModuleToImportFrom, SearchFunctionName);
}

/* 查找函数的导入，并从导入的模块返回该函数的地址 */
inline void* GetAddressOfImportedFunction(const char* SearchModuleName, const char* ModuleToImportFrom, const char* SearchFunctionName)
{
	PIMAGE_THUNK_DATA FuncThunk = GetImportAddress(SearchModuleName, ModuleToImportFrom, SearchFunctionName);

	if (!FuncThunk)
		return nullptr;

	return reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(FuncThunk->u1.AddressOfData);
}

// 从任何模块获取导入函数的地址
inline void* GetAddressOfImportedFunctionFromAnyModule(const char* ModuleToImportFrom, const char* SearchFunctionName)
{
	PEB* Peb = GetPEB();
	PEB_LDR_DATA* Ldr = Peb->Ldr;

	int NumEntriesLeft = Ldr->Length;

	for (LIST_ENTRY* P = Ldr->InMemoryOrderModuleList.Flink; P && NumEntriesLeft-- > 0; P = P->Flink)
	{
		LDR_DATA_TABLE_ENTRY* Entry = reinterpret_cast<LDR_DATA_TABLE_ENTRY*>(P);

		PIMAGE_THUNK_DATA Import = GetImportAddress(reinterpret_cast<uintptr_t>(Entry->DllBase), ModuleToImportFrom, SearchFunctionName);

		if (Import)
			return reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(Import->u1.AddressOfData);
	}

	return nullptr;
}

/* 获取导出函数的地址 */
inline void* GetExportAddress(const char* SearchModuleName, const char* SearchFunctionName)
{
	/* 获取导出此函数的模块 */
	uintptr_t ModuleBase = reinterpret_cast<uintptr_t>(GetModuleAddress(SearchModuleName));
	PIMAGE_DOS_HEADER DosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(ModuleBase);

	if (ModuleBase == 0x0 || DosHeader->e_magic != IMAGE_DOS_SIGNATURE)
		return nullptr;

	PIMAGE_NT_HEADERS NtHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(ModuleBase + reinterpret_cast<PIMAGE_DOS_HEADER>(ModuleBase)->e_lfanew);

	if (!NtHeader)
		return nullptr;

	/* 获取模块导出的函数表 */
	PIMAGE_EXPORT_DIRECTORY ExportTable = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(ModuleBase + NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

	const DWORD* NameOffsets = reinterpret_cast<const DWORD*>(ModuleBase + ExportTable->AddressOfNames);
	const DWORD* FunctionOffsets = reinterpret_cast<const DWORD*>(ModuleBase + ExportTable->AddressOfFunctions);

	const WORD* Ordinals = reinterpret_cast<const WORD*>(ModuleBase + ExportTable->AddressOfNameOrdinals);

	/* 遍历所有名称，如果名称与我们查找的匹配，则返回函数地址 */
	for (int i = 0; i < ExportTable->NumberOfFunctions; i++)
	{
		const WORD NameIndex = Ordinals[i];
		const char* Name = reinterpret_cast<const char*>(ModuleBase + NameOffsets[NameIndex]);

		if (str_tolower(Name) == str_tolower(SearchFunctionName))
			return reinterpret_cast<void*>(ModuleBase + FunctionOffsets[NameIndex]);
	}

	return nullptr;
}

// 在给定范围内查找字节模式
inline void* FindPatternInRange(std::vector<int>&& Signature, const uint8_t* Start, uintptr_t Range, bool bRelative = false, uint32_t Offset = 0, int SkipCount = 0)
{
	const uint8_t* End = Start + Range;
	auto Ret = std::search(Start, End, Signature.begin(), Signature.end(), [](const uint8_t Val, const int Sig) { return Sig == -1 || Val == Sig; });

	if (Ret == End)
		return nullptr;

	if (SkipCount > 0)
		return FindPatternInRange(std::move(Signature), Ret + 1, (End - Ret) - 1, bRelative, Offset, --SkipCount);

	if (bRelative)
		return reinterpret_cast<void*>((uintptr_t)Ret + *(int32_t*)((uintptr_t)Ret + Offset) + (Offset + 4));

	return (void*)Ret;
}

// 在给定范围内查找字符串模式
inline void* FindPatternInRange(const char* Signature, const uint8_t* Start, uintptr_t Range, bool bRelative = false, uint32_t Offset = 0)
{
	std::vector<int> Sig;
	std::string Mask;

	for (int i = 0; Signature[i];)
	{
		if (Signature[i] != ' ' && Signature[i] != '?')
		{
			Sig.push_back(std::stoi(std::string(&Signature[i], 2), 0, 16));
			i += 2;
		}
		else if (Signature[i] == '?')
		{
			Sig.push_back(-1);
			i += (Signature[i + 1] == '?' ? 2 : 1);
		}
		else
		{
			i++;
		}
	}

	return FindPatternInRange(std::move(Sig), Start, Range, bRelative, Offset);
}

// 查找字节模式
inline void* FindPattern(const char* Signature, uint32_t Offset = 0, bool bSearchAllSections = false, uintptr_t StartAddress = 0x0)
{
	uintptr_t ImageBase = GetModuleBase(nullptr);

	if (bSearchAllSections)
	{
		const PIMAGE_DOS_HEADER DosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(ImageBase);
		const PIMAGE_NT_HEADERS NtHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(ImageBase + DosHeader->e_lfanew);

		PIMAGE_SECTION_HEADER Sections = IMAGE_FIRST_SECTION(NtHeaders);

		DWORD TextSize = 0;

		for (int i = 0; i < NtHeaders->FileHeader.NumberOfSections; i++)
		{
			IMAGE_SECTION_HEADER& CurrentSection = Sections[i];

			void* Found = FindPatternInRange(Signature, reinterpret_cast<const uint8_t*>(ImageBase + CurrentSection.VirtualAddress), CurrentSection.SizeOfRawData, false, Offset);

			if (Found)
				return Found;
		}
	}
	else
	{
		auto [ImageStart, ImageSize] = GetImageBaseAndSize(nullptr);

		if (StartAddress > 0x0)
		{
			ImageSize = ImageSize - (StartAddress - ImageStart);
			ImageStart = StartAddress;
		}


		return FindPatternInRange(Signature, reinterpret_cast<const uint8_t*>(ImageStart), ImageSize, false, Offset);
	}

	return nullptr;
}


// 在给定的地址范围内，以指定的对齐方式搜索特定值
template<typename T>
inline T* FindAlignedValueInProcessInRange(T Value, int32_t Alignment, uintptr_t StartAddress, uint32_t Range)
{
	constexpr int32_t ElementSize = sizeof(T);

	for (uint32_t i = 0x0; i < Range; i += Alignment)
	{
		T* TypedPtr = reinterpret_cast<T*>(StartAddress + i);

		if (*TypedPtr == Value)
			return TypedPtr;
	}

	return nullptr;
}

// 在进程的特定节或所有节中，以指定的对齐方式搜索特定值
template<typename T>
inline T* FindAlignedValueInProcess(T Value, const std::string& Sectionname = ".data", int32_t Alignment = alignof(T), bool bSearchAllSections = false)
{
	const auto [ImageBase, ImageSize] = GetImageBaseAndSize();

	uintptr_t SearchStart = ImageBase;
	uintptr_t SearchRange = ImageSize;

	if (!bSearchAllSections)
	{
		const auto [SectionStart, SectionSize] = GetSectionByName(ImageBase, Sectionname);

		if (SectionStart != 0x0 && SectionSize != 0x0)
		{
			SearchStart = SectionStart;
			SearchRange = SectionSize;
		}
		else
		{
			bSearchAllSections = true;
		}
	}

	T* Result = FindAlignedValueInProcessInRange(Value, Alignment, SearchStart, SearchRange);

	if (!Result && SearchStart != ImageBase)
		return FindAlignedValueInProcess(Value, Sectionname, Alignment, true);

	return Result;
}

// 遍历虚函数表 (VTable) 中的函数
template<bool bShouldResolve32BitJumps = true>
inline std::pair<const void*, int32_t> IterateVTableFunctions(void** VTable, const std::function<bool(const uint8_t* Addr, int32_t Index)>& CallBackForEachFunc, int32_t NumFunctions = 0x150, int32_t OffsetFromStart = 0x0)
{
	[[maybe_unused]] auto Resolve32BitRelativeJump = [](const void* FunctionPtr) -> const uint8_t*
	{
		if constexpr (bShouldResolve32BitJumps)
		{
			const uint8_t* Address = reinterpret_cast<const uint8_t*>(FunctionPtr);
			if (*Address == 0xE9)
			{
				const uint8_t* Ret = ((Address + 5) + *reinterpret_cast<const int32_t*>(Address + 1));

				if (IsInProcessRange(Ret))
					return Ret;
			}
		}

		return reinterpret_cast<const uint8_t*>(FunctionPtr);
	};


	if (!CallBackForEachFunc)
		return { nullptr, -1 };

	for (int i = 0; i < 0x150; i++)
	{
		const uintptr_t CurrentFuncAddress = reinterpret_cast<uintptr_t>(VTable[i]);

		if (CurrentFuncAddress == NULL || !IsInProcessRange(CurrentFuncAddress))
			break;

		const uint8_t* ResolvedAddress = Resolve32BitRelativeJump(reinterpret_cast<const uint8_t*>(CurrentFuncAddress));

		if (CallBackForEachFunc(ResolvedAddress, i))
			return { ResolvedAddress, i };
	}

	return { nullptr, -1 };
}

// 内存地址操作的辅助类
struct MemAddress
{
public:
	uintptr_t Address;

private:
	//pasted
	// 将模式字符串转换为字节向量
	static std::vector<int32_t> PatternToBytes(const char* pattern)
	{
		auto bytes = std::vector<int>{};
		const auto start = const_cast<char*>(pattern);
		const auto end = const_cast<char*>(pattern) + strlen(pattern);

		for (auto current = start; current < end; ++current)
		{
			if (*current == '?')
			{
				++current;
				if (*current == '?')
					++current;
				bytes.push_back(-1);
			}
			else
			{
				bytes.push_back(strtol(current, &current, 16));
			}
		}
		return bytes;
	}

public:
	// 确定给定地址是否为函数返回指令
	static bool IsFunctionRet(const uint8_t* Address)
	{
		constexpr uint8_t AsmRetByte = 0xC3;
		constexpr uint8_t AsmRetnByte = 0xC2;
		constexpr uint8_t AsmPopByte = 0x5F; /* pop edi */

		if (*Address == AsmRetByte || *Address == AsmRetnByte)
		{
			const uint8_t ByteOneBeforeRet = *(Address - 1);

			if (ByteOneBeforeRet == AsmPopByte)
				return true;

			return false;
		}

		return false;
	}

public:
	inline MemAddress(uintptr_t Addr)
		: Address(Addr)
	{
	}
	inline MemAddress(void* Addr = nullptr)
		: Address(reinterpret_cast<uintptr_t>(Addr))
	{
	}

	inline MemAddress& operator=(const MemAddress& Other)
	{
		Address = Other.Address;
		return *this;
	}

	inline MemAddress& operator=(void* Addr)
	{
		Address = reinterpret_cast<uintptr_t>(Addr);
		return *this;
	}
	inline MemAddress& operator=(uintptr_t Addr)
	{
		Address = Addr;
		return *this;
	}

	inline explicit operator bool() const
	{
		return Address != 0;
	}

	inline operator uintptr_t() const
	{
		return Address;
	}

	inline operator void* () const
	{
		return reinterpret_cast<void*>(Address);
	}

	template<typename T>
	inline operator T* () const
	{
		return reinterpret_cast<T*>(Address);
	}

	inline MemAddress Add(uintptr_t ToAdd) const
	{
		return Address + ToAdd;
	}

	inline MemAddress Sub(uintptr_t ToSub) const
	{
		return Address - ToSub;
	}

	// 解引用指针
	inline MemAddress Deref(int32_t DerefCount = 1) const
	{
		uintptr_t TempAddress = Address;

		for (int i = 0; i < DerefCount; i++)
			TempAddress = *reinterpret_cast<uintptr_t*>(TempAddress);

		return TempAddress;
	}
	
	// 在指定范围内查找相对模式
	inline MemAddress RelativePattern(const char* Pattern, int32_t Range, int32_t Relative = 0) const
	{
		std::vector<int> SearchBytes = PatternToBytes(Pattern);

		for (int32_t i = 0; i < Range; i++)
		{
			uint8_t* CurrentByte = reinterpret_cast<uint8_t*>(Address + i);

			if (IsBadReadPtr(CurrentByte))
				continue;

			bool bFound = true;
			for (int j = 0; j < SearchBytes.size(); j++)
			{
				if (SearchBytes[j] != -1 && *reinterpret_cast<uint8_t*>(Address + i + j) != SearchBytes[j])
				{
					bFound = false;
					break;
				}
			}

			if (bFound)
				return (Address + i + Relative);
		}

		return nullptr;
	}
	
	// 获取RIP相对调用的函数
	inline MemAddress GetRipRelativeCalledFunction(int32_t OneBasedFuncIndex, bool(*IsWantedTarget)(MemAddress CalledAddr) = nullptr) const
	{
		uintptr_t TempAddress = Address;
		int32_t FuncCount = 0;

		if (OneBasedFuncIndex <= 0)
			return nullptr;

		for (int i = 0; ; i++)
		{
			if (IsBadReadPtr(TempAddress + i))
				return nullptr;

			if (*reinterpret_cast<uint8_t*>(TempAddress + i) == 0xE8) // is call
			{
				MemAddress CalledAddress = ASMUtils::Resolve32BitRIPRelativeJumpTarget(TempAddress + i);

				if (IsWantedTarget && !IsWantedTarget(CalledAddress))
					continue;

				if (++FuncCount == OneBasedFuncIndex)
					return CalledAddress;
			}

			if (IsFunctionRet(reinterpret_cast<uint8_t*>(TempAddress + i)))
				return nullptr;
		}

		return nullptr;
	}
	
	// 查找下一个函数的起始地址
	inline MemAddress FindNextFunctionStart() const
	{
		constexpr int MaxFunctionAlignment = 0x10;
		constexpr uint8_t PushEdiByte = 0x57;

		uintptr_t TempAddress = Address;

		for (int i = 0; i < 0x800; i++)
		{
			uint8_t* CurrentByte = reinterpret_cast<uint8_t*>(TempAddress + i);
			
			if (IsBadReadPtr(CurrentByte))
				return nullptr;

			/* Functions are aligned to 16 bytes */
			if (*CurrentByte == 0xCC && ((TempAddress + i + 1) % MaxFunctionAlignment) == 0)
			{
				uint8_t* ByteAfterPossibleAlignment = reinterpret_cast<uint8_t*>(TempAddress + i + 1);

				if (IsBadReadPtr(ByteAfterPossibleAlignment))
					return nullptr;

				/* Most functions start with push edi */
				if (*ByteAfterPossibleAlignment == PushEdiByte)
					return (TempAddress + i + 1);
			}
		}

		return nullptr;
	}
};

// 通过字符串引用在.rdata和.text节中查找地址
template<typename Type = const char*>
inline MemAddress FindByString(Type RefStr)
{
	const auto [ImageBase, ImageSize] = GetImageBaseAndSize();

	uintptr_t SearchStart = ImageBase;
	uintptr_t SearchRange = ImageSize;

	const auto [RDataSection, RDataSize] = GetSectionByName(ImageBase, ".rdata");
	const auto [TextSection, TextSize] = GetSectionByName(ImageBase, ".text");
	
	if (!RDataSection || !TextSection)
		return nullptr;

	uintptr_t StringAddress = NULL;

	const auto RetfStrLength = StrlenHelper(RefStr);

	for (int i = 0; i < RDataSize; i++)
	{
		if (StrnCmpHelper(RefStr, reinterpret_cast<Type>(RDataSection + i), RetfStrLength) == 0)
		{
			StringAddress = RDataSection + i;
			break;
		}
	}

	if (!StringAddress)
		return nullptr;

	for (int i = 0; i < TextSize; i++)
	{
		// opcode: lea
		const uint8_t CurrentByte = *reinterpret_cast<const uint8_t*>(TextSection + i);
		const uint8_t NextByte    = *reinterpret_cast<const uint8_t*>(TextSection + i + 0x1);

		if ((CurrentByte == 0x4C || CurrentByte == 0x48) && NextByte == 0x8D)
		{
			const uintptr_t StrPtr = ASMUtils::Resolve32BitRelativeLea(TextSection + i);

			if (StrPtr == StringAddress)
				return { TextSection + i };
		}
	}

	return nullptr;
}

// 通过宽字符串引用在.rdata和.text节中查找地址
inline MemAddress FindByWString(const wchar_t* RefStr)
{
	return FindByString<const wchar_t*>(RefStr);
}

/* 此函数比 FindByString 慢 */
// 在所有节中通过字符串引用查找地址
template<bool bCheckIfLeaIsStrPtr = false, typename CharType = char>
inline MemAddress FindByStringInAllSections(const CharType* RefStr, uintptr_t StartAddress = 0x0, int32_t Range = 0x0)
{
	static_assert(std::is_same_v<CharType, char> || std::is_same_v<CharType, wchar_t>, "FindByStringInAllSections only supports 'char' and 'wchar_t', but was called with other type.");

	/* Stop scanning when arriving 0x10 bytes before the end of the memory range */
	constexpr int32_t OffsetFromMemoryEnd = 0x10;

	const auto [ImageBase, ImageSize] = GetImageBaseAndSize();

	const uintptr_t ImageEnd = ImageBase + ImageSize;

	/* If the StartAddress is not default nullptr, and is out of memory-range */
	if (StartAddress != 0x0 && (StartAddress < ImageBase || StartAddress > ImageEnd))
		return nullptr;

	/* Add a few bytes to the StartAddress to prevent instantly returning the previous result */
	uint8_t* SearchStart = StartAddress ? (reinterpret_cast<uint8_t*>(StartAddress) + 0x5) : reinterpret_cast<uint8_t*>(ImageBase);
	DWORD SearchRange = StartAddress ? ImageEnd - StartAddress : ImageSize;

	if (Range != 0x0)
		SearchRange = min(Range, SearchRange);

	if ((StartAddress + SearchRange) >= ImageEnd)
		SearchRange -= OffsetFromMemoryEnd;

	const int32_t RefStrLen = StrlenHelper(RefStr);

	for (uintptr_t i = 0; i < SearchRange; i++)
	{
		// opcode: lea
		if ((SearchStart[i] == uint8_t(0x4C) || SearchStart[i] == uint8_t(0x48)) && SearchStart[i + 1] == uint8_t(0x8D))
		{
			const uintptr_t StrPtr = ASMUtils::Resolve32BitRelativeLea(reinterpret_cast<uintptr_t>(SearchStart + i));

			if (!IsInProcessRange(StrPtr))
				continue;

			if (StrnCmpHelper(RefStr, reinterpret_cast<const CharType*>(StrPtr), RefStrLen))
				return { SearchStart + i };

			if constexpr (bCheckIfLeaIsStrPtr)
			{
				const CharType* StrPtrContentFirst8Bytes = *reinterpret_cast<const CharType* const*>(StrPtr);

				if (!IsInProcessRange(StrPtrContentFirst8Bytes))
					continue;

				if (StrnCmpHelper(RefStr, StrPtrContentFirst8Bytes, RefStrLen))
					return { SearchStart + i };
			}
		}
	}

	return nullptr;
}

// 通过字符串引用查找Unreal Engine的UFunction::Exec函数
template<typename Type = const char*>
inline MemAddress FindUnrealExecFunctionByString(Type RefStr, void* StartAddress = nullptr)
{
	const auto [ImageBase, ImageSize] = GetImageBaseAndSize();

	uint8_t* SearchStart = StartAddress ? reinterpret_cast<uint8_t*>(StartAddress) : reinterpret_cast<uint8_t*>(ImageBase);
	DWORD SearchRange = ImageSize;

	const int32_t RefStrLen = StrlenHelper(RefStr);

	static auto IsValidExecFunctionNotSetupFunc = [](uintptr_t Address) -> bool
	{
		/* 
		* UFuntion construction functions setting up exec functions always start with these asm instructions:
		* sub rsp, 28h
		* 
		* In opcode bytes: 48 83 EC 28
		*/
		if (*reinterpret_cast<int32_t*>(Address) == 0x284883EC || *reinterpret_cast<int32_t*>(Address) == 0x4883EC28)
			return false;

		MemAddress AsAddress(Address);

		/* A signature specifically made for UFunctions-construction functions. If this signature is found we're in a function that we *don't* want. */
		if (AsAddress.RelativePattern("48 8B 05 ? ? ? ? 48 85 C0 75 ? 48 8D 15", 0x28) != nullptr)
			return false;

		return true;
	};

	for (uintptr_t i = 0; i < (SearchRange - 0x8); i += sizeof(void*))
	{
		const uintptr_t PossibleStringAddress = *reinterpret_cast<uintptr_t*>(SearchStart + i);
		const uintptr_t PossibleExecFuncAddress = *reinterpret_cast<uintptr_t*>(SearchStart + i + sizeof(void*));

		if (PossibleStringAddress == PossibleExecFuncAddress)
			continue;

		if (!IsInProcessRange(PossibleStringAddress) || !IsInProcessRange(PossibleExecFuncAddress))
			continue;

		if constexpr (std::is_same<Type, const char*>())
		{
			if (strncmp(reinterpret_cast<const char*>(RefStr), reinterpret_cast<const char*>(PossibleStringAddress), RefStrLen) == 0 && IsValidExecFunctionNotSetupFunc(PossibleExecFuncAddress))
			{
				// std::cerr << "FoundStr ref: " << reinterpret_cast<const char*>(PossibleStringAddress) << "\n";

				return { PossibleExecFuncAddress };
			}
		}
		else
		{
			if (wcsncmp(reinterpret_cast<const wchar_t*>(RefStr), reinterpret_cast<const wchar_t*>(PossibleStringAddress), RefStrLen) == 0 && IsValidExecFunctionNotSetupFunc(PossibleExecFuncAddress))
			{
				// std::wcout << L"FoundStr wref: " << reinterpret_cast<const wchar_t*>(PossibleStringAddress) << L"\n";

				return { PossibleExecFuncAddress };
			}
		}
	}

	return nullptr;
}

/* 此函数比 FindByWString 慢 */
// 在所有节中通过宽字符串引用查找地址
template<bool bCheckIfLeaIsStrPtr = false>
inline MemAddress FindByWStringInAllSections(const wchar_t* RefStr)
{
	return FindByStringInAllSections<bCheckIfLeaIsStrPtr, wchar_t>(RefStr);
}


namespace FileNameHelper
{
	// 规范化字符串，使其成为有效的文件名（替换非法字符）
	inline void MakeValidFileName(std::string& InOutName)
	{
		for (char& c : InOutName)
		{
			if (c == '<' || c == '>' || c == ':' || c == '\"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*')
				c = '_';
		}
	}
}
