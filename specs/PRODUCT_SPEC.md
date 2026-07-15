# Refrain 产品与界面规格

- 状态：可实现
- 版本：0.3
- 最后更新：2026-07-13
- 参考：Foobox 8 Basic、任务 001 中的五张原始截图、foobar2000 2.25.10
- 作用：这是 Refrain 产品范围和用户可见行为的唯一总规格；具体功能实现前仍需建立对应模块规格和中文程序逻辑
- 模块规格：[`Playback Shell`](modules/PLAYBACK_SHELL.md)、[`Album Cover Browser`](modules/ALBUM_COVER_BROWSER.md)、[`Now Playing Header`](modules/NOW_PLAYING_HEADER.md)

## 1. 产品定义

Refrain 是面向 Windows 10/11 x64、foobar2000 2.25.10 和 Columns UI 3.5.0 的原生整合组件。它使用 C++、Win32、Direct2D 和 DirectWrite 重建用户熟悉的 Foobox Basic 布局与日常操作，但不继承 JSplitter、Spider Monkey Panel、JScript Panel 等 JavaScript UI 宿主架构。

“重建”是还原经过确认的外观结构、信息层级和操作习惯，不是翻译或复制 Foobox JavaScript。界面状态、播放、媒体库、播放列表和菜单必须通过 foobar2000 与 Columns UI 正式 SDK 获取和操作。

## 2. 产品形态与安装

- Refrain 对用户呈现为一个 Columns UI 根面板，不是一组需要自行拼装的 Refrain 小面板。
- 最终主要交付物是 `.fb2k-component`，不是独立 EXE。
- 用户通过 foobar2000 Components 页面或打开组件包安装。
- 如果 SDK 无法在不覆盖用户配置的情况下安全建立完整根布局，可以另附版本化布局导入文件；不得擅自覆盖现有 Columns UI 布局。
- ESLyric、Playback Statistics 等第三方组件在再分发许可未核清前独立安装，不塞入 Refrain 包。
- 升级应保留兼容的快捷键、播放列表、列、分组、分隔比例、右下视图、字体和颜色配置；迁移冲突只能重置受影响项，并给出记录。

## 3. 第一套主界面

结构基准为只读参考中的 `foobox8 basic.fth` 与任务 001 的原始截图。`.fth` 不能直接转换成 Refrain 或 Columns UI 布局，只用于观察和重建。

### 3.0 固定结构与正式命名

Refrain 不是允许用户自由拼装、拖走、浮动、交换或增删面板的布局编辑器。区域拓扑和页面模式由 Refrain 固定；既定分隔线只用于调整相邻区域的宽度或高度，不能改变区域身份、顺序或归属。合法分隔尺寸保存并在重启后恢复。

第一版用户界面统一使用英文。中文名称只用于本规格解释，不作为第一版界面文案；代码标识符使用下表英文名。

