# Dumper-7

所有虚幻引擎游戏的 SDK 生成器。支持所有 UE4 和 UE5 版本。

## 如何使用

- 在 x64-Release模式下编译 dll
- 将 dll 注入到目标游戏中
- SDK 会生成到 `Settings::SDKGenerationPath` 指定的路径，默认为 `C:\\Dumper-7`
- **请参阅 [UsingTheSDK](UsingTheSDK.md) 获取入门指南，或从旧版 SDK 迁移。**
## 支持我

KoFi: https://ko-fi.com/fischsalat \
Patreon: https://patreon.com/user?u=119629245

## 覆写偏移量

- ### 仅在生成器未找到偏移量或偏移量不正确时才覆写
- 所有覆写都在 **Generator.cpp** 的 **Generator::InitEngineCore()** 中进行

- GObjects (也请参阅 [覆写GObjects布局](#覆写gobjects布局))
  ```cpp
  ObjectArray::Init(/*GObjectsOffset*/, /*ChunkSize*/, /*bIsChunked*/);
  ```
  ```cpp
  /* 确保只使用 sdk 中存在的类型 (例如 uint8, uint64) */
  InitObjectArrayDecryption([](void* ObjPtr) -> uint8* { return reinterpret_cast<uint8*>(uint64(ObjPtr) ^ 0x8375); });
  ```
- FName::AppendString
  - 强制使用 GNames:
    ```cpp
    FName::Init(/*bForceGNames*/); // 如果 AppendString 偏移量错误，此项很有用
    ```
  - 覆写偏移量:
    ```cpp
    FName::Init(/*OverrideOffset, OverrideType=[AppendString, ToString, GNames], bIsNamePool*/);
    ```
- ProcessEvent
  ```cpp
  Off::InSDK::InitPE(/*PEIndex*/);
  ```
## 覆写 GObjects 布局
- 仅当您的游戏无法自动找到 GObjects 时才添加新布局。
- 布局覆写位于 `ObjectArray.cpp` 的大约第 30 行
- 对于 UE4.11 到 UE4.20，将布局添加到 `FFixedUObjectArrayLayouts`
- 对于 UE4.21 及更高版本，将布局添加到 `FChunkedFixedUObjectArrayLayouts`
- **示例:**
  ```cpp
  FFixedUObjectArrayLayout // 默认 UE4.11 - UE4.20
  {
      .ObjectsOffset = 0x0,
      .MaxObjectsOffset = 0x8,
      .NumObjectsOffset = 0xC
  }
  ```
  ```cpp
  FChunkedFixedUObjectArrayLayout // 默认 UE4.21 及以上
  {
      .ObjectsOffset = 0x00,
      .MaxElementsOffset = 0x10,
      .NumElementsOffset = 0x14,
      .MaxChunksOffset = 0x18,
      .NumChunksOffset = 0x1C,
  }
  ```

## 配置文件
您可以选择通过 `Dumper-7.ini` 文件动态更改设置，而不是修改 `Settings.h`。
- **针对单个游戏**: 在游戏 exe 文件所在的目录中创建 `Dumper-7.ini`。
- **全局**: 在 `C:\Dumper-7` 下创建 `Dumper-7.ini`。

示例:
```ini
[Settings]
SleepTimeout=100
SDKNamespaceName=MyOwnSDKNamespace
```
## 问题

如果您在使用 Dumper 时遇到任何问题，请在此仓库中创建一个 Issue\
并**详细**说明问题。

- 如果您的游戏在转储时崩溃，请将 Visual Studio 的调试器附加到游戏进程，并在调试配置中注入 Dumper-7.dll。
然后附上导致崩溃的异常截图、调用堆栈截图以及控制台输出。

- 如果 SDK 中出现任何编译器错误，请发送截图。请注意，**只有构建错误**才被视为错误，因为 Intellisense 经常报告误报。
请务必发送导致第一个错误的代码截图，因为它很可能引发连锁错误反应。

- 如果您自己的 dll 项目崩溃，请彻底检查您的代码，以确保错误确实出在生成的 SDK 中。
