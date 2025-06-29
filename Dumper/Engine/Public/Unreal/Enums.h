#pragma once

#include <cstdint>
#include <type_traits>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <array>

#include <cassert>

// 8位有符号整数
typedef __int8 int8;
// 16位有符号整数
typedef __int16 int16;
// 32位有符号整数
typedef __int32 int32;
// 64位有符号整数
typedef __int64 int64;

// 8位无符号整数
typedef unsigned __int8 uint8;
// 16位无符号整数
typedef unsigned __int16 uint16;
// 32位无符号整数
typedef unsigned __int32 uint32;
// 64位无符号整数
typedef unsigned __int64 uint64;


// 内存对齐函数
template<typename T>
constexpr T Align(T Size, T Alignment)
{
	static_assert(std::is_integral_v<T>, "Align can only hanlde integral types!");
	assert(Alignment != 0 && "Alignment was 0, division by zero exception.");

	const T RequiredAlign = Alignment - (Size % Alignment);

	return Size + (RequiredAlign != Alignment ? RequiredAlign : 0x0);
}


// 为枚举类定义位运算符 | 和 &=
#define ENUM_OPERATORS(EEnumClass)																																		\
																																										\
inline constexpr EEnumClass operator|(EEnumClass Left, EEnumClass Right)																								\
{																																										\
	return (EEnumClass)((std::underlying_type<EEnumClass>::type)(Left) | (std::underlying_type<EEnumClass>::type)(Right));												\
}																																										\
																																										\
inline constexpr EEnumClass& operator|=(EEnumClass& Left, EEnumClass Right)																								\
{																																										\
	return (EEnumClass&)((std::underlying_type<EEnumClass>::type&)(Left) |= (std::underlying_type<EEnumClass>::type)(Right));											\
}																																										\
																																										\
inline bool operator&(EEnumClass Left, EEnumClass Right)																												\
{																																										\
	return (((std::underlying_type<EEnumClass>::type)(Left) & (std::underlying_type<EEnumClass>::type)(Right)) == (std::underlying_type<EEnumClass>::type)(Right));		\
}																																										


// 属性标志
enum class EPropertyFlags : uint64
{
	None							= 0x0000000000000000,	// 无标志

	Edit							= 0x0000000000000001,	// 属性可在编辑器中编辑。
	ConstParm						= 0x0000000000000002,	// 这是一个常量参数。
	BlueprintVisible				= 0x0000000000000004,	// 属性在蓝图编辑器中可见。
	ExportObject					= 0x0000000000000008,	// 对象值应在复制/粘贴时深度复制。
	BlueprintReadOnly				= 0x0000000000000010,	// 属性在蓝图中只读。
	Net								= 0x0000000000000020,	// 属性用于网络复制。
	EditFixedSize					= 0x0000000000000040,	// 固定大小的数组可在编辑器中编辑。
	Parm							= 0x0000000000000080,	// 这是一个函数参数。
	OutParm							= 0x0000000000000100,	// 这是一个输出函数参数。
	ZeroConstructor					= 0x0000000000000200,	// 属性应在构造时归零。
	ReturnParm						= 0x0000000000000400,	// 这是一个函数返回值。
	DisableEditOnTemplate 			= 0x0000000000000800,	// 在蓝图模板中禁用编辑。

	Transient						= 0x0000000000002000,	// 属性是瞬态的，意味着它不会被保存或加载。
	Config							= 0x0000000000004000,	// 属性可由 .ini 文件配置。

	DisableEditOnInstance			= 0x0000000000010000,	// 在世界的实例中禁用编辑。
	EditConst						= 0x0000000000020000,	// 属性在编辑器中是常量。
	GlobalConfig					= 0x0000000000040000,	// 属性是全局配置。
	InstancedReference				= 0x0000000000080000,	// 属性是一个实例化的对象引用。

	DuplicateTransient				= 0x0000000000200000,	// 属性在复制时是瞬态的。
	SubobjectReference				= 0x0000000000400000,	// 属性是一个子对象的引用。

	SaveGame						= 0x0000000001000000,	// 属性应包含在游戏存档中。
	NoClear							= 0x0000000002000000,	// 属性在编辑器中不应被清除。

	ReferenceParm					= 0x0000000008000000,	// 这是一个引用函数参数。
	BlueprintAssignable				= 0x0000000010000000,	// 委托可在蓝图中赋值。
	Deprecated						= 0x0000000020000000,	// 属性已弃用。
	IsPlainOldData					= 0x0000000040000000,	// 是 POD (Plain Old Data) 类型。
	RepSkip							= 0x0000000080000000,	// 属性应跳过复制。
	RepNotify						= 0x0000000100000000,	// 属性在复制时有通知函数。
	Interp							= 0x0000000200000000,	// 属性用于插值。
	NonTransactional				= 0x0000000400000000,	// 属性是非事务性的。
	EditorOnly						= 0x0000000800000000,	// 属性仅在编辑器中存在。
	NoDestructor					= 0x0000001000000000,	// 属性没有析构函数。

