#pragma once

#include <vector>
#include <unordered_map>

#include "Unreal/Enums.h"
#include "Unreal/UnrealTypes.h"

class UEClass;
class UEFField;
class UEObject;
class UEProperty;

// FFieldClass 的包装类
class UEFFieldClass
{
protected:
	// 指向 FFieldClass 实例的指针
	uint8* Class;

public:

	UEFFieldClass() = default;

	UEFFieldClass(void* NewFieldClass)
		: Class(reinterpret_cast<uint8*>(NewFieldClass))
	{
	}

	UEFFieldClass(const UEFFieldClass& OldFieldClass)
		: Class(reinterpret_cast<uint8*>(OldFieldClass.Class))
	{
	}

	// 获取底层 FFieldClass 的地址
	void* GetAddress();

	// 检查 FFieldClass 是否有效
	operator bool() const;

	// 获取字段类的ID
	EFieldClassID GetId() const;

	// 获取类的转换标志
	EClassCastFlags GetCastFlags() const;
	// 获取类的标志
	EClassFlags GetClassFlags() const;
	// 获取父类
	UEFFieldClass GetSuper() const;
	// 获取 FName
	FName GetFName() const;

	// 检查是否为指定类型
	bool IsType(EClassCastFlags Flags) const;

	// 获取名称字符串
	std::string GetName() const;
	// 获取有效的名称字符串
	std::string GetValidName() const;
	// 获取 C++ 风格的名称
	std::string GetCppName() const;
};

// FField 的包装类
class UEFField
{
protected:
	// 指向 FField 实例的指针
	uint8* Field;

public:

	UEFField() = default;

	UEFField(void* NewField)
		: Field(reinterpret_cast<uint8*>(NewField))
	{
	}

	UEFField(const UEFField& OldField)
		: Field(reinterpret_cast<uint8*>(OldField.Field))
	{
	}

	// 获取底层 FField 的地址
	void* GetAddress();

	// 获取对象标志
	EObjectFlags GetFlags() const;
	// 将所有者作为 UObject 获取
	class UEObject GetOwnerAsUObject() const;
	// 将所有者作为 FField 获取
	class UEFField GetOwnerAsFField() const;
	// 获取所有者 UObject
	class UEObject GetOwnerUObject() const;
	// 获取字段类
	UEFFieldClass GetClass() const;
	// 获取 FName
	FName GetFName() const;
	// 获取下一个字段
	UEFField GetNext() const;

	// 转换为指定的UE类型
	template<typename UEType>
	UEType Cast() const;

	// 检查所有者是否为 UObject
	bool IsOwnerUObject() const;
	// 检查是否为指定类型
	bool IsA(EClassCastFlags Flags) const;

	// 获取名称字符串
	std::string GetName() const;
	// 获取有效的名称字符串
	std::string GetValidName() const;
	// 获取 C++ 风格的名称
	std::string GetCppName() const;

	explicit operator bool() const;
	bool operator==(const UEFField& Other) const;
	bool operator!=(const UEFField& Other) const;
};

// UObject 的包装类
class UEObject
{
private:
	// ProcessEvent 函数指针
	static void(*PE)(void*, void*, void*);

protected:
	// 指向 UObject 实例的指针
	uint8* Object;

public:

	UEObject() = default;

	UEObject(void* NewObject)
		: Object(reinterpret_cast<uint8*>(NewObject))
	{
	}

	UEObject(const UEObject&) = default;

	// 获取底层 UObject 的地址
	void* GetAddress();

	// 获取虚函数表
	void* GetVft() const;
	// 获取对象标志
	EObjectFlags GetFlags() const;
	// 获取对象索引
	int32 GetIndex() const;
	// 获取对象的类
	UEClass GetClass() const;
	// 获取对象的 FName
	FName GetFName() const;
	// 获取外部对象
	UEObject GetOuter() const;

	// 获取包索引
	int32 GetPackageIndex() const;

	// 检查是否具有任何指定的标志
	bool HasAnyFlags(EObjectFlags Flags) const;

	// 检查是否为指定的类型（通过类型标志）
	bool IsA(EClassCastFlags TypeFlags) const;
	// 检查是否为指定的类型（通过类对象）
	bool IsA(UEClass Class) const;

	// 获取最外层的对象
	UEObject GetOutermost() const;

	// 将对象标志转换为字符串
	std::string StringifyObjFlags() const;

	// 获取名称
	std::string GetName() const;
	// 获取带路径的名称
	std::string GetNameWithPath() const;
	// 获取有效的名称
	std::string GetValidName() const;
	// 获取 C++ 风格的名称
	std::string GetCppName() const;
	// 获取全名
	std::string GetFullName(int32& OutNameLength) const;
	std::string GetFullName() const;
	// 获取路径名
	std::string GetPathName() const;

