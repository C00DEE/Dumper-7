#include "Wrappers/MemberWrappers.h"

/* ###################################################################
* 
*   PROPERTY WRAPPER
* 
* ################################################################### */

// 构造函数，用于包装一个"预定义"的成员
PropertyWrapper::PropertyWrapper(const std::shared_ptr<StructWrapper>& Str, const PredefinedMember* Predef)
    : PredefProperty(Predef), Struct(Str), Name()
{
}

// 构造函数，用于包装一个从引擎中获取的真实"UEProperty"
PropertyWrapper::PropertyWrapper(const std::shared_ptr<StructWrapper>& Str, UEProperty Prop)
    : Property(Prop), Name(MemberManager::GetNameCollisionInfo(Str->GetUnrealStruct(), Prop)), Struct(Str), bIsUnrealProperty(true)
{
}

// 获取成员的名称（处理过命名冲突）
std::string PropertyWrapper::GetName() const
{
    return bIsUnrealProperty ? MemberManager::StringifyName(Struct->GetUnrealStruct(), Name) : PredefProperty->Name;
}

// 获取预定义成员的类型字符串
std::string PropertyWrapper::GetType() const
{
    assert(!bIsUnrealProperty && "PropertyWrapper doesn't contain UnrealProperty. Illegal call to 'GetNameCollisionInfo()'.");

    return PredefProperty->Type;
}

// 获取UEProperty的名称冲突信息
NameInfo PropertyWrapper::GetNameCollisionInfo() const
{
    assert(bIsUnrealProperty && "PropertyWrapper doesn't contain UnrealProperty. Illegal call to 'GetNameCollisionInfo()'.");

    return Name;
}

// 检查属性是否是函数的返回值参数
bool PropertyWrapper::IsReturnParam() const
{
    return bIsUnrealProperty && Property.HasPropertyFlags(EPropertyFlags::ReturnParm);
}

// 获取底层的UEProperty对象
UEProperty PropertyWrapper::GetUnrealProperty() const
{
    return Property;
}

// 获取预定义成员的默认值
std::string PropertyWrapper::GetDefaultValue() const
{
    assert(!bIsUnrealProperty && "PropertyWrapper doesn't contain PredefiendMember. Illegal call to 'GetDefaultValue()'.");

    return PredefProperty->DefaultValue;
}

// 检查UEProperty是否属于某种类型（通过CastFlags）
bool PropertyWrapper::IsType(EClassCastFlags CombinedFlags) const
{
    if (!bIsUnrealProperty)
        return false;

    uint64 CastFlags = static_cast<uint64>(Property.GetCastFlags());

    return (CastFlags & static_cast<uint64>(CombinedFlags)) > 0x0;
}

// 检查UEProperty是否包含某些标志
bool PropertyWrapper::HasPropertyFlags(EPropertyFlags Flags) const
{
    if (!bIsUnrealProperty)
        return false;

    return Property.HasPropertyFlags(Flags);
}

// 检查成员是否是位域（bit-field）
bool PropertyWrapper::IsBitField() const
{
    if (bIsUnrealProperty)
        return Property.IsA(EClassCastFlags::BoolProperty) && !Property.Cast<UEBoolProperty>().IsNativeBool();

    return PredefProperty->bIsBitField;
}

// 检查预定义成员是否有默认值
bool PropertyWrapper::HasDefaultValue() const
{
    return !bIsUnrealProperty && !PredefProperty->DefaultValue.empty();
}

// 获取位域的位索引
uint8 PropertyWrapper::GetBitIndex() const
{
    assert(IsBitField() && "'GetBitIndex' was called on non-bitfield member!");

    return bIsUnrealProperty ? Property.Cast<UEBoolProperty>().GetBitIndex() : PredefProperty->BitIndex;
}

// 获取位域的位数（对于UEProperty总是1）
uint8 PropertyWrapper::GetBitCount() const
{
    assert(IsBitField() && "'GetBitSize' was called on non-bitfield member!");

    return bIsUnrealProperty ? 0x1 : PredefProperty->BitCount;
}

// 获取位域的字段掩码
uint8 PropertyWrapper::GetFieldMask() const
{
    assert(IsBitField() && "'GetFieldMask' was called on non-bitfield member!");

    return bIsUnrealProperty ? Property.Cast<UEBoolProperty>().GetFieldMask() : (1 << PredefProperty->BitIndex);
}

