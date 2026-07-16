# 014-还原顶部摘要与底部功能按钮

- 状态：已验收
- 对应规格：[`../../specs/modules/TOP_BAR_AND_PLAYBACK_TOOLS.md`](../../specs/modules/TOP_BAR_AND_PLAYBACK_TOOLS.md)
- 前置任务：[`../013-实现搜索筛选与Album-List入口/README.md`](../013-实现搜索筛选与Album-List入口/README.md)（已提交为 `c6c86a7 实现搜索筛选与 Album List 入口`）
- Fork 提交标题：`还原顶部摘要与底部功能按钮`
- 最后更新：2026-07-14

## 任务目标

把 Foobox Basic 截图中常驻的顶部播放摘要和底部三组控制补齐，同时保留 Windows 原生标题栏。完成后，开发实例只需一行 Main Menu 与 FooCrate 摘要，所有常用播放、页面、队列、顺序、输出和音量操作都从 FooCrate 底栏进入。

## 明确不做

- 不隐藏或自绘 Windows 标题栏和系统窗口按钮。
- 不建立第二套播放顺序、输出设备或文件打开逻辑。
- 不增加 Biography、Video、ReFacets、内嵌频谱或未来曲目预测列表。
- 不在本任务完成最终颜色、精确间距、最终矢量图标或深浅主题；它们分别属于步骤 15–16。
- 不静默覆盖用户整个 Columns UI 布局；开发实例的工具栏整理需要明确执行和验收。

## 中文程序逻辑

1. 顶栏摘要从 foobar2000 当前播放回调和真实曲目元数据生成；停止即清空，缺字段即省略，长文本只在可用宽度内截断。
2. FooCrate 把摘要注册为 Columns UI 正式 Toolbar，使其能放在 Main Menu 右侧；它不替代或控制 Windows 标题栏。
3. 底部 Open、播放顺序和输出设备分别调用 foobar2000 正式命令或服务；已经实现的传输、Workspace、Album List、Settings、Queue、Mute 和 Volume 继续使用唯一真实状态。
4. Playback Order 和 Output Device 打开菜单前读取最新项目及当前选择；用户选择成功后刷新，失败则保留旧状态并提示。
5. 外部菜单、快捷键或其他组件改变任一状态时，回调只把刷新请求交到 UI 主线程，窗口销毁后不再访问旧窗口。
6. 工具栏或正式服务缺失时只禁用相关入口，不妨碍播放、列表和其他已验收功能。

## 可检查步骤

### 步骤 1：冻结外壳与命令矩阵

- 输入：用户最新决定、Foobox 主界面截图、只读 `base.js`、Columns UI/foobar2000 SDK。
- 动作：确定唯一工具栏行、隐藏项、顶部字段、底部三组顺序和每个按钮的正式命令来源。
- 产物：模块规格 0.1 与任务 14 中文逻辑。
- 用户检查方法：核对顶栏结构、三组按钮以及 Playback Order/Output Device 行为。
- 通过标准：不存在装饰性假按钮、重复状态或与任务 15–16 混淆的视觉要求。
- 状态：已完成

### 步骤 2：实现顶栏摘要与宿主布局

- 输入：批准的顶栏规格和 Columns UI Toolbar SDK。
- 动作：注册 FooCrate Toolbar，生成播放摘要并处理播放、暂停、停止、元数据变化、宽度和生命周期。
- 产物：可放在 Main Menu 右侧的实时摘要，以及开发实例的一行工具栏布局。
- 用户检查方法：播放、暂停、停止和快速切歌，核对菜单、Alt、标题栏与摘要。
- 通过标准：摘要不显示旧曲目、不挤坏菜单，隐藏其他工具栏后没有丢失必要功能。
- 状态：已完成

### 步骤 3：补齐底部按钮与正式菜单

- 输入：现有 Playback Bar、核心 Playback Order 下拉服务、输出设备服务和正式 Open 命令。
- 动作：加入 Open、Playback Order、Output Device，重排三组按钮并完善提示、按下、选中、禁用和失败反馈。
- 产物：功能完整的底部控制栏。
- 用户检查方法：逐个按钮操作并从 foobar2000 外部菜单/快捷键反向改变状态。
- 通过标准：每个入口调用真实服务并双向同步；失败不改变真实状态。
- 状态：已完成

### 步骤 4：构建、部署与验收

- 输入：完整实现与隔离开发实例。
- 动作：完成 Debug/Release、自动测试、包审计、仅 dev 部署及一次用户可见验收。
- 产物：可提交的任务 14 组件版本和验证记录。
- 用户检查方法：按最终人工验收清单逐项检查。
- 通过标准：全部已验收功能无回归，目标实例只保留批准的顶部与底部结构。
- 状态：已完成

## 用户检查点

- [x] 用户已核对窗口与工具栏总体结构。
- [x] 用户已核对 Playback Order 从顶部迁入右下角。
- [x] 用户已核对完整顶部字段和底部按钮矩阵。
- [x] 用户已读懂并认可中文程序逻辑。
- [x] 用户已按人工验收步骤确认结果。

## 验证记录

