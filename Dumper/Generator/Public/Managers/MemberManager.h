#pragma once

#include <unordered_map>
#include <memory>

#include "ObjectArray.h"
#include "HashStringTable.h"
#include "CollisionManager.h"
#include "PredefinedMembers.h"


// 成员迭代器
template<bool bIsDeferredTemplateCreation = true>
class MemberIterator
{
private:
	using PredefType = std::conditional_t<bIsDeferredTemplateCreation, PredefinedMember, void>; // 预定义类型
	using DereferenceType = std::conditional_t<bIsDeferredTemplateCreation, class PropertyWrapper, void>; // 解引用类型

private:
	const std::shared_ptr<class StructWrapper> Struct; // 结构体

	const std::vector<UEProperty>& Members; // 成员
	const std::vector<PredefType>* PredefElements; // 预定义元素

	int32 CurrentIdx = 0x0; // 当前索引
	int32 CurrentPredefIdx = 0x0; // 当前预定义索引

	bool bIsCurrentlyPredefined = true; // 当前是否为预定义

public:
	inline MemberIterator(const std::shared_ptr<class StructWrapper>& Str, const std::vector<UEProperty>& Mbr, const std::vector<PredefType>* const Predefs = nullptr, int32 StartIdx = 0x0, int32 PredefStart = 0x0)
		: Struct(Str), Members(Mbr), PredefElements(Predefs), CurrentIdx(StartIdx), CurrentPredefIdx(PredefStart)
	{
		const int32 NextUnrealOffset = GetUnrealMemberOffset();
		const int32 NextPredefOffset = GetPredefMemberOffset();

		if (NextUnrealOffset < NextPredefOffset) [[likely]]
		{
			bIsCurrentlyPredefined = false;
		}
		else [[unlikely]]
		{
			bIsCurrentlyPredefined = true;
		}
	}

private:
	/* bIsProperty */
	// 是否为有效的虚幻成员索引
	inline bool IsValidUnrealMemberIndex() const { return CurrentIdx < Members.size(); }
	// 是否为有效的预定义成员索引
	inline bool IsValidPredefMemberIndex() const { return PredefElements ? CurrentPredefIdx < PredefElements->size() : false; }

	// 获取虚幻成员偏移量
	int32 GetUnrealMemberOffset() const { return IsValidUnrealMemberIndex() ? Members.at(CurrentIdx).GetOffset() : 0xFFFFFFF; }
	// 获取预定义成员偏移量
	int32 GetPredefMemberOffset() const { return IsValidPredefMemberIndex() ? PredefElements->at(CurrentPredefIdx).Offset : 0xFFFFFFF; }

public:
	DereferenceType operator*() const
	{
		return bIsCurrentlyPredefined ? DereferenceType(Struct, &PredefElements->at(CurrentPredefIdx)) : DereferenceType(Struct, Members.at(CurrentIdx));
	}

	inline MemberIterator& operator++()
	{
		bIsCurrentlyPredefined ? CurrentPredefIdx++ : CurrentIdx++;

		const int32 NextUnrealOffset = GetUnrealMemberOffset();
		const int32 NextPredefOffset = GetPredefMemberOffset();

		bIsCurrentlyPredefined = NextPredefOffset < NextUnrealOffset;
		
		return *this;
	}

public:
	inline bool operator==(const MemberIterator& Other) const { return CurrentIdx == Other.CurrentIdx && CurrentPredefIdx == Other.CurrentPredefIdx; }
	inline bool operator!=(const MemberIterator& Other) const { return CurrentIdx != Other.CurrentIdx || CurrentPredefIdx != Other.CurrentPredefIdx; }

public:
	inline MemberIterator begin() const { return *this; }

	inline MemberIterator end() const { return MemberIterator(Struct, Members, PredefElements, Members.size(), PredefElements ? PredefElements->size() : 0x0); }
};

// 函数迭代器
template<bool bIsDeferredTemplateCreation = true>
class FunctionIterator
{
private:
	using PredefType = std::conditional_t<bIsDeferredTemplateCreation, PredefinedFunction, void>; // 预定义类型
	using DereferenceType = std::conditional_t<bIsDeferredTemplateCreation, class FunctionWrapper, void> ; // 解引用类型

private:
	const std::shared_ptr<StructWrapper> Struct; // 结构体

	const std::vector<UEFunction>& Members; // 成员
	const std::vector<PredefType>* PredefElements; // 预定义元素

	int32 CurrentIdx = 0x0; // 当前索引
	int32 CurrentPredefIdx = 0x0; // 当前预定义索引

