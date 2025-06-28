#include "Unreal/ObjectArray.h"
#include "Managers/StructManager.h"

// StructInfoHandle 只是 StructInfo 的一个轻量级包装，通过指针访问，避免复制
StructInfoHandle::StructInfoHandle(const StructInfo& InInfo)
	: Info(&InInfo)
{
}

// 获取最后一个成员变量结束的位置
int32 StructInfoHandle::GetLastMemberEnd() const
{
	return Info->LastMemberEnd;
}

// 获取对齐后的结构体大小
int32 StructInfoHandle::GetSize() const
{
	return Align(Info->Size, Info->Alignment);
}

// 获取未对齐的结构体大小
int32 StructInfoHandle::GetUnalignedSize() const
{
	return Info->Size;
}

// 获取结构体对齐方式
int32 StructInfoHandle::GetAlignment() const
{
	return Info->Alignment;
}

// 是否应使用显式的对齐方式
bool StructInfoHandle::ShouldUseExplicitAlignment() const
{
	return Info->bUseExplicitAlignment;
}

// 获取结构体名称
const StringEntry& StructInfoHandle::GetName() const
{
	return StructManager::GetName(*Info);
}

// 结构体是否是"最终的"（没有子类继承）
bool StructInfoHandle::IsFinal() const
{
	return Info->bIsFinal;
}

// 是否复用了尾部填充（padding）
bool StructInfoHandle::HasReusedTrailingPadding() const
{
	return Info->bHasReusedTrailingPadding;
}

// 是否是循环依赖包的一部分
bool StructInfoHandle::IsPartOfCyclicPackage() const
{
	return Info->bIsPartOfCyclicPackage;
}

// 初始化所有结构体的对齐方式和名称
void StructManager::InitAlignmentsAndNames()
{
	// UClass的默认对齐是8字节
	constexpr int32 DefaultClassAlignment = 0x8;

	// 找到"Interface"这个特殊的UClass
	const UEClass InterfaceClass = ObjectArray::FindClassFast("Interface");

	// 第一次遍历：为每个UStruct（包括UClass和UFunction）设置初始对齐和名称
	for (auto Obj : ObjectArray())
	{
		if (!Obj.IsA(EClassCastFlags::Struct) /* || Obj.IsA(EClassCastFlags::Function)*/)
			continue;

		UEStruct ObjAsStruct = Obj.Cast<UEStruct>();

		// 为对象索引获取或创建一个StructInfo，并设置其名称
		StructInfo& NewOrExistingInfo = StructInfoOverrides[Obj.GetIndex()];
		NewOrExistingInfo.Name = UniqueNameTable.FindOrAdd(Obj.GetCppName(), !Obj.IsA(EClassCastFlags::Function)).first;

		// 接口（Interface）比较特殊，它们虽然继承UObject，但我们将其视为空结构体，作为一种变通处理
		if (ObjAsStruct.HasType(InterfaceClass))
		{
			NewOrExistingInfo.Alignment = 0x1;
			NewOrExistingInfo.bHasReusedTrailingPadding = false;
			NewOrExistingInfo.bIsFinal = true;
			NewOrExistingInfo.Size = 0x0;

			continue;
		}

		// 获取引擎要求的最小对齐和成员中的最大对齐
		int32 MinAlignment = ObjAsStruct.GetMinAlignment();
		int32 HighestMemberAlignment = 0x1; // 从1开始，因为我们要检查所有成员，而不仅仅是结构体属性

		// 遍历所有属性，找到最大的对齐要求
		for (UEProperty Property : ObjAsStruct.GetProperties())
		{
			int32 CurrentPropertyAlignment = Property.GetAlignment();

			if (CurrentPropertyAlignment > HighestMemberAlignment)
				HighestMemberAlignment = CurrentPropertyAlignment;
		}

		/* 在某些奇怪的游戏中，存在不继承自UObject的蓝图生成类 (BlueprintGeneratedClass) */
		const bool bHasSuperClass = static_cast<bool>(ObjAsStruct.GetSuper());

		// 如果是UClass，且有父类，并且其成员的最大对齐小于默认的指针大小（8字节），则强制使用8字节对齐
		// 否则，取引擎要求的最小对齐和成员最大对齐中较大的一个
		if (ObjAsStruct.IsA(EClassCastFlags::Class) && bHasSuperClass && HighestMemberAlignment < DefaultClassAlignment)
		{
			NewOrExistingInfo.bUseExplicitAlignment = false;
			NewOrExistingInfo.Alignment = DefaultClassAlignment;
		}
		else
		{
			NewOrExistingInfo.bUseExplicitAlignment = MinAlignment > HighestMemberAlignment;
			NewOrExistingInfo.Alignment = max(MinAlignment, HighestMemberAlignment);
		}
	}

	// 第二次遍历：修正对齐方式，确保子类的对齐不小于父类
	for (auto Obj : ObjectArray())
	{
		if (!Obj.IsA(EClassCastFlags::Struct) || Obj.IsA(EClassCastFlags::Function) || Obj.Cast<UEStruct>().HasType(InterfaceClass))
			continue;

		UEStruct ObjAsStruct = Obj.Cast<UEStruct>();

		constexpr int MaxNumSuperClasses = 0x30;

		std::array<UEStruct, MaxNumSuperClasses> StructStack;
		int32 NumElementsInStructStack = 0x0;

		// 从当前结构体开始，向上遍历所有父类，构建一个从顶层到底层的继承链
		for (UEStruct S = ObjAsStruct; S; S = S.GetSuper())
		{
			StructStack[NumElementsInStructStack] = S;
			NumElementsInStructStack++;
		}

		int32 CurrentHighestAlignment = 0x0;

		// 从继承链的顶层（如UObject）开始向下遍历
		for (int i = NumElementsInStructStack - 1; i >= 0; i--)
		{
			StructInfo& Info = StructInfoOverrides[StructStack[i].GetIndex()];

			// 如果当前结构体的对齐大于等于父类的对齐，就更新"当前最大对齐"
			if (CurrentHighestAlignment < Info.Alignment)
			{
				CurrentHighestAlignment = Info.Alignment;
			}
			else // 否则，说明子类的对齐要求小于父类，这不符合C++规则，需要强制设为与父类一致
			{
				// 我们使用父类的对齐方式，所以不需要显式设置
				Info.bUseExplicitAlignment = false; 
				Info.Alignment = CurrentHighestAlignment;
			}
		}
	}
}