// 获取成员的数组维度
int32 PropertyWrapper::GetArrayDim() const
{
    return bIsUnrealProperty ? Property.GetArrayDim() : PredefProperty->ArrayDim;
}

// 获取成员的大小
int32 PropertyWrapper::GetSize() const
{
    if (bIsUnrealProperty)
    {
        UEStruct UnderlayingStruct = nullptr;

        // 如果是结构体属性，需要特殊处理，获取其包装后的大小
        if (Property.IsA(EClassCastFlags::StructProperty) && (UnderlayingStruct = Property.Cast<UEStructProperty>().GetUnderlayingStruct()))
        {
            const int32 Size = StructManager::GetInfo(UnderlayingStruct).GetSize();

            return Size > 0x0 ? Size : 0x1;
        }

        return Property.GetSize();
    }

    return PredefProperty->Size;
}

// 获取成员的偏移量
int32 PropertyWrapper::GetOffset() const
{
    return bIsUnrealProperty ? Property.GetOffset() : PredefProperty->Offset;
}

// 获取UEProperty的属性标志
EPropertyFlags PropertyWrapper::GetPropertyFlags() const
{
    return bIsUnrealProperty ? Property.GetPropertyFlags() : EPropertyFlags::None;
}

// 将属性标志转换为字符串
std::string PropertyWrapper::StringifyFlags() const
{
    return bIsUnrealProperty ? Property.StringifyFlags() : "NoFlags";
}

// 获取标志字符串或自定义注释
std::string PropertyWrapper::GetFlagsOrCustomComment() const
{
    return bIsUnrealProperty ? Property.StringifyFlags() : PredefProperty->Comment;
}

// 检查是否是真实的UEProperty
bool PropertyWrapper::IsUnrealProperty() const
{
    return bIsUnrealProperty;
}

// 检查是否是静态成员（仅预定义成员可以是）
bool PropertyWrapper::IsStatic() const
{
    return bIsUnrealProperty ? false : PredefProperty->bIsStatic;
}

// 检查是否是零大小成员（仅预定义成员可以是）
bool PropertyWrapper::IsZeroSizedMember() const
{
    return bIsUnrealProperty ? false : PredefProperty->bIsZeroSizeMember;
}


/* ###################################################################
*
*   FUNCTION WRAPPER
*
* ################################################################### */

// 构造函数，用于包装一个"预定义"的函数
FunctionWrapper::FunctionWrapper(const std::shared_ptr<StructWrapper>& Str, const PredefinedFunction* Predef)
    : PredefFunction(Predef), Struct(Str), Name()
{
}

// 构造函数，用于包装一个从引擎中获取的真实"UEFunction"
FunctionWrapper::FunctionWrapper(const std::shared_ptr<StructWrapper>& Str, UEFunction Func)
    : Function(Func), Name(Str ? MemberManager::GetNameCollisionInfo(Str->GetUnrealStruct(), Func) : NameInfo()), Struct(Str), bIsUnrealFunction(true)
{
}

// 将函数本身作为一个StructWrapper返回（因为UEFunction继承自UEStruct）
StructWrapper FunctionWrapper::AsStruct() const
{
    return StructWrapper(bIsUnrealFunction ? Function : nullptr);
}

// 获取函数的名称
std::string FunctionWrapper::GetName() const
{
    if (bIsUnrealFunction)
    {
        if (Struct) [[likely]]
            return MemberManager::StringifyName(Struct->GetUnrealStruct(), Name);

        return Function.GetValidName();
    }

    // 从 "FunctionName(params...)" 中提取 "FunctionName"
    return PredefFunction->NameWithParams.substr(0, PredefFunction->NameWithParams.find_first_of('('));
}

// 获取UEFunction的名称冲突信息
NameInfo FunctionWrapper::GetNameCollisionInfo() const
{
    assert(bIsUnrealFunction && "FunctionWrapper doesn't contain UnrealFunction. Illegal call to 'GetNameCollisionInfo()'.");

    return Name;
}

