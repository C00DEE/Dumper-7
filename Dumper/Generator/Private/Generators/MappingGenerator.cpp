#include <iostream>
#include <string>

#include "Generators/MappingGenerator.h"
#include "Managers/PackageManager.h"
#include "Compression/zstd.h"

#include "../Settings.h"
#include "Utils.h"

// 获取映射类型
EMappingsTypeFlags MappingGenerator::GetMappingType(UEProperty Property)
{
	auto [Class, FieldClass] = Property.GetClass();

	EClassCastFlags Flags = Class ? Class.GetCastFlags() : FieldClass.GetCastFlags();
	// 字节属性
	if (Flags & EClassCastFlags::ByteProperty)
	{
		return EMappingsTypeFlags::ByteProperty;
	}
	// 16位无符号整数属性
	else if (Flags & EClassCastFlags::UInt16Property)
	{
		return EMappingsTypeFlags::UInt16Property;
	}
	// 32位无符号整数属性
	else if (Flags & EClassCastFlags::UInt32Property)
	{
		return EMappingsTypeFlags::UInt32Property;
	}
	// 64位无符号整数属性
	else if (Flags & EClassCastFlags::UInt64Property)
	{
		return EMappingsTypeFlags::UInt64Property;
	}
	// 8位有符号整数属性
	else if (Flags & EClassCastFlags::Int8Property)
	{
		return EMappingsTypeFlags::Int8Property;
	}
	// 16位有符号整数属性
	else if (Flags & EClassCastFlags::Int16Property)
	{
		return EMappingsTypeFlags::Int16Property;
	}
	// 32位有符号整数属性
	else if (Flags & EClassCastFlags::IntProperty)
	{
		return EMappingsTypeFlags::IntProperty;
	}
	// 64位有符号整数属性
	else if (Flags & EClassCastFlags::Int64Property)
	{
		return EMappingsTypeFlags::Int64Property;
	}
	// 浮点数属性
	else if (Flags & EClassCastFlags::FloatProperty)
	{
		return EMappingsTypeFlags::FloatProperty;
	}
	// 双精度浮点数属性
	else if (Flags & EClassCastFlags::DoubleProperty)
	{
		return EMappingsTypeFlags::DoubleProperty;
	}
	// 对象或类属性
	else if ((Flags & EClassCastFlags::ObjectProperty) || (Flags & EClassCastFlags::ClassProperty))
	{
		return EMappingsTypeFlags::ObjectProperty;
	}
	// 名称属性
	else if (Flags & EClassCastFlags::NameProperty)
	{
		return EMappingsTypeFlags::NameProperty;
	}
	// 字符串属性
	else if (Flags & EClassCastFlags::StrProperty)
	{
		return EMappingsTypeFlags::StrProperty;
	}
	// 文本属性
	else if (Flags & EClassCastFlags::TextProperty)
	{
		return EMappingsTypeFlags::TextProperty;
	}
	// 布尔属性
	else if (Flags & EClassCastFlags::BoolProperty)
	{
		return EMappingsTypeFlags::BoolProperty;
	}
	// 结构体属性
	else if (Flags & EClassCastFlags::StructProperty)
	{
		return EMappingsTypeFlags::StructProperty;
	}
	// 数组属性
	else if (Flags & EClassCastFlags::ArrayProperty)
	{
		return EMappingsTypeFlags::ArrayProperty;
	}
	// 弱对象属性
	else if (Flags & EClassCastFlags::WeakObjectProperty)
	{
		return EMappingsTypeFlags::WeakObjectProperty;
	}
	// 懒加载对象属性
	else if (Flags & EClassCastFlags::LazyObjectProperty)
	{
		return EMappingsTypeFlags::LazyObjectProperty;
	}
	// 软对象或软类属性
	else if ((Flags & EClassCastFlags::SoftObjectProperty) || (Flags & EClassCastFlags::SoftClassProperty))
	{
		return EMappingsTypeFlags::SoftObjectProperty;
	}
	// 映射属性
	else if (Flags & EClassCastFlags::MapProperty)
	{
		return EMappingsTypeFlags::MapProperty;
	}
	// 集合属性
	else if (Flags & EClassCastFlags::SetProperty)
	{
		return EMappingsTypeFlags::SetProperty;
	}
	// 枚举属性
	else if (Flags & EClassCastFlags::EnumProperty)
	{
		return EMappingsTypeFlags::EnumProperty;
	}
	// 接口属性
	else if (Flags & EClassCastFlags::InterfaceProperty)
	{
		return EMappingsTypeFlags::InterfaceProperty;
	}
	// 字段路径属性
	else if (Flags & EClassCastFlags::FieldPathProperty)
	{
		return EMappingsTypeFlags::FieldPathProperty;
	}
	// 可选属性
	else if (Flags & EClassCastFlags::OptionalProperty)
	{
		return EMappingsTypeFlags::OptionalProperty;
	}
	// 多播委托属性
	else if (Flags & EClassCastFlags::MulticastDelegateProperty)
	{
		return EMappingsTypeFlags::MulticastDelegateProperty;
	}
	// 委托属性
	else if (Flags & EClassCastFlags::DelegateProperty)
	{
		return EMappingsTypeFlags::DelegateProperty;
	}
	
	return EMappingsTypeFlags::Unknown;
}