| 英文正式名称 | 中文解释 | 固定位置或作用 |
| --- | --- | --- |
| `Native Window Frame` | Windows 原生窗口框架 | 最外层标题栏和系统窗口按钮，始终存在 |
| `Top Bar` | 顶栏 | 主菜单和当前播放摘要，始终存在 |
| `Main Menu` | 主菜单 | `Top Bar` 内的 foobar2000 正式菜单组 |
| `Now Playing Summary` | 顶栏播放摘要 | `Top Bar` 内菜单后的当前曲目摘要 |
| `Content Workspace` | 内容工作区 | 位于顶栏和底栏之间，承载下述页面模式 |
| `Playlist Browser` | 播放列表浏览器 | `Playlist Workspace` 左侧 |
| `Playlist View` | 播放列表视图 | `Playlist Workspace` 中央，显示活动播放列表 |
| `Column Header` | 列表表头 | `Playlist View` 内的列标题区域 |
| `Group Header` | 分组标题 | `Playlist View` 内的专辑等分组标题 |
| `Track List` | 曲目列表 | `Playlist View` 内的曲目行区域 |
| `Right Side Panel` | 右侧面板容器 | `Playlist Workspace` 右侧，在当前播放与播放队列模式间切换 |
| `Now Playing Panel` | 当前播放面板 | `Right Side Panel` 默认模式，容纳封面、歌词与歌曲信息 |
| `Playback Queue Panel` | 播放队列面板 | `Right Side Panel` 的整栏队列模式 |
| `Now Playing Header` | 当前播放摘要区 | `Now Playing Panel` 上部，固定显示封面、评分与摘要 |
| `Artwork View` | 封面视图 | `Now Playing Header` 内的封面内容 |
| `Lyrics View` | 歌词视图 | `Now Playing Panel` 下部的默认内容 |
| `Track Details View` | 歌曲信息视图 | 与 `Lyrics View` 共用右下位置，二者切换显示 |
| `Album Cover Browser Workspace` | 专辑封面浏览工作区 | 只以 Album 为浏览维度的独立页面模式 |
| `Album Source Selector` | 专辑来源选择器 | 只选择 `Media Library` 或某个播放列表 |
| `Album Cover Browser` | 专辑封面浏览器 | 工作区左侧的专辑浏览容器 |
| `Album Cover Grid` | 专辑封面网格 | 显示来源中按专辑归组后的封面 |
| `Album Track List` | 专辑曲目列表 | 工作区右侧，显示当前专辑的歌曲 |
| `Album Track Header` | 专辑曲目表头 | 显示当前专辑身份和曲目数量 |
| `Playback Bar` | 播放控制栏 | 底部常驻区域，包含进度和快捷控制 |
| `Seek Bar` | 播放进度条 | `Playback Bar` 上沿，始终存在 |
| `Control Bar` | 快捷控制区 | `Playback Bar` 下部，始终存在 |
| `Workspace Switcher` | 页面切换组 | `Control Bar` 左侧的页面入口与设置齿轮 |
| `Transport Controls` | 传输控制组 | `Control Bar` 中央的打开、上一首、播放、下一首、停止 |
| `Playback Tools` | 播放工具组 | `Control Bar` 右侧的队列、媒体库、输出、静音和音量 |

`Playlist Workspace` 的固定顺序为 `Playlist Browser | Playlist View | Right Side Panel`。右栏默认显示 `Now Playing Panel`，可由底部正式按钮整栏切换为 `Playback Queue Panel`；切换不改变宽度、顺序或父级。`Album Cover Browser Workspace` 的固定顺序为 `Album Cover Browser | Album Track List`。页面切换只能在这些已定义模式之间发生，不能把其中一个区域拖进另一模式或生成自定义组合。

允许并保存的分隔尺寸只有四组：`Playlist Browser / Playlist View` 的左右宽度、`Playlist View / Now Playing Panel` 的左右宽度、`Now Playing Header / 右下视图` 的上下高度，以及 `Album Cover Browser / Album Track List` 的左右宽度。分隔线不能改变顺序、隐藏区域、生成浮动窗口或重组页面。

### 3.1 顶部区域

- 保留 Windows 原生标题栏、系统菜单、最小化、最大化/还原和关闭行为。
- 不复刻 Foobox 隐藏系统标题栏并自绘窗口按钮的方式。
- foobar2000 的文件、编辑、视图、播放、媒体库、帮助等正式菜单保持可用。
- 当前播放摘要可以按参考显示在菜单后的顶部区域，但不能破坏 Alt 菜单、系统标题栏和窗口拖动。

### 3.2 左栏：播放列表管理与入口

- 左栏顶部只显示 `Playlists` 标题，其下直接显示普通/自动播放列表和各自项目数。
- Refrain 不提供 Playlist Filter 或歌曲搜索框，也不为其保留隐藏占位；歌曲搜索继续使用 foobar2000 的 `Ctrl+F`，原生 Album List 从底部入口打开。
- 当前活动列表用强调色整行标记，并与正在播放列表的状态明确区分。
- 支持选择、新建、取消式重命名、复制、删除确认、排序/拖动和接收曲目。
- 自动播放列表不能被当作普通列表错误修改；FavPop、FavScore 等名称和查询由用户及 foobar2000 管理，Refrain 不重建或覆盖。
- 媒体库入口固定调用 foobar2000 原生 Album List。Refrain 不改用 ReFacets，不重建 JS Smooth Browser，也不维护第二份媒体库分类。
- 本条不排除第 3.7 节经用户批准的 Album-only `Album Cover Browser`；该页面只是读取 foobar2000 媒体库或播放列表的另一种视图，不复制媒体库数据。

### 3.3 中栏：分组播放列表

