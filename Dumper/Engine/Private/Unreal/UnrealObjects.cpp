#include <format>

#include "Unreal/UnrealObjects.h"
#include "Unreal/ObjectArray.h"
#include "OffsetFinder/Offsets.h"


/// 获取 FFieldClass 的地址
void* UEFFieldClass::GetAddress()
{
	return Class;
}

/// 检查 FFieldClass 是否有效
UEFFieldClass::operator bool() const
{
	return Class != nullptr;
}

/// 获取字段类的 ID
EFieldClassID UEFFieldClass::GetId() const
{
	return *reinterpret_cast<EFieldClassID*>(Class + Off::FFieldClass::Id);
}

/// 获取类的转换标志
EClassCastFlags UEFFieldClass::GetCastFlags() const
{
	return *reinterpret_cast<EClassCastFlags*>(Class + Off::FFieldClass::CastFlags);
}

/// 获取类的标志
EClassFlags UEFFieldClass::GetClassFlags() const
{
	return *reinterpret_cast<EClassFlags*>(Class + Off::FFieldClass::ClassFlags);
}

/// 获取父类
UEFFieldClass UEFFieldClass::GetSuper() const
{
	return UEFFieldClass(*reinterpret_cast<void**>(Class + Off::FFieldClass::SuperClass));
}

/// 获取 FName
FName UEFFieldClass::GetFName() const
{
	return FName(Class + Off::FFieldClass::Name); //Not the real FName, but a wrapper which holds the address of a FName
}

/// 检查是否为指定类型
bool UEFFieldClass::IsType(EClassCastFlags Flags) const
{
	return (Flags != EClassCastFlags::None ? (GetCastFlags() & Flags) : true);
}

/// 获取名称
std::string UEFFieldClass::GetName() const
{
	return Class ? GetFName().ToString() : "None";
}

/// 获取有效的名称（用于 C++ 标识符）
std::string UEFFieldClass::GetValidName() const
{
	return Class ? GetFName().ToValidString() : "None";
}

/// 获取 C++ 风格的名称
std::string UEFFieldClass::GetCppName() const
{
	// This is evile dark magic code which shouldn't exist
	return "F" + GetValidName();
}

/// 获取 FField 的地址
void* UEFField::GetAddress()
{
	return Field;
}

/// 获取对象标志
EObjectFlags UEFField::GetFlags() const
{
	return *reinterpret_cast<EObjectFlags*>(Field + Off::FField::Flags);
}

/// 获取所有者，如果所有者是 UObject
class UEObject UEFField::GetOwnerAsUObject() const
{
	if (IsOwnerUObject())
	{
		if (Settings::Internal::bUseMaskForFieldOwner)
			return (void*)(*reinterpret_cast<uintptr_t*>(Field + Off::FField::Owner) & ~0x1ull);

		return *reinterpret_cast<void**>(Field + Off::FField::Owner);
	}

	return nullptr;
}

/// 获取所有者，如果所有者是 FField
class UEFField UEFField::GetOwnerAsFField() const
{
	if (!IsOwnerUObject())
		return *reinterpret_cast<void**>(Field + Off::FField::Owner);

	return nullptr;
}

/// 获取最顶层的 UObject 所有者
class UEObject UEFField::GetOwnerUObject() const
{
	UEFField Field = *this;

	while (!Field.IsOwnerUObject() && Field.GetOwnerAsFField())
	{
		Field = Field.GetOwnerAsFField();
	}

	return Field.GetOwnerAsUObject();
}

/// 获取字段的类
UEFFieldClass UEFField::GetClass() const
{
	return UEFFieldClass(*reinterpret_cast<void**>(Field + Off::FField::Class));
}

/// 获取 FName
FName UEFField::GetFName() const
{
	return FName(Field + Off::FField::Name); //Not the real FName, but a wrapper which holds the address of a FName
}

/// 获取下一个字段
UEFField UEFField::GetNext() const
{
	return UEFField(*reinterpret_cast<void**>(Field + Off::FField::Next));
}

/// 将字段转换为指定类型
template<typename UEType>
UEType UEFField::Cast() const
{
	return UEType(Field);
}

/// 检查所有者是否为 UObject
bool UEFField::IsOwnerUObject() const
{
	if (Settings::Internal::bUseMaskForFieldOwner)
	{
		return *reinterpret_cast<uintptr_t*>(Field + Off::FField::Owner) & 0x1;
	}

	return *reinterpret_cast<bool*>(Field + Off::FField::Owner + 0x8);
}

/// 检查是否为指定类型
bool UEFField::IsA(EClassCastFlags Flags) const
{
	return (Flags != EClassCastFlags::None ? GetClass().IsType(Flags) : true);
}

/// 获取名称
std::string UEFField::GetName() const
{
	return Field ? GetFName().ToString() : "None";
}

/// 获取有效的名称（用于 C++ 标识符）
std::string UEFField::GetValidName() const
{
	return Field ? GetFName().ToValidString() : "None";
}

/// 获取 C++ 风格的名称
std::string UEFField::GetCppName() const
{
	static UEClass ActorClass = ObjectArray::FindClassFast("Actor");
	static UEClass InterfaceClass = ObjectArray::FindClassFast("Interface");

	std::string Temp = GetValidName();

	if (IsA(EClassCastFlags::Class))
	{
		if (Cast<UEClass>().HasType(ActorClass))
		{
			return 'A' + Temp;
		}
		else if (Cast<UEClass>().HasType(InterfaceClass))
		{
			return 'I' + Temp;
		}

		return 'U' + Temp;
	}

	return 'F' + Temp;
}

