## Dumper-7 更新日志
**新增:**
- 添加了 TUObjectArrayWrapper，它在首次访问时自动初始化 GObjects
- 在 SDK 中添加了预定义成员 **ULevel::Actors**
- 添加了 FieldPathProperty 支持 (TFieldPath)
- 添加了 OptionalProperty 支持 (TOptional)
- 添加了 DelegateProperty 支持 (TDelegate)
- 添加了 FName::ToString 作为 FName::AppendString 在找不到 GNames 时的第二回退方案

**修改:**
- 静态函数现在在 SDK 中使用 `static` 关键字，并默认在其默认对象上调用
- Const 函数现在使用 `const` 关键字以进一步指明函数的作用

**修复:**
- 修复了类/结构体之间、枚举之间、成员/函数之间以及包之间的命名冲突
- 修复了循环依赖
- 修复了类上不正确的大小/对齐
- 修复了不正确的成员偏移
- 修复了 TWeakObjectPtr (测试 TagAtLastTest)
- 修复了某些游戏上无效的文件夹名称
- 修复了映射生成
- 修复了 BlueprintGeneratedClass 的 StaticClass (未经测试) 文件，直接在原文档中修改