- 显示当前活动播放列表，支持普通、自动、空列表和大列表。
- 截图基准列包括封面、序号、标题、艺术家、专辑、流派、等级、比特率、编码和时间。
- 支持专辑/艺术家/流派/目录等经批准的分组、组标题摘要、年份、组内封面和折叠规则。
- 支持列显示/隐藏、宽度、顺序、排序和标题格式安全回退。
- 支持滚动、单选、多选、范围选择、焦点、当前播放、队列、双击/键盘播放、上下文菜单和内外部拖放。
- 列宽、顺序、显示状态、分组方式和主列表样式必须持久化并可迁移。
- 评分使用 Playback Statistics 的 `%rating%`；当前范围不提供 Mood 控件，也不以 `MOOD` 替代评分。
- `Show Now Playing` 可以跨播放列表切换到真实播放位置，展开所属分组、选中并滚动到当前曲目；`Show in Default Playlist` 可以把任意曲目定位到已配置的 Default Playlist。两项操作只改变 Playlist Workspace 的查看位置，不改变播放、排序或列表内容。详细规则见 [`播放定位与 Playlist View 恢复规格`](modules/PLAYBACK_LOCATION_AND_RESTORE.md)。

### 3.4 右上：封面、评分和基本信息

详细执行要求以 [`Now Playing Header 模块规格`](modules/NOW_PLAYING_HEADER.md) 为准。从上到下固定为：

1. 正方形封面；
2. 五星评分；
3. 歌曲标题；
4. `歌手 | 专辑`；
5. `编码 | 码率`。

五类内容同时显示，不采用 Foobox 默认每 12 秒轮换的行为。Codec Profile、Sample Rate 和其他扩展技术字段不属于右上必需摘要，可进入右下 `Track Details View`。播放时默认跟随正在播放项；停止时按最终参考观察确定是否跟随列表焦点。

双击 `Track Title` 执行 `Show Now Playing`；双击 `Artwork View` 不承担定位命令，继续保留给封面自身交互。右键菜单仍可提供明确的播放定位命令。

封面异步加载，支持缺图、损坏、多来源、刷新和有上限的缓存。晚到结果不得覆盖新曲目，绘图线程不得等待磁盘。

五星评分调用 Playback Statistics 的 `Rating/1` 至 `Rating/5` 和取消评分命令。默认只写 Playback Statistics 数据库，不额外写音频文件 `RATING` 标签，确保现有自动播放列表继续查询同一个 `%rating%`。

### 3.5 右下：歌词与歌曲信息

- 默认嵌入 ESLyric 1.0.6.7 的 Columns UI 面板。
- 自动/手动歌词搜索、下载、解析和歌词源配置由 ESLyric 负责。
- Refrain 不截获歌词区域的双击，让 ESLyric 使用自身全屏歌词功能。
- 鼠标中键在歌词与 Refrain 原生只读歌曲信息之间切换，再次中键返回。
- 歌曲信息按“字段名—字段值”显示，支持滚动、选择和复制；编辑调用 foobar2000 Properties，不在列表中直接改标签。
- 切换不改变右上内容、不停止播放或歌词搜索；当前规格暂定保存切换状态，仍需用户最终确认。
- ESLyric 缺失或失败时在原区域显示安装/恢复提示，不能影响播放列表、封面和播放控制。
- Refrain 可以研究并提供一套 ESLyric 推荐实例配置，但只能使用 Columns UI/ESLyric 正式公开的实例配置接口；不得解析或直接改写 ESLyric 私有 `.cfg`。推荐值的首次应用、保留用户已有设置、重新应用和版本兼容规则见 [`ESLyric 推荐默认设置规格`](modules/ESLYRIC_RECOMMENDED_DEFAULTS.md)。

### 3.5.1 右栏播放队列模式

- 底部 `Playback Tools` 的队列按钮把整个 `Right Side Panel` 在 `Now Playing Panel` 与 `Playback Queue Panel` 间切换。
- 队列模式使用完整右栏高度；切回后恢复封面、歌词/歌曲信息及原上下分隔比例。
- 队列模式不创建临时播放列表；直接读取和管理 foobar2000 真实播放队列。
- 队列只显示用户显式排入的项目，不复制活动播放列表来预测未来顺序；重复项目允许存在。Remove/Clear 位于队列右键菜单，Delete 删除所选，双击/Enter 立即播放并消费目标项。
- 队列曲目表头与列表和 `Album Track Header` / `Album Track List` 共用绘制与基础交互资源，但数据、选择和特有命令相互隔离。
- 用户主动打开队列后，切歌和停止不自动关闭；队列模式不跨重启保存。

