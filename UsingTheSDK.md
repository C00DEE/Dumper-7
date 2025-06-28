# 使用 SDK 创建一个简单的 DLL 项目

## Visual Studio 项目设置
1. 创建一个新的空 C++ 项目
2. 前往 `项目 > 属性`
3. 将 `配置类型` 从 `应用程序 (.exe)` 设置为 `动态库 (.dll)`
4. 在同一设置中，将 `C++ 语言标准` 从 `默认 (ISO C++14 标准)` 设置为 `预览 - 最新...`
5. 点击应用并关闭设置 \
  ![image](https://github.com/Encryqed/Dumper-7/assets/64608145/e0170247-631d-466d-91c7-94a1c55b34a1)
7. 将您的构建配置从 `x86` 切换到 `x64` \
   ![image](https://github.com/Encryqed/Dumper-7/assets/64608145/5f8963c1-4e55-4f3a-b080-26445d585c86)
8. 在您的项目中添加一个 `Main.cpp` 文件
9. 添加一个 `DllMain` 函数和一个 `MainThread` 函数 (代码见[此处](#代码))

## 将 SDK 包含到项目中
1. 获取您的 CppSDK 文件夹的内容 (默认为 `C:\\Dumper-7\\GameName-GameVersion\\CppSDK`) \
   ![image](https://github.com/Encryqed/Dumper-7/assets/64608145/5a9404a7-1b49-4fd2-a3fa-a7467f18f39a)
2. 将内容拖放到您的 VS 项目目录中 \
  ![image](https://github.com/Encryqed/Dumper-7/assets/64608145/14d4bb1b-8a23-43f8-8994-8bdae25af005)
3. 如果您不关心项目的编译时间，请在 `Main.cpp` 文件顶部添加 `#include "SDK.hpp"`
4. 如果您**确实**关心，并希望获得更快的编译时间，请直接仅包含您需要的文件。 \
    在这种情况下，添加 `#include "SDK/Engine_classes.hpp"` 是一个不错的开始。
5. 将 `Basic.cpp` 和 `CoreUObject_functions.cpp` 添加到您的 VS 项目中
6. 如果您从 SDK 调用一个函数，您需要将包含该函数体的 .cpp 文件添加到您的项目中。 \
   示例: \
   从 `APlayerController` 调用 `GetViewportSize()` 需要您将 `Engine_functions.cpp` 添加到您的项目中。 \
   ![image](https://github.com/Encryqed/Dumper-7/assets/64608145/c9ecf0c7-ec73-4e6a-8c6d-d7c86c26b5c8)
7. 尝试构建 SDK 后，转到“错误列表”并确保选择 **`仅生成`**
  ![image](https://github.com/user-attachments/assets/cd72d55e-64de-4134-a115-6a9a0af80baa)
8. 如果在构建过程中有任何 static_asserts 失败或其他错误发生，请阅读 [ReadMe](README.md#问题) 的 [问题](README.md#问题) 部分

## 使用 SDK
### 1. 检索类/结构体的实例以操纵它们
   - FindObject，用于通过名称查找对象
     ```c++
     SDK::UObject* Obj1 = SDK::UObject::FindObject("ClassName PackageName.Outer1.Outer2.ObjectName");
     SDK::UObject* Obj2 = SDK::UObject::FindObjectFast("ObjectName");

     SDK::UObject* Obj3 = SDK::UObject::FindObjectFast("StructName", EClassCastFlags::Struct); // 查找一个 UStruct
     ```
   - StaticFunctions / GlobalVariables，用于从静态变量中检索类实例
     ```c++
     /* UWorld::GetWorld() 替代 GWorld，无需偏移量 */
     SDK::UWorld* World = SDK::UWorld::GetWorld();
     SDK::APlayerController* MyController = World->OwningGameInstance->LocalPlayers[0]->PlayerController;
     ```
### 2. 调用函数
  - 非静态函数
    ```c++
    SDK::APlayerController* MyController = MagicFuncToGetPlayerController();
    
    float OutX, OutY;
    MyController->GetMousePosition(&OutX, &OutY);
    ```
  - 静态函数
    ```c++
    /* 静态函数不需要实例，它们会自动使用其 DefaultObject 调用 */
    SDK::FName MyNewName = SDK::UKismetStringLibrary::Conv_StringToName(L"DemoNetDriver");
    ```
### 3. 检查 UObject 的类型
  - 使用 EClassCastFlags
    ```c++
    /* 仅限于少数几种类型，但速度快 */
    const bool bIsActor = Obj->IsA(EClassCastFlags::Actor);
    ```
  - 使用 `UClass*`
    ```c++
    /* 适用于每个类，但速度较慢 (比字符串比较快) */
    const bool bIsSpecificActor = Obj->IsA(ASomeSpecificActor::StaticClass());
    ```
### 4. 类型转换
  虚幻引擎严重依赖继承，并经常使用指向基类的指针，这些指针稍后会被赋予以 \
  子类实例的地址。
  ```c++
  if (MyController->Pawn->IsA(SDK::AGameSpecificPawn::StaticClass()))
  {
      SDK::AGameSpecificPawn* MyGamePawn = static_cast<SDK::AGameSpecificPawn*>(MyController->Pawn);
      MyGamePawn->GameSpecificVariable = 30; 
  }
  ```

## 代码
### DllMain 和 MainThread
```c++
#include <Windows.h>
#include <iostream>

DWORD MainThread(HMODULE Module)
{
        /* 打开控制台窗口的代码 */
        AllocConsole();
        FILE* Dummy;
        freopen_s(&Dummy, "CONOUT$", "w", stdout);
        freopen_s(&Dummy, "CONIN$", "r", stdin);

        // 你的代码在这里

        return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
        switch (reason)
        {
                case DLL_PROCESS_ATTACH:
                CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, 0);
                break;
        }

        return TRUE;
}
```
### 启用虚幻引擎控制台的示例程序
```c++
#include <Windows.h>
#include <iostream>

#include "SDK/Engine_classes.hpp"

// Basic.cpp 已添加到 VS 项目
// Engine_functions.cpp 已添加到 VS 项目

DWORD MainThread(HMODULE Module)
{
    /* 打开控制台窗口的代码 */
    AllocConsole();
    FILE* Dummy;
    freopen_s(&Dummy, "CONOUT$", "w", stdout);
    freopen_s(&Dummy, "CONIN$", "r", stdin);

    /* 返回"静态"实例的函数 */
    SDK::UEngine* Engine = SDK::UEngine::GetEngine();
    SDK::UWorld* World = SDK::UWorld::GetWorld();

    /* 获取 PlayerController, World, OwningGameInstance, ... 都应检查不为 nullptr！ */
    SDK::APlayerController* MyController = World->OwningGameInstance->LocalPlayers[0]->PlayerController;

    /* 打印对象的全名 ("ClassName PackageName.OptionalOuter.ObjectName") */
    std::cout << Engine->ConsoleClass->GetFullName() << std::endl;

    /* 手动迭代 GObjects 并打印每个作为 Pawn 的 UObject 的全名 (不推荐) */
    for (int i = 0; i < SDK::UObject::GObjects->Num(); i++)
    {
        SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);

        if (!Obj)
            continue;

        if (Obj->IsDefaultObject())
            continue;

        /* 只需要使用 cast flags 的 'IsA' 检查，另一个 'IsA' 是多余的 */
        if (Obj->IsA(SDK::APawn::StaticClass()) || Obj->HasTypeFlag(SDK::EClassCastFlags::Pawn))
        {
            std::cout << Obj->GetFullName() << "\n";
        }
    }

    /* 您可能需要遍历 UWorld::Levels 中的所有关卡 */
    SDK::ULevel* Level = World->PersistentLevel;
    SDK::TArray<SDK::AActor*>& Actors = Level->Actors;

    for (SDK::AActor* Actor : Actors)
    {
        /* 第2和第3个检查是相等的，如果您的类可用，请首选使用 EClassCastFlags。 */
        if (!Actor || !Actor->IsA(SDK::EClassCastFlags::Pawn) || !Actor->IsA(SDK::APawn::StaticClass()))
            continue;

        SDK::APawn* Pawn = static_cast<SDK::APawn*>(Actor);
        // 在这里使用 Pawn
    }

    /* 
    * 更改用于打开 UE 控制台的键盘按键
    * 
    * 这是一个罕见的更改 DefaultObject 成员变量的案例。
    * 默认情况下您不希望使用 DefaultObject，这是一个罕见的例外。
    */
    SDK::UInputSettings::GetDefaultObj()->ConsoleKeys[0].KeyName = SDK::UKismetStringLibrary::Conv_StringToName(L"F2");

    /* 创建一个由 Engine->ConsoleClass 指定的类类型的新 UObject */
    SDK::UObject* NewObject = SDK::UGameplayStatics::SpawnObject(Engine->ConsoleClass, Engine->GameViewport);

    /* 我们创建的对象是 UConsole 的子类，所以这个转换是 **安全** 的。 */
    Engine->GameViewport->ViewportConsole = static_cast<SDK::UConsole*>(NewObject);

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, 0);
        break;
    }

    return TRUE;
}
```
