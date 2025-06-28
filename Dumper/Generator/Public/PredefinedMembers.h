#pragma once
#include <string>
#include <vector>
#include <unordered_map>

#include "Unreal/Enums.h"
#include "Unreal/UnrealObjects.h"

// 预定义成员
struct PredefinedMember
{
    std::string Comment; // 注释

    std::string Type; // 类型
    std::string Name; // 名称

    int32 Offset; // 偏移
    int32 Size; // 大小
    int32 ArrayDim; // 数组维度
    int32 Alignment; // 对齐

    bool bIsStatic; // 是否为静态
    bool bIsZeroSizeMember; // 是否为零大小成员

    bool bIsBitField; // 是否为位域
    uint8 BitIndex; // 位索引
    uint8 BitCount = 0x1; // 位计数

    std::string DefaultValue = std::string(); // 默认值
};

// 预定义函数
struct PredefinedFunction
{
    std::string CustomComment; // 自定义注释
    std::string CustomTemplateText = std::string(); // 自定义模板文本
    std::string ReturnType; // 返回类型
    std::string NameWithParams; // 带参数的名称
    std::string NameWithParamsWithoutDefaults = std::string(); // 不带默认值的带参数名称

    std::string Body; // 函数体

    bool bIsStatic; // 是否为静态
    bool bIsConst; // 是否为常量
    bool bIsBodyInline; // 函数体是否内联
};

// 预定义元素
struct PredefinedElements
{
    std::vector<PredefinedMember> Members; // 成员
    std::vector<PredefinedFunction> Functions; // 函数
};

// 预定义结构体
struct PredefinedStruct
{
    std::string CustomTemplateText = std::string(); // 自定义模板文本
    std::string UniqueName; // 唯一名称
    int32 Size; // 大小
    int32 Alignment; // 对齐
    bool bUseExplictAlignment; // 是否使用显式对齐
    bool bIsFinal; // 是否为final
    bool bIsClass; // 是否为类
    bool bIsUnion; // 是否为联合

    const PredefinedStruct* Super; // 父类

    std::vector<PredefinedMember> Properties; // 属性
    std::vector<PredefinedFunction> Functions; // 函数
};

/* unordered_map<StructIndex, Members/Functions> */
// 预定义成员查找映射类型
using PredefinedMemberLookupMapType = std::unordered_map<int32 /* StructIndex */, PredefinedElements /* Members/Functions */>;

// requires strict weak ordering
// 比较虚幻属性（要求严格弱序）
inline bool CompareUnrealProperties(UEProperty Left, UEProperty Right)
{
    if (Left.IsA(EClassCastFlags::BoolProperty) && Right.IsA(EClassCastFlags::BoolProperty))
    {
        if (Left.GetOffset() == Right.GetOffset())
        {
            return Left.Cast<UEBoolProperty>().GetFieldMask() < Right.Cast<UEBoolProperty>().GetFieldMask();
        }
    }

    return Left.GetOffset() < Right.GetOffset();
};

// requires strict weak ordering
// 比较预定义成员（要求严格弱序）
inline bool ComparePredefinedMembers(const PredefinedMember& Left, const PredefinedMember& Right)
{
    // if both members are static, sort lexically
    // 如果两个成员都是静态的，则按字典顺序排序
    if (Left.bIsStatic && Right.bIsStatic)
        return Left.Name < Right.Name;

    // if one member is static, return true if Left is static, false if Right
    // 如果一个成员是静态的，则如果Left是静态的，则返回true，否则返回false
    if (Left.bIsStatic || Right.bIsStatic)
        return Left.bIsStatic > Right.bIsStatic;

    return Left.Offset < Right.Offset;
};

/*
Order:
    static non-inline
    non-inline
    static inline
    inline
顺序:
    静态非内联
    非内联
    静态内联
    内联
*/

// requires strict weak ordering
// 比较虚幻函数（要求严格弱序）
inline bool CompareUnrealFunctions(UEFunction Left, UEFunction Right)
{
    const bool bIsLeftStatic = Left.HasFlags(EFunctionFlags::Static);
    const bool bIsRightStatic = Right.HasFlags(EFunctionFlags::Static);

    const bool bIsLeftConst = Left.HasFlags(EFunctionFlags::Const);
    const bool bIsRightConst = Right.HasFlags(EFunctionFlags::Const);

    // Static members come first
    // 静态成员在前
    if (bIsLeftStatic != bIsRightStatic)
        return bIsLeftStatic > bIsRightStatic;

    // Const members come last
    // 常量成员在后
    if (bIsLeftConst != bIsRightConst)
        return bIsLeftConst < bIsRightConst;

    return Left.GetIndex() < Right.GetIndex();
};

// requires strict weak ordering
// 比较预定义函数（要求严格弱序）
inline bool ComparePredefinedFunctions(const PredefinedFunction& Left, const PredefinedFunction& Right)
{
    // Non-inline members come first
    // 非内联成员在前
    if (Left.bIsBodyInline != Right.bIsBodyInline)
        return Left.bIsBodyInline < Right.bIsBodyInline;

    // Static members come first
    // 静态成员在前
    if (Left.bIsStatic != Right.bIsStatic)
        return Left.bIsStatic > Right.bIsStatic;

    // Const members come last
    // 常量成员在后
    if (Left.bIsConst != Right.bIsConst)
        return Left.bIsConst < Right.bIsConst;

    return Left.NameWithParams < Right.NameWithParams;
};