### 3.6 底部控制区

- 上沿为横跨主窗口的播放进度条，左侧显示已播放时间，右侧显示总时长或经观察批准的剩余时间。
- 中央为上一首、播放/暂停、下一首和停止。
- 左右功能按钮组、静音和音量按截图位置与分组还原；具体图标命令必须由实际参考观察确认，不能看图猜测。
- 播放状态、进度、音量、静音、输出设备和播放顺序与 foobar2000 双向同步。目标布局隐藏 Columns UI 原生 `Default` 下拉框，Refrain 在右下角提供同一套正式 Playback Order 的视觉入口；它不建立第二套规则，也不显示未来曲目预测列表。
- 不可跳转的流显示明确禁用状态；用户拖动进度时先预览，松开后请求跳转，不伪造成功。
- 设置齿轮打开 foobar2000 Preferences 中的 Refrain 设置页。

### 3.7 专辑封面浏览器页面

- 详细执行要求以 [`Album Cover Browser 模块规格`](modules/ALBUM_COVER_BROWSER.md) 为准。
- `Album Cover Browser Workspace` 是与 `Playlist Workspace` 并列的固定页面模式，不是可拖入主界面的独立面板。
- 唯一浏览维度是 Album；明确不提供 Artist、Album Artist、Folder、Library Directory、Genre 或其他分类模式。
- 页面级工具只保留 `Album Source Selector`，用于显示和选择 `Media Library` 或某个播放列表；不保留 Foobox 的分类模式按钮、通用过滤框和其他浏览工具。
- 左侧 `Album Cover Browser` 以 `Album Cover Grid` 浏览专辑；点击封面后，右侧 `Album Track List` 显示该专辑的歌曲。
- 两区左右顺序固定，分隔线只调整宽度；顶栏和 `Playback Bar` 切换前后始终保留。
- Foobox 的设置页面截图只用于盘点参考配置项，不构成 Refrain 内嵌设置页面需求。

## 4. 输入与交互规则

### 4.1 鼠标

- 图标按钮具有稳定命中区和简短提示；按下后移出再松开应取消激活，拖动控件除外。
- 单击、双击和中键只执行已批准行为，不能因异步刷新改变目标对象。
- 右键菜单针对鼠标对象或当前选择的规则保持一致；菜单打开后通过稳定身份识别目标。
- 滚轮优先作用于指针所在列表；音量和进度上的滚轮行为需参考观察后确定。

### 4.2 键盘与输入法

- Tab/Shift+Tab 顺序可预测，焦点始终可见。
- 列表支持方向键、Page Up/Down、Home/End、Enter、空格和适用的 Ctrl/Shift 多选。
- 重命名使用 Enter 确认、Esc 取消；危险快捷键遵循确认规则。
- Refrain 不无条件吞掉 foobar2000 全局快捷键。
- Refrain 不提供文本搜索控件；`Ctrl+F` 及其输入法处理继续由 foobar2000 负责。

### 4.3 拖放和分隔条

- 拖动开始、允许/禁止、插入位置和目标列表都有可见反馈。
- 失败不得删除来源；窗口外、Esc、失焦或目标消失时安全取消。
- 分隔条有可见指针反馈、连续拖动、最小尺寸和经批准的窄窗口策略。
- 分隔位置以可迁移的逻辑尺寸或比例保存，不把截图像素写死。
- 分隔条只改变相邻区域尺寸；任何区域都不能通过拖拽改变顺序、父级、页面模式或停靠状态。

## 5. 状态与视觉规则

每个模块实现前必须覆盖：初始加载、正常、空数据、加载中、失败、禁用、悬停/按下、选中/焦点、外部数据变化和关闭/切换。

- 当前播放、当前选择、键盘焦点和鼠标悬停是不同状态，不能只使用同一种背景色。
- 封面加载、缺图和失败使用稳定占位，不改变行高或使布局跳动。
- 长中文、英文、数字和无空格文本必须有截断、换行或提示规则。
- 错误信息说明影响与恢复动作，不直接倾倒 HRESULT 或 C++ 异常文本。
- 自绘控件必须提供可访问名称、角色、状态和键盘焦点。

## 6. 主题、设置和快捷键

