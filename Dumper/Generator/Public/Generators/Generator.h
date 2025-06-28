#pragma once

#include <filesystem>

#include "Unreal/ObjectArray.h"
#include "Managers/DependencyManager.h"
#include "Managers/MemberManager.h"
#include "HashStringTable.h"


namespace fs = std::filesystem;

// 生成器实现概念
template<typename GeneratorType>
concept GeneratorImplementation = requires(GeneratorType t)
{
    /* Require static variables of type */
    /* 需要类型的静态变量 */
    GeneratorType::PredefinedMembers;
    requires(std::same_as<decltype(GeneratorType::PredefinedMembers), PredefinedMemberLookupMapType>);

    GeneratorType::MainFolderName;
    requires(std::same_as<decltype(GeneratorType::MainFolderName), std::string>);
    GeneratorType::SubfolderName;
    requires(std::same_as<decltype(GeneratorType::SubfolderName), std::string>);

    GeneratorType::MainFolder;
    requires(std::same_as<decltype(GeneratorType::MainFolder), fs::path>);
    GeneratorType::Subfolder;
    requires(std::same_as<decltype(GeneratorType::Subfolder), fs::path>);
    
    /* Require static functions */
    /* 需要静态函数 */
    GeneratorType::Generate();

    GeneratorType::InitPredefinedMembers();
    GeneratorType::InitPredefinedFunctions();
};

// 生成器类
class Generator
{
private:
    friend class GeneratorTest;

private:
    static inline fs::path DumperFolder; // Dumper文件夹路径
    static inline bool bDumpedGObjects = false; // 是否已转储GObjects

public:
    // 初始化引擎核心
    static void InitEngineCore();
    // 初始化内部
    static void InitInternal();

private:
    // 设置Dumper文件夹
    static bool SetupDumperFolder();

    // 设置文件夹
    static bool SetupFolders(std::string& FolderName, fs::path& OutFolder);
    // 设置文件夹
    static bool SetupFolders(std::string& FolderName, fs::path& OutFolder, std::string& SubfolderName, fs::path& OutSubFolder);

public:
    // 生成
    template<GeneratorImplementation GeneratorType>
    static void Generate() 
    { 
        if (DumperFolder.empty())
        {
            if (!SetupDumperFolder())
                return;

            if (!bDumpedGObjects)
            {
                bDumpedGObjects = true;
                ObjectArray::DumpObjects(DumperFolder);

                if (Settings::Internal::bUseFProperty)
                    ObjectArray::DumpObjectsWithProperties(DumperFolder);
            }
        }

        if (!SetupFolders(GeneratorType::MainFolderName, GeneratorType::MainFolder, GeneratorType::SubfolderName, GeneratorType::Subfolder))
            return;

        GeneratorType::InitPredefinedMembers();
        GeneratorType::InitPredefinedFunctions();

        MemberManager::SetPredefinedMemberLookupPtr(&GeneratorType::PredefinedMembers);

        GeneratorType::Generate();
    };
};