	AutoWeak						= 0x0000004000000000,	// 属性是一个自动弱引用。
	ContainsInstancedReference		= 0x0000008000000000,	// 包含实例化引用。
	AssetRegistrySearchable			= 0x0000010000000000,	// 可在资源注册表中搜索。
	SimpleDisplay					= 0x0000020000000000,	// 属性应以简单方式显示。
	AdvancedDisplay					= 0x0000040000000000,	// 属性应以高级方式显示。
	Protected						= 0x0000080000000000,	// 属性是受保护的。
	BlueprintCallable				= 0x0000100000000000,	// 委托可在蓝图中调用。
	BlueprintAuthorityOnly			= 0x0000200000000000,	// 委托仅在权威端可调用。
	TextExportTransient				= 0x0000400000000000,	// 在文本导出时是瞬态的。
	NonPIEDuplicateTransient		= 0x0000800000000000,	// 在非PIE复制时是瞬态的。
	ExposeOnSpawn					= 0x0001000000000000,	// 在生成时公开。
	PersistentInstance				= 0x0002000000000000,	// 实例是持久的。
	UObjectWrapper					= 0x0004000000000000,	// 属性是一个UObject包装器。
	HasGetValueTypeHash				= 0x0008000000000000,	// 具有GetValueTypeHash。
	NativeAccessSpecifierPublic		= 0x0010000000000000,	// 原生访问说明符: public
	NativeAccessSpecifierProtected	= 0x0020000000000000,	// 原生访问说明符: protected
	NativeAccessSpecifierPrivate	= 0x0040000000000000,	// 原生访问说明符: private
	SkipSerialization				= 0x0080000000000000,	// 跳过序列化。
};

// 函数标志
enum class EFunctionFlags : uint32
{
	None					= 0x00000000,	// 无标志

	Final					= 0x00000001,	// 函数是最终的，不能被覆盖。
	RequiredAPI				= 0x00000002,	// 函数是模块API的一部分。
	BlueprintAuthorityOnly	= 0x00000004,	// 函数只能在具有网络权威的服务器上执行。
	BlueprintCosmetic		= 0x00000008,	// 函数是装饰性的，仅在客户端上执行。
	Net						= 0x00000040,	// 函数是网络的。
	NetReliable				= 0x00000080,	// 函数是可靠的网络函数。
	NetRequest				= 0x00000100,	// 这是一个网络请求。
	Exec					= 0x00000200,	// 函数可由控制台执行。
	Native					= 0x00000400,	// 函数是原生C++实现的。
	Event					= 0x00000800,	// 函数是一个事件。
	NetResponse				= 0x00001000,	// 这是一个网络响应。
	Static					= 0x00002000,	// 函数是静态的。
	NetMulticast			= 0x00004000,	// 函数是一个网络多播。
	UbergraphFunction		= 0x00008000,	// 函数是Ubergraph的一部分。
	MulticastDelegate		= 0x00010000,	// 函数是一个多播委托。
	Public					= 0x00020000,	// 函数是公共的。
	Private					= 0x00040000,	// 函数是私有的。
	Protected				= 0x00080000,	// 函数是受保护的。
	Delegate				= 0x00100000,	// 函数是一个委托。
	NetServer				= 0x00200000,	// 函数仅在服务器上执行。
	HasOutParms				= 0x00400000,	// 函数有输出参数。
	HasDefaults				= 0x00800000,	// 函数有默认参数。
	NetClient				= 0x01000000,	// 函数仅在客户端上执行。
	DLLImport				= 0x02000000,	// 函数从DLL导入。
	BlueprintCallable		= 0x04000000,	// 函数可在蓝图中调用。
	BlueprintEvent			= 0x08000000,	// 函数是一个蓝图事件。
	BlueprintPure			= 0x10000000,	// 函数是一个蓝图纯函数。
	EditorOnly				= 0x20000000,	// 函数仅在编辑器中可用。
	Const					= 0x40000000,	// 函数是常量函数。
	NetValidate				= 0x80000000,	// 函数需要网络验证。

	AllFlags = 0xFFFFFFFF, // 所有标志
};

// 对象标志
enum class EObjectFlags
{
	NoFlags							= 0x00000000,	// 无标志

	Public							= 0x00000001,	// 对象对外部可见。
	Standalone						= 0x00000002,	// 对象是独立的，不属于任何包。
	MarkAsNative					= 0x00000004,	// 对象被标记为原生。
	Transactional					= 0x00000008,	// 对象支持事务。
	ClassDefaultObject				= 0x00000010,	// 对象是类的默认对象（CDO）。
	ArchetypeObject					= 0x00000020,	// 对象是一个原型。
	Transient						= 0x00000040,	// 对象是瞬态的。

	MarkAsRootSet					= 0x00000080,	// 对象被标记为根集合，防止被垃圾回收。
	TagGarbageTemp					= 0x00000100,	// 临时垃圾回收标记。

	NeedInitialization				= 0x00000200,	// 对象需要初始化。
	NeedLoad						= 0x00000400,	// 对象需要从磁盘加载。
	KeepForCooker					= 0x00000800,	// 对象应为烘焙器保留。
	NeedPostLoad					= 0x00001000,	// 对象需要在加载后进行处理。
	NeedPostLoadSubobjects			= 0x00002000,	// 对象的子对象需要在加载后进行处理。
	NewerVersionExists				= 0x00004000,	// 存在更新版本的对象。
	BeginDestroyed					= 0x00008000,	// 对象已开始销毁。
	FinishDestroyed					= 0x00010000,	// 对象已完成销毁。

