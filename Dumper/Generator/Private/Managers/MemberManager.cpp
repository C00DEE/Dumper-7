#include <algorithm>

#include "Managers/MemberManager.h"
#include "Wrappers/MemberWrappers.h"

/*
* MemberManager.cpp 实现了一个用于管理结构体或类成员（包括属性和函数）的系统。
*
* 它的核心功能是整合两种不同来源的成员：
* 1. 从虚幻引擎本身解析出的成员（UEStruct）。
* 2. 在Dumper中为特定类型硬编码的预定义成员（PredefinedStruct）。
*
* 通过提供自定义的迭代器（MemberIterator, FunctionIterator），此类允许代码生成器
* 以统一的方式无缝地遍历这两种成员，而无需关心其具体来源。这大大简化了
* 生成器处理成员时的逻辑。
*
* 此外，它还负责对成员按其在内存中的偏移进行排序，以确保生成的代码布局正确。
*/


// 构造函数，用于处理从引擎解析出的UEStruct。
MemberManager::MemberManager(UEStruct Str)
	: Struct(std::make_shared<StructWrapper>(Str))
	, Functions(Str.GetFunctions())
	, Members(Str.GetProperties())
{
	// 对函数/成员进行排序 O(n*log(n))，虽然可以使用基数排序 O(n)，但开销可能不值得。
	// 排序是为了确保成员按内存偏移的顺序处理。
	std::sort(Functions.begin(), Functions.end(), CompareUnrealFunctions);
	std::sort(Members.begin(), Members.end(), CompareUnrealProperties);

	if (!PredefinedMemberLookup)
		return;

	// 查找当前结构体是否存在预定义的成员。
	auto It = PredefinedMemberLookup->find(Struct->GetUnrealStruct().GetIndex());
	if (It != PredefinedMemberLookup->end())
	{
		PredefMembers = &It->second.Members;
		PredefFunctions = &It->second.Functions;
	}
}

// 构造函数，用于处理在Dumper中硬编码的PredefinedStruct。
MemberManager::MemberManager(const PredefinedStruct* Str)
	: Struct(std::make_shared<StructWrapper>(Str))
	, Functions()
	, Members()
	, PredefMembers(&Str->Properties)
	, PredefFunctions(&Str->Functions)
{
}

// 获取引擎函数的数量。
int32 MemberManager::GetNumFunctions() const
{
	return Functions.size();
}

// 获取预定义函数的数量。
int32 MemberManager::GetNumPredefFunctions() const
{
	return PredefFunctions ? PredefFunctions->size() : 0x0;
}

// 获取引擎成员（属性）的数量。
int32 MemberManager::GetNumMembers() const
{
	return Members.size();
}

// 获取预定义成员（属性）的数量。
int32 MemberManager::GetNumPredefMembers() const
{
	return PredefMembers ? PredefMembers->size() : 0x0;
}

// 检查是否存在任何函数（包括引擎和预定义的）。
bool MemberManager::HasFunctions() const
{
	return GetNumFunctions() > 0x0 || GetNumPredefFunctions() > 0x0;
}

// 检查是否存在任何成员（包括引擎和预定义的）。
bool MemberManager::HasMembers() const
{
	return GetNumMembers() > 0x0 || GetNumPredefMembers() > 0x0;
}

// 返回一个用于遍历所有成员（属性）的迭代器。
// 这个迭代器会无缝地合并引擎成员和预定义成员。
MemberIterator<true> MemberManager::IterateMembers() const
{
	return MemberIterator<true>(Struct, Members, PredefMembers);
}

// 返回一个用于遍历所有函数的迭代器。
// 这个迭代器会无缝地合并引擎函数和预定义函数。
FunctionIterator<true> MemberManager::IterateFunctions() const
{
	return FunctionIterator<true>(Struct, Functions, PredefFunctions);
}

