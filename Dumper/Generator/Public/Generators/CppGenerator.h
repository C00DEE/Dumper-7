#pragma once

#include <filesystem>
#include <fstream>

#include "Managers/DependencyManager.h"
#include "Managers/StructManager.h"
#include "Managers/MemberManager.h"
#include "Wrappers/StructWrapper.h"
#include "Wrappers/MemberWrappers.h"
#include "Wrappers/EnumWrapper.h"
#include "Managers/PackageManager.h"

#include "HashStringTable.h"
#include "Generator.h"


namespace fs = std::filesystem;

// Cpp代码生成器
class CppGenerator
{
private:
    friend class CppGeneratorTest;
    friend class Generator;

private:
    // 参数信息
    struct ParamInfo
    {
        bool bIsOutPtr; // 是否为输出指针
        bool bIsOutRef; // 是否为输出引用
        bool bIsMoveParam; // 是否为移动参数
        bool bIsRetParam; // 是否为返回参数
        bool bIsConst; // 是否为常量
        EPropertyFlags PropFlags; // 属性标志

        std::string Type; // 类型
        std::string Name; // 名称
    };

    // 函数信息
    struct FunctionInfo
    {
        bool bIsReturningVoid; // 是否返回void
        EFunctionFlags FuncFlags = EFunctionFlags::None; // 函数标志

        std::string RetType; // 返回类型
        std::string FuncNameWithParams; // 带参数的函数名

        std::vector<ParamInfo> UnrealFuncParams; // for unreal-functions only
    };

    // 文件类型枚举
    enum class EFileType
    {
        Classes, // 类
        Structs, // 结构体
        Parameters, // 参数
        Functions, // 函数

        NameCollisionsInl, // 名称冲突内联文件

        BasicHpp, // 基础Hpp
        BasicCpp, // 基础Cpp

        UnrealContainers, // Unreal容器
        UnicodeLib, // Unicode库

        PropertyFixup, // 属性修复
        SdkHpp, // Sdk Hpp

        DebugAssertions, // 调试断言
    };

private:
    using StreamType = std::ofstream; // 流类型

public:
    static inline PredefinedMemberLookupMapType PredefinedMembers; // 预定义成员

    static inline std::string MainFolderName = "CppSDK"; // 主文件夹名
    static inline std::string SubfolderName = "SDK"; // 子文件夹名

    static inline fs::path MainFolder; // 主文件夹路径
    static inline fs::path Subfolder; // 子文件夹路径

private:
    static inline std::vector<PredefinedStruct> PredefinedStructs; // 预定义结构体

private:
    // 创建成员字符串
    static std::string MakeMemberString(const std::string& Type, const std::string& Name, std::string&& Comment);
    // 创建无名称的成员字符串
    static std::string MakeMemberStringWithoutName(const std::string& Type);

    // 生成字节填充
    static std::string GenerateBytePadding(const int32 Offset, const int32 PadSize, std::string&& Reason);
    // 生成位填充
    static std::string GenerateBitPadding(uint8 UnderlayingSizeBytes, const uint8 PrevBitPropertyEndBit, const int32 Offset, const int32 PadSize, std::string&& Reason);

    // 生成成员
    static std::string GenerateMembers(const StructWrapper& Struct, const MemberManager& Members, int32 SuperSize, int32 SuperLastMemberEnd, int32 SuperAlign, int32 PackageIndex = -1);
    // 生成函数信息
    static FunctionInfo GenerateFunctionInfo(const FunctionWrapper& Func);

    // return: In-header function declarations and inline functions
    // 返回: 头文件中的函数声明和内联函数
    static std::string GenerateSingleFunction(const FunctionWrapper& Func, const std::string& StructName, StreamType& FunctionFile, StreamType& ParamFile);
    // 生成函数
    static std::string GenerateFunctions(const StructWrapper& Struct, const MemberManager& Members, const std::string& StructName, StreamType& FunctionFile, StreamType& ParamFile);