	BeingRegenerated				= 0x00020000,	// 对象正在被重新生成。
	DefaultSubObject				= 0x00040000,	// 对象是一个默认子对象。
	WasLoaded						= 0x00080000,	// 对象已被加载。
	TextExportTransient				= 0x00100000,	// 在文本导出时是瞬态的。
	LoadCompleted					= 0x00200000,	// 对象的加载已完成。
	InheritableComponentTemplate	= 0x00400000,	// 是一个可继承的组件模板。
	DuplicateTransient				= 0x00800000,	// 在复制时是瞬态的。
	StrongRefOnFrame				= 0x01000000,	// 在帧上保持强引用。
	NonPIEDuplicateTransient		= 0x02000000,	// 在非PIE复制时是瞬态的。
	Dynamic							= 0x04000000,	// 对象是动态创建的。
	WillBeLoaded					= 0x08000000,	// 对象将被加载。
};

// 字段类ID
enum class EFieldClassID : uint64
{
	Int8					= 1llu << 1,
	Byte					= 1llu << 6,
	Int						= 1llu << 7,
	Float					= 1llu << 8,
	UInt64					= 1llu << 9,
	Class					= 1llu << 10,
	UInt32					= 1llu << 11,
	Interface				= 1llu << 12,
	Name					= 1llu << 13,
	String					= 1llu << 14,
	Object					= 1llu << 16,
	Bool					= 1llu << 17,
	UInt16					= 1llu << 18,
	Struct					= 1llu << 20,
	Array					= 1llu << 21,
	Int64					= 1llu << 22,
	Delegate				= 1llu << 23,
	SoftObject				= 1llu << 27,
	LazyObject				= 1llu << 28,
	WeakObject				= 1llu << 29,
	Text					= 1llu << 30,
	Int16					= 1llu << 31,
	Double					= 1llu << 32,
	SoftClass				= 1llu << 33,
	Map						= 1llu << 46,
	Set						= 1llu << 47,
	Enum					= 1llu << 48,
	MulticastInlineDelegate = 1llu << 50,
	MulticastSparseDelegate = 1llu << 51,
	ObjectPointer			= 1llu << 53
};

// 类转换标志
enum class EClassCastFlags : uint64
{
	None								= 0x0000000000000000,	// 无

	Field								= 0x0000000000000001,	// FField
	Int8Property						= 0x0000000000000002,	// FInt8Property
	Enum								= 0x0000000000000004,	// UEnum
	Struct								= 0x0000000000000008,	// UStruct
	ScriptStruct						= 0x0000000000000010,	// UScriptStruct
	Class								= 0x0000000000000020,	// UClass
	ByteProperty						= 0x0000000000000040,	// FByteProperty
	IntProperty							= 0x0000000000000080,	// FIntProperty
	FloatProperty						= 0x0000000000000100,	// FFloatProperty
	UInt64Property						= 0x0000000000000200,	// FUInt64Property
	ClassProperty						= 0x0000000000000400,	// FClassProperty
	UInt32Property						= 0x0000000000000800,	// FUInt32Property
	InterfaceProperty					= 0x0000000000001000,	// FInterfaceProperty
	NameProperty						= 0x0000000000002000,	// FNameProperty
	StrProperty							= 0x0000000000004000,	// FStrProperty
	Property							= 0x0000000000008000,	// FProperty
	ObjectProperty						= 0x0000000000010000,	// FObjectProperty
	BoolProperty						= 0x0000000000020000,	// FBoolProperty
	UInt16Property						= 0x0000000000040000,	// FUInt16Property
	Function							= 0x0000000000080000,	// UFunction
	StructProperty						= 0x0000000000100000,	// FStructProperty
	ArrayProperty						= 0x0000000000200000,	// FArrayProperty
	Int64Property						= 0x0000000000400000,	// FInt64Property
	DelegateProperty					= 0x0000000000800000,	// FDelegateProperty
	NumericProperty						= 0x0000000001000000,	// FNumericProperty
	MulticastDelegateProperty			= 0x0000000002000000,	// FMulticastDelegateProperty
	ObjectPropertyBase					= 0x0000000004000000,	// FObjectPropertyBase
	WeakObjectProperty					= 0x0000000008000000,	// FWeakObjectProperty
	LazyObjectProperty					= 0x0000000010000000,	// FLazyObjectProperty
	SoftObjectProperty					= 0x0000000020000000,	// FSoftObjectProperty
	TextProperty						= 0x0000000040000000,	// FTextProperty
	Int16Property						= 0x0000000080000000,	// FInt16Property
	DoubleProperty						= 0x0000000100000000,	// FDoubleProperty
	SoftClassProperty					= 0x0000000200000000,	// FSoftClassProperty
	Package								= 0x0000000400000000,	// UPackage
	Level								= 0x0000000800000000,	// ULevel
	Actor								= 0x0000001000000000,	// AActor
	PlayerController					= 0x0000002000000000,	// APlayerController
	Pawn								= 0x0000004000000000,	// APawn
	SceneComponent						= 0x0000008000000000,	// USceneComponent
	PrimitiveComponent					= 0x0000010000000000,	// UPrimitiveComponent
	SkinnedMeshComponent				= 0x0000020000000000,	// USkinnedMeshComponent
	SkeletalMeshComponent				= 0x0000040000000000,	// USkeletalMeshComponent
	Blueprint							= 0x0000080000000000,	// UBlueprint
	DelegateFunction					= 0x0000100000000000,	// UDelegateFunction
	StaticMeshComponent					= 0x0000200000000000,	// UStaticMeshComponent
	MapProperty							= 0x0000400000000000,	// FMapProperty
	SetProperty							= 0x0000800000000000,	// FSetProperty
	EnumProperty						= 0x0001000000000000,	// FEnumProperty
	SparseDelegateFunction				= 0x0002000000000000,	// USparseDelegateFunction
	MulticastInlineDelegateProperty     = 0x0004000000000000,	// FMulticastInlineDelegateProperty
	MulticastSparseDelegateProperty		= 0x0008000000000000,	// FMulticastSparseDelegateProperty
	FieldPathProperty					= 0x0010000000000000,	// FFieldPathProperty
	LargeWorldCoordinatesRealProperty	= 0x0080000000000000,	// FLargeWorldCoordinatesRealProperty
	OptionalProperty					= 0x0100000000000000,	// FOptionalProperty
	VValueProperty						= 0x0200000000000000,	// FVValueProperty
	VerseVMClass						= 0x0400000000000000,	// UVerseVMClass
	VRestValueProperty					= 0x0800000000000000,	// FVRestValueProperty
};

