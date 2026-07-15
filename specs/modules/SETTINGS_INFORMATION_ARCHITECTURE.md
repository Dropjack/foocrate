# Refrain 设置项与信息架构规格草案

- 状态：已验收
- 版本：0.8
- 最后更新：2026-07-15
- 对应任务：`17 重构 Refrain 设置菜单与配置界面`

## 1. 本轮证据范围

本轮只读检查了重置后的 `D:\Dev\foobar2000\profile\foobox`。没有修改、部署或清理参考目录。

主要文件证据：

- `script/js_panels/jsplaylist/WSHsettings.js`：Foobox 主设置的五页结构和控件行为。
- `config/common`、`config/miscconf`、`config/groups`、`config/columns`、`config/layoutsv2`：主设置保存值、分组、列和播放列表布局。
- `script/js_panels/infoArt.js`：右侧封面、信息、歌词/属性切换及封面菜单。
- `script/js_panels/properties.js`：Track Properties 区段显隐和点击创建自动播放列表。
- `script/js_panels/jssb.js`：封面浏览器属性。
- `script/js_panels/jsspm.js`：播放列表管理器属性。
- `script/js_panels/search.js`：搜索框属性。

以下结论是“由参考文件直接证明”，尚未把每一页在 Foobox 界面中逐项截图观察。最终线框核对时只需要观察会影响布局或文案理解的项目，不需要把脚本内部参数全部搬到界面。

## 2. Foobox 主设置五页

### 2.1 Playlist View

| Foobox 设置 | Foobox 行为 | Refrain 建议 |
| --- | --- | --- |
| Smooth scrolling | 对脚本列表做缓动滚动 | 不增加；Refrain 保持原生滚轮行为，避免额外动画状态 |
| Touch scrolling control | 拖动列表滚动，并禁用拖放 | 不在任务 17 增加；触摸与拖放冲突留任务 18 统一处理 |
| Wheel scroll step / Touch scroll step | 直接输入 1–20 的步长 | 不暴露数值；如用户以后确有需求，改为系统/慢/标准/快三档 |
| Default Playlist Action | 双击曲目时选择 Play 或 Enqueue | 新增 `Activate track (double-click or Enter): Play / Add to queue`。默认 Play；用户批准先实现并在实际使用后决定是否保留 |
| Row height of list | 15–100 的全局像素行高 | 新增 `Track row layout: Compact / Standard / Two-line`；用户已批准，不暴露物理像素 |
| Selection 子菜单 | 把选择操作再包一层右键菜单 | 不增加；Refrain 保持标准 foobar2000 上下文菜单 |
| Play next playlist automatically | Default playback order 下跨列表续播 | 不建议；跨列表副作用较大，Playback Order 已是正式控制入口 |
| Queue Content playlist | 队列非空时建立镜像播放列表 | 不增加；Refrain 已有独立 Playback Queue Workspace |
| External applications | 在 Open with 中维护 MusicTag/Mp3tag/spek 路径 | 不增加；涉及命令行转义和外部程序信任，复用 foobar2000 正式上下文菜单 |

### 2.2 Columns

Foobox 支持列列表以及 `New`、`Remove`、右键 Duplicate，并为每列编辑：Label、Title format、Extra-line title format、Sort order、Left/Center/Right。

Refrain 当前已有列显隐、顺序、复制、删除、Label、Display format、Sort format、Alignment 和 Width weight。任务 17 建议：

- 把 `Copy` 改名为 `Duplicate`，另增明确的 `New column`。
- 保留 `Move up`、`Move down`，只允许删除用户列；内置列允许隐藏但不删除。
- 长格式使用多行或独立宽编辑器，并在原处显示语法错误。**用户已批准。**
- 每个普通文字列新增 `Secondary display format`；Two-line 模式绘制主行和附加行，默认附加内容复现 Foobox。
- Width weight 在界面中改称 `Relative width`，同时保留拖动表头直接调整。

### 2.3 Groups

Foobox 支持分组列表以及 `New`、`Remove`、右键 Duplicate，并编辑：Label、Title format、Sort order、两行各左右字段。

Refrain 已有 Group key、Sort、Title left/right、Subtitle left/right 和 Artwork。任务 17 建议：

