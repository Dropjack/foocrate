# 011-实现专辑封面浏览器与专辑曲目列表

- 状态：已验收
- 对应规格：[`../../specs/modules/ALBUM_COVER_BROWSER.md`](../../specs/modules/ALBUM_COVER_BROWSER.md)
- 前置任务：[`../010-实现播放列表队列拖放与评分交互/README.md`](../010-实现播放列表队列拖放与评分交互/README.md)（已验收并提交为 `daaed39 实现播放列表队列拖放与评分交互`）
- Fork 提交标题：`实现专辑封面浏览器与专辑曲目列表`
- 最后更新：2026-07-14

## 任务目标

完成一个只按 Album 浏览的独立 Workspace：用户从左下入口切换后，可以选择 Media Library 或某个播放列表作为来源，在左侧虚拟封面网格浏览专辑，并在右侧查看、选择、播放和操作当前专辑的真实曲目。顶栏和底部播放控制始终保留，页面不能被拖散或重组。

## 明确不做

- 不加入 Artist、Album Artist、Folder、Library Directory、Genre 或通用分类切换。
- 不加入通用过滤框、搜索系统或第二套媒体库；搜索与原生 Album List 入口属于步骤 13。
- 不提前实现步骤 12 的 Playlist Browser 和完整手动/自动列表管理。
- 临时 Workspace 图标只保证语义和状态，最终 SVG/视觉属于步骤 14/15。
- 不在未获用户批准时静默创建用于媒体库播放的可见保留播放列表。

## 中文程序逻辑

1. 输入来自 foobar2000 Media Library 快照或用户选择的稳定播放列表 GUID，以及对应的 library、playlist、metadb 和 album-art 回调。
2. 程序保存当前 Workspace、来源身份、专辑归组结果、所选专辑、两栏分隔比例、虚拟滚动位置、曲目选择和带代次的封面缓存；媒体库、播放列表和曲目元数据仍由 foobar2000 持有。
3. 切换来源时先冻结新来源身份并增加代次，再取得真实曲目、按批准规则归为 Album、排序并选择可恢复的专辑；旧代次封面和刷新结果全部丢弃。
4. 点击封面只更新右侧 Album Track Header/List；双击或 Enter 创建/复用锁定的 `Refrain Album View` 核心桥接并开始仅限当前专辑的顺序或随机会话，随后恢复来源活动列表；右键交给正式 foobar2000 上下文菜单。
5. 左下同时提供 Playlist/Album 两个互斥 Workspace 入口，当前项显示灰底；它们只替换顶栏与 Playback Bar 之间的内容，返回 Playlist Workspace 后恢复原有主列表、右栏和队列状态。
6. 保存分隔比例和进程内来源状态；重启来源留给步骤 12 的 `Default Playlist` 设置。来源消失、媒体库关闭、空来源、缺标签/封面、加载失败、窗口销毁和结果晚到时显示明确状态并保持其他 Workspace 可用。

## 可检查步骤

### 步骤 1：核准规格与播放上下文

- 输入：产品规格、Foobox 截图/只读脚本、用户新增的左下入口要求。
- 动作：冻结 Workspace 按钮、metadata-only 专辑身份、只读来源交集和 Media Library 播放桥接规则。
- 产物：Album Cover Browser 规格 0.2 和本任务逻辑。
- 用户检查方法：核对规格第 8 节三个决定。
- 通过标准：不存在会改变播放列表集合或主要视觉结构的未决问题。
- 状态：已完成

### 步骤 2：实现来源与专辑模型

- 输入：Media Library/播放列表真实曲目和标题格式字段。
- 动作：实现稳定身份、归组、排序、多碟/同名/缺标签规则、来源代次和纯模型测试。
- 产物：可测试的 Album 数据模型和来源快照。
- 用户检查方法：用同名、多碟、合辑、Single、空来源检查数量和顺序。
- 通过标准：归组不碰撞、不丢曲；来源变化不会把旧结果套到新来源。
- 状态：已完成

### 步骤 3：实现 Workspace 入口与固定两栏

- 输入：左下按钮、当前 Workspace、保存的分隔比例。
- 动作：实现代码临时图标、灰底选中状态、页面切换、固定两栏和可调整边界。
- 产物：可往返且不破坏 Playlist Workspace 的 Album Workspace。
- 用户检查方法：反复切换、调整分隔线、播放中切换并重启。
- 通过标准：顶栏/底栏常驻，状态恢复准确，面板不能拖走或交换。
- 状态：已完成

### 步骤 4：实现虚拟封面网格与异步缓存

- 输入：专辑模型、Front Cover、可见区域和 DPI。
- 动作：实现虚拟布局、选择/键盘/滚动、占位、异步请求、预取、缓存上限和代次校验。
- 产物：大媒体库可流畅浏览的 Album Cover Grid。
- 用户检查方法：快速滚动、切换来源、缺图、损坏图和快速返回页面。
- 通过标准：无旧封面串位、闪烁式全量重载、同步图片 I/O 或无限缓存。
- 状态：已完成

