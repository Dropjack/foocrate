# 012-实现播放列表管理侧栏

- 状态：已验收
- 对应规格：[`../../specs/modules/PLAYLIST_BROWSER.md`](../../specs/modules/PLAYLIST_BROWSER.md)
- 前置任务：[`../011-实现专辑封面浏览器与专辑曲目列表/README.md`](../011-实现专辑封面浏览器与专辑曲目列表/README.md)（已验收并提交为 `b924747 实现专辑封面浏览器与专辑曲目列表`）
- Fork 提交标题：`实现播放列表管理侧栏`
- 最后更新：2026-07-22

## 任务目标

完成 `Playlist Workspace` 左侧的 `Playlist Browser`。它显示 foobar2000 已存在的普通、自动和 FooCrate 保留播放列表，区分当前活动与正在播放状态，并提供安全的新建、重命名、复制、删除、排序、默认列表和曲目拖放入口。FooCrate 不维护第二份播放列表数据库，也不重写用户现有自动列表查询。

## 明确不做

- 不在步骤 12 实现“搜索当前列表”、按名称筛选播放列表或原生 Album List 入口；这些属于步骤 13。
- 不复制 Foobox 的互联网电台、搜索历史列表或强制置顶 `Library` 自动列表。用户已要求加入基于 foobar2000 正式查询引擎的 Foobox 原理预设自动列表。
- 不在侧栏重复提供 Clear、Remove Duplicates、Remove Dead Items 等曲目内容清理命令；继续使用 foobar2000 正式菜单。
- 不制作步骤 14–16 的最终图标、颜色和精确视觉，但所有真实状态必须可区分。
- 未经用户核准，不通过双击播放列表名称启动其中某个同索引曲目，也不在空白区域拖放时静默创建列表。

## 当前中文程序逻辑草案

1. 输入来自 `playlist_manager_v5` 的播放列表 GUID、名称、顺序、项目数、活动列表和正在播放列表，以及 `autoplaylist_manager` 与 playlist lock 的类型/权限信息。
2. UI 只保存管理选择、滚动位置、左栏分隔比例和用户选择的 `Default Playlist` GUID；播放列表内容、顺序、自动查询和生命周期仍由 foobar2000 持有。
3. 外部创建、删除、改名、重排、锁定或内容变化后，按 GUID 合并刷新；不能把数组索引当稳定身份，也不能让确认框关闭后作用到另一个列表。
4. 自动列表通过正式 autoplaylist API 识别；`FooCrate Album View` 通过步骤 11 保存的桥接 GUID 识别。普通、自动、保留三种类型使用不同菜单和拖放权限。
5. 新建、重命名、复制、删除和拖放先冻结目标 GUID/来源快照，再检查当前权限，成功后才更新选择；失败、取消或目标变化都不能丢失来源曲目或修改自动查询。
6. 设置使用稳定 GUID 和版本化迁移；默认列表消失、列表为空、自动列表提供者缺失、锁状态变化、窗口销毁或异步拖放晚到时安全回退并给出明确反馈。

## 可检查步骤

### 步骤 1：核准管理与启动规则

- 输入：产品规格、Foobox 左栏截图与 `jsspm.js`、步骤 10/11 决策、foobar2000 自动列表与锁 API。
- 动作：核准 Default Playlist、单击/多选、自动列表、保留列表、删除和拖放语义。
- 产物：`PLAYLIST_BROWSER.md` 0.2 可实现规格。
- 用户检查方法：逐项回复规格第 8 节选择。
- 通过标准：不存在会误删列表、误改查询或改变启动行为的未决问题。
- 状态：已完成

### 步骤 2：实现稳定播放列表模型与设置

- 输入：核准规格和真实 playlist/autoplaylist/lock 快照。
- 动作：实现 GUID 身份、类型/权限、活动/播放/管理选择、默认列表回退和纯模型测试。
- 产物：可测试的 Playlist Browser 状态模型和版本化设置。
- 状态：已完成

### 步骤 3：实现固定左栏与基础交互

- 输入：播放列表模型、保存的分隔比例和现有三栏布局。
- 动作：实现虚拟列表、计数、滚动、状态绘制、选择、激活、键盘、分隔线和实时回调。
- 产物：不含搜索占位的可用 Playlist Browser。
- 状态：已完成

### 步骤 4：实现生命周期与自动列表菜单

- 输入：稳定选择和类型权限。
- 动作：实现新建、内联重命名、复制、属性、转换、删除确认与批量重排。
- 产物：普通/自动/保留列表各自安全的管理菜单。
- 状态：已完成

### 步骤 5：接入跨列表曲目拖放

- 输入：步骤 10 的 OLE 曲目快照和侧栏目标 GUID。
- 动作：实现目标高亮、已核准的 Copy-only、重复项、锁拒绝、失败回滚和自动滚动。
- 产物：Playlist View 到左侧列表名称的正式落点。
- 状态：已完成

### 步骤 6：生命周期、构建与验收