// 类标志
enum class EClassFlags
{
	None						= 0x00000000u,	// 无标志
	Abstract					= 0x00000001u,	// 类是抽象的。
	DefaultConfig				= 0x00000002u,	// 从默认配置加载配置。
	Config						= 0x00000004u,	// 从配置加载。
	Transient					= 0x00000008u,	// 瞬态。
	Parsed						= 0x00000010u,	// 已解析。
	MatchedSerializers			= 0x00000020u,	// 匹配的序列化器。
	ProjectUserConfig			= 0x00000040u,	// 项目用户配置。
	Native						= 0x00000080u,	// 原生的。
	NoExport					= 0x00000100u,	// 不导出。
	NotPlaceable				= 0x00000200u,	// 不可放置。
	PerObjectConfig				= 0x00000400u,	// 每个对象配置。
	ReplicationDataIsSetUp		= 0x00000800u,	// 复制数据已设置。
	EditInlineNew				= 0x00001000u,	// 可内联编辑新建。
	CollapseCategories			= 0x00002000u,	// 折叠类别。
	Interface					= 0x00004000u,	// 接口。
	CustomConstructor			= 0x00008000u,	// 自定义构造函数。
	Const						= 0x00010000u,	// 常量。
	LayoutChanging				= 0x00020000u,	// 布局正在改变。
	CompiledFromBlueprint		= 0x00040000u,	// 从蓝图编译。
	MinimalAPI					= 0x00080000u,	// 最小API。
	RequiredAPI					= 0x00100000u,	// 需要API。
	DefaultToInstanced			= 0x00200000u,	// 默认为实例化。
	TokenStreamAssembled		= 0x00400000u,	// 令牌流已组装。
	HasInstancedReference		= 0x00800000u,	// 包含实例化引用。
	Hidden						= 0x01000000u,	// 隐藏。
	Deprecated					= 0x02000000u,	// 已弃用。
	HideDropDown				= 0x04000000u,	// 隐藏下拉列表。
	GlobalUserConfig			= 0x08000000u,	// 全局用户配置。
	Intrinsic					= 0x10000000u,	// 内在的。
	Constructed					= 0x20000000u,	// 已构造。
	ConfigDoNotCheckDefaults	= 0x40000000u,	// 配置不检查默认值。
	NewerVersionExists			= 0x80000000u,	// 存在新版本。
};

// 映射类型标志
enum class EMappingsTypeFlags : uint8
{
	ByteProperty,
	BoolProperty,
	IntProperty,
	FloatProperty,
	ObjectProperty,
	NameProperty,
	DelegateProperty,
	DoubleProperty,
	ArrayProperty,
	StructProperty,
	StrProperty,
	TextProperty,
	InterfaceProperty,
	MulticastDelegateProperty,
	WeakObjectProperty, // 弱引用对象属性
	LazyObjectProperty, // 延迟加载对象属性
	AssetObjectProperty, // 资源对象属性（在反序列化时，这3个属性将是软对象）
	SoftObjectProperty,
	UInt64Property,
	UInt32Property,
	UInt16Property,
	Int64Property,
	Int16Property,
	Int8Property,
	MapProperty,
	SetProperty,
	EnumProperty,
	FieldPathProperty,
	OptionalProperty,

	Unknown = 0xFF // 未知
};

// Usmap压缩方法
enum class EUsmapCompressionMethod : uint8
{
	None,		// 无压缩
	Oodle,		// Oodle压缩
	Brotli,		// Brotli压缩
	ZStandard,	// ZStandard压缩
	Unknown = 0xFF // 未知
};

ENUM_OPERATORS(EObjectFlags);
ENUM_OPERATORS(EFunctionFlags);
ENUM_OPERATORS(EPropertyFlags);
ENUM_OPERATORS(EClassCastFlags);
ENUM_OPERATORS(EClassFlags);
ENUM_OPERATORS(EMappingsTypeFlags);
ENUM_OPERATORS(EFieldClassID);