	explicit operator bool() const;
	explicit operator uint8* ();
	bool operator==(const UEObject& Other) const;
	bool operator!=(const UEObject& Other) const;

	// 处理事件
	void ProcessEvent(class UEFunction Func, void* Params);

public:
	// 转换为指定的UE类型
	template<typename UEType>
	inline UEType Cast()
	{
		return UEType(Object);
	}

	template<typename UEType>
	inline const UEType Cast() const
	{
		return UEType(Object);
	}
};

// UField 的包装类
class UEField : public UEObject
{
	using UEObject::UEObject;

public:
	// 获取下一个字段
	UEField GetNext() const;
	// 检查下一个字段是否有效
	bool IsNextValid() const;
};

// UEnum 的包装类
class UEEnum : public UEField
{
	using UEField::UEField;

public:
	// 获取名称-值对
	std::vector<std::pair<FName, int64>> GetNameValuePairs() const;
	// 获取单个枚举条目的名称
	std::string GetSingleName(int32 Index) const;
	// 获取带枚举前缀的名称
	std::string GetEnumPrefixedName() const;
	// 获取枚举类型的字符串表示
	std::string GetEnumTypeAsStr() const;
};

// UStruct 的包装类
class UEStruct : public UEField
{
	using UEField::UEField;

public:
	// 获取父结构
	UEStruct GetSuper() const;
	// 获取子字段
	UEField GetChild() const;
	// 获取子属性
	UEFField GetChildProperties() const;
	// 获取最小对齐方式
	int32 GetMinAlignment() const;
	// 获取结构体大小
	int32 GetStructSize() const;

	// 检查是否为指定类型或其子类
	bool HasType(UEStruct Type) const;

	// 获取所有属性
	std::vector<UEProperty> GetProperties() const;
	// 获取所有函数
	std::vector<UEFunction> GetFunctions() const;

	// 查找成员
	UEProperty FindMember(const std::string& MemberName, EClassCastFlags TypeFlags = EClassCastFlags::None) const;

	// 检查是否有成员
	bool HasMembers() const;
};

// UFunction 的包装类
class UEFunction : public UEStruct
{
	using UEStruct::UEStruct;

public:
	// 获取函数标志
	EFunctionFlags GetFunctionFlags() const;
	// 检查是否具有指定的函数标志
	bool HasFlags(EFunctionFlags Flags) const;

	// 获取函数的执行函数地址
	void* GetExecFunction() const;

	// 获取返回值属性
	UEProperty GetReturnProperty() const;

	// 将函数标志转换为字符串
	std::string StringifyFlags(const char* Seperator = ", ") const;
	// 获取参数结构体的名称
	std::string GetParamStructName() const;
};

// UClass 的包装类
class UEClass : public UEStruct
{
	using UEStruct::UEStruct;

public:
	// 获取类的转换标志
	EClassCastFlags GetCastFlags() const;
	// 将转换标志转换为字符串
	std::string StringifyCastFlags() const;
	// 检查是否为指定类型
	bool IsType(EClassCastFlags TypeFlag) const;
	// 获取默认对象（CDO）
	UEObject GetDefaultObject() const;
	// 获取实现的接口
	TArray<FImplementedInterface> GetImplementedInterfaces() const;

	// 获取指定名称的函数
	UEFunction GetFunction(const std::string& ClassName, const std::string& FuncName) const;
};

// UProperty 的包装类
class UEProperty
{
protected:
	// 指向 UProperty 实例的指针
	uint8* Base;

public:
	UEProperty() = default;
	UEProperty(const UEProperty&) = default;

	UEProperty(void* NewProperty)
		: Base(reinterpret_cast<uint8*>(NewProperty))
	{
	}

public:
	// 获取底层 UProperty 的地址
	void* GetAddress();

	// 获取属性的类
	std::pair<UEClass, UEFFieldClass> GetClass() const;
	// 获取转换标志
	EClassCastFlags GetCastFlags() const;

	operator bool() const;

	// 检查是否为指定类型
	bool IsA(EClassCastFlags TypeFlags) const;

	// 获取 FName
	FName GetFName() const;
	// 获取数组维度
	int32 GetArrayDim() const;
	// 获取属性大小
	int32 GetSize() const;
	// 获取属性偏移量
	int32 GetOffset() const;
	// 获取属性标志
	EPropertyFlags GetPropertyFlags() const;
	// 检查是否具有指定的属性标志
	bool HasPropertyFlags(EPropertyFlags PropertyFlag) const;
	// 检查是否为指定类型
	bool IsType(EClassCastFlags PossibleTypes) const;