- 输入：完整实现和隔离开发实例。
- 动作：完成 Debug/Release、自动测试、包审计、仅 dev 部署和人工验收。
- 产物：可提交的任务 12 组件版本与证据。
- 状态：已完成

## 用户检查点

- [x] 用户已核对功能范围。
- [x] 用户已读懂并认可中文程序逻辑。
- [x] 用户已核对可见界面和交互。
- [x] 用户已按人工验收步骤确认结果。

## 验证记录

- 2026-07-14：核对步骤 11 已提交为 `b924747`，工作树干净，步骤 12 可以开始。
- 2026-07-14：只读检查 Foobox 主界面截图与 `jsspm.js`。确认其显示名称/数量、区分活动/播放/自动、支持批量选择/重排、内联重命名、删除确认、自动列表属性/转换和列表名称曲目落点；同时确认它还包含 FooCrate 已排除的 Library 自动列表、预设智能列表、电台、搜索历史和空白区自动建表行为。
- 2026-07-14：只读检查 foobar2000 2.x SDK。`playlist_manager_v5` 提供稳定 GUID；`autoplaylist_manager` 可识别客户端、打开属性 UI并读取配置；playlist lock 提供逐项权限。自动列表不需要靠名字猜测。
- 2026-07-14：用户完成第一轮选择：两个 Workspace 共用永久 Default，删除后按安全规则回退；创建入口全部进入右键；单击/多选采用推荐方案；`FooCrate Album View` 在 FooCrate 侧栏隐藏；删除始终确认；跨列表永远复制曲目引用；空白拖放创建普通列表；菜单先保持生命周期范围；左栏采用 15% 与 180 DIPs/35% 边界。
- 2026-07-14：用户完成收尾核准：自动列表采用 Edit/Duplicate Rules、Freeze as Manual 等音乐管理语义；预设采用 Foobox 同义查询与默认排序但排除 Mood/电台；背景菜单加入 Add/Load/Save，单行加入 Save This；普通列表不允许同一精确曲目引用重复，空白拖放创建、唯一命名并立即激活 `Dragged Items`。规格 0.2 封闭并进入实现。
- 2026-07-14：新增按 GUID 合并的纯 Playlist Browser 模型、默认列表回退、冲突安全命名和选择修复测试；新增稳定配置 GUID 及 `Preferences > FooCrate > Playlist Browser` 下拉入口。根 `AGENTS.md` 同步记录 Visual Studio 自带 CMake/CTest 的绝对路径，避免 PowerShell PATH 差异重复阻断构建。
- 2026-07-14：实现固定左栏、15% 默认和 180 DIPs/35% 有界分隔线、项目数、Active/Playing/Selected/Automatic/Locked 状态、鼠标/键盘选择、内联重命名、确认删除和按 GUID 的全局播放列表重排。`FooCrate Album View` 继续只在底层存在，不进入 FooCrate 左栏。
- 2026-07-14：实现普通列表复制/保存、自动列表正式属性 UI、规则复制、冻结为普通列表、Foobox 同义 11 个预设，以及背景 Add/Load/Save 菜单。第三方自动列表没有匹配 factory 时明确禁用 Duplicate Rules，不静默生成普通快照。
- 2026-07-14：实现 Playlist View 到列表名称和侧栏空白的 OLE Copy-only 落点；目标及批次精确去重，自动/锁定目标拒绝，空白仅在解析出真实曲目后创建并激活唯一命名的 `Dragged Items`。
- 2026-07-14：`x64 Debug`、`x64 Release` 均在 `/W4 /WX` 下构建成功；Debug/Release 各 9/9 自动测试通过。Release 包 `dist/FooCrate-0.1.0.fb2k-component` 已生成并只部署到 `.local/foobar-dev`。
- 2026-07-14：按用户追加验收意见，为 Default Playlist 的名称和项目数加入临时灰蓝色 `#5F7495`；Active/Playing/Selected 的原状态表达保持不变。重新完成 Debug/Release 构建与两套 9/9 测试，生成组件包并部署、重启且确认 `.local/foobar-dev` 已加载同哈希 Release DLL。
- 2026-07-14：用户完成任务 12 人工验收，确认全部内容未发现逻辑问题，Default Playlist 独立颜色也符合当前阶段要求；剩余视觉优化明确留到步骤 14–16。任务 12 标记为已验收，等待用户以 `实现播放列表管理侧栏` 提交。
- 2026-07-14：用户已提交 `b8fd427 实现播放列表管理侧栏`；后续独立提交 `c4edb56` 只补充构建工具路径说明。工作树干净，任务 13 可以开始。
- 2026-07-22：用户发现 Never played、History、Played often 和 Recently added 四个预设会在 autoplaylist API 之前被通用搜索预检查拒绝，并只显示失去上下文的底层提示。用户已核准移除错误预检查、由正式 autoplaylist API 统一验证、失败删除空容器、保留历史类倒序并改善错误反馈；本轮作为 `1.0.4-beta.1` 候选包实现和验证。
- 2026-07-22：`1.0.4-beta.1` 完成 x64 Debug/Release 构建，两种配置各 14/14 测试通过；包审计只含 `foo_crate.dll`。自动检查已覆盖预设数量、四个动态统计依赖、三个历史倒序和完整错误反馈；等待用户在真实 foobar2000 媒体库中人工验收。
- 2026-07-22：用户确认四个原失败预设通过人工验收，批准将同一功能源码正式化为 1.0.4。正式包完成 Release 构建、单 DLL 结构审计和 SHA-256 生成。