	bool bIsCurrentlyPredefined = true; // 当前是否为预定义

public:
	inline FunctionIterator(const std::shared_ptr<StructWrapper>& Str, const std::vector<UEFunction>& Mbr, const std::vector<PredefType>* const Predefs = nullptr, int32 StartIdx = 0x0, int32 PredefStart = 0x0)
		: Struct(Str), Members(Mbr), PredefElements(Predefs), CurrentIdx(StartIdx), CurrentPredefIdx(PredefStart)
	{
		bIsCurrentlyPredefined = bShouldNextMemberBePredefined();
	}

private:
	/* bIsFunction */
	// 下一个预定义函数是否为内联
	inline bool IsNextPredefFunctionInline() const { return PredefElements ? PredefElements->at(CurrentPredefIdx).bIsBodyInline : false; }
	// 下一个预定义函数是否为静态
	inline bool IsNextPredefFunctionStatic() const { return PredefElements ? PredefElements->at(CurrentPredefIdx).bIsStatic : false; }
	// 下一个虚幻函数是否为内联
	inline bool IsNextUnrealFunctionInline() const { return HasMoreUnrealMembers() ? Members.at(CurrentIdx).HasFlags(EFunctionFlags::Static) : false; }

	// 是否有更多预定义成员
	inline bool HasMorePredefMembers() const { return PredefElements ? CurrentPredefIdx < PredefElements->size() : false; }
	// 是否有更多虚幻成员
	inline bool HasMoreUnrealMembers() const { return CurrentIdx < Members.size(); }

private:
	// 下一个成员是否应为预定义
	inline bool bShouldNextMemberBePredefined() const
	{
		const bool bHasMorePredefMembers = HasMorePredefMembers();
		const bool bHasMoreUnrealMembers = HasMoreUnrealMembers();

		if (!bHasMorePredefMembers)
			return false;

		if (PredefElements)
		{
			const PredefType& PredefFunc = PredefElements->at(CurrentPredefIdx);

			// Inline-body predefs are always last
			// 内联体预定义始终在最后
			if (PredefFunc.bIsBodyInline && bHasMoreUnrealMembers)
				return false;

			// Non-inline static predefs are always first
			// 非内联静态预定义始终在最前
			if (PredefFunc.bIsStatic)
				return true;

			// Switch from static predefs to static unreal functions
			// 从静态预定义切换到静态虚幻函数
			if (bHasMoreUnrealMembers && Members.at(CurrentIdx).HasFlags(EFunctionFlags::Static))
				return false;

			return !PredefFunc.bIsBodyInline || !bHasMoreUnrealMembers;
		}

		return !bHasMoreUnrealMembers;
	}

public:
	inline DereferenceType operator*() const
	{
		return bIsCurrentlyPredefined ? DereferenceType(Struct, &PredefElements->at(CurrentPredefIdx)) : DereferenceType(Struct, Members.at(CurrentIdx));
	}

	inline FunctionIterator& operator++()
	{
		bIsCurrentlyPredefined ? CurrentPredefIdx++ : CurrentIdx++;

		bIsCurrentlyPredefined = bShouldNextMemberBePredefined();
		
		return *this;
	}

public:
	inline bool operator==(const FunctionIterator& Other) const { return CurrentIdx == Other.CurrentIdx && CurrentPredefIdx == Other.CurrentPredefIdx; }
	inline bool operator!=(const FunctionIterator& Other) const { return CurrentIdx != Other.CurrentIdx || CurrentPredefIdx != Other.CurrentPredefIdx; }

public:
	inline FunctionIterator begin() const { return *this; }

	inline FunctionIterator end() const { return FunctionIterator(Struct, Members, PredefElements, Members.size(), PredefElements ? PredefElements->size() : 0x0); }
};


// 成员管理器
class MemberManager
{
private:
	friend class StructWrapper;
	friend class FunctionWrapper;
	friend class MemberManagerTest;
	friend class CollisionManagerTest;

private:
	/* Map to lookup if a struct has predefined members */
	/* 用于查找结构体是否具有预定义成员的映射 */
	static inline const PredefinedMemberLookupMapType* PredefinedMemberLookup = nullptr;

	/* CollisionManager containing information on colliding member-/function-names */
	/* 包含冲突成员/函数名称信息的CollisionManager */
	static inline CollisionManager MemberNames;

private:
	const std::shared_ptr<StructWrapper> Struct; // 结构体

	std::vector<UEProperty> Members; // 成员
	std::vector<UEFunction> Functions; // 函数

	const std::vector<PredefinedMember>* PredefMembers = nullptr; // 预定义成员
	const std::vector<PredefinedFunction>* PredefFunctions = nullptr; // 预定义函数

private:
	MemberManager(UEStruct Str);
	MemberManager(const PredefinedStruct* Str);

public:
	// 获取函数数量
	int32 GetNumFunctions() const;
	// 获取成员数量
	int32 GetNumMembers() const;

	// 获取预定义函数数量
	int32 GetNumPredefFunctions() const;
	// 获取预定义成员数量
	int32 GetNumPredefMembers() const;

	// 是否有函数
	bool HasFunctions() const;
	// 是否有成员
	bool HasMembers() const;

	// 迭代成员
	MemberIterator<true> IterateMembers() const;
	// 迭代函数
	FunctionIterator<true> IterateFunctions() const;

public:
	// 设置预定义成员查找指针
	static inline void SetPredefinedMemberLookupPtr(const PredefinedMemberLookupMapType* Lookup)
	{
		PredefinedMemberLookup = Lookup;
	}