// 将名称添加到数据中
int32 MappingGenerator::AddNameToData(std::stringstream& NameTable, const std::string& Name)
{
	if constexpr (Settings::MappingGenerator::bShouldCheckForDuplicatedNames)
	{
		static std::unordered_map<std::string, int32> NameMap;
		
		auto [It, bInserted] = NameMap.insert({ Name, NameCounter });

		/* 如果名称尚未出现，则将其写入NameTable */
		if (bInserted)
		{
			WriteToStream(NameTable, static_cast<uint16>(Name.length()));
			NameTable.write(Name.c_str(), Name.length());
			return NameCounter++;
		}

		return It->second;
	}

	WriteToStream(NameTable, static_cast<uint16>(Name.length()));
	NameTable.write(Name.c_str(), Name.length());

	return NameCounter++;
}

// 生成属性类型
void MappingGenerator::GeneratePropertyType(UEProperty Property, std::stringstream& Data, std::stringstream& NameTable)
{
	if (!Property)
	{
		WriteToStream(Data, static_cast<uint8>(EMappingsTypeFlags::Unknown));
		return;
	}

	EMappingsTypeFlags MappingType = GetMappingType(Property);

	/* 如果内部枚举有效，则将ByteProperty序列化为'UnderlayingType == uint8'的EnumProperty */
	const bool bIsFakeEnumProperty = MappingType == EMappingsTypeFlags::ByteProperty && Property.Cast<UEByteProperty>().GetEnum();

	WriteToStream(Data, static_cast<uint8>(!bIsFakeEnumProperty ? MappingType : EMappingsTypeFlags::EnumProperty));

	/* 将ByteProperty作为伪EnumProperty的底层类型写入 */
	if (bIsFakeEnumProperty)
		WriteToStream(Data, static_cast<uint8>(EMappingsTypeFlags::ByteProperty));
	// 如果是枚举属性
	if (MappingType == EMappingsTypeFlags::EnumProperty)
	{
		GeneratePropertyType(Property.Cast<UEEnumProperty>().GetUnderlayingProperty(), Data, NameTable);

		const int32 EnumNameIdx = AddNameToData(NameTable, Property.Cast<UEEnumProperty>().GetEnum().GetName());
		WriteToStream(Data, EnumNameIdx);
	}
	// 如果是伪枚举属性
	else if (bIsFakeEnumProperty)
	{
		const int32 EnumNameIdx = AddNameToData(NameTable, Property.Cast<UEByteProperty>().GetEnum().GetName());
		WriteToStream(Data, EnumNameIdx);
	}
	// 如果是结构体属性
	else if (MappingType == EMappingsTypeFlags::StructProperty)
	{
		const int32 StructNameIdx = AddNameToData(NameTable, Property.Cast<UEStructProperty>().GetUnderlayingStruct().GetName());
		WriteToStream(Data, StructNameIdx);
	}
	// 如果是集合属性
	else if (MappingType == EMappingsTypeFlags::SetProperty)
	{
		GeneratePropertyType(Property.Cast<UESetProperty>().GetElementProperty(), Data, NameTable);
	}
	// 如果是数组属性
	else if (MappingType == EMappingsTypeFlags::ArrayProperty)
	{
		GeneratePropertyType(Property.Cast<UEArrayProperty>().GetInnerProperty(), Data, NameTable);
	}
	// 如果是可选属性
	else if (MappingType == EMappingsTypeFlags::OptionalProperty)
	{
		GeneratePropertyType(Property.Cast<UEOptionalProperty>().GetValueProperty(), Data, NameTable);
	}
	// 如果是映射属性
	else if (MappingType == EMappingsTypeFlags::MapProperty)
	{
		UEMapProperty AsMapProperty = Property.Cast<UEMapProperty>();
		GeneratePropertyType(AsMapProperty.GetKeyProperty(), Data, NameTable);
		GeneratePropertyType(AsMapProperty.GetValueProperty(), Data, NameTable);
	}
}

