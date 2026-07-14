# 013-实现搜索筛选与 Album List 入口

- 状态：已验收（Playlist Filter 已在任务 15 按最新决定移除）
- 对应规格：[`../../specs/modules/PLAYLIST_FILTER_AND_ALBUM_LIST.md`](../../specs/modules/PLAYLIST_FILTER_AND_ALBUM_LIST.md)
- 前置任务：[`../012-实现播放列表管理侧栏/README.md`](../012-实现播放列表管理侧栏/README.md)（已验收并提交为 `b8fd427 实现播放列表管理侧栏`）
- Fork 提交标题：`实现搜索筛选与 Album List 入口`
- 最后更新：2026-07-14

## 任务目标

让用户在播放列表较多时按名称快速收窄左栏，同时从底部左侧打开熟悉的 foobar2000 原生 Album List。歌曲搜索继续使用 foobar2000 的 `Ctrl+F`，Refrain 不重复实现或改变它。

> 最新产品边界（2026-07-14）：本任务当时实现并验收的 Playlist Filter 已在任务 15 完整删除，不是隐藏。下面关于 Filter 的内容只保留为历史实施与验收记录；当前有效规则见模块规格 0.3。`Ctrl+F` 与原生 Album List 入口继续保留。

## 明确不做

- 不实现 Refrain 歌曲搜索、搜索结果播放列表或搜索历史。
- 不接管 `Ctrl+F`，不改变用户的 foobar2000 快捷键配置。
- 不嵌入或重写 Album List，不加入 ReFacets、JS Smooth Browser 或第二份媒体库。
- 不在本任务制作最终图标、颜色和精确间距。

## 中文程序逻辑

1. Filter 输入来自 Playlist Browser 顶部的原生 Windows 编辑框；播放列表名称和状态仍来自步骤 12 的 GUID 快照。
2. 程序只保存当前进程内的 Filter 文本和由它计算出的可见 GUID 行，不持久化查询，也不改写真实列表。
3. 输入变化、外部新建/删除/改名后，用大小写不敏感的名称包含关系重新计算可见行，再按 GUID 修复焦点、选择和滚动。
4. Filter 非空时禁止全局列表重排与空白建表；对可见普通列表的激活、菜单和行目标拖放仍按任务 12 权限执行。
5. 只有清除按钮能把文本清空并恢复全部行；Esc 不清除筛选。原生编辑框必须允许用户以任意当前输入法连续输入，输入和输入法组合过程中不能丢失焦点。
6. Album List 按钮请求 foobar2000 正式 `Album List` 主菜单命令；成功后配置与生命周期由 foobar2000 管理，命令缺失时只显示提示。

## 可检查步骤

### 步骤 1：冻结职责与参考证据

- 输入：用户决定、Foobox `base.js`/`jsspm.js`、foobar2000 主菜单 SDK。
- 动作：区分 Ctrl+F、Playlist Filter 和 Album List，固定入口位置与失败行为。
- 产物：模块规格 0.1。
- 用户检查方法：确认四项范围回复。
- 通过标准：不存在把 Album List 当播放列表搜索、或重复实现歌曲搜索的冲突。
- 状态：已完成

### 步骤 2：实现名称 Filter

- 输入：真实播放列表 GUID 快照与原生编辑框文本。
- 动作：实现 Unicode 输入、实时筛选、清除、无结果、外部变化和筛选期安全边界。
- 产物：可日常使用的 Playlist Browser 名称 Filter。
- 状态：已完成

### 步骤 3：实现 Album List 入口

- 输入：底部左侧按钮与 foobar2000 正式主菜单服务。
- 动作：实现临时图标、提示、命令调用和缺失反馈。
- 产物：不接管配置的原生 Album List 启动入口。
- 状态：已完成

### 步骤 4：构建、部署与验收

- 输入：完整实现和隔离开发实例。
- 动作：完成 Debug/Release、自动测试、包审计、仅 dev 部署和人工验收。
- 产物：可提交的任务 13 组件版本与证据。
- 状态：已完成并验收

## 用户检查点

- [x] 用户已核对功能范围。
- [x] 用户已读懂并认可职责边界。
- [x] 用户已核对入口位置与临时图标策略。
- [x] 用户已按人工验收步骤确认结果。

