#include "Wrappers/EnumWrapper.h"
#include "Managers/EnumManager.h"

// 构造函数：接收一个UEEnum对象，并通过EnumManager获取其附加信息
EnumWrapper::EnumWrapper(const UEEnum Enm)
    : Enum(Enm), InfoHandle(EnumManager::GetInfo(Enm))
{
}

// 获取底层的UEEnum对象
UEEnum EnumWrapper::GetUnrealEnum() const
{
    return Enum;
}

// 获取带"E"前缀的枚举名称，例如："EEnumType"
std::string EnumWrapper::GetName() const
{
    return Enum.GetEnumPrefixedName();
}

// 获取原始的枚举名称，例如："EnumType"
std::string EnumWrapper::GetRawName() const
{
    return Enum.GetName();
}

// 获取完整的枚举名称，通常包含路径
std::string EnumWrapper::GetFullName() const
{
    return Enum.GetFullName();
}

// 获取经过冲突处理后的唯一名称
std::pair<std::string, bool> EnumWrapper::GetUniqueName() const
{
    const StringEntry& Name = InfoHandle.GetName();

    return { Name.GetName(), Name.IsUnique() };
}

// 获取枚举的底层类型大小（如uint8, uint16等）
uint8 EnumWrapper::GetUnderlyingTypeSize() const
{
    return InfoHandle.GetUnderlyingTypeSize();
}

// 获取枚举成员的数量
int32 EnumWrapper::GetNumMembers() const
{
    return InfoHandle.GetNumMembers();
}

// 获取用于遍历成员的迭代器（已处理命名冲突）
CollisionInfoIterator EnumWrapper::GetMembers() const
{
    return InfoHandle.GetMemberCollisionInfoIterator();
}

// 检查包装的UEEnum对象是否有效
bool EnumWrapper::IsValid() const
{
    return Enum != nullptr;
}

