#include "Wrappers/StructWrapper.h"
#include "Managers/MemberManager.h"

// 构造函数，用于包装一个"预定义"的结构体
StructWrapper::StructWrapper(const PredefinedStruct* const Predef)
    : PredefStruct(Predef), InfoHandle()
{
}

// 构造函数，用于包装一个从引擎中获取的真实"UEStruct"
StructWrapper::StructWrapper(const UEStruct Str)
    : Struct(Str), InfoHandle(StructManager::GetInfo(Str)), bIsUnrealStruct(true)
{
}

// 获取底层的UEStruct对象
UEStruct StructWrapper::GetUnrealStruct() const
{
    assert(bIsUnrealStruct && "StructWrapper doesn't contain UnrealStruct. Illegal call to 'GetUnrealStruct()'.");

    return bIsUnrealStruct ? Struct : nullptr;
}

// 获取结构体的有效名称（可能经过处理）
std::string StructWrapper::GetName() const
{
    return bIsUnrealStruct ? Struct.GetValidName() : PredefStruct->UniqueName;
}

// 获取结构体的原始名称
std::string StructWrapper::GetRawName() const
{
    return bIsUnrealStruct ? Struct.GetName() : PredefStruct->UniqueName;
}

// 获取结构体的完整名称（包含路径等）
std::string StructWrapper::GetFullName() const
{
    return bIsUnrealStruct ? Struct.GetFullName() : "Predefined struct " + PredefStruct->UniqueName;
}

// 获取父结构体（Super）的包装器
StructWrapper StructWrapper::GetSuper() const
{
    return bIsUnrealStruct ? StructWrapper(Struct.GetSuper()) : PredefStruct->Super;
}

// 获取此结构体的成员管理器
MemberManager StructWrapper::GetMembers() const
{
    return bIsUnrealStruct ? MemberManager(Struct) : MemberManager(PredefStruct);
}


/* 获取唯一的名称及其是否唯一的标志 */
std::pair<std::string, bool> StructWrapper::GetUniqueName() const
{
    return { bIsUnrealStruct ? InfoHandle.GetName().GetName() : PredefStruct->UniqueName, bIsUnrealStruct ? InfoHandle.GetName().IsUnique() : true };
}

// 获取最后一个成员结束的位置
int32 StructWrapper::GetLastMemberEnd() const
{
    return bIsUnrealStruct ? InfoHandle.GetLastMemberEnd() : 0x0;
}

// 获取对齐方式
int32 StructWrapper::GetAlignment() const
{
    return bIsUnrealStruct ? InfoHandle.GetAlignment() : PredefStruct->Alignment;
}

// 获取对齐后的大小
int32 StructWrapper::GetSize() const
{
    return bIsUnrealStruct ? InfoHandle.GetSize() : Align(PredefStruct->Size, PredefStruct->Alignment);
}

// 获取未对齐的大小
int32 StructWrapper::GetUnalignedSize() const
{
    return bIsUnrealStruct ? InfoHandle.GetUnalignedSize() : PredefStruct->Size;
}

// 检查是否应使用显式对齐
bool StructWrapper::ShouldUseExplicitAlignment() const
{
    return bIsUnrealStruct ? InfoHandle.ShouldUseExplicitAlignment() : PredefStruct->bUseExplictAlignment;
}

// 检查是否复用了尾部填充（padding）
bool StructWrapper::HasReusedTrailingPadding() const
{
    return bIsUnrealStruct && InfoHandle.HasReusedTrailingPadding();
}

// 检查是否是"最终"结构体（没有子类）
bool StructWrapper::IsFinal() const
{
    return bIsUnrealStruct ? InfoHandle.IsFinal() : PredefStruct->bIsFinal;
}

// 检查是否是UClass
bool StructWrapper::IsClass() const
{
    return bIsUnrealStruct ? Struct.IsA(EClassCastFlags::Class) : PredefStruct->bIsClass;
}

// 检查是否是联合体（union），仅预定义结构体可以是
bool StructWrapper::IsUnion() const
{
    return !bIsUnrealStruct && PredefStruct->bIsUnion;
}

// 检查是否是UFunction
bool StructWrapper::IsFunction() const
{
    return bIsUnrealStruct && Struct.IsA(EClassCastFlags::Function);
}

// 检查是否是接口（Interface）
bool StructWrapper::IsInterface() const
{
    static UEClass InterfaceClass = ObjectArray::FindClassFast("Interface");

    return bIsUnrealStruct && Struct.IsA(EClassCastFlags::Class) && Struct.HasType(InterfaceClass);
}

// 检查是否是某个特定类型的UClass
bool StructWrapper::IsAClassWithType(UEClass TypeClass) const
{
    return IsUnrealStruct() && IsClass() && Struct.Cast<UEClass>().IsA(TypeClass);
}

// 检查包装的结构体是否有效（指针不为空）
bool StructWrapper::IsValid() const
{
    // Struct 和 PredefStruct 共享相同的内存位置，如果 Struct 为空，PredefStruct 也为空
    return PredefStruct != nullptr;
}

// 检查是否是真实的UEStruct
bool StructWrapper::IsUnrealStruct() const
{
    return bIsUnrealStruct;
}

// 检查此结构体是否与给定的包存在循环依赖
bool StructWrapper::IsCyclicWithPackage(int32 PackageIndex) const
{
    if (!bIsUnrealStruct || PackageIndex == -1)
        return false;

    if (!InfoHandle.IsPartOfCyclicPackage())
        return false;

    return StructManager::IsStructCyclicWithPackage(Struct.GetIndex(), PackageIndex);
}

// 检查预定义结构体是否有自定义模板文本
bool StructWrapper::HasCustomTemplateText() const
{
    return !IsUnrealStruct() && !PredefStruct->CustomTemplateText.empty();
}

// 获取预定义结构体的自定义模板文本
std::string StructWrapper::GetCustomTemplateText() const
{
    assert(!IsUnrealStruct() && "StructWrapper doesn't contain PredefStruct. Illegal call to 'GetCustomTemplateText()'.");

    return PredefStruct->CustomTemplateText;
}