## 验证记录

- 2026-07-14：核对 `b8fd427` 为任务 12 提交，`c4edb56` 仅修改 AGENTS.md，工作树干净。
- 2026-07-14：只读确认 Foobox 底部 `LibBtn` 调用 `Library/Album List`；`jsspm.js` 把列表名称 Filter 与歌曲搜索分开；SDK 支持正式命令查找、执行和失败返回。
- 2026-07-14：名称 Filter 使用原生 Windows 编辑框实现，加入仅按钮清除、无匹配、实时外部变化、筛选期禁止重排/空白建表及 `Ctrl+F` 快捷键转交；Album List 使用正式主菜单命令查找与执行，缺失时明确提示。
- 2026-07-14：修复 Filter 每次文本变化都无条件取消重命名、进而把焦点移回主面板的问题；现在仅在重命名编辑器确实存在时取消重命名，Filter 可在任意输入法下连续输入，并移除 Esc 清空行为。
- 2026-07-14：焦点修复后 `x64 Debug`、`x64 Release` 均在 `/W4 /WX` 下构建成功；Debug/Release 各 9/9 自动测试通过。组件包 `dist/Refrain-0.1.0.fb2k-component` 已重新生成，SHA-256 为 `D9800137A1C264771334FB77BD09AF69AADB58AB7E723A19D44F14CA2B248BCF`。
- 2026-07-14：修复后的 Release DLL 已只部署到 `.local/foobar-dev`，构建与部署 DLL SHA-256 同为 `3D24BEC4CA071C743C07FE3C056ABD3662F64FB1BCC3065C011C41D62988D9EE`；等待用户人工确认连续输入、输入法组合及仅 `×` 清空行为。
- 2026-07-14：用户提供开发实例截图，确认 Filter 与左下 Playlist Workspace、Album Workspace、Album List、Settings 四按钮的布局无覆盖。为避免干扰用户前台游戏，后续可见检查由用户操作或提供截图，Codex 不循环抢占前台窗口。
- 2026-07-14：用户复测确认任务 13 验收通过，包括任意输入法连续输入不丢焦点、Esc 保留筛选文字、仅 `×` 清空，以及此前已确认的其余筛选与 Album List 行为；任务状态更新为已验收，等待用户提交。

## 人工验收步骤

1. 在 Filter 使用当前任意输入法连续输入多个字符，确认光标和焦点始终留在文本框内，输入法组合正常，并且只剩名称包含该文字的播放列表；拉丁字母大小写不同仍能匹配。
2. 按 Esc，确认筛选文字保持不变；点击 `×`，确认立即清空并恢复全部列表。
3. 输入不存在的名称，确认显示 `No matching playlists`，且没有创建或删除任何播放列表。
4. Filter 非空时，确认可见列表仍可激活、右键管理和接收曲目；列表之间不能拖动排序，空白处不能误建 `Dragged Items`。清空后排序与空白拖放恢复。
5. 在 Filter 有焦点和无焦点时分别按 `Ctrl+F`，确认仍执行你原有的 foobar2000 搜歌快捷键，而不是把 `F` 输入 Filter。
6. 点击左下第三个 Album List 图标，确认打开 foobar2000 原生 Album List；关闭再打开后，原生视图配置仍由 foobar2000 保留。

## 改动文件

- `specs/modules/PLAYLIST_FILTER_AND_ALBUM_LIST.md`：任务 13 的唯一模块规格。
- `tasks/013-实现搜索筛选与Album-List入口/README.md`：中文逻辑、步骤、验证和人工验收入口。
- `tasks/README.md`、`tasks/TODO.md`：推进当前路线并记录任务 12 提交。
- `src/playlist_browser_model.h`、`tests/playlist_browser_model_tests.cpp`：大小写不敏感的 Unicode 名称筛选模型和中英文/空结果回归测试。
- `src/playback_panel.cpp`：原生 Filter 编辑框与清除按钮、筛选期安全交互、左下 Album List 按钮和正式命令调用。

## 决策与未决问题

用户已核准功能范围、左下位置和临时图标策略。当前无产品未决问题；实现只需验证不同 foobar2000 安装中 Album List 正式命令是否存在，并按规格处理缺失。