- 增加 `New group`、`Duplicate`、`Move up`、`Move down`；内置分组不可删除。
- 保留两行左右字段和 Artwork source。
- 把 `Group key` 帮助写清楚：它决定哪些曲目属于同一组；`Sort` 决定应用分组时的曲目顺序。
- 提供 `Show group artwork` 开关，再在开启时选择 Front cover / Artist artwork。Placeholder 是失败状态，不应作为用户主动选择的来源。

### 2.4 foobox

| Foobox 设置 | Refrain 处理 |
| --- | --- |
| Show pseudo main menu bar | 不增加；Refrain 使用 Columns UI 正式 Main Menu，任务 14 已冻结结构 |
| Scrollbar width: System / Narrow | 不增加；任务 15 已验收 Refrain 细滚动条语义，任务 18 再做系统可访问性回退 |
| Show Open and Stop buttons | 暂不增加；底部真实命令组是任务 14 已验收结构 |
| Library button: Album List / ReFacets | 不增加；产品边界明确只使用 Album List，不集成 ReFacets |
| Interface color follows cover | 已由任务 16 的 `Follow album artwork` 完整替代 |
| Except background / Except ESLyric highlight | 不增加；Refrain 主题使用统一语义色，不拆成互相矛盾的局部来源 |
| Cover colour threshold / background chroma | 不暴露算法参数；取色必须保持可读性和确定性 |
| Colorful seek bar | 不增加；进度条使用当前主题 Accent |
| Cover filenames for directory groups | 不增加；Refrain 通过 foobar2000 Artwork API 读取，不自行扫描文件名 |
| Cover cache dimension | 不增加；缓存按实现安全上限自动管理，不让用户用像素值调资源占用 |
| Write rating to file tag | 不增加；Playback Statistics 是 Refrain 唯一评分数据源，避免静默改写文件标签 |
| Right panels follow cursor | 建议新增为 `Right panel follows: Playing track / Playlist selection`；封面主题仍跟随正在播放项，避免浏览选择时整套颜色不停变化 |
| More track info on titlebar | 建议改造成 `Now Playing summary format` 高级设置；作用于 Refrain 顶部摘要，不改 Windows 标题栏 |
| Enable D2D rendering | 不增加；Refrain 的渲染后端是实现要求，不是用户功能开关 |

### 2.5 Playlist layouts

Foobox 为播放列表保存独立布局，布局包含：分组模式、是否显示组封面、自动折叠、折叠/展开组头高度、默认折叠状态和列宽/显隐；并允许删除当前布局和清理无效布局。

这是最值得 Refrain 借鉴、但也是任务 17 中范围最大的一项。建议采用更清楚的模型：

1. `Layout profiles` 是有名称的完整档案，包含分组、列、组封面、自动折叠和密度。
2. 始终有不可删除的 `Default` 档案。
3. 每个播放列表可选择 `Use Default` 或一个命名档案；删除档案时引用它的播放列表回退 Default。
4. 档案支持 New、Duplicate、Rename、Move、Delete；删除前明确确认。
5. 不提供“Remove invalid layouts”这种脚本维护按钮；迁移时自动清理失效引用并记录回退。

## 3. Foobox 其他面板设置

### 3.1 右侧封面与信息区

Foobox 的文件证据包含：

- 封面来源 Front、Back、Disc、Artist、Icon、Genre；单图/轮播、暂停、轮播间隔。
- Stretch、Keep aspect ratio、Fill panel、插值模式、淡入淡出、缓存容量和清缓存。
- 右栏跟随播放或光标。
- 完整/简化信息头、显示 Rating、显示 Mood、循环 Track Info。
- Lyrics / Properties 手动切换和自动切换。
- Properties 区段：Metadata、Location、Tech Info、Playback Statistics、ReplayGain。

Refrain 建议新增：