- 第一套先实现与用户截图相符、所有状态可辨的 Foobox Basic 默认视觉；它本身必须完整可用，不能靠后续主题补救。
- 深色、浅色、强调色和完整颜色系统是后续独立完整模块，在 1.0 前实现，主要改变视觉而不任意改变布局。
- 系统高对比度、Columns UI 颜色和 Refrain 颜色的优先级在主题模块规格中固定。
- 设置统一进入 foobar2000 Preferences，不建立内嵌 `Settings View`；底部齿轮直接打开 Refrain 对应设置页。
- 设置具有稳定 GUID、类型、默认值、合法范围、版本、迁移和恢复默认。
- Refrain Preferences 提供默认开启的 `Restore Playlist View to last played track`。它只恢复 Playlist View 的列表、专辑组、曲目焦点与滚动位置；实际播放启动和时间点恢复继续服从 foobar2000 核心，不由 Refrain 建立第二套播放会话。
- 快捷键继续由 foobar2000 Preferences 管理；Refrain 注册稳定命令，不建立第二套全局快捷键系统，也不无故改变已发布命令身份。

## 7. 外部依赖和排除范围

### 7.1 已批准依赖

- Columns UI：运行基础；首个已验证稳定基线为 3.5.0。
- ESLyric x64：歌词面板、搜索、下载和全屏。首个已验证基线为 1.0.6.7；其 QuickJS 只允许用于歌词源与解析，不是 Refrain UI 宿主。
- Playback Statistics：五星评分和 `%rating%` 数据源。首个已验证基线为 3.1.10。

Columns UI、ESLyric 与 Playback Statistics 由各自作者独立发布和更新，不是 Refrain 的私有运行库。Refrain 安装包不捆绑、不替换、不静默下载或自动升级它们，也不要求用户为了使用 Refrain 始终追逐最新版。Refrain 在运行时检查所需宿主、面板、命令和数据能力：能力存在就继续工作，不因版本号高于首个验证基线而阻止使用或显示无意义警告；ESLyric 或评分能力缺失/实际不兼容时只禁用对应功能并给出提示。Columns UI 是 Refrain 的必要宿主，缺失或宿主接口实际不兼容时 Refrain 本身无法加载，但不会损坏 foobar2000 的其他组件或用户数据。

Refrain 每个正式版本只记录发布时实际验证过的基线环境，不承诺也不要求为第三方每次更新重新打包。用户可以长期保留一个稳定可用的 foobar2000、Columns UI、ESLyric、Playback Statistics 与 Refrain 组合；只有用户主动更新后真的发生故障，或项目准备发布新的 Refrain 版本时，才需要恢复开发环境进行兼容调查和回归。

Refrain Preferences 提供只读的 `Dependencies` 状态区，显示已检测版本以及缺失/实际不兼容原因，并提供正确的官方更新入口。该状态区不后台联网、不催促用户追新、不代替用户确认安装，也不把“版本较新但能力正常”标成错误。

### 7.2 明确排除

- Biography；
- Refrain 内嵌频谱或专用频谱按钮；用户自行安装的频谱仍可从 foobar2000 正式菜单启动；
- 收音机；
- JSplitter、Spider Monkey Panel、JScript Panel 或其他 JavaScript UI 宿主；
- ReFacets、JS Smooth Browser 或 Refrain 自建媒体库；
- 移动端、云同步、在线账户、音乐商店、解码器和 DSP；
- 修改 foobar2000 核心、参考目录或用户日常 C 盘安装。

## 8. 数据归属

| 数据 | 保存方 | Refrain 的责任 |
| --- | --- | --- |
| 音频文件和标签 | 用户与 foobar2000 | 通过正式服务读取/编辑，不假定文件可写 |
| 媒体库与播放列表 | foobar2000 | 订阅变化、发出合法操作、显示结果 |
| 评分 | Playback Statistics | 调用正式命令并显示 `%rating%`，默认不写文件标签 |
| 歌词及歌词源 | ESLyric | 托管面板、处理缺失，不维护冲突副本 |
| 播放、音量、输出和顺序 | foobar2000 | 双向同步，不伪造成功状态 |
| 布局、列、分组、右下视图和主题 | Refrain | 版本化保存、校验、迁移、恢复默认 |
| Playlist View 上次播放位置 | Refrain | 保存稳定播放列表/曲目身份、分组和滚动锚点；不复制音频或播放队列 |
| 封面缓存 | Refrain/foobar2000 服务 | 有容量和失效规则，不作为唯一来源 |
| Foobox 参考配置 | 只读参考 | 不写入、不直接加载为运行配置 |

