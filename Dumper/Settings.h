#pragma once

#include <string>

#include "Unreal/Enums.h"


namespace Settings
{
	// 全局配置文件路径
	inline constexpr const char* GlobalConfigPath = "C:/Dumper-7/Dumper-7.ini";

	namespace Config
	{
		// 以毫秒为单位的休眠超时时间
		inline int SleepTimeout = 0;
		// SDK的命名空间名称
		inline std::string SDKNamespaceName = "SDK";

		// 加载配置
		void Load();
	};

	namespace EngineCore
	{
		/* 一个特殊的设置，用于修复UEnum::Names，其中类型有时是TArray<FName>，有时是TArray<TPair<FName, Some8ByteData>> */
		constexpr bool bCheckEnumNamesInUEnum = false;
	}

	namespace Generator
	{
		//如果未提供覆盖，则自动生成
		inline std::string GameName = "";
		inline std::string GameVersion = "";

		// SDK生成路径
		inline constexpr const char* SDKGenerationPath = "C:/Dumper-7";
	}

	namespace CppGenerator
	{
		/* 文件无前缀->FilePrefix = "" */
		constexpr const char* FilePrefix = "";

		/* 参数没有单独的命名空间 -> ParamNamespaceName = nullptr */
		constexpr const char* ParamNamespaceName = "Params";

		/* 功能当前不受支持/无法正常工作。*/
		/* 不要对字符串进行异或运算 -> XORString = nullptr。与 https://github.com/JustasMasiulis/xorstr 不同的自定义XorStr实现可能需要更改CppGenerator.cpp中的 'StringLiteral' 结构体。*/
		constexpr const char* XORString = nullptr;

		/* Cpp代码的可自定义部分，以允许自定义 'uintptr_t InSDKUtils::GetImageBase()' 函数 */
		constexpr const char* GetImageBaseFuncBody = 
R"({
	return reinterpret_cast<uintptr_t>(GetModuleHandle(0));
}
)";
		/* Cpp代码的可自定义部分，以允许自定义 'InSDKUtils::CallGameFunction' 函数 */
		constexpr const char* CallGameFunction =
R"(
	template<typename FuncType, typename... ParamTypes>
	requires std::invocable<FuncType, ParamTypes...>
	inline auto CallGameFunction(FuncType Function, ParamTypes&&... Args)
	{
		return Function(std::forward<ParamTypes>(Args)...);
	}
)";
		/* 一个选项，强制SDK中的UWorld::GetWorld()函数通过UEngine的实例来获取世界。对于Dumper找到错误GWorld偏移的游戏很有用。*/
		constexpr bool bForceNoGWorldInSDK = false;

		/* 这将允许用户在SDK中手动初始化全局变量地址（例如GObjects、GNames、AppendString）。*/
		constexpr bool bAddManualOverrideOptions = true;
	}

	namespace MappingGenerator
	{
		/* MappingGenerator是否应检查名称是否已写入名称表。用于减小映射大小。*/
		constexpr bool bShouldCheckForDuplicatedNames = true;

		/* 是否应从映射文件中排除EditorOnly属性。*/
		constexpr bool bExcludeEditorOnlyProperties = true;

		/* 生成文件时使用的压缩方法。*/
		constexpr EUsmapCompressionMethod CompressionMethod = EUsmapCompressionMethod::ZStandard;
	}

	/* 部分实现 */
	namespace Debug
	{
		// 是否生成断言文件
		inline constexpr bool bGenerateAssertionFile = false;

		/* 为结构体大小和对齐添加static_assert */
		inline constexpr bool bGenerateInlineAssertionsForStructSize = true;

		/* 为成员偏移添加static_assert */
		inline constexpr bool bGenerateInlineAssertionsForStructMembers = true;


		/* 在映射生成期间打印调试信息 */
		inline constexpr bool bShouldPrintMappingDebugData = false;
	}

	//* * * * * * * * * * * * * * * * * * * * *// 
	// **不要** 更改这些设置中的任何一个 //
	//* * * * * * * * * * * * * * * * * * * * *//
	namespace Internal
	{
		/* UEnum::Names是仅存储枚举值的名称，还是存储一个Pair<Name, Value> */
		inline bool bIsEnumNameOnly = false; // EDemoPlayFailure

		/* Pair<Name, Value> UEnum::Names中的 'Value' 组件是否为uint8值，而不是默认的int64 */
		inline bool bIsSmallEnumValue = false;

		/* TWeakObjectPtr是否包含 'TagAtLastTest' */
		inline bool bIsWeakObjectPtrWithoutTag = false;

		/* 此游戏引擎版本是使用FProperty还是UProperty */
		inline bool bUseFProperty = false;

		/* 此游戏引擎版本是使用FNamePool还是TNameEntryArray */
		inline bool bUseNamePool = false;

		/* UObject::Name或UObject::Class哪个在前。影响fixup代码中FName大小的计算。在Off::Init()之后不使用；*/
		inline bool bIsObjectNameBeforeClass = false;

		/* 此游戏是否使用区分大小写的FNames，将int32 DisplayIndex添加到FName */
		inline bool bUseCasePreservingName = false;

		/* 此游戏是否使用FNameOutlineNumber，将 'Number' 组件从FName移动到FNamePool中的FNameEntry */
		inline bool bUseOutlineNumberName = false;


		/* 此游戏引擎版本是否使用constexpr标志来确定FFieldVariant是持有UObject*还是FField* */
		inline bool bUseMaskForFieldOwner = false;

		/* 此游戏引擎版本是否对FVector使用double而不是float。也就是引擎版本是否为UE5.0或更高。*/
		inline bool bUseLargeWorldCoordinates = false;
	}
}