- 不提供 Artwork fit 设置。右侧大封面和 Album Browser 使用等比放大、居中裁切；Playlist View 的 Cover 是明确例外，必须完整显示原始封面形状，等比缩放并以主题底色填充剩余区域。所有封面都绝不拉伸。**用户已批准。**
- 必须实现 Front、Back、Disc、Artist 多封面轮播；缺失来源自动跳过。提供启用开关、四类来源和 10/20/30/60 秒间隔，默认开启全部来源并使用 20 秒。Follow album artwork 主题固定使用正在播放项的 Front，Front 缺失时使用第一个可用来源，不随视觉轮播周期变色。**用户已批准。**
- `Right panel follows: Playing track / Playlist selection`。Playing track 保持当前行为；Playlist selection 在播放不中断的情况下让封面、标题、评分和详情跟随列表焦点。主题颜色始终跟随正在播放项。**用户已批准，默认 Playing track。**
- Track Details 区段复选框：Metadata、Location、Technical、Playback statistics、ReplayGain。把现有单独的 `Show ReplayGain` 合并进这里。**用户已批准。**

暂不建议：Genre/Icon 文件来源、插值算法、缓存容量、清缓存按钮、Mood、自动循环 Track Info、点击字段创建自动播放列表。它们要么依赖 Foobox 目录约定，要么是实现参数，要么会引入隐藏副作用。

### 3.2 Album Cover Browser

Foobox 暴露 Library/Playlist 来源、Album/Artist/Folder/Tag 模式、列表/缩略图显示、ALL 项、加载动画、缩略图最小宽度、行高、缓存尺寸、触摸控制、总数和保持比例。

Refrain 的 Album-only 产品边界不变，不增加 Artist、Folder、Genre、Tag 或通用分类模式。建议只新增：

- `Album tile size: Small / Medium / Large`。目标尺寸确定为 144 / 184 / 240 DIP，其中 184 DIP 保持当前默认。**用户已批准。**
- Album Browser 与右侧大封面使用等比 Fill + 居中裁切；播放列表组封面完整等比显示并居中。两种规则都没有用户开关。
- `Show album count`，默认开；如果当前界面没有合适位置则不实现。

来源选择继续是工作区内的即时操作，不放进 Preferences。

### 3.3 Playlist Browser

Foobox 暴露新列表排序格式、保留列表锁定、网格线、删除确认、触摸控制和 Filter。Refrain 建议：

- 保留现有 `Default playlist` 设置。
- 删除确认与自动/保留列表保护保持强制安全行为，不提供关闭开关。
- 不恢复 Filter；任务 15 已按用户决定完整移除。
- 不增加网格线、触摸开关或脚本排序格式。

### 3.4 Search

Foobox 搜索框有 Source、Auto-validation、Scope、Keep Playlist、Follow Cursor、History 和 Reset Button。Refrain 已明确把搜索交给 foobar2000 `Ctrl+F`，因此这些设置全部不进入 Refrain。

## 4. Refrain 当前设置清单

### 主 Refrain 页面

- Right-side time：Total / Remaining。
- Show control tooltips。
- Show settings button。
- Automatically show lyrics while playing。
- Manual/default view：Lyrics / Track details。
- Show ReplayGain section in track details。
- Colour mode：Follow Windows / Refrain preset / Follow album artwork。
- Preset：四套主题，仅在 Refrain preset 下启用。
- Dependencies：Columns UI、ESLyric、Playback Statistics 的只读检测和官方页面。

### Playlist View 子页

- Group preset、Auto-collapse、Collapse groups by default。
- 分组 Copy/Delete、Group key、Sort、两行左右字段、Artwork source。
- 列显隐、顺序、Copy/Delete、Label、Display format、Sort format、Alignment、Width weight。

### Playlist Browser 子页

- Default playlist。
- Playlist Browser 分隔比例由用户直接拖动保存，Reset 恢复 15%，不是独立控件。

## 5. 建议进入任务 17 的新增功能

### A 级：建议本任务实现