	// 获取对齐方式
	int32 GetAlignment() const;

	// 获取名称
	std::string GetName() const;
	// 获取有效的名称
	std::string GetValidName() const;

	// 获取 C++ 类型字符串
	std::string GetCppType() const;

	// 获取属性的类名
	std::string GetPropClassName() const;

	// 将属性标志转换为字符串
	std::string StringifyFlags() const;

public:
	// 转换为指定的UE类型
	template<typename UEType>
	UEType Cast()
	{
		return UEType(Base);
	}

	template<typename UEType>
	const UEType Cast() const
	{
		return UEType(Base);
	}
};

// 字节属性
class UEByteProperty : public UEProperty
{
	using UEProperty::UEProperty;

public:
	// 获取关联的枚举
	UEEnum GetEnum() const;

	std::string GetCppType() const;
};

// 布尔属性
class UEBoolProperty : public UEProperty
{
	using UEProperty::UEProperty;

public:
	// 获取字段掩码
	uint8 GetFieldMask() const;
	// 获取位索引
	uint8 GetBitIndex() const;
	// 是否为原生布尔类型
	bool IsNativeBool() const;

	std::string GetCppType() const;
};

// 对象属性
class UEObjectProperty : public UEProperty
{
	using UEProperty::UEProperty;

public:
	// 获取属性的类
	UEClass GetPropertyClass() const;

	std::string GetCppType() const;
};

// 类属性
class UEClassProperty : public UEObjectProperty
{
	using UEObjectProperty::UEObjectProperty;

public:
	// 获取元类
	UEClass GetMetaClass() const;

	std::string GetCppType() const;
};

// 弱对象属性
class UEWeakObjectProperty : public UEObjectProperty
{
	using UEObjectProperty::UEObjectProperty;

public:
	std::string GetCppType() const;
};

// 懒加载对象属性
class UELazyObjectProperty : public UEObjectProperty
{
	using UEObjectProperty::UEObjectProperty;

public:
	std::string GetCppType() const;
};

// 软对象属性
class UESoftObjectProperty : public UEObjectProperty
{
	using UEObjectProperty::UEObjectProperty;

public:
	std::string GetCppType() const;
};

// 软类属性
class UESoftClassProperty : public UEClassProperty
{
	using UEClassProperty::UEClassProperty;

public:
	std::string GetCppType() const;
};

// 接口属性
class UEInterfaceProperty : public UEObjectProperty
{
	using UEObjectProperty::UEObjectProperty;

public:
	std::string GetCppType() const;
};

// 结构体属性
class UEStructProperty : public UEProperty
{
	using UEProperty::UEProperty;

public:
	// 获取底层的结构体
	UEStruct GetUnderlayingStruct() const;

	std::string GetCppType() const;
};

// 数组属性
class UEArrayProperty : public UEProperty
{
	using UEProperty::UEProperty;

public:
	// 获取数组内部元素的属性
	UEProperty GetInnerProperty() const;

	std::string GetCppType() const;
};

// 委托属性
class UEDelegateProperty : public UEProperty
{
	using UEProperty::UEProperty;

public:
	// 获取签名函数
	UEFunction GetSignatureFunction() const;

	std::string GetCppType() const;
};

// 多播内联委托属性
class UEMulticastInlineDelegateProperty : public UEProperty
{
	using UEProperty::UEProperty;

public:
	// 获取签名函数
	UEFunction GetSignatureFunction() const;

	std::string GetCppType() const;
};

// 映射属性
class UEMapProperty : public UEProperty
{
	using UEProperty::UEProperty;

public:
	// 获取键的属性
	UEProperty GetKeyProperty() const;
	// 获取值的属性
	UEProperty GetValueProperty() const;

	std::string GetCppType() const;
};

// 集合属性
class UESetProperty : public UEProperty
{
	using UEProperty::UEProperty;

public:
	// 获取元素的属性
	UEProperty GetElementProperty() const;

	std::string GetCppType() const;
};

// 枚举属性
class UEEnumProperty : public UEProperty
{
	using UEProperty::UEProperty;

public:
	// 获取底层属性
	UEProperty GetUnderlayingProperty() const;
	// 获取枚举
	UEEnum GetEnum() const;

	std::string GetCppType() const;
};

// 字段路径属性
class UEFieldPathProperty : public UEProperty
{
	using UEProperty::UEProperty;

public:
	// 获取字段类
	UEFFieldClass GetFielClass() const;

	std::string GetCppType() const;
};

// 可选属性
class UEOptionalProperty : public UEProperty
{
	using UEProperty::UEProperty;

public:
	// 获取值的属性
	UEProperty GetValueProperty() const;

	std::string GetCppType() const;
};