// 将函数标志转换为字符串
static std::string StringifyFunctionFlags(EFunctionFlags FunctionFlags, const char* Seperator = ", ")
{
	/* Make sure the size of this vector is always greater, or equal, to the number of flags existing */
	std::array<const char*, 0x30> StringFlags;
	int32 CurrentIdx = 0x0;

	if (FunctionFlags & EFunctionFlags::Final) { StringFlags[CurrentIdx++] = "Final"; }
	if (FunctionFlags & EFunctionFlags::RequiredAPI) { StringFlags[CurrentIdx++] = "RequiredAPI"; }
	if (FunctionFlags & EFunctionFlags::BlueprintAuthorityOnly) { StringFlags[CurrentIdx++] = "BlueprintAuthorityOnly"; }
	if (FunctionFlags & EFunctionFlags::BlueprintCosmetic) { StringFlags[CurrentIdx++] = "BlueprintCosmetic"; }
	if (FunctionFlags & EFunctionFlags::Net) { StringFlags[CurrentIdx++] = "Net"; }
	if (FunctionFlags & EFunctionFlags::NetReliable) { StringFlags[CurrentIdx++] = "NetReliable"; }
	if (FunctionFlags & EFunctionFlags::NetRequest) { StringFlags[CurrentIdx++] = "NetRequest"; }
	if (FunctionFlags & EFunctionFlags::Exec) { StringFlags[CurrentIdx++] = "Exec"; }
	if (FunctionFlags & EFunctionFlags::Native) { StringFlags[CurrentIdx++] =  "Native"; }
	if (FunctionFlags & EFunctionFlags::Event) { StringFlags[CurrentIdx++] = "Event"; }
	if (FunctionFlags & EFunctionFlags::NetResponse) { StringFlags[CurrentIdx++] = "NetResponse"; }
	if (FunctionFlags & EFunctionFlags::Static) { StringFlags[CurrentIdx++] = "Static"; }
	if (FunctionFlags & EFunctionFlags::NetMulticast) { StringFlags[CurrentIdx++] = "NetMulticast"; }
	if (FunctionFlags & EFunctionFlags::UbergraphFunction) { StringFlags[CurrentIdx++] =  "UbergraphFunction"; }
	if (FunctionFlags & EFunctionFlags::MulticastDelegate) { StringFlags[CurrentIdx++] =  "MulticastDelegate"; }
	if (FunctionFlags & EFunctionFlags::Public) { StringFlags[CurrentIdx++] = "Public"; }
	if (FunctionFlags & EFunctionFlags::Private) { StringFlags[CurrentIdx++] = "Private"; }
	if (FunctionFlags & EFunctionFlags::Protected) { StringFlags[CurrentIdx++] = "Protected"; }
	if (FunctionFlags & EFunctionFlags::Delegate) { StringFlags[CurrentIdx++] = "Delegate"; }
	if (FunctionFlags & EFunctionFlags::NetServer) { StringFlags[CurrentIdx++] = "NetServer"; }
	if (FunctionFlags & EFunctionFlags::HasOutParms) { StringFlags[CurrentIdx++] =  "HasOutParams"; }
	if (FunctionFlags & EFunctionFlags::HasDefaults) { StringFlags[CurrentIdx++] =  "HasDefaults"; }
	if (FunctionFlags & EFunctionFlags::NetClient) { StringFlags[CurrentIdx++] = "NetClient"; }
	if (FunctionFlags & EFunctionFlags::DLLImport) { StringFlags[CurrentIdx++] = "DLLImport"; }
	if (FunctionFlags & EFunctionFlags::BlueprintCallable) { StringFlags[CurrentIdx++] = "BlueprintCallable"; }
	if (FunctionFlags & EFunctionFlags::BlueprintEvent) { StringFlags[CurrentIdx++] = "BlueprintEvent"; }
	if (FunctionFlags & EFunctionFlags::BlueprintPure) { StringFlags[CurrentIdx++] = "BlueprintPure"; }
	if (FunctionFlags & EFunctionFlags::EditorOnly) { StringFlags[CurrentIdx++] = "EditorOnly"; }
	if (FunctionFlags & EFunctionFlags::Const) { StringFlags[CurrentIdx++] = "Const"; }
	if (FunctionFlags & EFunctionFlags::NetValidate) { StringFlags[CurrentIdx++] = "NetValidate"; }

	std::string RetFlags;
	RetFlags.reserve(CurrentIdx * 0xF);

	for (int i = 0; i < CurrentIdx; i++)
	{
		RetFlags += StringFlags[i];

		if (i != (CurrentIdx - 1))
			RetFlags += Seperator;
	}

	return RetFlags;
}