1. Playlist View 的响应式重排与长格式编辑器。宽页面使用“对象列表在左、编辑器在右”，窄页面自动改成“列表在上、编辑器在下”并垂直滚动；这不是设置开关。**用户已批准。**
2. Group/Column 的 New、Duplicate、Move up/down、Delete 与内置项保护。**用户已批准。**
3. Track Details 五类区段显隐，合并现有 ReplayGain 单项。**用户已批准。**
4. `Right panel follows: Playing track / Playlist selection`，动态主题仍跟随播放项。**用户已批准。**
5. `Track row layout: Compact / Standard / Two-line`。Compact 为紧凑单行，Standard 保持当前单行，Two-line 绘制主格式与 Secondary display format。**用户已批准。**
6. `Activate track: Play / Add to queue`。用户批准先实现并在实际使用后决定是否保留。
7. 封面显示规则不提供设置：Album Browser 与右侧大封面使用 Fill，Playlist View Cover 使用完整等比显示。**用户已批准。**
8. `Album tile size: Small / Medium / Large`（144 / 184 / 240 DIP）。**用户已批准。**

### B 级：需要用户决定是否扩大本任务

1. 完整 `Layout profiles` 与每播放列表分配。**用户已批准。**
2. Front/Back/Disc/Artist 多封面轮播及其设置。**用户已批准默认开启、20 秒、主题固定 Front。**
3. 可编辑 `Artist / album line format`，放在 Advanced 区；Track Title 永远只显示歌名。旧的两行 `Now Playing summary format` 继续兼容读取第二行。**用户已批准。**
4. Album Browser 始终显示专辑总数，不提供开关。**用户已批准。**

## 6. 建议信息架构

保留 foobar2000 Preferences 树，不做一个无限增高的大页面：

| 页面 | 内容 |
| --- | --- |
| `Refrain` | Playback controls、基础界面选项、Appearance |
| `Now Playing & Details` | 右栏跟随对象、Artwork fit/轮播、Lyrics 行为、Track Details 区段、顶部摘要格式 |
| `Playlist View` | Layout profiles、Groups；窄宽度改为单栏，Column 定义不再单独暴露高级编辑页 |
| `Cover Browser` 内联栏 | `Album Source Selector` 行右侧固定放置 Default playlist 与 Album tile size 两个连续下拉栏 |
| `Dependencies` | 三个依赖的只读状态、版本和官方页面 |

所有页面共享 foobar2000 的 Apply/Cancel/Reset 语义。Appearance 保留任务 16 的即时预览层；其他设置只修改草稿，Apply 后生效。任何分组、列、档案或标题格式错误都在当前编辑区显示，不用弹窗遮住其他字段。

## 7. Two-line 默认附加内容

用户要求附加行内容与 Foobox 默认行为一致，但不得复制 Foobox 的 JavaScript 实现。Refrain 使用自身 C++ 标题格式模型复现以下默认映射：

| Refrain 列 | Secondary display format |
| --- | --- |
| `#` | `$if2(%play_count%,0)` |
| `Title` | `$if2(%album artist%,'Unknown artist')` |
| `Album` | `$if2(%genre%,'Other')` |
| `Length` | Foobox Time 附加行的动态码率表达式；Refrain 以等价安全标题格式实现 |
| 其他内置列 | 空；用户可自行填写 |

Two-line 约 42 DIP。Cover、State、Rating 等特殊列跨两行垂直居中；普通文字次行使用较小的 `TextSecondary`。Compact 与 Standard 不绘制次行，但保留其草稿值。

## 8. Group header style

只提供两种模式，不暴露任意高度数值：

- `Detailed`：展开和折叠都使用当前 26 DIP 填充式详细组头。
- `Compact line`：展开和折叠都保留约 26 DIP 的可点击、可聚焦命中行，但视觉只绘制最前方组文字、末尾年份/右字段以及两者之间的 2 DIP 主题色横线；不绘制整行填充背景。双击组头或键盘操作继续折叠/展开。

Compact line 不允许用字符数估算文字宽度。绘制时由 DirectWrite 分别测量左主字段、左附加字段和右字段：左字段从展开箭头后开始，右字段贴近内容区右边缘，中间横线只占用两者真实可见边界之间的剩余空间，并在两端保留固定留白。窄到没有安全间距时不画横线，不能让横线穿过文字。左附加字段使用次要文字色，明确区分专辑标题与艺术家等第二信息。

Compact line 的横线使用严格 1 DIP 高的实心矩形，避免居中描边在像素栅格上显得忽粗忽细，并把视觉中心让给专辑名称。专辑名使用 17 DIP，专辑表演者使用 13 DIP；右侧辅助字段仍为 11 DIP。DirectWrite 字号以 DIP 表示，在 100% 缩放下 1 point 约等于 1.333 DIP。

