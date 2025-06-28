#pragma once

#include "Managers/EnumManager.h"


// 枚举包装器
class EnumWrapper
{
private:
    const UEEnum Enum; // Unreal引擎的枚举
    EnumInfoHandle InfoHandle; // 枚举信息句柄

public:
    EnumWrapper(const UEEnum Enm);

public:
    // 获取Unreal引擎的枚举
    UEEnum GetUnrealEnum() const;

public:
    // 获取名称
    std::string GetName() const;
    // 获取原始名称
    std::string GetRawName() const;
    // 获取完整名称
    std::string GetFullName() const;

    // 获取唯一名称
    std::pair<std::string, bool> GetUniqueName() const;
    // 获取底层类型的大小
    uint8 GetUnderlyingTypeSize() const;

    // 获取成员数量
    int32 GetNumMembers() const;

    // 获取成员
    CollisionInfoIterator GetMembers() const;

    // 是否有效
    bool IsValid() const;
};
