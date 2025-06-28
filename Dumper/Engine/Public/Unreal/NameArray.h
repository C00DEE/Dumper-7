#pragma once

#include "Unreal/UnrealTypes.h"

// FName条目，表示FName池中的单个名称
class FNameEntry
{
private:
	friend class NameArray;

private:
	// 名称宽字符掩码
	static constexpr int32 NameWideMask = 0x1;

private:
	// FName条目长度移位计数
	static inline int32 FNameEntryLengthShiftCount = 0x0;

	// 获取字符串的函数指针
	static inline std::wstring(*GetStr)(uint8* NameEntry) = nullptr;

private:
	// 条目地址
	uint8* Address;

public:
	FNameEntry() = default;

	FNameEntry(void* Ptr);

public:
	// 获取宽字符串
	std::wstring GetWString();
	// 获取字符串
	std::string GetString();
	// 获取地址
	void* GetAddress();

private:
	//Optional to avoid code duplication for FNamePool
	// 可选的初始化函数，以避免 FNamePool 的代码重复
	static void Init(const uint8_t* FirstChunkPtr = nullptr, int64 NameEntryStringOffset = 0x0);
};

// 名称数组，用于管理FName
class NameArray
{
private:
	// FName 块偏移位数
	static inline uint32 FNameBlockOffsetBits = 0x10;

private:
	// GNames 的指针
	static uint8* GNames;

	// 名称条目的步长
	static inline int64 NameEntryStride = 0x0;

	// 通过索引获取条目的函数指针
	static inline void* (*ByIndex)(void* NamesArray, int32 ComparisonIndex, int32 NamePoolBlockOffsetBits) = nullptr;

private:
	// 初始化名称数组
	static bool InitializeNameArray(uint8_t* NameArray);
	// 初始化名称池
	static bool InitializeNamePool(uint8_t* NamePool);

public:
	/* Should be changed later and combined */
	// 稍后应更改并合并
	// 尝试查找名称数组
	static bool TryFindNameArray();
	// 尝试查找名称池
	static bool TryFindNamePool();

	// 尝试初始化
	static bool TryInit(bool bIsTestOnly = false);
	// 尝试使用偏移量覆盖进行初始化
	static bool TryInit(int32 OffsetOverride, bool bIsNamePool, const char* const ModuleName = nullptr);

	/* Initializes the GNames offset, but doesn't call NameArray::InitializeNameArray() or NameArray::InitializedNamePool() */
	// 初始化 GNames 偏移量，但不调用 NameArray::InitializeNameArray() 或 NameArray::InitializedNamePool()
	static bool SetGNamesWithoutCommiting();

	// 后初始化
	static void PostInit();
	
public:
	// 获取块数
	static int32 GetNumChunks();

	// 获取元素数
	static int32 GetNumElements();
	// 获取字节游标
	static int32 GetByteCursor();

	// 通过名称指针获取名称条目
	static FNameEntry GetNameEntry(const void* Name);
	// 通过索引获取名称条目
	static FNameEntry GetNameEntry(int32 Idx);
};
