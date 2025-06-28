#pragma once

#include <iostream>
#include <string>

#include "Unreal/ObjectArray.h"
#include "PredefinedMembers.h"


// IDA映射生成器
class IDAMappingGenerator
{
public:
    static inline PredefinedMemberLookupMapType PredefinedMembers; // 预定义成员

    static inline std::string MainFolderName = "IDAMappings"; // 主文件夹名称
    static inline std::string SubfolderName = ""; // 子文件夹名称

    static inline fs::path MainFolder; // 主文件夹路径
    static inline fs::path Subfolder; // 子文件夹路径

private:
    using StreamType = std::ofstream; // 流类型

private:
    // 将值写入流
    template<typename InStreamType, typename T>
    static void WriteToStream(InStreamType& InStream, T Value)
    {
        InStream.write(reinterpret_cast<const char*>(&Value), sizeof(T));
    }

    // 将值写入流
    template<typename InStreamType, typename T>
    static void WriteToStream(InStreamType& InStream, T* Value, int32 Size)
    {
        InStream.write(reinterpret_cast<const char*>(Value), Size);
    }

private:
    // 修饰函数名
    static std::string MangleFunctionName(const std::string& ClassName, const std::string& FunctionName);

private:
    // 写入ReadMe
    static void WriteReadMe(StreamType& ReadMe);

    // 生成虚函数表名称
    static void GenerateVTableName(StreamType& IdmapFile, UEObject DefaultObject);
    // 生成类函数
    static void GenerateClassFunctions(StreamType& IdmapFile, UEClass Class);

public:
    // 生成
    static void Generate();

    /* Always empty, there are no predefined members for IDAMappings */
    /* 始终为空，IDA映射没有预定义成员 */
    static void InitPredefinedMembers() { }
    static void InitPredefinedFunctions() { }
};