- 2026-07-14：确认任务 13 已提交为 `c6c86a7`，其后的 `3be8e82` 只修改截图协作规则；开始任务 14 时工作树干净。
- 2026-07-14：只读核对 Foobox 主界面截图与 `base.js`；确认参考底栏含 Open、传输、页面/设置、播放顺序、输出、静音和音量。用户最新决定覆盖此前“不增加 FooCrate Playback Order 入口”的旧结论。
- 2026-07-14：只读核对 Columns UI SDK 8.1.0，确认正式 Toolbar 扩展和稳定 Playback Order toolbar GUID 可用；没有写入 Foobox 参考目录。
- 2026-07-14：用户批准顶部摘要字段、中央五按钮和右侧五工具的完整矩阵，任务进入实现中。
- 2026-07-14：实现 `FooCrate Now Playing Summary` 正式 Columns UI Toolbar；播放、暂停、停止、动态信息和当前曲目编辑均刷新单行摘要，缺字段省略并由 Windows 单行省略号处理窄宽度。
- 2026-07-14：底栏加入正式 Open、Playback Order 与 Output Device；顺序菜单使用 `playlist_manager` 的真实项目和活动项，输出菜单使用 `output_manager_v2` 的真实设备及配置回调。队列选中态改为明确灰底，其余已验收控制保持原数据源。
- 2026-07-14：`x64 Debug` 与 `x64 Release` 均在 `/W4 /WX` 下构建成功；Debug/Release 各 9/9 自动测试通过，摘要组合测试覆盖播放、暂停、停止、缺字段和技术字段连接。
- 2026-07-14：组件包 `dist/FooCrate-0.1.0.fb2k-component` 已生成，SHA-256 为 `6BC365DF6F3D722A80B5BD33BCCF37FA8F5E85FC37B05775F57BB953F4F983EB`。Release DLL 只部署到 `.local/foobar-dev`，构建与部署 SHA-256 同为 `0FE1E04E1D58F65348BFC41A1D7E1B06E128166155397B0919D91181BF78FC5E`，运行进程确认加载该 DLL。
- 2026-07-14：用户在 `foobar-dev` 中完成最终布局整理，确认移除原生工具栏、Playlist Tabs 和状态栏后界面结构正确，并明确宣布任务 14 验收通过。

## 改动文件

- `specs/modules/TOP_BAR_AND_PLAYBACK_TOOLS.md`：任务 14 的唯一模块规格。
- `tasks/014-还原顶部摘要与底部功能按钮/README.md`：中文逻辑、步骤、验证和验收入口。
- `specs/PRODUCT_SPEC.md`、`tasks/README.md`、`tasks/TODO.md`：同步最新 Playback Order 决定和当前路线。
- `src/now_playing_toolbar.cpp`：正式 Columns UI 顶栏扩展、摘要格式化、回调与生命周期。
- `src/playback_panel.cpp`：底部 Open、Playback Order、Output Device、按钮排列、图标、菜单和同步回调。
- `src/now_playing_state.h`、`tests/now_playing_state_tests.cpp`：可独立验证的摘要组合规则及回归测试。
- `CMakeLists.txt`：把新 Toolbar 编入组件。

## 人工验收步骤

1. 在 Columns UI 顶部工具栏区域加入 `FooCrate Now Playing Summary`，与 `Main Menu` 放在同一行；移除原生 Buttons、Seek Bar、Playback Order、Playlist Switcher 和频谱工具栏，隐藏 Main Menu 下方独占一整行的 Playlist Tabs（例如 `Library (full)` 标签栏）及底部状态栏。不要移除 Main Menu，也不要隐藏 FooCrate 左侧 `Playlists` 侧栏。
2. 播放一首包含完整 metadata 的歌曲，确认顶部依次显示状态、`Title - Artist`、Album 及存在的技术字段；暂停图标变化，停止后摘要清空，长内容只截断而不挤坏菜单。
3. 核对底部左侧 `Playlist Workspace | Album Workspace | Album List | Settings`，中央 `Open | Previous | Play/Pause | Next | Stop`，右侧 `Playback Queue | Playback Order | Output Device | Mute | Volume`。
4. 点击 Open 并取消文件对话框；确认没有创建列表或改变播放。逐项操作传输、Workspace、Album List、Settings 和 Queue，确认既有功能无回归且 Queue 打开时按钮为灰底。
5. 打开 Playback Order 菜单，依次选择至少两个模式；再从 foobar2000 `Playback > Order` 修改一次，确认 FooCrate 菜单的勾选和 tooltip 都跟随同一核心状态。
6. 打开 Output Device 菜单，确认当前设备有勾选；若存在安全的第二设备可切换后再切回。若只有一个设备，只确认名称和勾选，不为测试破坏日常输出配置。
7. 最大化/还原窗口并使用 Alt 打开菜单，确认 Windows 标题栏、系统按钮和 Main Menu 仍正常；确认界面没有 Biography、Video 或内嵌频谱入口。

## 决策与未决问题

已经决定保留 Windows 标题栏；顶部只保留单行 Main Menu 与 FooCrate 摘要；隐藏其他 Columns UI 工具栏、Main Menu 下方的原生 Playlist Tabs 整行和底部状态栏；左侧 FooCrate `Playlists` 侧栏继续保留；把正式 Playback Order 入口放到 FooCrate 右下角。顶部字段、中央五按钮和右侧五工具已经全部获用户批准，当前无产品未决问题。