// 获取UEFunction的函数标志
EFunctionFlags FunctionWrapper::GetFunctionFlags() const
{
    return bIsUnrealFunction ? Function.GetFunctionFlags() : EFunctionFlags::None;
}

// 获取此函数的成员管理器（用于访问其参数）
MemberManager FunctionWrapper::GetMembers() const
{
    assert(bIsUnrealFunction && "FunctionWrapper doesn't contain UnrealFunction. Illegal call to 'GetMembers()'.");

    return MemberManager(Function);
}

// 将函数标志转换为字符串
std::string FunctionWrapper::StringifyFlags(const char* Seperator) const
{
    return bIsUnrealFunction ? Function.StringifyFlags(Seperator) : "NoFlags";
}

// 获取用于存放函数参数的结构体的名称
std::string FunctionWrapper::GetParamStructName() const
{
    assert(bIsUnrealFunction && "FunctionWrapper doesn't contain UnrealFunction. Illegal call to 'GetParamStructName()'.");

    return Struct->GetName() + "_" + GetName();
}

// 获取参数结构体的大小
int32  FunctionWrapper::GetParamStructSize() const
{
    return bIsUnrealFunction ? Function.GetStructSize() : 0x0;
}

// 获取预定义函数的自定义注释
std::string FunctionWrapper::GetPredefFunctionCustomComment() const
{
    assert(!bIsUnrealFunction && "FunctionWrapper doesn't contain PredefinedFunction. Illegal call to 'GetPredefFunctionCustomComment()'.");

    return PredefFunction->CustomComment;
}

// 获取预定义函数的自定义模板文本
std::string FunctionWrapper::GetPredefFunctionCustomTemplateText() const
{
    assert(!bIsUnrealFunction && "FunctionWrapper doesn't contain PredefinedFunction. Illegal call to 'GetPredefFunctionCustomTemplateText()'.");

    return PredefFunction->CustomTemplateText;
}

// 获取预定义函数的带参数的完整名称
std::string FunctionWrapper::GetPredefFuncNameWithParams() const
{
    assert(!bIsUnrealFunction && "FunctionWrapper doesn't contain PredefinedFunction. Illegal call to 'GetPredefFuncNameWithParams()'.");

    return PredefFunction->NameWithParams;
}

// 获取用于生成.cpp文件的带参数的完整名称
std::string FunctionWrapper::GetPredefFuncNameWithParamsForCppFile() const
{
    assert(!bIsUnrealFunction && "FunctionWrapper doesn't contain PredefinedFunction. Illegal call to 'GetPredefFuncNameWithParamsForCppFile()'.");

    if (PredefFunction->NameWithParamsForCppFile.empty())
        return PredefFunction->NameWithParams;

    return PredefFunction->NameWithParamsForCppFile;
}

// 获取预定义函数的返回类型
std::string FunctionWrapper::GetPredefFuncReturnType() const
{
    assert(!bIsUnrealFunction && "FunctionWrapper doesn't contain PredefinedFunction. Illegal call to 'GetPredefFuncReturnType()'.");

    return PredefFunction->ReturnType;
}

// 获取预定义函数的主体（实现代码）
std::string FunctionWrapper::GetPredefFuncBody() const
{
    assert(!bIsUnrealFunction && "FunctionWrapper doesn't contain PredefinedFunction. Illegal call to 'GetPredefFuncBody()'.");

    return PredefFunction->Body;
}

// 检查UEFunction是否包含某些标志
bool FunctionWrapper::HasFunctionFlag(EFunctionFlags Flag) const
{
    return bIsUnrealFunction && Function.HasFlags(Flag);
}

// 检查是否是静态函数
bool FunctionWrapper::IsStatic() const
{
    return bIsUnrealFunction ? Function.HasFlags(EFunctionFlags::Static) : PredefFunction->bIsStatic;
}

// 检查是否是常量函数
bool FunctionWrapper::IsConst() const
{
    return bIsUnrealFunction ? Function.HasFlags(EFunctionFlags::Const) : PredefFunction->bIsConst;
}

// 检查是否是预定义的函数
bool FunctionWrapper::IsPredefined() const
{
    return !bIsUnrealFunction;
}

// 检查是否是真实的UEFunction
bool FunctionWrapper::IsUnrealFunction() const
{
    return bIsUnrealFunction;
}

