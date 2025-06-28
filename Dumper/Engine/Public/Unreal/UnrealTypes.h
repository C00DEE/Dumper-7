#pragma once

#include <array>
#include <string>
#include <iostream>
#include <Windows.h>

#include "Unreal/Enums.h"
#include "OffsetFinder/Offsets.h"

#include "Encoding/UnicodeNames.h"

#include "Utils.h"
#include "UnrealContainers.h"

using namespace UC;

// 将给定的宽字符串名称转换为有效的标识符字符串。
extern std::string MakeNameValid(std::wstring&& Name);

// 表示已实现接口的模板结构体。
template<typename Type>
struct TImplementedInterface
{
	// 接口的类
	Type InterfaceClass;
	// 接口指针在实现类中的偏移量
	int32 PointerOffset;
	// 是否通过蓝图实现
	bool bImplementedByK2;
};

// FImplementedInterface 是 TImplementedInterface<UEClass> 的类型别名
using FImplementedInterface = TImplementedInterface<class UEClass>;

// 一个可释放的FString版本，用于当我们手动分配字符串内存时
class FFreableString : public FString
{
public:
	using FString::FString;

	// 构造函数，保留指定数量的元素
	FFreableString(uint32_t NumElementsToReserve)
	{
		if (NumElementsToReserve > 0x1000000)
			return;

		this->Data = static_cast<wchar_t*>(malloc(sizeof(wchar_t) * NumElementsToReserve));
		this->NumElements = 0;
		this->MaxElements = NumElementsToReserve;
	}

	~FFreableString()
	{
		/* If we're using FName::ToString the allocation comes from the engine and we can not free it. Just leak those 2048 bytes. */
		// 如果我们使用的是 FName::ToString，内存分配来自引擎，我们无法释放它。
		// 只能泄漏这部分内存（比如2048字节）。
		if (Off::InSDK::Name::bIsUsingAppendStringOverToString)
			FreeArray();
	}

public:
	// 重置字符串长度为0
	inline void ResetNum()
	{
		this->NumElements = 0;
	}

private:
	// 释放手动分配的数组内存
	inline void FreeArray()
	{
		this->NumElements = 0;
		this->MaxElements = 0;
		if (this->Data) free(this->Data);
		this->Data = nullptr;
	}
};

// FName 的包装类，用于处理虚幻引擎中的名称
class FName
{
public:
	// 用于 Init 函数的偏移覆盖类型
	enum class EOffsetOverrideType
	{
		AppendString,
		ToString,
		GNames
	};

private:
	// 指向 FName::AppendString 的函数指针
	inline static void(*AppendString)(const void*, FString&) = nullptr;

	// 指向 FName::ToString 的函数指针
	inline static std::wstring(*ToStr)(const void* Name) = nullptr;

private:
	// 指向 FNameEntry 的指针
	const uint8* Address;

public:
	FName() = default;

	FName(const void* Ptr);

public:
	// 初始化 FName 系统，bForceGNames 为 true 时强制使用 GNames
	static void Init(bool bForceGNames = false);
	// 初始化 FName 的备用方法
	static void InitFallback();

	// 使用指定的偏移和类型初始化 FName 系统
	static void Init(int32 OverrideOffset, EOffsetOverrideType OverrideType = EOffsetOverrideType::AppendString, bool bIsNamePool = false, const char* const ModuleName = nullptr);

public:
	// 获取 FNameEntry 的地址
	inline const void* GetAddress() const { return Address; }

	// 转换为宽字符串
	std::wstring ToWString() const;
	// 转换为原始宽字符串（可能包含非显示字符）
	std::wstring ToRawWString() const;

	// 转换为标准字符串
	std::string ToString() const;
	// 转换为原始标准字符串
	std::string ToRawString() const;
	// 转换为有效的标识符字符串
	std::string ToValidString() const;

	// 获取比较索引
	int32 GetCompIdx() const;
	// 获取数字部分（如果没有则为0）
	uint32 GetNumber() const;

	bool operator==(FName Other) const;

	bool operator!=(FName Other) const;

	// 将比较索引转换为字符串
	static std::string CompIdxToString(int CmpIdx);

	// 【调试用】获取 AppendString 函数的地址
	static void* DEBUGGetAppendString();
};