### 步骤 5：实现 Album Track Header/List 与播放

- 输入：当前专辑真实曲目和批准的播放上下文。
- 动作：复用紧凑曲目资源，实现列、选择、播放、键盘、正式菜单、队列和当前播放标记。
- 产物：与网格选择严格一致的右侧曲目列表。
- 用户检查方法：选择不同专辑，双击封面/曲目，使用 Enter、右键、队列和上一首/下一首。
- 通过标准：不会播放旧专辑或错误曲目；播放顺序和工作列表行为符合批准规则。
- 状态：已完成

### 步骤 6：持久化、生命周期与验证

- 输入：完整实现、版本化设置、隔离开发实例。
- 动作：完成迁移/回退，构建 Debug/Release，运行自动测试、包审计并只部署到 `.local/foobar-dev`。
- 产物：可安装组件、验证记录和人工验收清单。
- 用户检查方法：覆盖所有来源、异常状态、快速切换、重启和步骤 08–10 回归。
- 通过标准：自动验证通过且用户明确完成人工验收。
- 状态：已完成并验收

## 用户检查点

- [x] 用户已核对功能范围。
- [x] 用户已读懂并认可中文程序逻辑。
- [x] 用户已核对可见界面和交互。
- [x] 用户已按人工验收步骤确认结果。

## 验证记录

- 2026-07-14：核对 HEAD 为 `daaed39`，工作树干净；步骤 11 可开始。
- 2026-07-14：只读检查归档截图和 Foobox `jssb.js`；确认截图的 Album 标题顺序、右侧曲目列表和 Foobox `Library View` 工作列表事实。尚未构建或部署功能代码。
- 2026-07-14：新增纯 `Album Browser` 模型测试，覆盖同名不同 Album Artist、多碟排序、缺 Album 独立 Single 和来源子集；Debug/Release 全量构建均通过，MSVC `/W4 /WX` 未产生新增警告。
- 2026-07-14：8 项 CTest 全部通过：component identity、playback state、settings、now playing、playlist view、playlist grouping、playlist interaction、album browser model。
- 2026-07-14：Release 包审计通过，包只含根目录 `foo_refrain.dll`；产物为 `dist/Refrain-0.1.0.fb2k-component`。
- 2026-07-14：只部署到 `.local/foobar-dev/profile/user-components-x64/foo_refrain`。一次强制结束烟雾进程导致下一次进入 foobar2000 恢复启动且不加载用户组件；随后正常关闭并重新启动，进程确认同时加载允许目录中的 `foo_ui_columns.dll`、`foo_uie_eslyric.dll` 和最新 `foo_refrain.dll`。人工验收实例现保持可见，不再由 Codex 操作。
- 2026-07-14：用户首轮验收确认检查点 1–5、8、9 通过；Playlist Workspace 往返恢复也通过。检查点 6 改由步骤 12 的 `Default Playlist` 处理。检查点 7 发现 Album Track List 双击不播放、右键 Play 正常；另发现部分专辑封面反复闪烁，任务退回实现中修正。
- 2026-07-14：双击播放根因是桥接列表在调用 foobar2000 默认动作前已经加锁，锁定列表会接管默认动作；现改为播放成功后立即加锁。部分封面闪烁根因是 Album Grid 把封面按 1024 px 解码，少量封面即可耗尽 64 MiB 缓存并互相驱逐；现改为 320 px 缩略图、160 项上限和最近使用优先淘汰。
- 2026-07-14：修正版 Debug/Release 均在 `/W4 /WX` 下构建通过；两套各 8 项 CTest 全部通过。Release 包仅含根目录 `foo_refrain.dll`，包 SHA-256 为 `8468C87C06CA9B07E3981750D77C0FFC66279A672A03D037BD6443DC56EA9DF0`。
- 2026-07-14：开发实例正常关闭后重新部署并可见启动；进程确认加载允许目录中的 Refrain、Columns UI 和 ESLyric。部署 DLL SHA-256 为 `E73918077D56E9263CEBF66E3758EF009FFAF3CA2AD20B29922BCA7C660CB2F3`。
- 2026-07-14：第二轮人工复验确认封面不再闪烁。双击会播放 Playlist Workspace 的同索引歌曲，右键 Play 播放正确且上一首/下一首正常；确认剩余问题是默认动作尚未真正开始前过早恢复活动列表，进入第二轮窄修正。
- 2026-07-14：双击第二轮修正版在默认动作解析期间保持桥接列表活动，收到新曲回调后再经窗口消息恢复来源，避免在播放回调内修改列表状态。Debug/Release 均构建通过，两套各 8 项 CTest 全部通过。
- 2026-07-14：Release 包仍只含根目录 `foo_refrain.dll`，包 SHA-256 为 `6EC0438B461D212F4618708A0C1F036493320233A82F1EF0824EB0E34599C2AB`；开发实例正常关闭、部署和可见重启，部署 DLL SHA-256 为 `AD92A39B51FA8CFF90BB7E2163A2C44AA217992C04B0815EA1B71E5E1DA9AC65`，Refrain、Columns UI、ESLyric 均已加载。
- 2026-07-14：用户最终确认双击播放正确曲目，上一首/下一首保持在当前专辑内，返回 Playlist Workspace 后仍显示原播放列表；此前封面不闪烁也已确认，任务 11 正式验收通过。

