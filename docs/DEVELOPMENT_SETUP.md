# FooCrate 开发环境与运行边界

- 最后核对日期：2026-07-13
- 作用：记录可以从新会话直接复核和使用的本机环境、第三方版本、实例边界、构建入口和部署规则
- 权威关系：若本文与 `docs/PROJECT_RULES.md` 冲突，以项目总则为准；版本变化后必须以实际文件重新核对并更新本文

## 1. 路径与用途

| 路径 | 用途 | 是否允许写入 |
| --- | --- | --- |
| `D:\Dev\FooCrate` | 项目根目录和 Git 仓库 | 允许，在当前任务范围内修改 |
| `D:\Dev\FooCrate\.local\foobar-dev` | 人工开发调试实例 | 允许部署和修改，但不得提交其运行数据 |
| `D:\Dev\FooCrate\.local\foobar-test` | 自动或隔离验证实例 | 允许部署和修改，但不得提交其运行数据 |
| `D:\Dev\foobar2000` | Foobox 行为与文件证据参考 | 严格只读，不部署、不保存设置、不清理 |
| C 盘日常 foobar2000 | 用户日常听歌与正式媒体库 | 完全不在项目范围内，不读取配置、不部署、不修改 |

`.local/` 已被 `.gitignore` 忽略。任何构建或测试脚本都必须要求显式目标为上述两个本地实例之一；不能把用户日常安装作为默认值或回退目标。

## 2. 已安装工具链

### 2.1 Visual Studio

- 产品：Visual Studio Community 2022
- 已核对产品版本：17.14.31
- 安装路径：`C:\Program Files\Microsoft Visual Studio\2022\Community`
- C++ 工具集目录版本：MSVC 14.44.35207（v143）
- 已核对 Windows SDK：10.0.26100.0；机器上另有 10.0.19041.0

本项目实际需要的 Visual Studio 能力是：

1. “使用 C++ 的桌面开发”核心功能；
2. MSVC v143 x64/x86 生成工具；
3. Windows 11 SDK 10.0.26100.0；
4. 用于 Windows 的 C++ CMake 工具。

测试适配器、Boost.Test、Google Test、IntelliCode、实时调试器、AddressSanitizer 等可以保留，但不是用户必须单独操作的构建前提。项目不得因为某个非必要可选项存在，就无理由把它变成运行时或构建硬依赖。

### 2.2 CMake 与 Ninja