    // 生成结构体
    static void GenerateStruct(const StructWrapper& Struct, StreamType& StructFile, StreamType& FunctionFile, StreamType& ParamFile, int32 PackageIndex = -1, const std::string& StructNameOverride = std::string());

    // 生成枚举
    static void GenerateEnum(const EnumWrapper& Enum, StreamType& StructFile);

private: /* utility functions */
    // 获取成员类型字符串
    static std::string GetMemberTypeString(const PropertyWrapper& MemberWrapper, int32 PackageIndex = -1, bool bAllowForConstPtrMembers = false /* const USomeClass* Member; */);
    // 获取成员类型字符串
    static std::string GetMemberTypeString(UEProperty Member, int32 PackageIndex = -1, bool bAllowForConstPtrMembers = false);
    // 获取无const的成员类型字符串
    static std::string GetMemberTypeStringWithoutConst(UEProperty Member, int32 PackageIndex = -1);

    // 获取函数签名
    static std::string GetFunctionSignature(UEFunction Func);

    // 获取带前缀的结构体名称
    static std::string GetStructPrefixedName(const StructWrapper& Struct);
    // 获取带前缀的枚举名称
    static std::string GetEnumPrefixedName(const EnumWrapper& Enum);
    // 获取枚举的底层类型
    static std::string GetEnumUnderlayingType(const EnumWrapper& Enm);

    // 获取循环修复类型
    static std::string GetCycleFixupType(const StructWrapper& Struct, bool bIsForInheritance);

    // 获取未知属性
    static std::unordered_map<std::string, UEProperty> GetUnknownProperties();

private:
    // 生成枚举前向声明
    static void GenerateEnumFwdDeclarations(StreamType& ClassOrStructFile, PackageInfoHandle Package, bool bIsClassFile);

private:
    // 生成名称冲突内联文件
    static void GenerateNameCollisionsInl(StreamType& NameCollisionsFile);
    // 生成属性修复文件
    static void GeneratePropertyFixupFile(StreamType& PropertyFixup);
    // 生成调试断言
    static void GenerateDebugAssertions(StreamType& AssertionStream);
    // 写入文件头
    static void WriteFileHead(StreamType& File, PackageInfoHandle Package, EFileType Type, const std::string& CustomFileComment = "", const std::string& CustomIncludes = "");
    // 写入文件尾
    static void WriteFileEnd(StreamType& File, EFileType Type);

    // 生成SDK头文件
    static void GenerateSDKHeader(StreamType& SdkHpp);

    // 生成基础文件
    static void GenerateBasicFiles(StreamType& BasicH, StreamType& BasicCpp);

    /*
    * Creates the UnrealContainers.hpp file (without allocation code) for the SDK. 
    * File contains the implementation of TArray, FString, TSparseArray, TSet, TMap and iterators for them
    *
    * See https://github.com/Fischsalat/UnrealContainers/blob/master/UnrealContainers/UnrealContainersNoAlloc.h 
    */
    /*
    * 为SDK创建UnrealContainers.hpp文件 (无分配代码)。
    * 文件包含 TArray, FString, TSparseArray, TSet, TMap 及其迭代器的实现。
    *
    * 参见 https://github.com/Fischsalat/UnrealContainers/blob/master/UnrealContainers/UnrealContainersNoAlloc.h 
    */
    static void GenerateUnrealContainers(StreamType& UEContainersHeader);

    /*
    * Creates the UtfN.hpp file for the SDK.
    *
    * See https://github.com/Fischsalat/UTF-N
    */
    /*
    * 为SDK创建UtfN.hpp文件。
    *
    * 参见 https://github.com/Fischsalat/UTF-N
    */
    static void GenerateUnicodeLib(StreamType& UnicodeLib);

public:
    // 生成
    static void Generate();

    // 初始化预定义成员
    static void InitPredefinedMembers();
    // 初始化预定义函数
    static void InitPredefinedFunctions();
};