/// 检查字段是否有效
UEFField::operator bool() const
{
	return Field != nullptr && reinterpret_cast<void*>(Field + Off::FField::Class) != nullptr;
}

/// 比较两个 UEFField 是否相等
bool UEFField::operator==(const UEFField& Other) const
{
	return Field == Other.Field;
}

/// 比较两个 UEFField 是否不相等
bool UEFField::operator!=(const UEFField& Other) const
{
	return Field != Other.Field;
}

/// ProcessEvent 函数指针
void(*UEObject::PE)(void*, void*, void*) = nullptr;

/// 获取 UObject 的地址
void* UEObject::GetAddress()
{
	return Object;
}

/// 获取虚函数表 (VFT)
void* UEObject::GetVft() const
{
	return *reinterpret_cast<void**>(Object);
}

/// 获取对象标志
EObjectFlags UEObject::GetFlags() const
{
	return *reinterpret_cast<EObjectFlags*>(Object + Off::UObject::Flags);
}

/// 获取对象索引
int32 UEObject::GetIndex() const
{
	return *reinterpret_cast<int32*>(Object + Off::UObject::Index);
}

/// 获取对象的类
UEClass UEObject::GetClass() const
{
	return UEClass(*reinterpret_cast<void**>(Object + Off::UObject::Class));
}

/// 获取 FName
FName UEObject::GetFName() const
{
	return FName(Object + Off::UObject::Name); //Not the real FName, but a wrapper which holds the address of a FName
}

/// 获取外部对象
UEObject UEObject::GetOuter() const
{
	return UEObject(*reinterpret_cast<void**>(Object + Off::UObject::Outer));
}

/// 获取包索引
int32 UEObject::GetPackageIndex() const
{
	return GetOutermost().GetIndex();
}

/// 检查是否包含任何指定的标志
bool UEObject::HasAnyFlags(EObjectFlags Flags) const
{
	return GetFlags() & Flags;
}

/// 通过类型标志检查是否为指定类型
bool UEObject::IsA(EClassCastFlags TypeFlags) const
{
	return (TypeFlags != EClassCastFlags::None ? GetClass().IsType(TypeFlags) : true);
}

/// 通过类对象检查是否为指定类型
bool UEObject::IsA(UEClass Class) const
{
	if (!Class)
		return false;

	for (UEClass Clss = GetClass(); Clss; Clss = Clss.GetSuper().Cast<UEClass>())
	{
		if (Clss == Class)
			return true;
	}

	return false;
}

/// 获取最外层的对象
UEObject UEObject::GetOutermost() const
{
	UEObject Outermost = *this;

	for (UEObject Outer = *this; Outer; Outer = Outer.GetOuter())
	{
		Outermost = Outer;
	}

	return Outermost;
}

/// 将对象标志转换为字符串
std::string UEObject::StringifyObjFlags() const
{
	return *this ? StringifyObjectFlags(GetFlags()) : "NoFlags";
}

/// 获取名称
std::string UEObject::GetName() const
{
	return Object ? GetFName().ToString() : "None";
}

/// 获取带路径的名称
std::string UEObject::GetNameWithPath() const
{
	return Object ? GetFName().ToRawString() : "None";
}

/// 获取有效的名称（用于 C++ 标识符）
std::string UEObject::GetValidName() const
{
	return Object ? GetFName().ToValidString() : "None";
}

/// 获取 C++ 风格的名称
std::string UEObject::GetCppName() const
{
	static UEClass ActorClass = nullptr;
	static UEClass InterfaceClass = nullptr;

	if (ActorClass == nullptr)
		ActorClass = ObjectArray::FindClassFast("Actor");

	if (InterfaceClass == nullptr)
		InterfaceClass = ObjectArray::FindClassFast("Interface");

	std::string Temp = GetValidName();

	if (IsA(EClassCastFlags::Class))
	{
		if (Cast<UEClass>().HasType(ActorClass))
		{
			return 'A' + Temp;
		}
		else if (Cast<UEClass>().HasType(InterfaceClass))
		{
			return 'I' + Temp;
		}

		return 'U' + Temp;
	}

	return 'F' + Temp;
}

/// 获取带类型和路径的全名，并输出名称长度
std::string UEObject::GetFullName(int32& OutNameLength) const
{
	if (*this)
	{
		std::string Temp;

		for (UEObject Outer = GetOuter(); Outer; Outer = Outer.GetOuter())
		{
			Temp = Outer.GetName() + '.' + Temp;
		}

		std::string Name = GetName();
		OutNameLength = Name.size() + 1;

		Name = GetClass().GetName() + ' ' + Temp + Name;

		return Name;
	}

	return "None";
}

/// 获取带类型和路径的全名
std::string UEObject::GetFullName() const
{
	if (*this)
	{
		std::string Temp;

		for (UEObject Outer = GetOuter(); Outer; Outer = Outer.GetOuter())
		{
			Temp = Outer.GetName() + "." + Temp;
		}

		std::string Name = GetClass().GetName();
		Name += " ";
		Name += Temp;
		Name += GetName();

		return Name;
	}

	return "None";
}

/// 获取路径名
std::string UEObject::GetPathName() const
{
	if (*this)
	{
		std::string Temp;

		for (UEObject Outer = GetOuter(); Outer; Outer = Outer.GetOuter())
		{
			Temp = Outer.GetNameWithPath() + "." + Temp;
		}

		std::string Name = GetClass().GetNameWithPath();
		Name += " ";
		Name += Temp;
		Name += GetNameWithPath();

		return Name;
	}

	return "None";
}


/// 检查对象是否有效
UEObject::operator bool() const
{
	// if an object is 0x10000F000 it passes the nullptr check
	return Object != nullptr && reinterpret_cast<void*>(Object + Off::UObject::Class) != nullptr;
}

