# 003 建立可安装的 x64 组件工程

- 状态：已验收
- TODO 步骤：`03 建立可安装的 x64 组件工程`
- Fork 提交标题：`建立可安装的 x64 组件工程`
- 前置提交：`3f5fe39 配置隔离测试依赖-完成`
- 对应规格：[`../../specs/PRODUCT_SPEC.md`](../../specs/PRODUCT_SPEC.md)
- 环境记录：[`../../docs/DEVELOPMENT_SETUP.md`](../../docs/DEVELOPMENT_SETUP.md)
- 最后更新：2026-07-13

## 任务目标

建立可以从干净构建目录重复生成 Windows x64 原生 foobar2000 组件的正式工程入口。完成后，Codex 能构建 Debug/Release、运行基础测试、生成标准 `.fb2k-component`、安全部署到隔离实例，并证明包中只有 Refrain 自己的 DLL。

本任务只建立组件身份和工程能力，不注册任何空白 Columns UI 面板。用户在 Components 页面会看到 Refrain 组件信息，但真正的根面板与播放控制从步骤 04 开始实现。

## 明确不做

- 不实现主界面、播放控制、设置页面或任何占位 UI。
- 不部署到 C 盘日常安装或 `D:\Dev\foobar2000`。
- 不把 SDK、foobar2000、Columns UI、ESLyric 或 Playback Statistics 二进制打进 Refrain 包。
- 不提交 `build/`、DLL、PDB、LIB、EXE 或 `.fb2k-component` 构建产物。
- 不把 Debug 构建冒充发布包。

## 中文程序逻辑

1. CMake 读取仓库内已批准的 foobar2000 SDK、PFC、libPPUI 与 Columns UI SDK 路径，并拒绝非 Windows、非 MSVC 或非 64 位目标。
2. 工程把第三方 SDK 源码编译到已忽略的构建目录；Refrain 代码只注册组件名称、版本、说明和固定文件名，不创建 UI 服务。
3. 稳定组件 GUID 与版本常量保存在源代码中，基础测试检查它们没有被意外清空或改变格式。
4. Debug 和 Release 使用相同源代码、动态 MSVC 运行库和 x64 ABI；Refrain 自有代码启用严格警告，第三方 SDK 警告不冒充 Refrain 新警告。
5. 打包脚本只接受当前 Release 构建中的 `foo_refrain.dll`，在根目录 `dist/` 生成容易找到的 `Refrain-0.1.0.fb2k-component`；包内 DLL 必须直接位于根目录，随后审计条目、模拟安装布局和哈希。
6. 部署脚本先解析目标绝对路径，只允许 `.local/foobar-dev` 或 `.local/foobar-test`；任何 C 盘、参考目录、仓库外路径或不明实例都必须拒绝。
7. 安装或卸载失败时不删除其他组件、不修改布局、不回退到日常实例；构建目录可以整体删除后重新生成。

## 可检查步骤

### 步骤 1：冻结工程和组件身份

- 输入：SDK 版本、目标平台、组件名称和固定 GUID。
- 动作：创建 CMake、版本资源、组件声明、任务文档和测试入口。
- 产物：可配置的 x64 工程，不含 UI 注册。
- 用户检查方法：无需阅读 C++；确认本步骤只会在 Components 页面增加 Refrain。
- 通过标准：配置阶段拒绝非 x64/MSVC；组件身份由仓库文件固定。
- 状态：已完成

### 步骤 2：构建与自动测试

- 输入：全新 Debug/Release 构建目录。
- 动作：配置、构建组件与测试，运行 CTest，检查新增警告。
- 产物：两个配置的 DLL、测试结果和构建记录。
- 用户检查方法：无需手工编译。
- 通过标准：Debug/Release 成功；测试通过；Refrain 自有代码无新增警告。
- 状态：已完成

### 步骤 3：打包、安全部署和包审计

- 输入：Release DLL 和两个允许实例路径。
- 动作：生成 `.fb2k-component`，核对压缩包条目，测试合法/非法部署目标，将组件部署到 `foobar-dev`。
- 产物：已忽略的发布包、哈希、路径保护测试与开发实例安装。
- 用户检查方法：暂不要求操作；Codex 完成自动部署后提供 Components 页面检查步骤。
- 通过标准：包中只有根目录 `foo_refrain.dll`，模拟解压后 DLL 直接位于 `user-components-x64/foo_refrain/`；非法目标全部拒绝；仅 dev 收到组件。
- 状态：已完成

### 步骤 4：开发实例人工验收

- 输入：已部署 Refrain 的 `foobar-dev`。
- 动作：用户查看组件信息，执行卸载/重装并确认无空白 Refrain 面板。
- 产物：人工验收记录。
- 用户检查方法：按任务完成后提供的短步骤操作。
- 通过标准：名称、版本、说明正确；卸载/重装正常；没有占位 UI。
- 状态：已完成

## 用户检查点

- [x] Components 页面显示 Refrain 名称、版本和说明。
- [x] Refrain 可以卸载并由生成包重新安装。
- [x] 安装后 foobar2000 正常，Columns UI 布局没有被覆盖。
- [x] 本步骤没有出现空白或无功能的 Refrain UI 面板。

## 验证记录