## 人工验收步骤

1. 启动 `.local/foobar-dev/foobar2000.exe`，确认左下出现 `Playlist Workspace` 列表图标和 `Album Workspace` 四格图标；当前项有灰底，顶栏、进度条和底部控制始终存在。
2. 先在 foobar2000 选择一个只包含某专辑部分曲目的播放列表，再点 Album Workspace；确认 `Current source` 是该列表，左侧只出现该来源的专辑，右侧只列出该专辑在来源中实际存在的曲目。
3. 打开 `Current source` 下拉，切换到 Media Library 和另一个播放列表；确认没有 Artist、Folder、Genre 等模式，并确认切回某来源时恢复本会话上次选中的专辑。
4. 检查同名但 Album Artist 不同的专辑、多碟专辑和缺 Album 曲目：同名不同艺术家不得合并，多碟合并且右侧按 `Disc.Track` 顺序，缺 Album 的歌曲各自显示为 Single。
5. 快速滚动大量封面并连续切换来源；确认只出现对应单元的 Loading/No cover，没有旧来源封面串位、全页狂闪或界面卡死。
6. 单击封面切换右侧，拖动中间边界后返回 Playlist Workspace 再回来；确认结构不能拖走、比例和本进程来源/专辑状态保存。重启后的来源将在步骤 12 按 `Default Playlist` 决定，本步骤不再验收“恢复上次来源”。
7. 在右侧用单击、Ctrl/Shift、方向键、Home/End、Ctrl+A 和右键菜单检查选择；Delete 不应删除或改动来源，拖动也不应重排或添加曲目。
8. 在 foobar Playback Order 选择普通顺序，从某专辑第六首双击播放；确认 `Refrain Album View` 只在真正播放时创建、内容是完整有序专辑、前五首仍在，Previous 能回到第五首，返回 Playlist Workspace 显示原来源而非桥接列表。
9. 在 foobar Playback Order 选择 Random/Shuffle，再从任意曲目开始；确认播放只在当前专辑内且本轮不重复，不进入正式 Playback Queue；专辑末尾停止，不跨到来源中的下一张专辑。
10. 播放专辑期间切到 Playlist Workspace，检查上一首/下一首仍在该专辑；随后从原播放列表双击另一首歌，确认专辑会话结束且 foobar 原 Playback Order 恢复。最后尝试在原生播放列表标签中增删/重排/删除 `Refrain Album View`，应被 Refrain 锁拒绝。

## 改动文件

- `specs/modules/ALBUM_COVER_BROWSER.md`：步骤 11 唯一模块规格和三个待核准决定。
- `tasks/011-实现专辑封面浏览器与专辑曲目列表/README.md`：本步骤程序逻辑、实施顺序和验收入口。
- `tasks/README.md`、`tasks/TODO.md`：把当前路线推进到步骤 11。
- `src/album_browser_model.h`、`tests/album_browser_model_tests.cpp`：Album-only 身份、排序和来源交集的纯模型与自动测试。
- `src/playback_panel.cpp`：Workspace 入口、来源回调、虚拟网格、独立封面线程/缓存、只读曲目表、交互和播放会话。
- `src/artwork_loader.h`、`src/artwork_loader.cpp`：支持调用方指定解码上限，使 Album Grid 使用独立缩略图而不降低 Now Playing 封面质量。
- `src/settings.h`、`src/settings.cpp`：稳定 GUID 的来源、桥接列表、分隔比例和专辑选择持久化。
- `CMakeLists.txt`：接入 Album Browser 模型和第八项自动测试。

## 决策与未决问题

用户已决定左下同时提供 Playlist/Album 两个按钮，临时图标可由实现选择，当前 Workspace 按钮显示灰底，未来 Workspace 入口继续加入这里。专辑只按 Album + Album Artist metadata 归组；右侧是当前来源交集产生、固定排序且不可编辑的智能结果视图。唯一真实 `Refrain Album View` 使用持久化 GUID 并锁定编辑，只在实际播放时更新；Playlist Workspace 始终恢复来源列表。Album Playback Session 只在当前专辑内顺序或随机一次，结束后停止，不自动跨回来源续播。重启来源改由步骤 12 的 `Default Playlist` 设置决定。全部可见行为已由用户验收。