/// 转换为 uint8*
UEObject::operator uint8* ()
{
	return Object;
}

/// 比较两个 UObject 是否相等
bool UEObject::operator==(const UEObject& Other) const
{
	return Object == Other.Object;
}

/// 比较两个 UObject 是否不相等
bool UEObject::operator!=(const UEObject& Other) const
{
	return Object != Other.Object;
}

/// 处理事件
void UEObject::ProcessEvent(UEFunction Func, void* Params)
{
	void** VFT = *reinterpret_cast<void***>(GetAddress());

	void(*Prd)(void*, void*, void*) = decltype(Prd)(VFT[Off::InSDK::ProcessEvent::PEIndex]);

	Prd(Object, Func.GetAddress(), Params);
}

/// 获取下一个字段
UEField UEField::GetNext() const
{
	return UEField(*reinterpret_cast<void**>(Object + Off::UField::Next));
}

/// 检查下一个字段是否有效
bool UEField::IsNextValid() const
{
	return (bool)GetNext();
}

/// 获取枚举的名称和值对
std::vector<std::pair<FName, int64>> UEEnum::GetNameValuePairs() const
{
	struct alignas(0x4) Name08Byte { uint8 Pad[0x08]; };
	struct alignas(0x4) Name16Byte { uint8 Pad[0x10]; };
	struct alignas(0x4) UInt8As64  { uint8 Bytes[0x8]; inline operator int64() const { return Bytes[0]; }; };

	/// 根据索引获取名称-值对
	static auto GetNameValuePairsWithIndex = []<typename NameType, typename ValueType>(const TArray<TPair<NameType, ValueType>>& EnumNameValuePairs)
	{
		std::vector<std::pair<FName, int64>> Ret;

		for (int i = 0; i < EnumNameValuePairs.Num(); i++)
		{
			Ret.push_back({ FName(&EnumNameValuePairs[i].First), EnumNameValuePairs[i].Second });
		}

		return Ret;
	};

	/// 获取名称-值对（值为索引）
	static auto GetNameValuePairs = []<typename NameType>(const TArray<NameType>& EnumNameValuePairs)
	{
		std::vector<std::pair<FName, int64>> Ret;

		for (int i = 0; i < EnumNameValuePairs.Num(); i++)
		{
			Ret.push_back({ FName(&EnumNameValuePairs[i]), i });
		}

		return Ret;
	};


	if constexpr (Settings::EngineCore::bCheckEnumNamesInUEnum)
	{
		/// 如果开发人员嗑药了，就设置 IsNamesOnly
		static auto SetIsNamesOnlyIfDevsTookCrack = [&]<typename NameType>(const TArray<TPair<NameType, UInt8As64>>& EnumNames)
		{
			/* This is a hacky workaround for UEnum::Names which somtimes store the enum-value and sometimes don't. I've seem much of UE, but what drugs did some devs take???? */
			Settings::Internal::bIsEnumNameOnly = EnumNames[0].Second != 0 || EnumNames[1].Second != 1;
		};

		if (Settings::Internal::bUseCasePreservingName)
		{
			SetIsNamesOnlyIfDevsTookCrack(*reinterpret_cast<TArray<TPair<Name16Byte, UInt8As64>>*>(Object + Off::UEnum::Names));
		}
		else
		{
			SetIsNamesOnlyIfDevsTookCrack(*reinterpret_cast<TArray<TPair<Name08Byte, UInt8As64>>*>(Object + Off::UEnum::Names));
		}
	}

	if (Settings::Internal::bIsEnumNameOnly)
	{
		if (Settings::Internal::bUseCasePreservingName)
			return GetNameValuePairs(*reinterpret_cast<TArray<Name16Byte>*>(Object + Off::UEnum::Names));
		
		return GetNameValuePairs(*reinterpret_cast<TArray<Name08Byte>*>(Object + Off::UEnum::Names));
	}
	else
	{
		/* This only applies very very rarely on weir UE4.13 or UE4.14 games where the devs didn't know what they were doing. */
		if (Settings::Internal::bIsSmallEnumValue)
		{
			if (Settings::Internal::bUseCasePreservingName)
				return GetNameValuePairsWithIndex(*reinterpret_cast<TArray<TPair<Name16Byte, UInt8As64>>*>(Object + Off::UEnum::Names));

			return GetNameValuePairsWithIndex(*reinterpret_cast<TArray<TPair<Name08Byte, UInt8As64>>*>(Object + Off::UEnum::Names));
		}

		if (Settings::Internal::bUseCasePreservingName)
			return GetNameValuePairsWithIndex(*reinterpret_cast<TArray<TPair<Name16Byte, int64>>*>(Object + Off::UEnum::Names));
		
		return GetNameValuePairsWithIndex(*reinterpret_cast<TArray<TPair<Name08Byte, int64>>*>(Object + Off::UEnum::Names));
	}
}

/// 根据索引获取单个枚举名称
std::string UEEnum::GetSingleName(int32 Index) const
{
	return GetNameValuePairs()[Index].first.ToString();
}

/// 获取带 'E' 前缀的枚举名称
std::string UEEnum::GetEnumPrefixedName() const
{
	std::string Temp = GetValidName();

	return Temp[0] == 'E' ? Temp : 'E' + Temp;
}

/// 获取枚举类型的字符串表示
std::string UEEnum::GetEnumTypeAsStr() const
{
	return "enum class " + GetEnumPrefixedName();
}

/// 获取父结构体
UEStruct UEStruct::GetSuper() const
{
	return UEStruct(*reinterpret_cast<void**>(Object + Off::UStruct::SuperStruct));
}