// 将属性标志转换为字符串
static std::string StringifyPropertyFlags(EPropertyFlags PropertyFlags)
{
	std::string RetFlags;

	if (PropertyFlags & EPropertyFlags::Edit) { RetFlags += "Edit, "; }
	if (PropertyFlags & EPropertyFlags::ConstParm) { RetFlags += "ConstParm, "; }
	if (PropertyFlags & EPropertyFlags::BlueprintVisible) { RetFlags += "BlueprintVisible, "; }
	if (PropertyFlags & EPropertyFlags::ExportObject) { RetFlags += "ExportObject, "; }
	if (PropertyFlags & EPropertyFlags::BlueprintReadOnly) { RetFlags += "BlueprintReadOnly, "; }
	if (PropertyFlags & EPropertyFlags::Net) { RetFlags += "Net, "; }
	if (PropertyFlags & EPropertyFlags::EditFixedSize) { RetFlags += "EditFixedSize, "; }
	if (PropertyFlags & EPropertyFlags::Parm) { RetFlags += "Parm, "; }
	if (PropertyFlags & EPropertyFlags::OutParm) { RetFlags += "OutParm, "; }
	if (PropertyFlags & EPropertyFlags::ZeroConstructor) { RetFlags += "ZeroConstructor, "; }
	if (PropertyFlags & EPropertyFlags::ReturnParm) { RetFlags += "ReturnParm, "; }
	if (PropertyFlags & EPropertyFlags::DisableEditOnTemplate) { RetFlags += "DisableEditOnTemplate, "; }
	if (PropertyFlags & EPropertyFlags::Transient) { RetFlags += "Transient, "; }
	if (PropertyFlags & EPropertyFlags::Config) { RetFlags += "Config, "; }
	if (PropertyFlags & EPropertyFlags::DisableEditOnInstance) { RetFlags += "DisableEditOnInstance, "; }
	if (PropertyFlags & EPropertyFlags::EditConst) { RetFlags += "EditConst, "; }
	if (PropertyFlags & EPropertyFlags::GlobalConfig) { RetFlags += "GlobalConfig, "; }
	if (PropertyFlags & EPropertyFlags::InstancedReference) { RetFlags += "InstancedReference, "; }
	if (PropertyFlags & EPropertyFlags::DuplicateTransient) { RetFlags += "DuplicateTransient, "; }
	if (PropertyFlags & EPropertyFlags::SubobjectReference) { RetFlags += "SubobjectReference, "; }
	if (PropertyFlags & EPropertyFlags::SaveGame) { RetFlags += "SaveGame, "; }
	if (PropertyFlags & EPropertyFlags::NoClear) { RetFlags += "NoClear, "; }
	if (PropertyFlags & EPropertyFlags::ReferenceParm) { RetFlags += "ReferenceParm, "; }
	if (PropertyFlags & EPropertyFlags::BlueprintAssignable) { RetFlags += "BlueprintAssignable, "; }
	if (PropertyFlags & EPropertyFlags::Deprecated) { RetFlags += "Deprecated, "; }
	if (PropertyFlags & EPropertyFlags::IsPlainOldData) { RetFlags += "IsPlainOldData, "; }
	if (PropertyFlags & EPropertyFlags::RepSkip) { RetFlags += "RepSkip, "; }
	if (PropertyFlags & EPropertyFlags::RepNotify) { RetFlags += "RepNotify, "; }
	if (PropertyFlags & EPropertyFlags::Interp) { RetFlags += "Interp, "; }
	if (PropertyFlags & EPropertyFlags::NonTransactional) { RetFlags += "NonTransactional, "; }
	if (PropertyFlags & EPropertyFlags::EditorOnly) { RetFlags += "EditorOnly, "; }
	if (PropertyFlags & EPropertyFlags::NoDestructor) { RetFlags += "NoDestructor, "; }
	if (PropertyFlags & EPropertyFlags::AutoWeak) { RetFlags += "AutoWeak, "; }
	if (PropertyFlags & EPropertyFlags::ContainsInstancedReference) { RetFlags += "ContainsInstancedReference, "; }
	if (PropertyFlags & EPropertyFlags::AssetRegistrySearchable) { RetFlags += "AssetRegistrySearchable, "; }
	if (PropertyFlags & EPropertyFlags::SimpleDisplay) { RetFlags += "SimpleDisplay, "; }
	if (PropertyFlags & EPropertyFlags::AdvancedDisplay) { RetFlags += "AdvancedDisplay, "; }
	if (PropertyFlags & EPropertyFlags::Protected) { RetFlags += "Protected, "; }
	if (PropertyFlags & EPropertyFlags::BlueprintCallable) { RetFlags += "BlueprintCallable, "; }
	if (PropertyFlags & EPropertyFlags::BlueprintAuthorityOnly) { RetFlags += "BlueprintAuthorityOnly, "; }
	if (PropertyFlags & EPropertyFlags::TextExportTransient) { RetFlags += "TextExportTransient, "; }
	if (PropertyFlags & EPropertyFlags::NonPIEDuplicateTransient) { RetFlags += "NonPIEDuplicateTransient, "; }
	if (PropertyFlags & EPropertyFlags::ExposeOnSpawn) { RetFlags += "ExposeOnSpawn, "; }
	if (PropertyFlags & EPropertyFlags::PersistentInstance) { RetFlags += "PersistentInstance, "; }
	if (PropertyFlags & EPropertyFlags::UObjectWrapper) { RetFlags += "UObjectWrapper, "; }
	if (PropertyFlags & EPropertyFlags::HasGetValueTypeHash) { RetFlags += "HasGetValueTypeHash, "; }
	if (PropertyFlags & EPropertyFlags::NativeAccessSpecifierPublic) { RetFlags += "NativeAccessSpecifierPublic, "; }
	if (PropertyFlags & EPropertyFlags::NativeAccessSpecifierProtected) { RetFlags += "NativeAccessSpecifierProtected, "; }
	if (PropertyFlags & EPropertyFlags::NativeAccessSpecifierPrivate) { RetFlags += "NativeAccessSpecifierPrivate, "; }

	return RetFlags.size() > 2 ? RetFlags.erase(RetFlags.size() - 2) : RetFlags;
}

