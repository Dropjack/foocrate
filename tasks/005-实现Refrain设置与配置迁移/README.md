# 005 实现 Refrain 设置与配置迁移

- 状态：已验收
- TODO 步骤：`05 实现 Refrain 设置与配置迁移`
- Fork 提交标题：`实现 Refrain 设置与配置迁移`
- 前置提交：`abcb67b 已实现原生主界面外壳与播放控制`
- 对应规格：[`../../specs/modules/SETTINGS_AND_MIGRATION.md`](../../specs/modules/SETTINGS_AND_MIGRATION.md)
- 最后更新：2026-07-13

## 任务目标

建立唯一、正式、可升级的 Refrain 设置入口。用户可以在 foobar2000 Preferences 中编辑设置，也可通过真实齿轮或稳定菜单命令进入；配置升级时保留已知设置，异常值安全回退。

## 明确不做

- 不复制 Foobox 占用中央区域的设置页面。
- 不建立独立快捷键、更新器、下载器或版本追新提醒。
- 不实现主题、封面、歌词、评分、播放列表或步骤 14 的最终按钮排列。
- 不修改 Columns UI 布局、第三方配置、C 盘日常实例或只读参考目录。

## 中文程序逻辑

1. 组件用稳定 GUID 注册 `Display > Refrain` Preferences 页面和 `View > Refrain Settings...` 命令；底部齿轮调用同一页面 GUID。
2. 页面创建时读取并校验当前持久值，形成独立草稿。控件变化只通知 foobar2000 页面已改变，不立即写配置。
3. `Apply` 写入三个稳定字段并通知所有存活面板；`Cancel` 由 foobar2000 直接销毁未应用草稿；`Reset` 只恢复草稿默认值。
4. 配置读取先检查架构版本：旧版本按已知规则升级，非法枚举回退；未来版本不被旧代码降级或删除未知值。
5. 面板收到设置变更后重新计算时间、提示和齿轮命中区域并重绘，不重建播放服务，也不改变正在播放状态。
6. Dependencies 枚举已经加载的组件名称和版本。按钮只在用户点击时把批准的官方地址交给 Windows；检测和绘制本身不联网。

## 可检查步骤

### 步骤 1：稳定配置模型与迁移

- 输入：已批准设置规格与冻结 GUID。
- 动作：实现默认值、合法值校验、版本 0 到 1 迁移、未来版本保护和单元测试。
- 产物：与 UI 无关的配置规则和 foobar2000 持久化适配。
- 通过标准：非法枚举回退，应用/默认值行为可自动验证。
- 状态：已完成

### 步骤 2：Preferences 与稳定命令

- 输入：配置模型和 foobar2000 Preferences/主菜单接口。
- 动作：实现页面、Apply/Cancel/Reset、稳定命令和官方依赖入口。
- 产物：`Display > Refrain` 页面与 `View > Refrain Settings...`。
- 通过标准：取消不保存，应用立即生效，命令可由 foobar2000 绑定快捷键。
- 状态：已完成

### 步骤 3：已实现控制栏接入

- 输入：三项设置和存活 Refrain 面板。
- 动作：接入剩余时间、工具提示与齿轮显隐，并处理运行时通知。
- 产物：可见且可恢复的设置行为。
- 通过标准：隐藏齿轮后仍能从 Preferences/菜单恢复；播放控制无回归。
- 状态：已完成

### 步骤 4：构建、部署与人工验收

- 输入：完整代码和隔离开发实例。
- 动作：Debug/Release、测试、包审计、安全部署和启动检查。
- 产物：开发实例中的待验收组件。
- 通过标准：自动检查通过并提供可重复人工步骤。
- 状态：已完成

## 用户检查点

- [x] 用户已听取入口、稳定 GUID、命令和设置边界说明并要求开始实现。
- [x] 用户检查 Apply、Cancel、Reset、重启和损坏值回退。
- [x] 用户检查齿轮隐藏后仍可从 Preferences/菜单恢复。
- [x] 用户明确回复验收通过。

## 验证记录