/// 获取子字段
UEField UEStruct::GetChild() const
{
	return UEField(*reinterpret_cast<void**>(Object + Off::UStruct::Children));
}

/// 获取子属性
UEFField UEStruct::GetChildProperties() const
{
	return UEFField(*reinterpret_cast<void**>(Object + Off::UStruct::ChildProperties));
}

/// 获取最小对齐方式
int32 UEStruct::GetMinAlignment() const
{
	return *reinterpret_cast<int32*>(Object + Off::UStruct::MinAlignemnt);
}

/// 获取结构体大小
int32 UEStruct::GetStructSize() const
{
	return *reinterpret_cast<int32*>(Object + Off::UStruct::Size);
}

/// 检查是否为指定类型
bool UEStruct::HasType(UEStruct Type) const
{
	if (Type == nullptr)
		return false;

	for (UEStruct S = *this; S; S = S.GetSuper())
	{
		if (S == Type)
			return true;
	}

	return false;
}

/// 获取所有属性
std::vector<UEProperty> UEStruct::GetProperties() const
{
	std::vector<UEProperty> Properties;

	if (Settings::Internal::bUseFProperty)
	{
		for (UEFField Field = GetChildProperties(); Field; Field = Field.GetNext())
		{
			if (Field.IsA(EClassCastFlags::Property))
				Properties.push_back(Field.Cast<UEProperty>());
		}

		return Properties;
	}

	for (UEField Field = GetChild(); Field; Field = Field.GetNext())
	{
		if (Field.IsA(EClassCastFlags::Property))
			Properties.push_back(Field.Cast<UEProperty>());
	}

	return Properties;
}

/// 获取所有函数
std::vector<UEFunction> UEStruct::GetFunctions() const
{
	std::vector<UEFunction> Functions;

	for (UEField Field = GetChild(); Field; Field = Field.GetNext())
	{
		if (Field.IsA(EClassCastFlags::Function))
			Functions.push_back(Field.Cast<UEFunction>());
	}

	return Functions;
}

/// 查找成员
UEProperty UEStruct::FindMember(const std::string& MemberName, EClassCastFlags TypeFlags) const
{
	if (!Object)
		return nullptr;

	if (Settings::Internal::bUseFProperty)
	{
		for (UEFField Field = GetChildProperties(); Field; Field = Field.GetNext())
		{
			if (Field.IsA(TypeFlags) && Field.GetName() == MemberName)
			{
				return Field.Cast<UEProperty>();
			}
		}
	}

	for (UEField Field = GetChild(); Field; Field = Field.GetNext())
	{
		if (Field.IsA(TypeFlags) && Field.GetName() == MemberName)
		{
			return Field.Cast<UEProperty>();
		}
	}

	return nullptr;
}

/// 检查是否有成员
bool UEStruct::HasMembers() const
{
	if (!Object)
		return false;

	if (Settings::Internal::bUseFProperty)
	{
		for (UEFField Field = GetChildProperties(); Field; Field = Field.GetNext())
		{
			if (Field.IsA(EClassCastFlags::Property))
				return true;
		}
	}
	else
	{
		for (UEField F = GetChild(); F; F = F.GetNext())
		{
			if (F.IsA(EClassCastFlags::Property))
				return true;
		}
	}

	return false;
}

/// 获取类的转换标志
EClassCastFlags UEClass::GetCastFlags() const
{
	return *reinterpret_cast<EClassCastFlags*>(Object + Off::UClass::CastFlags);
}

/// 将转换标志转换为字符串
std::string UEClass::StringifyCastFlags() const
{
	return StringifyClassCastFlags(GetCastFlags());
}

/// 检查是否为指定类型
bool UEClass::IsType(EClassCastFlags TypeFlag) const
{
	return (TypeFlag != EClassCastFlags::None ? (GetCastFlags() & TypeFlag) : true);
}

/// 获取默认对象
UEObject UEClass::GetDefaultObject() const
{
	return UEObject(*reinterpret_cast<void**>(Object + Off::UClass::ClassDefaultObject));
}

/// 获取已实现的接口
TArray<FImplementedInterface> UEClass::GetImplementedInterfaces() const
{
	return *reinterpret_cast<TArray<FImplementedInterface>*>(Object + Off::UClass::ImplementedInterfaces);
}

/// 获取函数
UEFunction UEClass::GetFunction(const std::string& ClassName, const std::string& FuncName) const
{
	for (UEStruct Struct = *this; Struct; Struct = Struct.GetSuper())
	{
		if (Struct.GetName() != ClassName)
			continue;

		for (UEField Field = Struct.GetChild(); Field; Field = Field.GetNext())
		{
			if (Field.IsA(EClassCastFlags::Function) && Field.GetName() == FuncName)
			{
				return Field.Cast<UEFunction>();
			}
		}

	}

	return nullptr;
}

/// 获取函数标志
EFunctionFlags UEFunction::GetFunctionFlags() const
{
	return *reinterpret_cast<EFunctionFlags*>(Object + Off::UFunction::FunctionFlags);
}

/// 检查是否包含指定的函数标志
bool UEFunction::HasFlags(EFunctionFlags FuncFlags) const
{
	return GetFunctionFlags() & FuncFlags;
}

/// 获取函数的执行函数
void* UEFunction::GetExecFunction() const
{
	return *reinterpret_cast<void**>(Object + Off::UFunction::ExecFunction);
}

/// 获取返回属性
UEProperty UEFunction::GetReturnProperty() const
{
	for (auto Prop : GetProperties())
	{
		if (Prop.HasPropertyFlags(EPropertyFlags::ReturnParm))
			return Prop;
	}

	return nullptr;
}


