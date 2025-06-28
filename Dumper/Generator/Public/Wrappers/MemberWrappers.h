#pragma once

#include <memory>

#include "Unreal/ObjectArray.h"
#include "Managers/CollisionManager.h"
#include "Wrappers/StructWrapper.h"

// 属性包装器
class PropertyWrapper
{
private:
    union
    {
        const UEProperty Property; // 虚幻属性
        const PredefinedMember* PredefProperty; // 预定义成员
    };

    const std::shared_ptr<StructWrapper> Struct; // 结构体

    NameInfo Name; // 名称信息

    bool bIsUnrealProperty = false; // 是否为虚幻属性

public:
    PropertyWrapper(const PropertyWrapper&) = default;

    PropertyWrapper(const std::shared_ptr<StructWrapper>& Str, const PredefinedMember* Predef);

    PropertyWrapper(const std::shared_ptr<StructWrapper>& Str, UEProperty Prop);

public:
    // 获取名称
    std::string GetName() const;
    // 获取类型
    std::string GetType() const;

    // 获取名称冲突信息
    NameInfo GetNameCollisionInfo() const;

    // 是否为返回参数
    bool IsReturnParam() const;
    // 是否为虚幻属性
    bool IsUnrealProperty() const;
    // 是否为静态
    bool IsStatic() const;
    // 是否为零大小成员
    bool IsZeroSizedMember() const;

    // 是否为某种类型
    bool IsType(EClassCastFlags CombinedFlags) const;
    // 是否拥有属性标志
    bool HasPropertyFlags(EPropertyFlags Flags) const;
    // 是否为位域
    bool IsBitField() const;
    // 是否有默认值
    bool HasDefaultValue() const;

    // 获取位索引
    uint8 GetBitIndex() const;
    // 获取字段掩码
    uint8 GetFieldMask() const;
    // 获取位计数
    uint8 GetBitCount() const;

    // 获取数组维度
    int32 GetArrayDim() const;
    // 获取大小
    int32 GetSize() const;
    // 获取偏移量
    int32 GetOffset() const;
    // 获取属性标志
    EPropertyFlags GetPropertyFlags() const;

    // 获取虚幻属性
    UEProperty GetUnrealProperty() const;

    // 获取默认值
    std::string GetDefaultValue() const;

    // 字符串化标志
    std::string StringifyFlags() const;
    // 获取标志或自定义注释
    std::string GetFlagsOrCustomComment() const;
};

// 参数集合
struct ParamCollection
{
private:
    std::vector<std::pair<std::string, std::string>> TypeNamePairs; // 类型名称对

public:
    /* always exists, std::pair<"void", "+InvalidName-"> if ReturnValue is void */
    /* 始终存在，如果返回值为void，则为 std::pair<"void", "+InvalidName-"> */
    inline std::pair<std::string, std::string>& GetRetValue() { return TypeNamePairs[0]; }

    inline auto begin() { return TypeNamePairs.begin() + 1; /* skip ReturnValue */ } // 跳过返回值
    inline auto end() { return TypeNamePairs.begin() + 1; /* skip ReturnValue */ } // 跳过返回值
};

// 函数包装器
class FunctionWrapper
{
public:
    using GetTypeStringFunctionType = std::string(*)(UEProperty Param); // 获取类型字符串的函数类型

private:
    union
    {
        const UEFunction Function; // 虚幻函数
        const PredefinedFunction* PredefFunction; // 预定义函数
    };

    const std::shared_ptr<StructWrapper> Struct; // 结构体

    NameInfo Name; // 名称信息

    bool bIsUnrealFunction = false; // 是否为虚幻函数

public:
    FunctionWrapper(const std::shared_ptr<StructWrapper>& Str, const PredefinedFunction* Predef);

    FunctionWrapper(const std::shared_ptr<StructWrapper>& Str, UEFunction Func);

public:
    // 作为结构体
    StructWrapper AsStruct() const;

    // 获取名称
    std::string GetName() const;

    // 获取名称冲突信息
    NameInfo GetNameCollisionInfo() const;

    // 获取函数标志
    EFunctionFlags GetFunctionFlags() const;

    // 获取成员
    MemberManager GetMembers() const;

    // 字符串化标志
    std::string StringifyFlags(const char* Seperator = ", ") const;
    // 获取参数结构体名称
    std::string GetParamStructName() const;
    // 获取参数结构体大小
    int32 GetParamStructSize() const;

    // 获取预定义函数的自定义注释
    std::string GetPredefFunctionCustomComment() const;
    // 获取预定义函数的自定义模板文本
    std::string GetPredefFunctionCustomTemplateText() const;
    // 获取带参数的预定义函数名称
    std::string GetPredefFuncNameWithParams() const;
    // 获取C++文件中带参数的预定义函数名称
    std::string GetPredefFuncNameWithParamsForCppFile() const;
    // 获取预定义函数的返回类型
    std::string GetPredefFuncReturnType() const;
    // 获取预定义函数体
    std::string GetPredefFunctionBody() const;
    // 获取预定义函数的内联函数体
    std::string GetPredefFunctionInlineBody() const;

    // 获取执行函数偏移量
    uintptr_t GetExecFuncOffset() const;

    // 获取虚幻函数
    UEFunction GetUnrealFunction() const;

    // 是否为静态
    bool IsStatic() const;
    // 是否为常量
    bool IsConst() const;
    // 是否在接口中
    bool IsInInterface() const;
    // 是否为预定义
    bool IsPredefined() const;
    // 是否有内联函数体
    bool HasInlineBody() const;
    // 是否有自定义模板文本
    bool HasCustomTemplateText() const;
    // 是否有函数标志
    bool HasFunctionFlag(EFunctionFlags Flag) const;
};
