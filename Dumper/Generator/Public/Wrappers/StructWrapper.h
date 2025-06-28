#pragma once

#include "Unreal/ObjectArray.h"

#include "Managers/CollisionManager.h"
#include "Managers/StructManager.h"
#include "Managers/MemberManager.h"

#include "PredefinedMembers.h"


// 结构体包装器
class StructWrapper
{
private:
    friend class PropertyWrapper;
    friend class FunctionWrapper;

private:
    union
    {
        const UEStruct Struct; // 虚幻结构体
        const PredefinedStruct* PredefStruct; // 预定义结构体
    };

    StructInfoHandle InfoHandle; // 结构体信息句柄

    bool bIsUnrealStruct = false; // 是否为虚幻结构体

public:
    StructWrapper(const PredefinedStruct* const Predef);

    StructWrapper(const UEStruct Str);

public:
    // 获取虚幻结构体
    UEStruct GetUnrealStruct() const;

public:
    // 获取名称
    std::string GetName() const;
    // 获取原始名称
    std::string GetRawName() const;
    // 获取完整名称
    std::string GetFullName() const;
    // 获取父级结构体
    StructWrapper GetSuper() const;

    /* Name, bIsUnique */
    /* 名称，是否唯一 */
    std::pair<std::string, bool> GetUniqueName() const;
    // 获取最后一个成员的末尾位置
    int32 GetLastMemberEnd() const;
    // 获取对齐方式
    int32 GetAlignment() const;
    // 获取大小
    int32 GetSize() const;
    // 获取未对齐的大小
    int32 GetUnalignedSize() const;

    // 是否应使用显式对齐
    bool ShouldUseExplicitAlignment() const;
    // 是否重用了尾部填充
    bool HasReusedTrailingPadding() const;
    // 是否为最终
    bool IsFinal() const;

    // 是否为类
    bool IsClass() const;
    // 是否为联合体
    bool IsUnion() const;
    // 是否为函数
    bool IsFunction() const;
    // 是否为接口
    bool IsInterface() const;

    // 是否为某种类型的类
    bool IsAClassWithType(UEClass TypeClass) const;

    // 是否有效
    bool IsValid() const;
    // 是否为虚幻结构体
    bool IsUnrealStruct() const;

    // 是否与包存在循环依赖
    bool IsCyclicWithPackage(int32 PackageIndex) const;

    // 是否有自定义模板文本
    bool HasCustomTemplateText() const;
    // 获取自定义模板文本
    std::string GetCustomTemplateText() const;

    // 获取成员
    MemberManager GetMembers() const;
};