Compact line 的目标外观以 [`../../tasks/001-Foobox-Basic交互取证/screenshots/compact expanded分割线预览.jpg`](../../tasks/001-Foobox-Basic交互取证/screenshots/compact%20expanded分割线预览.jpg) 为证据。用户确认展开和折叠均使用该外观。

## 9. Layout profiles 数据边界

用户批准完整 Layout profiles 与每播放列表分配。为避免复制大量标题格式，模型分成两层：

1. Group definitions 与 Column definitions 是全局定义库，稳定保存 New/Duplicate/Move/Delete 后的对象。
2. Layout profile 引用这些定义，保存：选定 Group、列顺序/显隐/相对宽度、Track row layout、Group header style、Auto-collapse 和 Collapse by default。
3. 始终存在不可删除、不可改名的 `Default` profile。
4. 普通播放列表按稳定 GUID 保存 `Use Default` 或命名 profile；重命名 profile 不破坏引用。
5. 删除 profile 前列出受影响播放列表并确认；Apply 后这些列表回退 `Default`。删除播放列表产生的孤立 assignment 在读取时自动清理。
6. New 建立安全默认 profile；Duplicate 复制当前 profile 的全部引用与尺寸；Rename 校验空名和重名；Move 只改变设置列表顺序。
7. Reset page 恢复默认定义、Default profile 并清除自定义 assignment，但仍要 Apply；执行前在页面内明确列出影响。

## 10. 页面与响应式线框

使用 foobar2000 Preferences 树，根节点和三个子页如下；不再保留独立 `Browsers` 设置页：

| 页面 | 分区 |
| --- | --- |
| `Refrain` | Playback controls、Interface、Appearance |
| `Now Playing & Details` | Right panel follows、Artwork rotation、Lyrics、Track Details sections、Advanced Artist/Album line format |
| `Playlist View` | Layout profiles、Groups 两个内部标签页 |
| `Dependencies` | 三项只读检测、版本、官方页面 |

Playlist View 的两页共用响应式编辑框架：

- 可用宽度足够时：对象列表固定在左，属性编辑器占右侧剩余空间。
- 可用宽度不足时：对象列表移到上方，属性编辑器移到下方，页面只出现纵向滚动。
- 断点不写死屏幕像素；布局以“列表最小约 190 DIP + 编辑器最小约 420 DIP + 间距”能否同时满足为判断。
- 操作按钮固定跟随对象列表，长格式编辑器始终获得最大可用宽度。
- 页面切换、响应式重排和 DPI 变化不重建草稿、不改变当前对象选择。

Playlist View / Layout profiles 内容：

- Profile 使用真实列表呈现；列表最多同时显示 5 行，超过后只在列表内部滚动。选中项使用独立名称编辑框；New、Duplicate、Move up/down、Delete 紧邻列表，排序结果必须立即可见。
- 当前活动播放列表名称，以及 `Use profile: Default / <named profile>`。
- Selected group、Track row layout、Group header style、Auto-collapse、Collapse by default。
- 当前 profile 的列顺序、显隐和相对宽度摘要；具体定义跳转到 Columns 页编辑。

Playlist View / Groups 与 Columns 使用已批准的完整管理操作和宽编辑区；内置对象不可删除，自定义对象删除前确认。

## 11. 待用户批准的关键决定

无。页面树、响应式框架和两层 Layout profiles 模型均已批准。

## 12. 已批准决定