// 初始化所有结构体的大小和 bIsFinal 标志
void StructManager::InitSizesAndIsFinal()
{
	const UEClass InterfaceClass = ObjectArray::FindClassFast("Interface");

	// 遍历所有UStruct
	for (auto Obj : ObjectArray())
	{
		if (!Obj.IsA(EClassCastFlags::Struct) || Obj.Cast<UEStruct>().HasType(InterfaceClass))
			continue;

		UEStruct ObjAsStruct = Obj.Cast<UEStruct>();

		StructInfo& NewOrExistingInfo = StructInfoOverrides[Obj.GetIndex()];

		// 如果我们之前设置的大小（可能来自子类）比引擎报告的大小要小，就用引擎的大小
		if (NewOrExistingInfo.Size > ObjAsStruct.GetStructSize())
			NewOrExistingInfo.Size = ObjAsStruct.GetStructSize();

		UEStruct Super = ObjAsStruct.GetSuper();

		// 如果结构体大小为0但有父类，则继承父类的大小
		if (NewOrExistingInfo.Size == 0x0 && Super != nullptr)
			NewOrExistingInfo.Size = Super.GetStructSize();

		int32 LastMemberEnd = 0x0;
		int32 LowestOffset = INT_MAX;

		// 遍历所有属性，找到最低的偏移量和最后一个成员结束的位置
		for (UEProperty Property : ObjAsStruct.GetProperties())
		{
			const int32 PropertyOffset = Property.GetOffset();
			const int32 PropertySize = Property.GetSize();

			if (PropertyOffset < LowestOffset)
				LowestOffset = PropertyOffset;

			if ((PropertyOffset + PropertySize) > LastMemberEnd)
				LastMemberEnd = PropertyOffset + PropertySize;
		}

		/* 无需检查其他结构体，因为寻找LastMemberEnd只涉及当前结构体本身 */
		NewOrExistingInfo.LastMemberEnd = LastMemberEnd;

		// 如果没有父类，或者它是一个函数（UFunction），则跳过后续处理
		if (!Super || Obj.IsA(EClassCastFlags::Function))
			continue;

		/*
		* 循环遍历所有父结构体，并将其结构体大小设置为我们找到的最低偏移量。
		* 这个大小会设置在直接父类和所有更上层的*空*父类上。
		* 
		* 在遇到一个非空（即拥有成员变量）的父结构体后，循环会中断。
		* 这是为了处理子类复用父类尾部填充（padding）的情况。
		*/
		for (UEStruct S = Super; S; S = S.GetSuper())
		{
			auto It = StructInfoOverrides.find(S.GetIndex());

			if (It == StructInfoOverrides.end())
			{
				std::cerr << "\n\n\nDumper-7: Error, struct wasn't found in 'StructInfoOverrides'! Exiting...\n\n\n" << std::endl;
				Sleep(10000);
				exit(1);
			}

			StructInfo& Info = It->second;

			// 结构体不是 final 的，因为它是另一个结构体的父类
			Info.bIsFinal = false;

			const int32 SizeToCheck = Info.Size == INT_MAX ? S.GetStructSize() : Info.Size;

			// 仅当找到的最低偏移量小于已经确定的最低偏移量（默认为结构体大小）时，才更改它
			if (Align(SizeToCheck, Info.Alignment) > LowestOffset)
			{
				if (Info.Size > LowestOffset)
					Info.Size = LowestOffset;

				Info.bHasReusedTrailingPadding = true;
			}

			// 如果父类已经有成员了，就没必要再向上调整大小了
			if (S.HasMembers())
				break;
		}
	}
}

// StructManager 的总初始化函数
void StructManager::Init()
{
	if (bIsInitialized)
		return;

	bIsInitialized = true;

	StructInfoOverrides.reserve(0x2000); // 预分配内存

	InitAlignmentsAndNames(); // 初始化对齐和名称
	InitSizesAndIsFinal(); // 初始化大小和 final 标志

	/* 
	* 8字节的默认类对齐仅为具有有效父类的类设置，因为它们继承自UObject。
	* 然而，UObject本身没有父类，因此需要手动设置。
	*/
	const UEObject UObjectClass = ObjectArray::FindClassFast("Object");
	StructInfoOverrides.find(UObjectClass.GetIndex())->second.Alignment = 0x8;

	/* 我仍然讨厌在某些UE版本中把"UStruct"叫做"Ustruct"的决定。*/
	// 修正某些UE版本中 UStruct 名称不一致的问题
	if (const UEObject UStructClass = ObjectArray::FindClassFast("struct"))
		StructInfoOverrides.find(UStructClass.GetIndex())->second.Name = UniqueNameTable.FindOrAdd(std::string("UStruct"), false).first;
}