// 生成属性信息
void MappingGenerator::GeneratePropertyInfo(const PropertyWrapper& Property, std::stringstream& Data, std::stringstream& NameTable, int32& Index)
{
	if (!Property.IsUnrealProperty())
	{
		std::cerr << "\nInvalid non-Unreal property!\n" << std::endl;
		return;
	}

	WriteToStream(Data, static_cast<uint16>(Index));
	WriteToStream(Data, static_cast<uint8>(Property.GetArrayDim()));

	const int32 MemberNameIdx = AddNameToData(NameTable, Property.GetUnrealProperty().GetName());
	WriteToStream(Data, MemberNameIdx);

	GeneratePropertyType(Property.GetUnrealProperty(), Data, NameTable);

	Index += Property.GetArrayDim();
}

// 生成结构体
void MappingGenerator::GenerateStruct(const StructWrapper& Struct, std::stringstream& Data, std::stringstream& NameTable)
{
	if (!Struct.IsValid())
		return;

	const int32 StructNameIndex = AddNameToData(NameTable, Struct.GetRawName());
	WriteToStream(Data, StructNameIndex);

	StructWrapper Super = Struct.GetSuper();

	if (Super.IsValid())
	{
		/* 很可能会在名称表中添加重复项。稍后找到更好的解决方案！ */
		const int32 SuperNameIndex = AddNameToData(NameTable, Super.GetRawName());
		WriteToStream(Data, SuperNameIndex);
	}
	else
	{
		WriteToStream(Data, -1);
	}

	const std::vector<MemberProperty>& Members = Struct.GetMembers();

	WriteToStream(Data, static_cast<uint16>(Struct.GetPropertyCount()));
	WriteToStream(Data, static_cast<uint16>(Struct.GetSize()));

	if (Members.empty())
		return;

	int32 PropIndex = 0;

	for (const MemberProperty& Member : Members)
	{
		/*if (!Member.IsUnrealProperty())
			continue;*/

		GeneratePropertyInfo(Member.AsProperty(), Data, NameTable, PropIndex);
	}
}

// 生成枚举
void MappingGenerator::GenerateEnum(const EnumWrapper& Enum, std::stringstream& Data, std::stringstream& NameTable)
{
	if (!Enum.IsValid())
		return;

	const int32 EnumNameIndex = AddNameToData(NameTable, Enum.GetRawName());
	WriteToStream(Data, EnumNameIndex);

	const std::vector<std::pair<std::string, int64>>& Members = Enum.GetMembers();

	WriteToStream(Data, static_cast<uint8>(Members.size()));

	for (const auto& [Name, Value] : Members)
	{
		const int32 EnumMemberNameIndex = AddNameToData(NameTable, Name);
		WriteToStream(Data, EnumMemberNameIndex);
	}
}

