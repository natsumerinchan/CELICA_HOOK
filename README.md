# CELICA_HOOK

一个基于Detours的C++编写的32位x86平台的游戏程序hook工具，主要用于游戏翻译。

## 功能特性

- **文件重定向**: 将游戏目录下的`CHSFiles`文件夹内容递归映射到游戏根目录
- **字体修改**: 修改游戏字体和字符集，支持中文显示
- **代码页转换**: 转换文本编码，支持日文到中文的转换
- **窗口标题修改**: 修改游戏窗口标题，支持ANSI和Unicode版本
- **日志系统**: UTF-8-BOM编码的详细日志输出
- **配置驱动**: 通过配置文件灵活控制各项功能

## 项目结构

```md
CELICA_HOOK/
├── CMakeLists.txt          # CMake构建配置
├── celica_hook.ini               # 配置文件模板
├── README.md              # 项目说明
├── detours/               # Detours库文件
│   ├── detours.h
│   └── detours.lib
└── src/                   # 源代码
    ├── main.cpp           # 主程序入口
    ├── settings.h         # 配置管理头文件
    ├── config_manager.cpp # 配置管理器实现
    ├── logger.h           # 日志系统头文件
    ├── logger.cpp         # 日志系统实现
    ├── hook_manager.h     # Hook管理器头文件
    ├── hook_manager.cpp   # Hook管理器实现
    ├── file_redirect_hook.h   # 文件重定向hook头文件
    ├── file_redirect_hook.cpp # 文件重定向hook实现
    ├── font_hook.h        # 字体hook头文件
    ├── font_hook.cpp      # 字体hook实现
    ├── codepage_hook.h    # 代码页hook头文件
    ├── codepage_hook.cpp  # 代码页hook实现
    ├── window_title_hook.h    # 窗口标题hook头文件
    ├── window_title_hook.cpp  # 窗口标题hook实现
    ├── utils.h            # 工具类头文件
    └── utils.cpp          # 工具类实现
```

## 编译说明

### 环境要求

- Windows10 或 Windows11
- Visual Studio 2026
- git

### 编译方法

#### 克隆本仓库

```pwsh
git clone https://github.com/natsumerinchan/CELICA_HOOK.git
```

#### 开始编译

使用Visual Studio 2026打开本项目文件夹，待`CMake 生成完毕`后  
在菜单栏的`生成`中执行`全部生成`即可。

## 使用方法

### 1. 配置文件设置

在游戏根目录创建`celica_hook.ini`文件，根据需求配置各项功能：

```ini
; CELICA_HOOK 配置文件
; 注释以分号开头

[General]
; 启用或禁用功能
EnableFileRedirect=1
EnableFontHook=1
EnableCodepageHook=1
EnableWindowTitleHook=1
EnableLogging=1

[FileRedirect]
; 文件重定向文件夹
RedirectFolder=CHSFiles

[Font]
; 字体配置
; 字体名称，留空使用系统默认
FontName=黑体
; 字符集 (十六进制)
; 0x80: Shift-JIS (日文)
; 0x81: (韩文)
; 0x86: GB2312 (简体中文)
; 0x88: BIG5 (繁体中文)
Charset=0x86
; 字体高度 (0表示不修改)
FontHeight=0
; 字体宽度 (0表示不修改)
FontWidth=0
; 字体粗细 (0表示不修改)
FontWeight=0

[Codepage]
; 代码页配置
; 原代码页 (游戏原始编码)
SourceCodepage=932
; 目标代码页 (要转换成的编码)
TargetCodepage=936

[WindowTitle]
; 窗口标题配置
; 是否启用标题检查 (1=启用, 0=禁用)
EnableTitleCheck=1
; 原标题 (即便其是乱码。留空表示不检查)
OriginalWindowTitle=
; 新标题 (留空表示不修改)
NewWindowTitle=

[Logging]
; 日志文件路径
LogFile=celica_hook.log
```

### 2. 文件重定向

- 在游戏根目录创建`CHSFiles`文件夹
- 将需要替换的游戏文件放入对应目录结构中
- 例如：`CHSFiles/data/text.txt` 会替换 `data/text.txt`

### 3. 字体配置

- **FontName**: 字体名称，留空使用系统默认
- **Charset**: 字符集 (十六进制)
  - `0x80`: Shift-JIS (日文)
  - `0x86`: GB2312 (简体中文)
  - `0x88`: BIG5 (繁体中文)
- **FontHeight**: 字体高度，0表示不修改
- **FontWidth**: 字体宽度，0表示不修改
- **FontWeight**: 字体粗细，0表示不修改

### 4. 代码页配置

- **SourceCodepage**: 原代码页 (游戏使用的编码)
  - `932`: 日文Shift-JIS
  - `936`: 简体中文GBK
  - `950`: 繁体中文BIG5
- **TargetCodepage**: 目标代码页 (要转换成的编码)

### 5. 窗口标题配置

**配置说明：**

- **EnableTitleCheck**: 启用/禁用标题检查功能
  - `1`: 启用标题检查，只有当原标题匹配时才修改
  - `0`: 禁用标题检查，直接修改所有窗口标题
- **OriginalWindowTitle**: 要匹配的原始窗口标题
  - 留空：匹配所有窗口标题
  - 设置具体标题：只有当窗口标题完全匹配时才修改
- **NewWindowTitle**: 修改后的新窗口标题
  - 留空：不修改标题
  - 设置具体标题：将窗口标题修改为此内容

**使用技巧：**

1. 如果不知道游戏的实际窗口标题，可以启用日志功能，在日志中查看实际捕获的窗口标题
2. 如果希望修改所有窗口标题，可以将 `OriginalWindowTitle` 留空
3. 如果希望禁用标题检查直接修改所有标题，可以设置 `EnableTitleCheck=0`

### 6. 修改DLL导入表

使用DLL导入表修改工具如Detours项目的setdll.exe(已放在仓库的tools文件夹)将编译生成的`CELICA_HOOK.dll`导入到目标游戏exe中。

## Hook的函数

### 文件重定向

- `CreateFileA`
- `CreateFileW`

### 字体修改

- `CreateFontA`
- `CreateFontW`
- `CreateFontIndirectA`
- `CreateFontIndirectW`
- `EnumFontFamiliesExA`
- `EnumFontFamiliesExW`

### 代码页转换

- `MultiByteToWideChar`
- `WideCharToMultiByte`

### 窗口标题修改

- `CreateWindowExA` - ANSI版本窗口创建函数
- `CreateWindowExW` - Unicode版本窗口创建函数
- `SetWindowTextA` - ANSI版本窗口标题设置函数
- `SetWindowTextW` - Unicode版本窗口标题设置函数

## 注意事项

1. 本项目仅支持32位x86应用程序
2. 使用前请备份重要文件
3. 某些游戏可能有反调试反作弊保护，注入前请确认
4. 日志文件会记录详细的hook操作，可用于调试

## 许可证

[MIT License](./LICENSE)

## 贡献

欢迎提交Issue和Pull Request来改进这个项目。

## Credits

- [microsoft/Detours](https://github.com/microsoft/Detours.git): Detours is a software package for monitoring and instrumenting API calls on Windows. It is distributed in source code form.