/// 将函数标志转换为字符串
std::string UEFunction::StringifyFlags(const char* Seperator)  const
{
	return StringifyFunctionFlags(GetFunctionFlags(), Seperator);
}

/// 获取参数结构体名称
std::string UEFunction::GetParamStructName() const
{
	return GetOuter().GetCppName() + "_" + GetValidName() + "_Params";
}

/// 获取属性的地址
void* UEProperty::GetAddress()
{
	return Base;
}

/// 获取属性的类
std::pair<UEClass, UEFFieldClass> UEProperty::GetClass() const
{
	if (Settings::Internal::bUseFProperty)
	{
		return { UEClass(0), UEFField(Base).GetClass() };
	}

	return { UEObject(Base).GetClass(), UEFFieldClass(0) };
}

/// 获取属性的转换标志
EClassCastFlags UEProperty::GetCastFlags() const
{
	auto [Class, FieldClass] = GetClass();

	return Class ? Class.GetCastFlags() : FieldClass.GetCastFlags();
}

/// 检查属性是否有效
UEProperty::operator bool() const
{
	return Base != nullptr && ((Base + Off::UObject::Class) != nullptr || (Base + Off::FField::Class) != nullptr);
}


/// 检查是否为指定类型
bool UEProperty::IsA(EClassCastFlags TypeFlags) const
{
	if (GetClass().first)
		return GetClass().first.IsType(TypeFlags);

	return GetClass().second.IsType(TypeFlags);
}

/// 获取 FName
FName UEProperty::GetFName() const
{
	if (Settings::Internal::bUseFProperty)
	{
		return FName(Base + Off::FField::Name); //Not the real FName, but a wrapper which holds the address of a FName
	}

	return FName(Base + Off::UObject::Name); //Not the real FName, but a wrapper which holds the address of a FName
}

/// 获取数组维度
int32 UEProperty::GetArrayDim() const
{
	return *reinterpret_cast<int32*>(Base + Off::Property::ArrayDim);
}

/// 获取属性大小
int32 UEProperty::GetSize() const
{
	return *reinterpret_cast<int32*>(Base + Off::Property::ElementSize);
}

/// 获取偏移量
int32 UEProperty::GetOffset() const
{
	return *reinterpret_cast<int32*>(Base + Off::Property::Offset_Internal);
}

/// 获取属性标志
EPropertyFlags UEProperty::GetPropertyFlags() const
{
	return *reinterpret_cast<EPropertyFlags*>(Base + Off::Property::PropertyFlags);
}

/// 检查是否包含指定的属性标志
bool UEProperty::HasPropertyFlags(EPropertyFlags PropertyFlag) const
{
	return GetPropertyFlags() & PropertyFlag;
}

/// 检查是否为指定类型
bool UEProperty::IsType(EClassCastFlags PossibleTypes) const
{
	return (static_cast<uint64>(GetCastFlags()) & static_cast<uint64>(PossibleTypes)) != 0;
}

/// 获取名称
std::string UEProperty::GetName() const
{
	return Base ? GetFName().ToString() : "None";
}

/// 获取有效的名称（用于 C++ 标识符）
std::string UEProperty::GetValidName() const
{
	return Base ? GetFName().ToValidString() : "None";
}