// 生成
void MappingGenerator::Generate()
{
	std::stringstream StructData;
	std::stringstream EnumData;
	std::stringstream NameTable;

	NameCounter = 0;
	// 生成所有枚举
	EnumManager::VisitAllEnums([&](const EnumWrapper& Enum) -> void
	{
		GenerateEnum(Enum, EnumData, NameTable);
	});
	// 准备要生成的所有结构体
	std::vector<StructWrapper> StructsToGenerate;
	StructManager::GetAllStructs(StructsToGenerate);
	// 定义生成结构体的回调
	DependencyManager::OnVisitCallbackType GenerateStructCallback = [&](int32 Index) -> void
	{
		GenerateStruct(StructManager::GetStructByIndex(Index), StructData, NameTable);
	};
	// 遍历所有包
	PackageManager::VisitAllPackages([&](const PackageWrapper& Package) -> bool
	{
		// 获取包的依赖管理器
		DependencyManager& Dependencies = Package.GetDependencyManager();
		// 按依赖顺序生成
		Dependencies.Generate(GenerateStructCallback);

		return true;
	});

	std::cout << "Writing mapping file..." << std::endl;
	// 定义文件名
	std::string MappingFileName = (Settings::Generator::GameVersion + '-' + Settings::Generator::GameName + ".usmap");
	// 使文件名有效
	FileNameHelper::MakeValidFileName(MappingFileName);
	// 创建文件流
	std::ofstream MapFile(DumperFolder / MappingFileName, std::ios::binary);

	/*
	* Magic, should be 'UE4'
	* Version, should be '0'
	* 
	* Some useless bools
	* 
	* NameCount
	* Names, serialized as:
	* - uint16_t Length
	* - char Name[Length]
	* 
	* EnumCount
	* Enums, serialized as:
	* - int32 EnumNameIndex
	* - uint8_t NumMembers
	* - ForEach: int32 MemberNameIndex
	*
	* StructCount
	* Structs, serialized as:
	* - int32 StructNameIndex
	* - int32 SuperNameIndex (-1 for none)
	* - uint16_t PropertyCount
	* - uint16_t SerializablePropertyCount
	* - ForEach Property:
	*   - uint16_t Index
	*   - uint8_t ArrayDim
	*   - int32_t MemberNameIndex
	*   - uint8_t Type
	*   - uint8_t UnderlayingType (for enums only)
	*   - ForEach Sub-Type:
	*     - uint8_t SubType
	*     - int32_t TypeNameIndex
	*/
	
	// 'UE4\0'
	MapFile.write("UE4", 4);
	// Version
	WriteToStream(MapFile, static_cast<uint8>(0));
	// bHasVersioning
	WriteToStream(MapFile, false);
	// Useless
	WriteToStream(MapFile, static_cast<uint32>(0));

	const std::string& NameTableStr = NameTable.str();
	const std::string& EnumDataStr = EnumData.str();
	const std::string& StructDataStr = StructData.str();

	const uint32 CompressedNameSize = static_cast<uint32>(ZSTD_compressBound(NameTableStr.length()));
	const uint32 CompressedEnumSize = static_cast<uint32>(ZSTD_compressBound(EnumDataStr.length()));
	const uint32 CompressedStructSize = static_cast<uint32>(ZSTD_compressBound(StructDataStr.length()));

	uint8_t* CompressedNames = new uint8_t[CompressedNameSize];
	uint8_t* CompressedEnums = new uint8_t[CompressedEnumSize];
	uint8_t* CompressedStructs = new uint8_t[CompressedStructSize];
	// 压缩
	const uint32 FinalNameSize = static_cast<uint32>(ZSTD_compress(CompressedNames, CompressedNameSize, NameTableStr.c_str(), NameTableStr.length(), ZSTD_maxCLevel()));
	const uint32 FinalEnumSize = static_cast<uint32>(ZSTD_compress(CompressedEnums, CompressedEnumSize, EnumDataStr.c_str(), EnumDataStr.length(), ZSTD_maxCLevel()));
	const uint32 FinalStructSize = static_cast<uint32>(ZSTD_compress(CompressedStructs, CompressedStructSize, StructDataStr.c_str(), StructDataStr.length(), ZSTD_maxCLevel()));
	// 写入名称计数和枚举计数
	WriteToStream(MapFile, NameCounter);
	WriteToStream(MapFile, EnumManager::GetEnumCount());
	WriteToStream(MapFile, StructManager::GetStructCount());
	// 写入压缩数据块
	WriteToStream(MapFile, static_cast<uint32>(NameTableStr.length()));
	WriteToStream(MapFile, FinalNameSize);
	MapFile.write(reinterpret_cast<const char*>(CompressedNames), FinalNameSize);

	WriteToStream(MapFile, static_cast<uint32>(EnumDataStr.length()));
	WriteToStream(MapFile, FinalEnumSize);
	MapFile.write(reinterpret_cast<const char*>(CompressedEnums), FinalEnumSize);

	WriteToStream(MapFile, static_cast<uint32>(StructDataStr.length()));
	WriteToStream(MapFile, FinalStructSize);
	MapFile.write(reinterpret_cast<const char*>(CompressedStructs), FinalStructSize);
	// 释放内存
	delete[] CompressedNames;
	delete[] CompressedEnums;
	delete[] CompressedStructs;

	std::cout << "Done!" << std::endl;
}