	/* Add special names like "Class", "Flags, "Parms", etc. to avoid collisions on them */
	/* 添加诸如"Class"、"Flags"、"Parms"之类的特殊名称，以避免在其上发生冲突 */
	static inline void InitReservedNames()
	{
		/* UObject reserved names */
		/* UObject 保留名称 */
		MemberNames.AddReservedClassName("Flags", false);
		MemberNames.AddReservedClassName("Index", false);
		MemberNames.AddReservedClassName("Class", false);
		MemberNames.AddReservedClassName("Name", false);
		MemberNames.AddReservedClassName("Outer", false);

		/* UFunction reserved names */
		/* UFunction 保留名称 */
		MemberNames.AddReservedClassName("FunctionFlags", false);

		/* Function-body reserved names */
		/* 函数体保留名称 */
		MemberNames.AddReservedClassName("Func", true);
		MemberNames.AddReservedClassName("Parms", true);
		MemberNames.AddReservedClassName("Params", true);
		MemberNames.AddReservedClassName("Flgs", true);


		/* Reserved C++ keywords, typedefs and macros */
		/* 保留的C++关键字、类型定义和宏 */
		MemberNames.AddReservedName("byte");
		MemberNames.AddReservedName("short");
		MemberNames.AddReservedName("int");
		MemberNames.AddReservedName("float");
		MemberNames.AddReservedName("double");
		MemberNames.AddReservedName("long");
		MemberNames.AddReservedName("signed");
		MemberNames.AddReservedName("unsigned");
		MemberNames.AddReservedName("operator");
		MemberNames.AddReservedName("return");

		/* Logical operators */
		/* 逻辑运算符 */
		MemberNames.AddReservedName("if");
    	MemberNames.AddReservedName("else");
		MemberNames.AddReservedName("or");
		MemberNames.AddReservedName("and");
		MemberNames.AddReservedName("xor");

		/* Additional reserved names */
		/* 其他保留名称 */
		MemberNames.AddReservedName("struct");
		MemberNames.AddReservedName("class");
		MemberNames.AddReservedName("for");
		MemberNames.AddReservedName("while");
		MemberNames.AddReservedName("switch");
		MemberNames.AddReservedName("case");
		MemberNames.AddReservedName("this");
		MemberNames.AddReservedName("default");
		MemberNames.AddReservedName("override");
		MemberNames.AddReservedName("private");
		MemberNames.AddReservedName("public");
		MemberNames.AddReservedName("const");

		MemberNames.AddReservedName("int8");
		MemberNames.AddReservedName("int16");
		MemberNames.AddReservedName("int32");
		MemberNames.AddReservedName("int64");
		MemberNames.AddReservedName("uint8");
		MemberNames.AddReservedName("uint16");
		MemberNames.AddReservedName("uint32");
		MemberNames.AddReservedName("uint64");

		MemberNames.AddReservedName("TRUE");
		MemberNames.AddReservedName("FALSE");
		MemberNames.AddReservedName("true");
		MemberNames.AddReservedName("false");

		MemberNames.AddReservedName("IN");
		MemberNames.AddReservedName("OUT");

		MemberNames.AddReservedName("min");
		MemberNames.AddReservedName("max");
	}

	// 初始化
	static inline void Init()
	{
		static bool bInitialized = false;

		if (bInitialized)
			return;

		bInitialized = true;

		/* Add special names first, to avoid name-collisions with predefined members */
		/* 首先添加特殊名称，以避免与预定义成员发生名称冲突 */
		InitReservedNames();

		/* Initialize member-name collisions  */
		/* 初始化成员名称冲突 */
		for (auto Obj : ObjectArray())
		{
			if (!Obj.IsA(EClassCastFlags::Struct) || Obj.IsA(EClassCastFlags::Function))
				continue;

			AddStructToNameContainer(Obj.Cast<UEStruct>());
		}
	}

	// 将结构体添加到名称容器
	static inline void AddStructToNameContainer(UEStruct Struct)
	{
		MemberNames.AddStructToNameContainer(Struct, (!Struct.IsA(EClassCastFlags::Class) && !Struct.IsA(EClassCastFlags::Function)));
	}

	// 获取名称冲突信息
	template<typename UEType>
	static inline NameInfo GetNameCollisionInfo(UEStruct Struct, UEType Member)
	{
		static_assert(std::is_same_v<UEType, UEProperty> || std::is_same_v<UEType, UEFunction>, "Type arguement in 'GetNameCollisionInfo' is of invalid type!"); // "GetNameCollisionInfo"中的类型参数无效！

		assert(Struct && "'GetNameCollisionInfo()' called with 'Struct' == nullptr"); // "GetNameCollisionInfo()"在'Struct'== nullptr的情况下被调用
		assert(Member && "'GetNameCollisionInfo()' called with 'Member' == nullptr"); // "GetNameCollisionInfo()"在'Member'== nullptr的情况下被调用
		
		return MemberNames.GetNameCollisionInfoUnchecked(Struct, Member);
	}

	// 字符串化名称
	static inline std::string StringifyName(UEStruct Struct, NameInfo Name)
	{
		return MemberNames.StringifyName(Struct, Name);
	}
};