## 9. 整体程序逻辑

### 9.1 启动

1. Columns UI 创建 Refrain 根面板。
2. Refrain 建立独立于绘图的状态模型，读取并校验版本化设置。
3. 程序连接播放、播放列表、媒体库、颜色、字体和菜单服务，取得第一份真实状态。
4. 界面按当前 DPI 和窗口尺寸计算布局并创建绘图资源。
5. 封面和大列表异步准备；结果返回 UI 前重新核对窗口生命期和目标身份。

### 9.2 用户操作

1. 输入先转成明确意图，例如“播放下一首”或“给当前曲目五星”。
2. 状态模型判断操作是否允许；不允许时禁用或说明原因。
3. 允许的操作交给 foobar2000/外部组件正式服务，Refrain 不预先伪造成功。
4. 回调确认变化后更新模型，只重绘受影响区域。
5. 用户偏好延迟合并保存；媒体库和播放列表数据继续由 foobar2000 管理。

### 9.3 关闭与恢复

1. 标记窗口关闭，拒绝新的异步显示请求。
2. 注销回调、计时器和托管面板，晚到结果只被丢弃。
3. 保存合法设置并按 RAII 释放窗口、Direct2D、DirectWrite、GDI 和 SDK 资源。
4. 设备丢失只重建设备相关资源，不丢选择和布局状态。
5. 单个封面、标题格式或 ESLyric 失败不能拖垮其他界面。

## 10. 质量要求

- 绘图和输入线程不得执行阻塞式文件或网络 I/O。
- 列表只处理可见部分；封面缓存有上限；资源有明确释放时机。
- 支持运行时 DPI 变化、字体缩放、系统高对比度和减少动画。
- 必须测试空列表、无封面、无媒体库、播放停止、网络流、文件失效、权限不足、依赖缺失、配置损坏和异步晚到。
- 必须检查反复创建/销毁、组件卸载、快速切歌、批量列表变化和设备丢失。
- x64 Debug/Release 均需构建；新增警告消除或记录批准原因；Release 不留下隐私路径或刷屏日志。
- 每个纵向模块包含规格、中文逻辑、自动检查、开发实例部署和用户人工验收，不提交空布局、假数据或只有成功路径的阶段性代码。

## 11. 仍需观察或确认

- `Playlist Workspace` 三栏、右侧上下分区及 `Album Cover Browser Workspace` 两栏的默认比例。
- 各区最小尺寸、窄窗口原则和不同 DPI 下的尺寸换算。
- ESLyric 与 Playback Statistics 的再分发许可。
- Columns UI 是否允许安全自动建立根布局，还是需要用户导入布局文件。
- ESLyric 1.0.6.7 的不透明实例配置能否在“全新安装、已有用户配置、版本升级”三种状态间安全区分并迁移；未完成隔离验证前不自动覆盖。

步骤 01 只冻结固定结构、页面模式和命名。默认比例由步骤 15 随默认视觉批准；最小尺寸、窄窗口和 DPI 由步骤 17 批准；底部图标、搜索、播放列表、封面、ESLyric 和生命周期等模块内部观察也已分派到后续对应步骤，不要求在步骤 01 一次决定完整个软件。影响架构或用户习惯的结果仍必须由用户批准后才能进入实现。

## 12. 规格批准条件

- [x] 产品是单一整合式 Columns UI 根面板。
- [x] 第一套结构使用 Foobox Basic 和用户截图。
- [x] 保留 Windows 原生标题栏。
- [x] 媒体库入口使用 Album List。
- [x] 歌词依赖 ESLyric；Biography 和内嵌频谱排除。
- [x] 评分使用 Playback Statistics `%rating%`，默认不写文件标签。
- [x] 设置进入 foobar2000 Preferences，快捷键由 foobar2000 管理。
- [x] 默认视觉先完成，完整主题系统作为后续完整模块。
- [x] 固定拓扑、两个页面模式、四条可调整分隔线和英文界面方向已经确定。
- [x] 专辑封面浏览页面属于第一套完整范围；只保留 Album 维度和媒体库/播放列表来源选择。
- [x] 顶栏、底栏、浏览器和右上内部结构名称已经批准；冲突项按 Album-only 范围删除。
- [x] 第 11 节剩余观察均已分派给后续对应步骤，不阻挡下一阶段。
- [x] 用户明确确认本规格可以进入实现。