// 将对象标志转换为字符串
static std::string StringifyObjectFlags(EObjectFlags ObjFlags)
{
	std::string RetFlags;

	if (ObjFlags & EObjectFlags::Public) { RetFlags += "Public, "; }
	if (ObjFlags & EObjectFlags::Standalone) { RetFlags += "Standalone, "; }
	if (ObjFlags & EObjectFlags::MarkAsNative) { RetFlags += "MarkAsNative, "; }
	if (ObjFlags & EObjectFlags::Transactional) { RetFlags += "Transactional, "; }
	if (ObjFlags & EObjectFlags::ClassDefaultObject) { RetFlags += "ClassDefaultObject, "; }
	if (ObjFlags & EObjectFlags::ArchetypeObject) { RetFlags += "ArchetypeObject, "; }
	if (ObjFlags & EObjectFlags::Transient) { RetFlags += "Transient, "; }
	if (ObjFlags & EObjectFlags::MarkAsRootSet) { RetFlags += "MarkAsRootSet, "; }
	if (ObjFlags & EObjectFlags::TagGarbageTemp) { RetFlags += "TagGarbageTemp, "; }
	if (ObjFlags & EObjectFlags::NeedInitialization) { RetFlags += "NeedInitialization, "; }
	if (ObjFlags & EObjectFlags::NeedLoad) { RetFlags += "NeedLoad, "; }
	if (ObjFlags & EObjectFlags::KeepForCooker) { RetFlags += "KeepForCooker, "; }
	if (ObjFlags & EObjectFlags::NeedPostLoad) { RetFlags += "NeedPostLoad, "; }
	if (ObjFlags & EObjectFlags::NeedPostLoadSubobjects) { RetFlags += "NeedPostLoadSubobjects, "; }
	if (ObjFlags & EObjectFlags::NewerVersionExists) { RetFlags += "NewerVersionExists, "; }
	if (ObjFlags & EObjectFlags::BeginDestroyed) { RetFlags += "BeginDestroyed, "; }
	if (ObjFlags & EObjectFlags::FinishDestroyed) { RetFlags += "FinishDestroyed, "; }
	if (ObjFlags & EObjectFlags::BeingRegenerated) { RetFlags += "BeingRegenerated, "; }
	if (ObjFlags & EObjectFlags::DefaultSubObject) { RetFlags += "DefaultSubObject, "; }
	if (ObjFlags & EObjectFlags::WasLoaded) { RetFlags += "WasLoaded, "; }
	if (ObjFlags & EObjectFlags::TextExportTransient) { RetFlags += "TextExportTransient, "; }
	if (ObjFlags & EObjectFlags::LoadCompleted) { RetFlags += "LoadCompleted, "; }
	if (ObjFlags & EObjectFlags::InheritableComponentTemplate) { RetFlags += "InheritableComponentTemplate, "; }
	if (ObjFlags & EObjectFlags::DuplicateTransient) { RetFlags += "DuplicateTransient, "; }
	if (ObjFlags & EObjectFlags::StrongRefOnFrame) { RetFlags += "StrongRefOnFrame, "; }
	if (ObjFlags & EObjectFlags::NonPIEDuplicateTransient) { RetFlags += "NonPIEDuplicateTransient, "; }
	if (ObjFlags & EObjectFlags::Dynamic) { RetFlags += "Dynamic, "; }
	if (ObjFlags & EObjectFlags::WillBeLoaded) { RetFlags += "WillBeLoaded, "; }

	return RetFlags.size() > 2 ? RetFlags.erase(RetFlags.size() - 2) : RetFlags;
}

