# Xmake 构建指南
要安装 Xmake，请访问：https://xmake.io/#/zh-cn/getting_started?id=installation

有关 Xmake 命令的更多信息，请访问：https://xmake.io/#/zh-cn/getting_started

<br>

我还推荐 VSCode 的这个扩展：https://marketplace.visualstudio.com/items?itemName=tboox.xmake-vscode

## 配置项目
```bash
xmake f -p windows -a <架构> -m <构建模式>

# 示例
xmake f -p windows -a x64 -m release # 作为 Release 
xmake f -p windows -a x64 -m debug # 作为 Debug
```

## 全新构建
```bash
xmake clean
xmake build
```

## 要生成一个 Visual Studio 项目以使用 Xmake 构建，您可以使用：
```bash
xmake project -k vs -m "debug;release"
# 或
xmake project -k vsxmake2022 -m "debug;release"
```


<br><sub>作者：[@Omega172</sub>](https://github.com/Omega172) 
