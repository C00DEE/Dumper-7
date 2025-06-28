#pragma once

#include <fstream>

#include "Unreal/ObjectArray.h"
#include "Wrappers/MemberWrappers.h"
#include "Wrappers/EnumWrapper.h"


/*
* USMAP-Header:
* USMAP-头:
* 
* uint16 magic;
* uint16 magic; // 魔术字
* uint8 version;
* uint8 version; // 版本
* if (version >= Packaging)
*     int32 bHasVersionInfo;
*     int32 bHasVersionInfo; // 是否有版本信息
*     if (bHasVersionInfo)
*         int32 FileVersionUE4;
*         int32 FileVersionUE4; // UE4文件版本
*         int32 FileVersionUE5;
*         int32 FileVersionUE5; // UE5文件版本
*         uint32 NetCL;
*         uint32 NetCL; // 网络CL
* uint8 CompressionMethode;
* uint8 CompressionMethode; // 压缩方法
* uint32 CompressedSize;
* uint32 CompressedSize; // 压缩后大小
* uint32 DecompressedSize;
* uint32 DecompressedSize; // 解压后大小
* 
* 
* USMAP-Data:
* USMAP-数据:
* 
* uint32 NameCount;
* uint32 NameCount; // 名称数量
* for (int i = 0; i < NameCount; i++)
*     [uint8|uint16] NameLength;
*     [uint8|uint16] NameLength; // 名称长度
*     uint8 StringData[NameLength];
*     uint8 StringData[NameLength]; // 字符串数据
* 
* uint32 EnumCount;
* uint32 EnumCount; // 枚举数量
* for (int i = 0; i < EnumCount; i++)
*     int32 EnumNameIdx;
*     int32 EnumNameIdx; // 枚举名称索引
*     uint8 NumNamesInEnum;
*     uint8 NumNamesInEnum; // 枚举中的名称数量
*     for (int j = 0; j < NumNamesInEnum; j++)
*         int32 EnumMemberNameIdx;
*         int32 EnumMemberNameIdx; // 枚举成员名称索引
* 
* uint32 StructCount;
* uint32 StructCount; // 结构体数量
* for (int i = 0; i < StructCount; i++)
*     int32 StructNameIdx;                                 // <-- START ParseStruct
*     int32 StructNameIdx;                                 // <-- 开始 ParseStruct
*     int32 SuperTypeNameIdx;
*     int32 SuperTypeNameIdx; // 父类型名称索引
*     uint16 PropertyCount;
*     uint16 PropertyCount; // 属性数量
*     uint16 SerializablePropertyCount;
*     uint16 SerializablePropertyCount; // 可序列化属性数量
*     for (int j = 0; j < SerializablePropertyCount; j++)
*         uint16 Index;                                    // <-- START ParsePropertyInfo
*         uint16 Index;                                    // <-- 开始 ParsePropertyInfo
*         uint8 ArrayDim;
*         uint8 ArrayDim; // 数组维度
*         int32 PropertyNameIdx;
*         int32 PropertyNameIdx; // 属性名称索引
*         uint8 MappingsTypeEnum;                         // <-- START ParsePropertyType      [[ByteProperty needs to be written as EnumProperty if it has an underlaying Enum]]
*         uint8 MappingsTypeEnum;                         // <-- 开始 ParsePropertyType      [[如果ByteProperty有底层枚举，则需要将其编写为EnumProperty]]
*         if (MappingsTypeEnum == EnumProperty || (MappingsTypeEnum == ByteProperty && UnderlayingEnum != null))
*             CALL ParsePropertyType;
*             int32 EnumName;
*         else if (MappingsTypeEnum == StructProperty)
*             int32 StructNameIdx;
*         else if (MappingsTypeEnum == (SetProperty | ArrayProperty | OptionalProperty))
*             CALL ParsePropertyType;
*         else if (MappingsTypeEnum == MapProperty)
*             CALL ParsePropertyType;
*             CALL ParsePropertyType;                       // <-- END ParsePropertyType, END ParsePropertyInfo, END ParseStruct
*             CALL ParsePropertyType;                       // <-- 结束 ParsePropertyType, 结束 ParsePropertyInfo, 结束 ParseStruct
*/

// 映射生成器
class MappingGenerator
{
private:
    using StreamType = std::ofstream; // 流类型

private:
    // USMAP版本枚举
    enum class EUsmapVersion : uint8
    {
        /* Initial format. */
        /* 初始格式。 */
        Initial,

        /* Adds package versioning to aid with compatibility */
        /* 添加包版本控制以帮助兼容性 */
        PackageVersioning,

        /* Adds support for 16-bit wide name-lengths (ushort/uint16) */
        /* 添加对16位宽名称长度的支持 (ushort/uint16) */
        LongFName,

        /* Adds support for enums with more than 255 values */
        /* 添加对超过255个值的枚举的支持 */
        LargeEnums,

        Latest,
        LatestPlusOne,
    };

private:
    static constexpr uint16 UsmapFileMagic = 0x30C4; // USMAP文件魔术字

private:
    static inline uint64 NameCounter = 0x0; // 名称计数器

public:
    static inline PredefinedMemberLookupMapType PredefinedMembers; // 预定义成员

    static inline std::string MainFolderName = "Mappings"; // 主文件夹名称
    static inline std::string SubfolderName = ""; // 子文件夹名称

    static inline fs::path MainFolder; // 主文件夹路径
    static inline fs::path Subfolder; // 子文件夹路径

private:
    // 将值写入流
    template<typename InStreamType, typename T>
    static void WriteToStream(InStreamType& InStream, T Value)
    {
        InStream.write(reinterpret_cast<const char*>(&Value), sizeof(T));
    }

    // 将流写入流
    template<typename InStreamType>
    static void WriteToStream(InStreamType& InStream, const std::stringstream& Data)
    {
        InStream << Data.rdbuf();
    }

private:
    /* Utility Functions */
    // 获取映射类型
    static EMappingsTypeFlags GetMappingType(UEProperty Property);
    // 向数据添加名称
    static int32 AddNameToData(std::stringstream& NameTable, const std::string& Name);

private:
    // 生成属性类型
    static void GeneratePropertyType(UEProperty Property, std::stringstream& Data, std::stringstream& NameTable);
    // 生成属性信息
    static void GeneratePropertyInfo(const PropertyWrapper& Property, std::stringstream& Data, std::stringstream& NameTable, int32& Index);

    // 生成结构体
    static void GenerateStruct(const StructWrapper& Struct, std::stringstream& Data, std::stringstream& NameTable);
    // 生成枚举
    static void GenerateEnum(const EnumWrapper& Enum, std::stringstream& Data, std::stringstream& NameTable);

    // 生成文件数据
    static std::stringstream GenerateFileData();
    // 生成文件头
    static void GenerateFileHeader(StreamType& InUsmap, const std::stringstream& Data);

public:
    // 生成
    static void Generate();

    /* Always empty, there are no predefined members for mappings */
    /* 始终为空，映射没有预定义成员 */
    static void InitPredefinedMembers() { }
    static void InitPredefinedFunctions() { }
};