1. Group 和 Column 均提供 `New`、`Duplicate`、`Move up`、`Move down`、`Delete`。内置项允许 Duplicate 和调整顺序，但不可删除；用户项删除前明确确认。
2. 长标题格式使用宽大的多行编辑区，并在当前字段旁原位显示语法错误。
3. Track Details 提供 Metadata、Location、Technical、Playback statistics、ReplayGain 五类区段显隐。
4. Activate track 使用清楚的 `Play` / `Add to queue` 英文选项，作用于 Playlist View 双击或 Enter；先实现并由用户实际体验后决定是否保留。
5. 右侧大封面与 Album Browser 等比 Fill、居中裁切；Playlist View Cover 完整显示原始形状并等比缩放，剩余区域使用主题底色。所有区域都绝不拉伸，也不提供 Fit/Fill 设置。
6. Album tile size 提供 Small / Medium / Large，目标为 144 / 184 / 240 DIP，默认 184 DIP。
7. 多封面轮播默认开启、20 秒，来源为 Front/Back/Disc/Artist；主题固定从 Front 或第一个可用来源取色。
8. Right panel 提供 Playing track / Playlist selection；即使跟随 Playlist selection，Follow album artwork 主题也继续跟随正在播放项。
9. Playlist View 设置页必须响应可用宽度自动重排，不能靠扩大默认窗口掩盖裁切。
10. Track row layout 使用 Compact / Standard / Two-line；Two-line 复现 Foobox 默认附加字段内容。
11. Group header style 只提供 Detailed / Compact line；Compact line 在展开和折叠时都显示前置文字、末尾字段和 1 DIP 横线。
12. 实现完整 Layout profiles 与每播放列表分配。
13. 顶部第二行 Artist/Album format 可编辑，放在 Advanced 区；Track Title 行不可被该格式覆盖。
14. Album Browser 始终显示专辑总数，不提供设置开关。
15. Preferences 树不再保留 `Browsers` 子页；Playlist View 只显示 Layout profiles / Groups 两个标签页。
16. Layout profiles 使用“全局 Group/Column 定义库 + profile 引用与尺寸”的两层模型。
17. `Default playlist` 与 `Album tile size` 移到 Cover Browser 的 `Album Source Selector` 同一行最右侧；两栏使用固定宽度和固定顺序，直接显示当前结果并以下拉菜单修改。
18. Playlist View Cover 是固定、不可拖宽的独立合并区域；右侧不绘制可见边框。组头横跨全宽并从列表最左侧显示专辑名/表演者；封面从第一首曲目行开始，完整等比、水平方向居中、垂直贴顶。曲目分隔线、选择焦点线和拖放插入线不进入 Cover 区域，留白直接使用列表背景。
19. Playlist View 紧凑组头优先完整显示专辑标题；专辑表演者和右侧辅助字段分别限制在约 22% 与 18%，横线只占剩余空间。只有整体页面确实缩窄时才允许标题省略。
20. Artwork View 右键提供 Front Cover / Back Cover / Disc / Artist 与 `Pin current artwork`；缺失来源禁用。取消固定后恢复 Preferences 中的轮播开关、来源掩码和间隔。
21. Now Playing & Details 的五个 Track Details 区段按 3+2 两行排列，长文案不得越过页面右边界；页面高度不足时提供整体纵向滚动。
22. Groups 编辑行压缩为约 34 DIP 节距、25 DIP 输入框，保留完整标签和编辑宽度；Playlist View 外层在高度不足时纵向滚动，Layout profile 的五行列表仍保持自己的独立滚动。
23. 切换 Group 不得以播放列表允许物理重排为前提。普通可重排播放列表继续使用 Group 的 `Sort` 整理曲目；Auto Playlist 或其他禁止重排的列表保留来源自身顺序，只切换视觉分组，不修改自动列表查询、排序配置或锁状态，也不显示“无法重排”错误。
24. Auto Playlist 允许作为拖拽来源：选中曲目可拖到 Playback Queue、其他普通播放列表或外部兼容目标。由于来源由查询自动维护，拖拽只提供 Copy，不从 Auto Playlist 删除项目；拖回同一个 Auto Playlist 的内部重排和向任何 Auto Playlist 写入仍然禁止。
25. `Album Artist / Album / Disc` 的默认 Sort 为 Album Artist → Date/Year → Album → Disc → Track → Title；Refrain 新建 Auto Playlist 使用同一默认 Sort。已有 Auto Playlist 的来源规则不静默迁移，用户通过该列表 Properties 修改一次 Sort，避免组件擅自改写查询配置。
26. Playlist View 与底部播放控制栏之间保留严格 1 DIP 分隔边界；可见行容量只计算完整行，不能把被底边裁掉的部分行当作可见行。Two-line 等任意密度的最后一行必须完整显示，否则留到后续滚动位置。