- CMake：3.31.6-msvc6
- CMake 路径：`C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
- Ninja：1.12.1
- Ninja 路径：`C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe`

重要：当前普通 PowerShell 会话的 `PATH` 中没有 `cmake`。这不表示 CMake 未安装。后续构建应采用下列方式之一，并在工程建立后固定成可重复命令：

1. 从 Visual Studio Developer PowerShell/Developer Command Prompt 运行；或
2. 调用上面的完整路径；或
3. 由仓库内受控脚本发现 Visual Studio 安装目录后调用。

不得要求用户为了规避这个问题再次下载安装另一套 CMake。

## 3. 已纳入仓库的 SDK

### 3.1 foobar2000 SDK

- 版本：2025-03-07
- 目录组成：`third_party/foobar2000`、`third_party/libPPUI`、`third_party/pfc`
- 来源说明：`third_party/sdk-readme.html`
- 许可证：`third_party/sdk-license.txt`

仓库保留官方 SDK 的完整可构建源码、工程和原许可证。官方包中的预编译 `.dll`、`.lib` 不进入 Git；需要的库由源码构建到已忽略的构建目录。项目不得修改第三方 SDK 源码来迁就 FooCrate。

### 3.2 Columns UI SDK

- 版本：8.1.0
- 路径：`third_party/columns_ui-sdk-8.1.0`
- 版本证据：`ui_extension.h` 中的 `UI_EXTENSION_VERSION "8.1.0"`
- 许可证：`third_party/columns_ui-sdk-8.1.0/LICENCE`

该 SDK 用于实现 Columns UI 面板、根分割容器、颜色/字体服务和托管其他 Columns UI 面板。它是开发期源码依赖，不等同于用户运行时安装的 Columns UI 组件。

## 4. 两个隔离 foobar2000 实例

`foobar-dev` 和 `foobar-test` 均已实际存在并核对：

- foobar2000：2.25.10 x64；
- Columns UI：用户确认安装 3.5.0 stable；
- 两个实例中的 `foo_ui_columns.dll` SHA-256 相同：`CE0AC312C25F1685C0571F567D5D56E961957DF7DA4333B349B62B1DB7C5D368`。

两个实例的 `profile/user-components-x64` 中均已存在以下相同依赖：

| 组件 | 已安装版本 | dev/test DLL SHA-256 |
| --- | --- | --- |
| Columns UI | 3.5.0 stable | `CE0AC312C25F1685C0571F567D5D56E961957DF7DA4333B349B62B1DB7C5D368` |
| ESLyric x64 | 1.0.6.7 | `E03FCDC635787610AFFF2406F8B8F14E2BF5BA3002C222DBB0A5C7A27AF07285` |
| Playback Statistics x64 | 3.1.10 | `4674FC4013B45DB3A004F71A61E449ED08222B46223269DFA71F847934D59F19` |

ESLyric 的版本来自 DLL 版本资源；Playback Statistics DLL 没有 Windows 文件版本资源，版本由二进制内嵌组件元数据 `3.1.10` 与 foobar2000 Components 页面共同核对。组件二进制只存在于已忽略的 `.local`，不能从只读参考目录复制，也不能提交进仓库。

### 4.1 第三方组件更新边界

- FooCrate 只维护兼容性，不成为 ESLyric 或 Playback Statistics 的下载器、镜像或自动更新器。
- Playback Statistics 托管于 foobar2000 官方组件仓库，可优先使用 foobar2000 的 `Help > Check for updated components` 或 Preferences 的 Components 页面检查更新。
- ESLyric 由其 GitHub Releases 独立发布，不在 foobar2000 官方组件仓库时由用户从作者发布页取得 `.fb2k-component`，通过 Components 页面安装更新。
- 用户不必更新这些组件，也不必在它们每次发布时重新测试或重新打包 FooCrate；一个验证可用的版本组合可以长期冻结使用。
- 用户自行更新后，FooCrate 以所需能力是否仍存在为运行判断，不因版本号高于基线就拒绝启动。缺少歌词面板、评分命令或 `%rating%` 行为实际改变时，对应功能进入不兼容状态并提示，其他功能继续使用。
- 只有更新后发生真实故障，或者准备发布新的 FooCrate 版本时，维护流程才使用 `foobar-dev` 和 `foobar-test` 复现、修复与回归。日常用户更新不要求保留或运行这两个实例。
- FooCrate 的正式版本说明记录发布时实际测试的基线组合。这是可复现证据，不是要求用户永远停留在该版本，也不是要求项目追随每一次上游发布。

### 4.2 开发目录不是运行依赖

正式 `.fb2k-component` 安装到 C 盘日常 foobar2000 后，运行只需要该实例中的 FooCrate、Columns UI 及用户选择启用的外部组件。`D:\Dev\FooCrate`、源代码、SDK、构建目录、`foobar-dev` 和 `foobar-test` 都不是运行时依赖。

项目完成后可以把源代码推送到 Git 远端或制作源码归档，再删除本机 D 盘工作副本；已安装的 FooCrate 仍可继续使用。只有将来需要修改、重新构建或调查兼容故障时，才重新克隆/恢复项目并建立隔离实例。

### 4.3 第三方再分发状态

- Columns UI 官方组件页将项目标为开源，但 FooCrate 仍不重复打包其运行时二进制，用户从官方渠道独立安装。
- Playback Statistics 官方组件页提供下载，但未发现授予 FooCrate 再分发其二进制的明确许可。
- ESLyric 作者 release 仓库未列出明确的二进制再分发许可。
- 因此 FooCrate 当前对三者均采用“记录权威来源、检测安装状态、用户独立安装”的方案。未来若要合包，必须先取得并保存明确授权，不能把公开下载误认为再分发许可。

## 5. 测试期间如何避免影响日常听歌环境

1. 构建产物只部署到 `foobar-dev` 或 `foobar-test`。
2. 初期调试优先使用网络流、专门测试音频或独立测试目录，不让开发实例扫描用户日常媒体库。
3. 用户日常 C 盘 foobar2000 可以继续用于听歌，但测试脚本绝不能查找或控制它。
4. 如同时运行多个 foobar2000 实例导致文件关联、音频设备独占或全局快捷键冲突，先关闭开发/测试实例或调整测试输出设备；不得改动日常实例配置来迁就测试。
5. 参考实例只用于观察。任何可能保存布局、播放历史或配置的观察都要先确认不会写回参考目录；无法保证时停止并改用文件证据或副本方案。

### 5.1 测试责任

- Codex 负责配置工程、执行 Debug/Release 构建、运行自动测试、检查日志、部署到隔离实例并记录实际命令与结果。
- 用户不需要为了“可能有用”而安装一整套额外测试框架，也不需要手工运行编译命令。
- 用户负责的是明确、短小、可重复的可见行为验收，例如打开 `foobar-dev`、执行指定操作并确认界面结果。
- 自动测试通过只能说明“实现完成待验收”；只有用户明确确认后才能标记“已验收”。
- 如果一个测试工具确实成为必要依赖，必须先说明解决什么问题、为什么现有工具不够、安装的最小内容和卸载/升级影响，再请用户决定。

## 6. 构建、测试、打包与部署

### 6.0 自动部署期间的用户操作边界

- Codex 过程消息中出现“正在关闭、部署、重启、启动检查或读取已加载模块”时，用户暂不操作 `foobar-dev`，即使窗口已经可见也等待本轮明确交付。
- Codex 明确说“现在可以测试”或给出最终人工验收步骤后，自动部署与检查已经结束，用户可以正常操作开发实例。
- 用户在自动检查期间提前操作不会影响源代码，但可能与窗口关闭、DLL 替换、进程状态读取或布局保存发生竞争，导致检查结果失真。
- Codex 每次重启可见开发实例后都要明确说明当前是“仍在自动检查”还是“已交给用户操作”，不能只以窗口是否出现作为完成信号。

步骤 03 已建立并实际验证 Visual Studio 2022 x64 CMake 工程。普通 PowerShell 中继续使用 Visual Studio 自带程序的完整路径：

```powershell
$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'

