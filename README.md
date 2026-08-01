# CELICA_HOOK

一个基于Detours的C++编写的游戏程序hook工具，支持32位x86和x86_64平台，主要用于游戏翻译。

## 功能特性

- **文件重定向**: 将游戏目录下的`CHSFiles`文件夹内容递归映射到游戏根目录
- **文件欺骗**: 隐藏特定文件或目录，假装文件不存在
- **字体修改**: 修改游戏字体和字符集，支持中文显示，支持字体免安装加载
- **窗口标题修改**: 修改游戏窗口标题，支持ANSI和Unicode版本
- **启动弹窗**: 游戏启动时显示作者信息弹窗，倒计时5秒后继续运行游戏
- **自动转区**: 在[LocaleEmulatorPlus-Core](https://github.com/julixian/LocaleEmulatorPlus-Core.git) 的作用下自动转区
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
│   ├── x86
│   │    ├── detours.lib
│   │    └── detours.h(软链接)
│   └── x64
│        ├── detours.lib
│        └── detours.h(软链接)
└── src/                   # 源代码
    ├── main.cpp           # 主程序入口
    ├── settings.h         # 配置管理头文件
    ├── config_manager.cpp # 配置管理器实现
    ├── author_window.h         # 弹窗头文件
    ├── author_window.cpp # 弹窗实现
    ├── launcher.cpp # 注入器实现
    ├── logger.h           # 日志系统头文件
    ├── logger.cpp         # 日志系统实现
    ├── hook_manager.h     # Hook管理器头文件
    ├── hook_manager.cpp   # Hook管理器实现
    ├── file_hook.h   # 文件hook头文件
    ├── file_hook.cpp # 文件hook实现
    ├── font_hook.h        # 字体hook头文件
    ├── font_hook.cpp      # 字体hook实现
    ├── window_title_hook.h    # 窗口标题hook头文件
    ├── window_title_hook.cpp  # 窗口标题hook实现
    ├── locale_emulator_plus.h      # 转区功能头文件
    ├── locale_emulator_plus.cpp    # 转区功能实现
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

#### 注入方法

- 在`CELICA_HOOK.ini`中的`TargetProcess=`配置目标程序，使用`CELICA_HOOK_LAUNCHER.exe`(可自行重命名)启动游戏
- 编译产物位于对应架构的输出目录（`bin\x86` 或 `bin\x64`），使用时需将
  `CELICA_HOOK.dll`、`CELICA_HOOK_LAUNCHER.exe` 与 `celica_hook.ini` 一同放入游戏根目录
- 若启用转区功能，还需将对应架构的 `LoaderDll_x86.dll`/`LoaderDll_x64.dll` 与
  `LocaleEmulatorPlus_x86.dll`/`LocaleEmulatorPlus_x64.dll` 放入游戏根目录（见下文转区说明）

## 使用方法

### 1. 配置文件设置

在游戏根目录创建`celica_hook.ini`文件，根据需求配置各项功能：

```ini
; CELICA_HOOK 配置文件
; 注释以分号开头

[General]
; 启用(=1)或禁用(=0)功能
EnableFileRedirect=1
EnableFileSpoofing=0
EnableFontHook=1
EnableWindowTitleHook=0
EnableLocaleEmulation=0
EnableLogging=0

; 目标进程名称，用于DLL注入器（例如：game.exe）
TargetProcess=

[File]
; 文件重定向文件夹
RedirectFolder=CHSFiles
; 检查重定向文件扩展名(0=禁用，1=启用)
EnableExtensionCheck=0
; 重定向文件扩展名(逗号分隔，如 .txt,.bin)
RedirectExtensions=.txt,.bin

; 文件欺骗功能配置
; 隐藏的文件路径列表(逗号分隔，基于程序根目录的相对路径(绝对路径亦可)，如 data\file.txt,config\settings.ini)
SpoofedFiles=
; 隐藏的目录路径列表(逗号分隔，基于程序根目录的相对路径(绝对路径亦可)，如 temp\logs,cache\data)
SpoofedDirectories=

[Font]
; 字体配置
; 字体名称，留空使用系统默认
FontName=VL ゴシック
; 自定义字体文件名（从游戏根目录加载，如：custom_font.ttf）
FontFileName=
; 字符集 (十六进制)
; 0x80: Shift-JIS (日文)
; 0x81: (韩文)
; 0x86: GB2312 (简体中文)
; 0x88: BIG5 (繁体中文)
Charset=0x80
; 字体高度 (0表示不修改)
FontHeight=0
; 字体宽度 (0表示不修改)
FontWidth=0
; 字体粗细 (0表示不修改)
FontWeight=0

; 字体hook细粒度控制 (1=启用, 0=禁用)
; CreateFontA
EnableCreateFontA=1
; CreateFontW
EnableCreateFontW=0
; CreateFontIndirectA
EnableCreateFontIndirectA=1
; CreateFontIndirectW
EnableCreateFontIndirectW=0

[WindowTitle]
; 窗口标题配置
; 是否启用标题检查 (1=启用, 0=禁用)
EnableTitleCheck=1
; 原标题 (即便标题为乱码亦可。留空表示不检查)
OriginalWindowTitle=
; 新标题 (留空表示不修改)
NewWindowTitle=

[LocaleEmulation]
; 转区功能配置
; 转区字符集直接使用[Font]中的Charset
; 代码页 (932=日文, 936=简体中文, 950=繁体中文)
LocaleCodepage=932
; 区域设置ID (1041=日文, 2052=简体中文, 1028=繁体中文)
LocaleId=1041
; 时区 (Tokyo Standard Time=东京时区, China Standard Time=中国标准时间)
Timezone=Tokyo Standard Time

[Logging]
; 日志文件路径
LogFile=celica_hook.log
```

### 2. 文件重定向

- **RedirectFolder**: 文件重定向文件夹，默认值为`CHSFiles`
  - 将需要替换的游戏文件放入对应目录结构中
  - 例如：`CHSFiles/data/text.txt` 会替换 `data/text.txt`
- **EnableExtensionCheck**: 检查重定向文件扩展名(0=禁用，1=启用)
- **RedirectExtensions**: 重定向文件扩展名(逗号分隔，如 .txt,.bin)

### 3. 文件欺骗

**功能说明：**

文件欺骗功能可以隐藏特定的文件或目录，让程序认为这些文件不存在。当程序尝试访问被欺骗的文件时，会返回文件不存在的错误。

**配置参数：**

- **SpoofedFiles**: 要欺骗的文件路径列表（逗号分隔，基于程序根目录的相对路径）
  - 例如：`data\file1.txt,config\settings.ini`
- **SpoofedDirectories**: 要欺骗的目录路径列表（逗号分隔，基于程序根目录的相对路径）
  - 例如：`temp\logs,cache\data`

**优先级说明：**

- **文件重定向优先级最高**：如果文件在重定向文件夹中存在，会优先进行重定向
- **文件欺骗优先级次之**：只有当文件不在重定向列表中时，才会检查是否在欺骗列表中
- **正常文件操作**：如果都不匹配，则执行正常的文件操作

**使用场景：**

- 1、目标游戏引擎支持免封包读取但需要重命名或删除原封包
- 2、隐藏翻译文件启动日文原版

### 4. 字体配置

- **FontName**: 字体名称，留空使用系统默认
- **FontFileName**: 自定义字体文件名（从游戏根目录加载，如：custom_font.ttf/.otf）
- **Charset**: 字符集 (十六进制)
  - `0x80`: Shift-JIS (日文)
  - `0x86`: GB2312 (简体中文)
  - `0x88`: BIG5 (繁体中文)
- **FontHeight**: 字体高度，0表示不修改
- **FontWidth**: 字体宽度，0表示不修改
- **FontWeight**: 字体粗细，0表示不修改
- 支持精准启用和禁用对`CreateFontA`、`CreateFontW`、`CreateFontIndirectA`及`CreateFontIndirectW`的hook

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
3. (危险操作)如果希望禁用标题检查直接修改所有标题，可以设置 `EnableTitleCheck=0`

### 6. 启动弹窗功能

**功能说明：**

- 游戏启动时会显示作者信息弹窗
- 倒计时5秒后弹窗会自动关闭并继续游戏
- 弹窗标题使用配置中的`NewWindowTitle`，如果未定义则使用默认标题
- 显示多个作者ID和主页链接（或作者在不同论坛使用不同的ID）
- 弹窗内的链接可点击并跳转

**弹窗内容：**

- 作者ID列表（支持多个论坛ID）
- 主页链接列表（支持多个主页）
- 附加说明和使用提示

**注意事项：**

- 弹窗信息在`settings.h`中定义，用户无法直接修改

### 7. Locale Emulator Plus (LEP) 转区功能配置

**功能说明：**

转区功能通过重新启动游戏进程来模拟目标区域的语言环境设置，解决某些游戏在非原生语言环境下运行的问题。
本功能基于 [LocaleEmulatorPlus-Core](https://github.com/julixian/LocaleEmulatorPlus-Core.git)，
由 LEP 的 `LoaderDll` 通过 `LepCreateProcess` 导出函数启动目标程序。

**配置参数：**

- **LocaleCodepage**: 原代码页 (游戏使用的编码)
  - `932`: 日文Shift-JIS
  - `936`: 简体中文GBK
  - `950`: 繁体中文BIG5
- **LocaleId**: 区域设置ID
  - `1041`: 日文
  - `2052`: 简体中文  
  - `1028`: 繁体中文
- **Timezone**: 时区设置
  - `Tokyo Standard Time`: 东京时区
  - `China Standard Time`: 中国标准时间

**重要说明：**

- 转区功能**直接使用[Font]中的Charset**
- 当系统代码页与目标代码页不同时，会自动触发转区操作
- 转区操作会重新启动游戏进程
- 需要自备 [LocaleEmulatorPlus-Core](https://github.com/julixian/LocaleEmulatorPlus-Core.git) 的 LoaderDll 与核心 DLL，且架构必须与游戏一致：
  - **32位x86游戏**：`LoaderDll_x86.dll` + `LocaleEmulatorPlus_x86.dll`
  - **x86_64游戏**：`LoaderDll_x64.dll` + `LocaleEmulatorPlus_x64.dll`
  - 请确保上述文件与 `CELICA_HOOK.dll`、`CELICA_HOOK_LAUNCHER.exe` 一同存在于游戏根目录

**工作流程：**

1. 检查当前系统代码页与目标代码页是否一致
2. 如果不一致，创建新的区域环境块(LEPB)
3. 加载对应架构的`LoaderDll_x86.dll`/`LoaderDll_x64.dll`并调用`LepCreateProcess`重新启动游戏进程
4. 新进程在目标区域环境下运行

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

1. 支持32位x86与x86_64应用程序，启动器和DLL的架构必须与目标程序一致（x86游戏用x86产物，x64游戏用x64产物）
2. 使用前请备份重要文件
3. 某些游戏可能有反调试反作弊保护，注入前请确认
4. 日志文件会记录详细的hook操作，可用于调试

## 许可证

[MIT License](./LICENSE)

## 贡献

欢迎提交Issue和Pull Request来改进这个项目。

## Credits

- [microsoft/Detours](https://github.com/microsoft/Detours.git): Detours is a software package for monitoring and instrumenting API calls on Windows. It is distributed in source code form.
- [julixian/LocaleEmulatorPlus-Core](https://github.com/julixian/LocaleEmulatorPlus-Core.git) :转区工具,[xupefei/Locale-Emulator](https://github.com/xupefei/Locale-Emulator.git)改进版
