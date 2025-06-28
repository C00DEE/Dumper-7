#pragma once

#include <string>
#include <vector>
#include <filesystem>

#include "Unreal/UnrealObjects.h"
#include "OffsetFinder/Offsets.h"

namespace fs = std::filesystem;

// 对象数组类，用于管理游戏中的所有UObject
class ObjectArray
{
private:
	friend struct FChunkedFixedUObjectArray;
	friend struct FFixedUObjectArray;
	friend class ObjectArrayValidator;

	friend bool IsAddressValidGObjects(const uintptr_t, const struct FFixedUObjectArrayLayout&);
	friend bool IsAddressValidGObjects(const uintptr_t, const struct FChunkedFixedUObjectArrayLayout&);

private:
	// 指向全局对象数组（GObjects）的指针
	static inline uint8* GObjects = nullptr;
	// 每个块中的元素数量
	static inline uint32 NumElementsPerChunk = 0x10000;
	// FUObjectItem的大小
	static inline uint32 SizeOfFUObjectItem = 0x18;
	// FUObjectItem的初始偏移量
	static inline uint32 FUObjectItemInitialOffset = 0x0;

public:
	// 解密Lambda表达式的字符串形式
	static inline std::string DecryptionLambdaStr;

private:
	// 通过索引获取对象的函数指针
	static inline void*(*ByIndex)(void* ObjectsArray, int32 Index, uint32 FUObjectItemSize, uint32 FUObjectItemOffset, uint32 PerChunk) = nullptr;

	// 解密对象指针的函数指针
	static inline uint8_t* (*DecryptPtr)(void* ObjPtr) = [](void* Ptr) -> uint8* { return static_cast<uint8*>(Ptr); };

private:
	// 初始化FUObjectItem
	static void InitializeFUObjectItem(uint8_t* FirstItemPtr);
	// 初始化块大小
	static void InitializeChunkSize(uint8_t* GObjects);

public:
	// 初始化解密函数
	static void InitDecryption(uint8_t* (*DecryptionFunction)(void* ObjPtr), const char* DecryptionLambdaAsStr);

	// 初始化对象数组
	static void Init(bool bScanAllMemory = false, const char* const ModuleName = nullptr);

	// 使用固定布局初始化对象数组
	static void Init(int32 GObjectsOffset, const FFixedUObjectArrayLayout& ObjectArrayLayout = FFixedUObjectArrayLayout(), const char* const ModuleName = nullptr);
	// 使用分块布局初始化对象数组
	static void Init(int32 GObjectsOffset, int32 ElementsPerChunk, const FChunkedFixedUObjectArrayLayout& ObjectArrayLayout = FChunkedFixedUObjectArrayLayout(), const char* const ModuleName = nullptr);

	// 转储对象到文件
	static void DumpObjects(const fs::path& Path, bool bWithPathname = false);
	// 转储带属性的对象到文件
	static void DumpObjectsWithProperties(const fs::path& Path, bool bWithPathname = false);

	// 获取对象总数
	static int32 Num();

	// 通过索引获取对象
	template<typename UEType = UEObject>
	static UEType GetByIndex(int32 Index);

	// 通过全名查找对象
	template<typename UEType = UEObject>
	static UEType FindObject(const std::string& FullName, EClassCastFlags RequiredType = EClassCastFlags::None);

	// 快速通过名称查找对象
	template<typename UEType = UEObject>
	static UEType FindObjectFast(const std::string& Name, EClassCastFlags RequiredType = EClassCastFlags::None);

	// 在指定外部对象中快速查找对象
	template<typename UEType = UEObject>
	static UEType FindObjectFastInOuter(const std::string& Name, std::string Outer);

	// 查找结构体
	static UEStruct FindStruct(const std::string& FullName);
	// 快速查找结构体
	static UEStruct FindStructFast(const std::string& Name);

	// 查找类
	static UEClass FindClass(const std::string& FullName);
	// 快速查找类
	static UEClass FindClassFast(const std::string& Name);

	// 对象迭代器
	class ObjectsIterator
	{
		UEObject CurrentObject;
		int32 CurrentIndex;

	public:
		ObjectsIterator(int32 StartIndex = 0);

		UEObject operator*();
		ObjectsIterator& operator++();
		bool operator!=(const ObjectsIterator& Other);

		int32 GetIndex() const;
	};

	// 获取起始迭代器
	ObjectsIterator begin();
	// 获取结束迭代器
	ObjectsIterator end();

	// 【调试用】获取GObjects指针
	static inline void* DEBUGGetGObjects()
	{
		return GObjects;
	}
};

#ifndef InitObjectArrayDecryption
#define InitObjectArrayDecryption(DecryptionLambda) ObjectArray::InitDecryption(DecryptionLambda, #DecryptionLambda)
#endif