// 将类转换标志转换为字符串
static std::string StringifyClassCastFlags(EClassCastFlags CastFlags)
{
	std::string RetFlags;

	if (CastFlags & EClassCastFlags::Field) { RetFlags += "Field, "; }
	if (CastFlags & EClassCastFlags::Int8Property) { RetFlags += "Int8Property, "; }
	if (CastFlags & EClassCastFlags::Enum) { RetFlags += "Enum, "; }
	if (CastFlags & EClassCastFlags::Struct) { RetFlags += "Struct, "; }
	if (CastFlags & EClassCastFlags::ScriptStruct) { RetFlags += "ScriptStruct, "; }
	if (CastFlags & EClassCastFlags::Class) { RetFlags += "Class, "; }
	if (CastFlags & EClassCastFlags::ByteProperty) { RetFlags += "ByteProperty, "; }
	if (CastFlags & EClassCastFlags::IntProperty) { RetFlags += "IntProperty, "; }
	if (CastFlags & EClassCastFlags::FloatProperty) { RetFlags += "FloatProperty, "; }
	if (CastFlags & EClassCastFlags::UInt64Property) { RetFlags += "UInt64Property, "; }
	if (CastFlags & EClassCastFlags::ClassProperty) { RetFlags += "ClassProperty, "; }
	if (CastFlags & EClassCastFlags::UInt32Property) { RetFlags += "UInt32Property, "; }
	if (CastFlags & EClassCastFlags::InterfaceProperty) { RetFlags += "InterfaceProperty, "; }
	if (CastFlags & EClassCastFlags::NameProperty) { RetFlags += "NameProperty, "; }
	if (CastFlags & EClassCastFlags::StrProperty) { RetFlags += "StrProperty, "; }
	if (CastFlags & EClassCastFlags::Property) { RetFlags += "Property, "; }
	if (CastFlags & EClassCastFlags::ObjectProperty) { RetFlags += "ObjectProperty, "; }
	if (CastFlags & EClassCastFlags::BoolProperty) { RetFlags += "BoolProperty, "; }
	if (CastFlags & EClassCastFlags::UInt16Property) { RetFlags += "UInt16Property, "; }
	if (CastFlags & EClassCastFlags::Function) { RetFlags += "Function, "; }
	if (CastFlags & EClassCastFlags::StructProperty) { RetFlags += "StructProperty, "; }
	if (CastFlags & EClassCastFlags::ArrayProperty) { RetFlags += "ArrayProperty, "; }
	if (CastFlags & EClassCastFlags::Int64Property) { RetFlags += "Int64Property, "; }
	if (CastFlags & EClassCastFlags::DelegateProperty) { RetFlags += "DelegateProperty, "; }
	if (CastFlags & EClassCastFlags::NumericProperty) { RetFlags += "NumericProperty, "; }
	if (CastFlags & EClassCastFlags::MulticastDelegateProperty) { RetFlags += "MulticastDelegateProperty, "; }
	if (CastFlags & EClassCastFlags::ObjectPropertyBase) { RetFlags += "ObjectPropertyBase, "; }
	if (CastFlags & EClassCastFlags::WeakObjectProperty) { RetFlags += "WeakObjectProperty, "; }
	if (CastFlags & EClassCastFlags::LazyObjectProperty) { RetFlags += "LazyObjectProperty, "; }
	if (CastFlags & EClassCastFlags::SoftObjectProperty) { RetFlags += "SoftObjectProperty, "; }
	if (CastFlags & EClassCastFlags::TextProperty) { RetFlags += "TextProperty, "; }
	if (CastFlags & EClassCastFlags::Int16Property) { RetFlags += "Int16Property, "; }
	if (CastFlags & EClassCastFlags::DoubleProperty) { RetFlags += "DoubleProperty, "; }
	if (CastFlags & EClassCastFlags::SoftClassProperty) { RetFlags += "SoftClassProperty, "; }
	if (CastFlags & EClassCastFlags::Package) { RetFlags += "Package, "; }
	if (CastFlags & EClassCastFlags::Level) { RetFlags += "Level, "; }
	if (CastFlags & EClassCastFlags::Actor) { RetFlags += "Actor, "; }
	if (CastFlags & EClassCastFlags::PlayerController) { RetFlags += "PlayerController, "; }
	if (CastFlags & EClassCastFlags::Pawn) { RetFlags += "Pawn, "; }
	if (CastFlags & EClassCastFlags::SceneComponent) { RetFlags += "SceneComponent, "; }
	if (CastFlags & EClassCastFlags::PrimitiveComponent) { RetFlags += "PrimitiveComponent, "; }
	if (CastFlags & EClassCastFlags::SkinnedMeshComponent) { RetFlags += "SkinnedMeshComponent, "; }
	if (CastFlags & EClassCastFlags::SkeletalMeshComponent) { RetFlags += "SkeletalMeshComponent, "; }
	if (CastFlags & EClassCastFlags::Blueprint) { RetFlags += "Blueprint, "; }
	if (CastFlags & EClassCastFlags::DelegateFunction) { RetFlags += "DelegateFunction, "; }
	if (CastFlags & EClassCastFlags::StaticMeshComponent) { RetFlags += "StaticMeshComponent, "; }
	if (CastFlags & EClassCastFlags::MapProperty) { RetFlags += "MapProperty, "; }
	if (CastFlags & EClassCastFlags::SetProperty) { RetFlags += "SetProperty, "; }
	if (CastFlags & EClassCastFlags::EnumProperty) { RetFlags += "EnumProperty, "; }
	if (CastFlags & EClassCastFlags::SparseDelegateFunction) { RetFlags += "SparseDelegateFunction, "; }
	if (CastFlags & EClassCastFlags::MulticastInlineDelegateProperty) { RetFlags += "MulticastInlineDelegateProperty, "; }
	if (CastFlags & EClassCastFlags::MulticastSparseDelegateProperty) { RetFlags += "MulticastSparseDelegateProperty, "; }
	if (CastFlags & EClassCastFlags::FieldPathProperty) { RetFlags += "MarkAsFieldPathPropertyRootSet, "; }
	if (CastFlags & EClassCastFlags::LargeWorldCoordinatesRealProperty) { RetFlags += "LargeWorldCoordinatesRealProperty, "; }
	if (CastFlags & EClassCastFlags::OptionalProperty) { RetFlags += "OptionalProperty, "; }
	if (CastFlags & EClassCastFlags::VValueProperty) { RetFlags += "VValueProperty, "; }
	if (CastFlags & EClassCastFlags::VerseVMClass) { RetFlags += "VerseVMClass, "; }
	if (CastFlags & EClassCastFlags::VRestValueProperty) { RetFlags += "VRestValueProperty, "; }

	return RetFlags.size() > 2 ? RetFlags.erase(RetFlags.size() - 2) : RetFlags;
}