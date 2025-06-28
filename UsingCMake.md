# 在 Visual Studio Code 和 Visual Studio 中使用 CMake

本指南涵盖了在 Visual Studio Code 和 Visual Studio 中使用 CMake 的方法。

## 目录
- [先决条件](#先决条件)
- [安装 CMake](#安装-cmake)
- [在 Visual Studio Code 中使用 CMake](#在-visual-studio-code-中使用-cmake)
- [在 Visual Studio 中使用 CMake](#在-visual-studio-中使用-cmake)
- [常用 CMake 命令](#常用-cmake-命令)
- [故障排除](#故障排除)

## 先决条件

在开始使用 CMake 之前，请确保您已具备：

- C/C++ 编译器 (如 MSVC, GCC, 或 Clang)
    - **通常 cmake 已预装在每个 Visual Studio 中**
- Git (可选，但推荐用于版本控制)

## 安装 CMake

### 使用原生安装程序
1. **下载 CMake**:
    - 访问 [CMake 官方网站](https://cmake.org/download/)
    - 下载适用于您平台的安装程序
    - 对于 Windows，选择 Windows x64 Installer

2. **安装 CMake**:
   - 运行安装程序
   - 确保在安装过程中选择 "Add CMake to the system PATH"
   - 选择 "Add for all users" 或 "Add for current user"

3. **验证安装**:
   - 打开终端/命令提示符
   - 运行 `cmake --version`
   - 您应该能看到已安装的 CMake 版本

### 使用 Visual Studio 安装程序

1. **打开 Visual Studio Installer**:
    - 从桌面或开始菜单启动 Visual Studio Installer

2. **选择您已安装的产品**
    - 选择您已安装的产品 (Build Tools 或 Visual Studio) 并按 **Modify**
    - 转到 "Individual components" 选项卡

3. **查找并安装 cmake**
    - 在搜索框中输入 "cmake" 以快速找到该组件
    - 勾选 "C++ CMake tools for Windows" 和/或 "CMake tools for Windows"
    - 点击 "Modify" 安装所选组件

## 在 Visual Studio Code 中使用 CMake

### 设置

1. **安装所需扩展**:
   - [C/C++ Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack)
   - [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)

2. **配置项目**:
   - 打开包含 `CMakeLists.txt` 文件的文件夹
   - VS Code 应自动检测 CMake 项目
   - 点击侧边栏中的 CMake 图标访问 CMake 工具
   - 在提示时选择编译器/工具集


## 在 Visual Studio 中使用 CMake
1. 在 VS 中打开文件夹
    - 选择配置预设
    - 编译 (Ctrl+B)

## 故障排除

### 常见问题

1. **"CMake not found" 错误**:
   - 确保 CMake 在您的 PATH 中
   - 安装 CMake 后重启 VS Code/Visual Studio

2. **找不到编译器**:
   - 安装 C/C++ 编译器
   - 确保编译器在您的 PATH 中
   - 对于 Visual Studio Code，手动选择一个工具集 (F1 > CMake: Select a Kit)

3. **构建错误**:
   - 查看 CMake 输出面板以获取详细错误信息
   - 验证所有必需的库是否已安装
   - 检查 `CMakeLists.txt` 中是否有错误

4. **Visual Studio CMake 缓存不同步**:
   - 删除构建目录并重新配置
   - 项目 > 清理 CMake 缓存

### 获取帮助

- CMake 文档: [https://cmake.org/documentation/](https://cmake.org/documentation/)
- CMake Tools 扩展文档: [https://vector-of-bool.github.io/docs/vscode-cmake-tools/](https://vector-of-bool.github.io/docs/vscode-cmake-tools/)
- Visual Studio CMake 文档: [https://docs.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio](https://docs.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio)