/// 获取对齐方式
int32 UEProperty::GetAlignment() const
{
	EClassCastFlags TypeFlags = (GetClass().first ? GetClass().first.GetCastFlags() : GetClass().second.GetCastFlags());

	if (TypeFlags & EClassCastFlags::ByteProperty)
	{
		return alignof(uint8); // 0x1
	}
	else if (TypeFlags & EClassCastFlags::UInt16Property)
	{
		return alignof(uint16); // 0x2
	}
	else if (TypeFlags & EClassCastFlags::UInt32Property)
	{
		return alignof(uint32); // 0x4
	}
	else if (TypeFlags & EClassCastFlags::UInt64Property)
	{
		return alignof(uint64); // 0x8
	}
	else if (TypeFlags & EClassCastFlags::Int8Property)
	{
		return alignof(int8); // 0x1
	}
	else if (TypeFlags & EClassCastFlags::Int16Property)
	{
		return alignof(int16); // 0x2
	}
	else if (TypeFlags & EClassCastFlags::IntProperty)
	{
		return alignof(int32); // 0x4
	}
	else if (TypeFlags & EClassCastFlags::Int64Property)
	{
		return alignof(int64); // 0x8
	}
	else if (TypeFlags & EClassCastFlags::FloatProperty)
	{
		return alignof(float); // 0x4
	}
	else if (TypeFlags & EClassCastFlags::DoubleProperty)
	{
		return alignof(double); // 0x8
	}
	else if (TypeFlags & EClassCastFlags::ClassProperty)
	{
		return alignof(void*); // 0x8
	}
	else if (TypeFlags & EClassCastFlags::NameProperty)
	{
		return alignof(int32); // FName is a bunch of int32s
	}
	else if (TypeFlags & EClassCastFlags::StrProperty)
	{
		return alignof(FString); // 0x8
	}
	else if (TypeFlags & EClassCastFlags::TextProperty)
	{
		return alignof(FString); // alignof member FString
	}
	else if (TypeFlags & EClassCastFlags::BoolProperty)
	{
		return alignof(bool); // 0x1
	}
	else if (TypeFlags & EClassCastFlags::StructProperty)
	{
		return Cast<UEStructProperty>().GetUnderlayingStruct().GetMinAlignment();
	}
	else if (TypeFlags & EClassCastFlags::ArrayProperty)
	{
		return alignof(TArray<int>); // 0x8
	}
	else if (TypeFlags & EClassCastFlags::DelegateProperty)
	{
		return alignof(int32); // 0x4
	}
	else if (TypeFlags & EClassCastFlags::WeakObjectProperty)
	{
		return alignof(int32); // TWeakObjectPtr is a bunch of int32s
	}
	else if (TypeFlags & EClassCastFlags::LazyObjectProperty)
	{
		return alignof(int32); // TLazyObjectPtr is a bunch of int32s
	}
	else if (TypeFlags & EClassCastFlags::SoftClassProperty)
	{
		return alignof(FString); // alignof member FString
	}
	else if (TypeFlags & EClassCastFlags::SoftObjectProperty)
	{
		return alignof(FString); // alignof member FString
	}
	else if (TypeFlags & EClassCastFlags::ObjectProperty)
	{
		return alignof(void*); // 0x8
	}
	else if (TypeFlags & EClassCastFlags::MapProperty)
	{
		return alignof(TArray<int>); // 0x8, TMap contains a TArray
	}
	else if (TypeFlags & EClassCastFlags::SetProperty)
	{
		return alignof(TArray<int>); // 0x8, TSet contains a TArray
	}
	else if (TypeFlags & EClassCastFlags::EnumProperty)
	{
		UEProperty P = Cast<UEEnumProperty>().GetUnderlayingProperty();

		return P ? P.GetAlignment() : 0x1;
	}
	else if (TypeFlags & EClassCastFlags::InterfaceProperty)
	{
		return alignof(void*); // 0x8
	}
	else if (TypeFlags & EClassCastFlags::FieldPathProperty)
	{
		return alignof(TArray<int>); // alignof member TArray<FName> and ptr;
	}
	else if (TypeFlags & EClassCastFlags::MulticastSparseDelegateProperty)
	{
		return 0x1; // size in PropertyFixup (alignment isn't greater than size)
	}
	else if (TypeFlags & EClassCastFlags::MulticastInlineDelegateProperty)
	{
		return alignof(TArray<int>);  // alignof member TArray<FName>
	}
	else if (TypeFlags & EClassCastFlags::OptionalProperty)
	{
		UEProperty ValueProperty = Cast<UEOptionalProperty>().GetValueProperty();

		/* If this check is true it means, that there is no bool in this TOptional to check if the value is set */
		if (ValueProperty.GetSize() == GetSize()) [[unlikely]]
			return ValueProperty.GetAlignment();

		return  GetSize() - ValueProperty.GetSize();
	}

	if (Settings::Internal::bUseFProperty)
	{
		static std::unordered_map<void*, int32> UnknownProperties;

		/// 尝试在 TOptional 中查找属性引用以获取对齐方式
		static auto TryFindPropertyRefInOptionalToGetAlignment = [](std::unordered_map<void*, int32>& OutProperties, void* PropertyClass) -> int32
		{
			/* Search for a TOptionalProperty that contains an instance of this property */
			for (UEObject Obj : ObjectArray())
			{
				if (!Obj.IsA(EClassCastFlags::Struct))
					continue;

				for (UEProperty Prop : Obj.Cast<UEStruct>().GetProperties())
				{
					if (!Prop.IsA(EClassCastFlags::OptionalProperty) || Prop.IsA(EClassCastFlags::ObjectPropertyBase))
						continue;

					UEOptionalProperty Optional = Prop.Cast<UEOptionalProperty>();

					/* Safe to use first member, as we're guaranteed to use FProperty */
					if (Optional.GetValueProperty().GetClass().second.GetAddress() == PropertyClass)
						return OutProperties.insert({ PropertyClass, Optional.GetAlignment() }).first->second;
				}
			}

			return OutProperties.insert({ PropertyClass, 0x1 }).first->second;
		};

		auto It = UnknownProperties.find(GetClass().second.GetAddress());

		/* Safe to use first member, as we're guaranteed to use FProperty */
		if (It == UnknownProperties.end())
			return TryFindPropertyRefInOptionalToGetAlignment(UnknownProperties, GetClass().second.GetAddress());

		return It->second;
	}

	return 0x1;
}