## 人工验收步骤

1. 启动 `.local/foobar-dev`，确认 Playlist Workspace 固定显示 `Playlist Browser | Playlist View | Right Side Panel`，左栏没有搜索或 Album List 假控件。
2. 拖动左栏右边界，确认最窄约 180 DIPs、最宽不超过窗口 35%，松手和重启后宽度保存；面板本身不能拖走或换位。
3. 单击普通播放列表，确认中央列表切换；Ctrl/Shift 多选只改变管理选中，双击列表名称不播放曲目。用方向键、Home/End、Page Up/Down 和滚轮检查导航。
4. 确认活动列表、正在播放列表、管理选中、自动列表 `A` 与锁定 `L` 能分别辨认，项目数随曲目增删实时变化；`FooCrate Album View` 不出现在左栏。
   另确认 Default Playlist 的名称和项目数使用独立灰蓝色；它同时为活动、播放或选中状态时，背景、播放三角和选择边框仍然可辨认。
5. 在普通列表行右键执行 Activate、Set as Default、Rename、Duplicate、Save This Playlist 和 Delete。检查空名/重名拒绝，Esc 取消改名，Delete 始终确认且说明不删除磁盘文件。
6. Ctrl/Shift 选中多个列表后按 Delete，确认只有一次列名/类型/数量的确认；取消不删除。可用测试列表验证删除活动、默认或播放列表时出现额外警告并安全回退。
7. 在侧栏空白处右键新建普通列表；确认立即进入内联改名。执行 Load Playlist 和 Save All Playlists，确认调用 foobar2000 正式文件对话框。
8. 新建预设自动列表，至少检查 Library、Never played、Recently added、Unrated 和 Rated 1–5；确认标为 `A`、内容由媒体库动态计算，查询使用 foobar2000 标准语法。
9. 在自动列表行右键检查 Edit Rules、Duplicate Rules、Freeze as Manual Playlist、Rename、Set as Default 和 Delete。Freeze 后普通快照保留当前引用；任何操作都不修改音频文件。
10. 把 Playlist View 的一首或多首曲目拖到另一个普通列表名称，确认鼠标反馈为 Copy，来源列表不减少，目标只增加引用；重复拖入同一曲目时跳过重复。
11. 把曲目拖到自动列表或锁定列表，确认拒绝；拖到侧栏空白处，确认创建并激活 `Dragged Items`，重名时使用 `(2)`、`(3)`，同一批次不产生重复。
12. 在侧栏内部拖动单个或多选播放列表，确认只改变全局播放列表顺序；曲目拖放与列表排序不会互相误判。
13. 打开 `Preferences > FooCrate > Playlist Browser`，选择 Default Playlist 并应用。关闭开发实例再重启，确认仅启动时切换到 Default；重建 FooCrate 面板不会再次强制切换。
14. 首次进入 Album Workspace，确认来源使用同一 Default；返回 Playlist Workspace 后仍恢复进入 Album Workspace 前的播放列表。删除 Default 后重启，确认回退到第一个非保留列表；完全无列表时创建普通 `Default Playlist`。

## 改动文件

- `specs/modules/PLAYLIST_BROWSER.md`：任务 12 的证据、边界和待核准选择。
- `tasks/012-实现播放列表管理侧栏/README.md`：程序逻辑、实施步骤和验收入口。
- `tasks/README.md`、`tasks/TODO.md`：把当前路线推进到步骤 12。
- `src/playlist_browser_model.h`、`tests/playlist_browser_model_tests.cpp`：GUID 身份、默认回退、唯一命名、选择修复和宽度边界纯模型。
- `src/settings.h`、`src/settings.cpp`、`src/playlist_browser_preferences.cpp`：稳定设置 GUID、Default Playlist 与分隔比例持久化、Preferences 子页。
- `src/playback_panel.cpp`：固定左栏、状态/选择、生命周期、自动列表、重排和 Copy-only 曲目落点。
- `CMakeLists.txt`：新增组件源文件和第 9 项模型测试。
- `AGENTS.md`：固定本机 Visual Studio CMake/CTest 调用路径。

## 决策与未决问题

规格 0.2 已无未决产品问题。实现中若遇到第三方自动列表缺少重建 factory、锁权限变化或 foobar2000 核心命令缺失，必须按已核准的禁用/失败反馈处理，不能静默降级为另一种语义。

Default Playlist 当前使用灰蓝色 `#5F7495` 作为任务 12 的可验收语义色；步骤 14–16 可以随完整主题系统调整具体色值，不得移除其独立识别状态。