- 2026-07-13：前置步骤已提交为 `abcb67b`，开始 05 时工作树干净。
- 2026-07-13：x64 Debug 与 Release 全量构建成功；自有代码继续使用 `/W4 /WX`，没有新增编译警告。
- 2026-07-13：Debug/Release 均通过 `component_identity`、`playback_state`、`settings_model` 三项测试；新增覆盖默认值、版本 0 到 1、非法枚举修复、未来版本保护和剩余时间格式。
- 2026-07-13：Release 安装包审计和模拟安装通过，包中只有根目录 `foo_refrain.dll`。最终 DLL SHA-256 为 `4D8C0556779C38E8A6FDB1A4B1B641A2D31070E560FEA2193E6EC1A9E8799D33`；安装包 SHA-256 为 `8B1CEA9AEC98D3A4B7F61650296BAD5FAA7D7682998D6AA26336082A5807C33A`。
- 2026-07-13：所有本轮维护文本严格通过 UTF-8 无 BOM、LF 检查，`git diff --check` 无格式错误。
- 2026-07-13：只部署到 `.local/foobar-dev/profile/user-components-x64/foo_refrain/`；最终开发进程 PID `31760` 实际加载该路径下唯一的最新 DLL，安装文件哈希与构建产物一致，进程保持运行且未产生新崩溃文件。检测到的 C 盘日常实例未被关闭或修改。
- 2026-07-13：用户完成 Preferences、设置入口、持久化、依赖状态和控制栏回归检查，明确回复“检查没问题”；步骤 05 正式通过验收。后续设置随已实现模块增加，必要时在稳定 `Display > Refrain` 入口下按功能增加非空子页面。

## 改动文件

- `specs/modules/SETTINGS_AND_MIGRATION.md`：冻结入口、三项设置、稳定身份、迁移、依赖检测和失败规则。
- `src/settings_model.h`：与 foobar2000 无关的默认值、合法值和版本迁移规则。
- `src/settings.h`、`src/settings.cpp`：稳定 GUID、持久化、运行时面板通知、Preferences 打开和组件版本检测。
- `src/preferences.cpp`：原生 Preferences 页面、Apply/Cancel/Reset、稳定主菜单命令和官方页面按钮。
- `src/playback_panel.cpp`、`src/playback_state.h`：真实齿轮、显隐、提示开关、总时长/剩余时间和即时设置响应。
- `tests/settings_model_tests.cpp`、`tests/playback_state_tests.cpp`：迁移与剩余时间回归检查。
- `CMakeLists.txt`、`src/component.cpp`：编译链接、测试目标和真实组件说明。

## 人工验收

1. 使用当前已经启动的 `.local/foobar-dev`，点击 Refrain 底部左侧齿轮；应直接打开 `File > Preferences > Display > Refrain`。
2. 关闭 Preferences，再从 `File > Preferences > Display > Refrain` 手动进入；还应在 `View > Refrain Settings...` 找到同一稳定命令。
3. 把 `Right-side time` 改为 `Remaining`，点击 `Apply`；播放有明确长度的歌曲，右侧时间应立即显示负号剩余时间。改回 `Total` 并应用，应恢复总时长。
4. 取消 `Show control tooltips` 并应用；悬停控制按钮不再弹提示。重新勾选并应用，提示恢复。
5. 取消 `Show settings button` 并应用；齿轮立即消失。不要关闭 Preferences，先确认其他播放按钮仍正常；随后仍可从当前页面重新勾选并应用。再测试隐藏后关闭页面，从 `File > Preferences > Display > Refrain` 或 `View > Refrain Settings...` 重新进入并恢复。
6. 修改任意选项但点击 `Cancel`；重新打开页面，确认未应用修改没有保存。点击 Preferences 的 `Reset page`，确认三个草稿值变为默认，但取消后旧设置仍保留；再次 Reset 并 Apply 才真正保存默认值。
7. 在 Dependencies 中确认三项显示：Columns UI `3.5.0`、ESLyric `1.0.6.7`、Playback Statistics `3.1.10`（组件公开版本文案可能带附加文字）；本轮不要求点击官方链接，如点击应只打开浏览器，不触发下载或安装。
8. 正常关闭并重启 `.local/foobar-dev`，确认已应用的时间、提示和齿轮设置保留；播放、暂停、切歌、停止、进度、静音和音量没有回归。