/// 获取 C++ 类型名称
std::string UEProperty::GetCppType() const
{
	EClassCastFlags TypeFlags = (GetClass().first ? GetClass().first.GetCastFlags() : GetClass().second.GetCastFlags());

	if (TypeFlags & EClassCastFlags::ByteProperty)
	{
		return Cast<UEByteProperty>().GetCppType();
	}
	else if (TypeFlags & EClassCastFlags::UInt16Property)
	{
		return "uint16";
	}
	else if (TypeFlags & EClassCastFlags::UInt32Property)
	{
		return "uint32";
	}
	else if (TypeFlags & EClassCastFlags::UInt64Property)
	{
		return "uint64";
	}
	else if (TypeFlags & EClassCastFlags::Int8Property)
	{
		return "int8";
	}
	else if (TypeFlags & EClassCastFlags::Int16Property)
	{
		return "int16";
	}
	else if (TypeFlags & EClassCastFlags::IntProperty)
	{
		return "int32";
	}
	else if (TypeFlags & EClassCastFlags::Int64Property)
	{
		return "int64";
	}
	else if (TypeFlags & EClassCastFlags::FloatProperty)
	{
		return "float";
	}
	else if (TypeFlags & EClassCastFlags::DoubleProperty)
	{
		return "double";
	}
	else if (TypeFlags & EClassCastFlags::ClassProperty)
	{
		return Cast<UEClassProperty>().GetCppType();
	}
	else if (TypeFlags & EClassCastFlags::NameProperty)
	{
		return "class FName";
	}
	else if (TypeFlags & EClassCastFlags::StrProperty)
	{
		return "class FString";
	}
	else if (TypeFlags & EClassCastFlags::TextProperty)
	{
		return "class FText";
	}
	else if (TypeFlags & EClassCastFlags::BoolProperty)
	{
		return Cast<UEBoolProperty>().GetCppType();
	}
	else if (TypeFlags & EClassCastFlags::StructProperty)
	{
		return Cast<UEStructProperty>().GetCppType();
	}
	else if (TypeFlags & EClassCastFlags::ArrayProperty)
	{
		return Cast<UEArrayProperty>().GetCppType();
	}
	else if (TypeFlags & EClassCastFlags::WeakObjectProperty)
	{
		return Cast<UEWeakObjectProperty>().GetCppType();
	}
	else if (TypeFlags & EClassCastFlags::LazyObjectProperty)
	{
		return Cast<UELazyObjectProperty>().GetCppType();
	}
	else if (TypeFlags & EClassCastFlags::SoftClassProperty)
	{
		return Cast<UESoftClassProperty>().GetCppType();
	}
	else if (TypeFlags & EClassCastFlags::SoftObjectProperty)
	{
		return Cast<UESoftObjectProperty>().GetCppType();
	}
	else if (TypeFlags & EClassCastFlags::ObjectProperty)
	{
		return Cast<UEObjectProperty>().GetCppType();
	}
	else if (TypeFlags & EClassCastFlags::MapProperty)
	{
		return Cast<UEMapProperty>().GetCppType();
	}
	else if (TypeFlags & EClassCastFlags::SetProperty)
	{
		return Cast<UESetProperty>().GetCppType();
	}
	else if (TypeFlags & EClassCastFlags::EnumProperty)
	{
		return Cast<UEEnumProperty>().GetCppType();
	}
	else if (TypeFlags & EClassCastFlags::InterfaceProperty)
	{
		return Cast<UEInterfaceProperty>().GetCppType();
	}
	else if (TypeFlags & EClassCastFlags::FieldPathProperty)
	{
		return Cast<UEFieldPathProperty>().GetCppType();
	}
	else if (TypeFlags & EClassCastFlags::DelegateProperty)
	{
		return Cast<UEDelegateProperty>().GetCppType();
	}
	else if (TypeFlags & EClassCastFlags::OptionalProperty)
	{
		return Cast<UEOptionalProperty>().GetCppType();
	}
	else
	{
		return (GetClass().first ? GetClass().first.GetCppName() : GetClass().second.GetCppName()) + "_";;
	}
}

/// 获取属性的类名
std::string UEProperty::GetPropClassName() const
{
	return GetClass().first ? GetClass().first.GetName() : GetClass().second.GetName();
}

/// 将属性标志转换为字符串
std::string UEProperty::StringifyFlags() const
{
	return StringifyPropertyFlags(GetPropertyFlags());
}

/// 获取字节属性的枚举
UEEnum UEByteProperty::GetEnum() const
{
	return UEEnum(*reinterpret_cast<void**>(Base + Off::ByteProperty::Enum));
}

/// 获取字节属性的 C++ 类型
std::string UEByteProperty::GetCppType() const
{
	if (UEEnum Enum = GetEnum())
	{
		return Enum.GetEnumTypeAsStr();
	}

	return "uint8";
}

/// 获取布尔属性的字段掩码
uint8 UEBoolProperty::GetFieldMask() const
{
	return reinterpret_cast<Off::BoolProperty::UBoolPropertyBase*>(Base + Off::BoolProperty::Base)->FieldMask;
}

/// 获取布尔属性的位索引
uint8 UEBoolProperty::GetBitIndex() const
{
	uint8 FieldMask = GetFieldMask();

	if (FieldMask != 0xFF)
	{
		if (FieldMask == 0x01) { return 0; }
		if (FieldMask == 0x02) { return 1; }
		if (FieldMask == 0x04) { return 2; }
		if (FieldMask == 0x08) { return 3; }
		if (FieldMask == 0x10) { return 4; }
		if (FieldMask == 0x20) { return 5; }
		if (FieldMask == 0x40) { return 6; }
		if (FieldMask == 0x80) { return 7; }
	}

	return 0xFF;
}

/// 检查是否为原生布尔类型
bool UEBoolProperty::IsNativeBool() const
{
	return reinterpret_cast<Off::BoolProperty::UBoolPropertyBase*>(Base + Off::BoolProperty::Base)->FieldMask == 0xFF;
}

/// 获取布尔属性的 C++ 类型
std::string UEBoolProperty::GetCppType() const
{
	return IsNativeBool() ? "bool" : "uint8";
}

/// 获取对象属性的属性类
UEClass UEObjectProperty::GetPropertyClass() const
{
	return UEClass(*reinterpret_cast<void**>(Base + Off::ObjectProperty::PropertyClass));
}

/// 获取对象属性的 C++ 类型
std::string UEObjectProperty::GetCppType() const
{
	return std::format("class {}*", GetPropertyClass() ? GetPropertyClass().GetCppName() : "UObject");
}

/// 获取类属性的元类
UEClass UEClassProperty::GetMetaClass() const
{
	return UEClass(*reinterpret_cast<void**>(Base + Off::ClassProperty::MetaClass));
}

/// 获取类属性的 C++ 类型
std::string UEClassProperty::GetCppType() const
{
	return HasPropertyFlags(EPropertyFlags::UObjectWrapper) ? std::format("TSubclassOf<class {}>", GetMetaClass().GetCppName()) : "class UClass*";
}