& $cmake --preset vs2022-x64
& $cmake --build --preset debug --parallel
& $ctest --preset debug
& $cmake --build --preset release --parallel
& $ctest --preset release
& $cmake --build --preset package-release
```

产物位于已忽略的 `build/vs2022-x64`：

- Debug DLL：`Debug/foo_crate.dll`；
- Release DLL：`Release/foo_crate.dll`；
- 方便用户找到的正式安装包：仓库根目录 `dist/FooCrate-1.0.2.fb2k-component`。

`dist/` 与 `build/` 一样被 Git 忽略。`build/` 保存编译中间产物，`dist/` 只保存用户可以直接安装的最终包；人工安装时不需要再进入多层构建目录。

部署到开发实例前必须先关闭该实例，然后执行：

```powershell
& powershell -NoProfile -ExecutionPolicy Bypass -File scripts/deploy-component.ps1 `
  -Instance dev `
  -ComponentDll build/vs2022-x64/Release/foo_crate.dll
```

脚本只接受 `dev` 或 `test`，只接受仓库 `build/` 内名为 `foo_crate.dll` 的文件，并把绝对目标限制在 `.local/foobar-dev` 或 `.local/foobar-test`。它不能接收 C 盘或参考目录作为目标，也没有日常实例回退路径。

已验证的工程形态：

1. 构建目标为 Windows x64 的 foobar2000 组件 DLL。
2. Debug 与 Release 都必须可构建；Release 用于最终打包，Debug 用于开发实例诊断。
3. 最终主要交付物是 `.fb2k-component` 组件包，而不是独立 EXE 安装器。
4. 用户通过 foobar2000 的 Components 页面或打开 `.fb2k-component` 安装 FooCrate；不能要求手工把开发 DLL 复制进日常安装。
5. 如果 Columns UI 无法在不覆盖用户配置的前提下自动建立完整根布局，则额外提供明确版本的布局导入文件，并把导入、升级和回滚写入安装说明。不能未经确认覆盖用户现有 Columns UI 布局。
6. ESLyric、Playback Statistics 等第三方组件独立安装。除非其再分发许可经过明确核对，否则不得塞入 FooCrate 包。

步骤 03 的包只包含根目录下的 `foo_crate.dll`。foobar2000 x64 会把包内容解压到 `profile/user-components-x64/foo_crate/`，因此包内不得再增加 `x64/` 目录；否则 DLL 会落到多余的嵌套目录而无法加载。SDK 构建会输出临时 `shared.dll` 以产生与 foobar2000 自带 `shared.dll` 匹配的导入库，但打包和部署脚本不会复制该临时 DLL。

## 7. 环境复核命令原则

- PowerShell 读取仓库中文文本前按 `AGENTS.md` 初始化 UTF-8，并显式使用 `-Encoding UTF8`。
- 优先用 `vswhere.exe` 发现 Visual Studio，不硬编码用户机器一定装在相同盘符。
- 版本检查必须读取实际二进制或头文件证据，不仅依赖聊天中的版本号。
- 构建/部署命令必须先打印并校验解析后的绝对目标路径位于项目根目录或两个允许实例内。
- 所有本机路径、组件安装状态和哈希都属于可变化事实；发现变化时先更新本文，再继续依赖它执行。
