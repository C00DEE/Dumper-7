#pragma once

#include "Unreal/ObjectArray.h"

#include "Managers/StructManager.h"
#include "Managers/PackageManager.h"

#include "Wrappers/EnumWrapper.h"
#include "Wrappers/StructWrapper.h"
#include "Wrappers/MemberWrappers.h"

#include "Dumpspace/DSGen.h"


// Dumpspace生成器
class DumpspaceGenerator
{
private:
    friend class CppGeneratorTest;
    friend class Generator;

private:
    using StreamType = std::ofstream; // 流类型

public:
    static inline PredefinedMemberLookupMapType PredefinedMembers; // 预定义成员

    static inline std::string MainFolderName = "Dumpspace"; // 主文件夹名称
    static inline std::string SubfolderName = ""; // 子文件夹名称

    static inline fs::path MainFolder; // 主文件夹路径
    static inline fs::path Subfolder; // 子文件夹路径

private:
    // 获取带前缀的结构体名称
    static std::string GetStructPrefixedName(const StructWrapper& Struct);
    // 获取带前缀的枚举名称
    static std::string GetEnumPrefixedName(const EnumWrapper& Enum);

private:
    // 将枚举大小转换为类型字符串
    static std::string EnumSizeToType(const int32 Size);

private:
    // 获取成员的EType
    static DSGen::EType GetMemberEType(const PropertyWrapper& Property);
    // 获取成员的EType
    static DSGen::EType GetMemberEType(UEProperty Property);
    // 获取成员类型字符串
    static std::string GetMemberTypeStr(UEProperty Property, std::string& OutExtendedType, std::vector<DSGen::MemberType>& OutSubtypes);
    // 获取成员类型
    static DSGen::MemberType GetMemberType(const StructWrapper& Struct);
    // 获取成员类型
    static DSGen::MemberType GetMemberType(UEProperty Property, bool bIsReference = false);
    // 获取成员类型
    static DSGen::MemberType GetMemberType(const PropertyWrapper& Property, bool bIsReference = false);
    // 手动创建成员类型
    static DSGen::MemberType ManualCreateMemberType(DSGen::EType Type, const std::string& TypeName, const std::string& ExtendedType = "");
    // 向结构体添加成员
    static void AddMemberToStruct(DSGen::ClassHolder& Struct, const PropertyWrapper& Property);

    // 递归获取父类
    static void RecursiveGetSuperClasses(const StructWrapper& Struct, std::vector<std::string>& OutSupers);
    // 获取父类
    static std::vector<std::string> GetSuperClasses(const StructWrapper& Struct);

private:
    // 生成结构体
    static DSGen::ClassHolder GenerateStruct(const StructWrapper& Struct);
    // 生成枚举
    static DSGen::EnumHolder GenerateEnum(const EnumWrapper& Enum);
    // 生成函数
    static DSGen::FunctionHolder GenearateFunction(const FunctionWrapper& Function);

    // 生成静态偏移
    static void GeneratedStaticOffsets();

public:
    // 生成
    static void Generate();

    // 初始化预定义成员
    static void InitPredefinedMembers() { };
    // 初始化预定义函数
    static void InitPredefinedFunctions() { };
};