- 2026-07-13：确认前置提交为 `3f5fe39`，启动步骤 03 时工作树干净。
- 2026-07-13：`cmake --preset vs2022-x64` 成功，使用 MSVC 19.44.35226、Windows SDK 10.0.26100.0，生成器明确为 Visual Studio 2022 x64。
- 2026-07-13：Debug 与 Release 全量构建成功；Refrain 自有代码使用 `/W4 /WX` 且无警告。
- 2026-07-13：Debug/Release 的 `component_identity` CTest 均通过；全新 `build/clean-check` 目录也完成 Release 构建、测试和打包。
- 2026-07-13：第三方未修改源码产生 5 处 C4996：PFC 的 `GetVersion`、shared 的 `GetVersionExW`、SDK 的 3 处旧 metadb 调用。它们来自官方 SDK 自身，未通过修改或全局屏蔽第三方代码伪装修复，不影响 Refrain `/WX` 门槛。
- 2026-07-13：Win32 配置被 CMake 拒绝；打包脚本拒绝 Debug DLL；部署脚本拒绝未知实例和 `build/` 外 DLL。
- 2026-07-13：首次安装包误用 `x64/foo_refrain.dll`。用户通过 Components 页面安装重启后组件消失；磁盘证据显示 foobar2000 将其原样解压成 `profile/user-components-x64/foo_refrain/x64/foo_refrain.dll`，比加载位置多一层目录。任务因此从待验收退回实现中，打包约束改为根目录 DLL，并增加模拟安装布局测试。
- 2026-07-13：修正版安装包审计只有根目录 `foo_refrain.dll`，模拟解压后 DLL 直接位于组件目录根部；反向测试确认旧式 `x64/foo_refrain.dll` 包会被验证脚本拒绝。
- 2026-07-13：修正后 Release DLL SHA-256 仍为 `CC141FE4FD0C66E3DB1E5E3B337716B033D14155DAF21ACA379199A828CCC85C`；安装包 SHA-256 为 `ABCDC50B32D75C5003D9FF6A74626BE2E793C82346C82BD107D8DFFD3F68730F`。
- 2026-07-13：用户使用修正版安装包完成卸载、安装与重启检查，明确回复“通过验收”；步骤 03 正式标记为已验收。
- 2026-07-13：Release DLL 只部署到 `foobar-dev`，部署后哈希一致；后台启动保持运行 5 秒，随后通过正式 Exit 命令正常退出，未生成新崩溃文件；`foobar-test` 未部署 Refrain。

## 改动文件

- `CMakeLists.txt`：x64/MSVC 门槛、组件目标、严格警告、测试和打包目标。
- `CMakePresets.json`：Visual Studio 2022 x64 的配置、Debug/Release 构建与测试入口。
- `cmake/Foobar2000Sdk.cmake`：从源码构建 PFC、foobar2000 SDK、component client 与 shared 导入库，并接入 Columns UI SDK/libPPUI 头文件目标。
- `src/component_identity.h`：稳定组件名称、版本、文件名和 GUID。
- `src/component.cpp`：foobar2000 组件元数据和 SDK 版本编译检查；不注册 UI。
- `src/foo_refrain.rc`：Windows DLL 版本资源。
- `tests/component_identity_tests.cpp`：组件身份基础测试。
- `tests/verify-package.ps1`：安装包条目审计。
- `scripts/package-component.ps1`：只从 Release DLL 生成标准 x64 组件包，并统一输出到已忽略的根目录 `dist/`。
- `scripts/deploy-component.ps1`：仅向两个隔离实例部署的绝对路径保护。
- `docs/DEVELOPMENT_SETUP.md`：实际可复制的构建、测试、打包和部署命令。
- `tasks/README.md`、`tasks/TODO.md`、任务 002/003：验收状态、当前入口和验证证据。

## 决策与未决问题

- 组件内部稳定 GUID：`5AEAC28D-ED01-4574-A99B-F9A244B5B815`。
- 初始工程版本使用 `0.1.0`；它代表工程骨架，不是 Refrain 1.0 产品完成度。
- x64 单组件包中的 `foo_refrain.dll` 直接位于包根目录；目标架构由安装实例的 `user-components-x64` 存储位置决定，包内不重复增加 `x64/` 层。
- SDK 源文件组合、系统链接库与包格式已经由两个构建配置和全新目录重建证明。

## 用户人工验收

1. 启动 `.local/foobar-dev/foobar2000.exe`。
2. 打开 `File > Preferences > Components`，确认列表存在 `Refrain 0.1.0`。
3. 双击或查看组件信息，确认说明以 `Native integrated Columns UI component for foobar2000.` 开头。
4. 确认 Columns UI 当前布局没有变化，布局编辑器中也没有可添加的 Refrain 空白面板。
5. 在 Components 页面卸载 Refrain并正常重启。
6. 在 `foobar-dev` 的 Components 页面点击 `Install...`，选择根目录 `dist/Refrain-0.1.0.fb2k-component`，确认并重启，再确认 `Refrain 0.1.0` 恢复。不要从资源管理器双击安装包，以免系统文件关联把它交给 C 盘日常实例。
7. 正常关闭 `foobar-dev`，确认没有错误提示。不要对 `foobar-test` 或 C 盘日常实例执行本轮操作。