/// 获取弱对象属性的 C++ 类型
std::string UEWeakObjectProperty::GetCppType() const
{
	return std::format("TWeakObjectPtr<class {}>", GetPropertyClass() ? GetPropertyClass().GetCppName() : "UObject");
}

/// 获取延迟加载对象属性的 C++ 类型
std::string UELazyObjectProperty::GetCppType() const
{
	return std::format("TLazyObjectPtr<class {}>", GetPropertyClass() ? GetPropertyClass().GetCppName() : "UObject");
}

/// 获取软对象属性的 C++ 类型
std::string UESoftObjectProperty::GetCppType() const
{
	return std::format("TSoftObjectPtr<class {}>", GetPropertyClass() ? GetPropertyClass().GetCppName() : "UObject");
}

/// 获取软类属性的 C++ 类型
std::string UESoftClassProperty::GetCppType() const
{
	return std::format("TSoftClassPtr<class {}>", GetMetaClass() ? GetMetaClass().GetCppName() : GetPropertyClass().GetCppName());
}

/// 获取接口属性的 C++ 类型
std::string UEInterfaceProperty::GetCppType() const
{
	return std::format("TScriptInterface<class {}>", GetPropertyClass().GetCppName());
}

/// 获取结构体属性的底层结构体
UEStruct UEStructProperty::GetUnderlayingStruct() const
{
	return UEStruct(*reinterpret_cast<void**>(Base + Off::StructProperty::Struct));
}

/// 获取结构体属性的 C++ 类型
std::string UEStructProperty::GetCppType() const
{
	return std::format("struct {}", GetUnderlayingStruct().GetCppName());
}

/// 获取数组成员的内部属性
UEProperty UEArrayProperty::GetInnerProperty() const
{
	return UEProperty(*reinterpret_cast<void**>(Base + Off::ArrayProperty::Inner));
}

/// 获取数组属性的 C++ 类型
std::string UEArrayProperty::GetCppType() const
{
	return std::format("TArray<{}>", GetInnerProperty().GetCppType());
}

/// 获取委托属性的签名函数
UEFunction UEDelegateProperty::GetSignatureFunction() const
{
	return UEFunction(*reinterpret_cast<void**>(Base + Off::DelegateProperty::SignatureFunction));
}

/// 获取委托属性的 C++ 类型
std::string UEDelegateProperty::GetCppType() const
{
	return "TDeleage<GetCppTypeIsNotImplementedForDelegates>";
}

/// 获取多播内联委托属性的签名函数
UEFunction UEMulticastInlineDelegateProperty::GetSignatureFunction() const
{
	// Uses "Off::DelegateProperty::SignatureFunction" on purpose
	return UEFunction(*reinterpret_cast<void**>(Base + Off::DelegateProperty::SignatureFunction));
}

/// 获取多播内联委托属性的 C++ 类型
std::string UEMulticastInlineDelegateProperty::GetCppType() const
{
	return "TMulticastInlineDelegate<GetCppTypeIsNotImplementedForDelegates>";
}

/// 获取映射属性的键属性
UEProperty UEMapProperty::GetKeyProperty() const
{
	return UEProperty(reinterpret_cast<Off::MapProperty::UMapPropertyBase*>(Base + Off::MapProperty::Base)->KeyProperty);
}

/// 获取映射属性的值属性
UEProperty UEMapProperty::GetValueProperty() const
{
	return UEProperty(reinterpret_cast<Off::MapProperty::UMapPropertyBase*>(Base + Off::MapProperty::Base)->ValueProperty);
}

/// 获取映射属性的 C++ 类型
std::string UEMapProperty::GetCppType() const
{
	return std::format("TMap<{}, {}>", GetKeyProperty().GetCppType(), GetValueProperty().GetCppType());
}

/// 获取集合属性的元素属性
UEProperty UESetProperty::GetElementProperty() const
{
	return UEProperty(*reinterpret_cast<void**>(Base + Off::SetProperty::ElementProp));
}

/// 获取集合属性的 C++ 类型
std::string UESetProperty::GetCppType() const
{
	return std::format("TSet<{}>", GetElementProperty().GetCppType());
}

/// 获取枚举属性的底层属性
UEProperty UEEnumProperty::GetUnderlayingProperty() const
{
	return UEProperty(reinterpret_cast<Off::EnumProperty::UEnumPropertyBase*>(Base + Off::EnumProperty::Base)->UnderlayingProperty);
}

/// 获取枚举属性的枚举
UEEnum UEEnumProperty::GetEnum() const
{
	return UEEnum(reinterpret_cast<Off::EnumProperty::UEnumPropertyBase*>(Base + Off::EnumProperty::Base)->Enum);
}

/// 获取枚举属性的 C++ 类型
std::string UEEnumProperty::GetCppType() const
{
	if (GetEnum())
		return GetEnum().GetEnumTypeAsStr();

	return GetUnderlayingProperty().GetCppType();
}

/// 获取字段路径属性的字段类
UEFFieldClass UEFieldPathProperty::GetFielClass() const
{
	return UEFFieldClass(*reinterpret_cast<void**>(Base + Off::FieldPathProperty::FieldClass));
}

/// 获取字段路径属性的 C++ 类型
std::string UEFieldPathProperty::GetCppType() const
{
	return std::format("TFieldPath<struct {}>", GetFielClass().GetCppName());
}

/// 获取可选属性的值属性
UEProperty UEOptionalProperty::GetValueProperty() const
{
	return UEProperty(*reinterpret_cast<void**>(Base + Off::OptionalProperty::ValueProperty));
}

/// 获取可选属性的 C++ 类型
std::string UEOptionalProperty::GetCppType() const
{
	return std::format("TOptional<{}>", GetValueProperty().GetCppType());
}