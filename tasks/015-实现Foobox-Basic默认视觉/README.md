# 015-实现 Foobox Basic 默认视觉

- 状态：已验收
- 对应规格：[`../../specs/modules/FOOBOX_BASIC_DEFAULT_VISUAL.md`](../../specs/modules/FOOBOX_BASIC_DEFAULT_VISUAL.md)
- 前置任务：[`../014-还原顶部摘要与底部功能按钮/README.md`](../014-还原顶部摘要与底部功能按钮/README.md)（已提交为 `356f9f3 还原顶部摘要与底部功能按钮`）
- Fork 提交标题：`实现 Foobox Basic 默认视觉`
- 最后更新：2026-07-15

## 任务目标

把任务 14 已经可用的完整界面整理为协调、可正式使用的 Foobox Basic 默认视觉。用户决定分阶段核对：先压缩底部控制区，再逐项批准内部结构、滚动可见性、右栏宽度和 Now Playing 自适应内容。

## 明确不做

- 除已批准的可拖右栏宽度和 Now Playing 内部自适应外，不改左、中、右默认比例及右侧上下默认比例。
- 不改列表、表头、分组、字体、完整颜色系统、矢量图标或未逐项讨论的细节间距。
- 不把视觉修改扩展成新的播放行为、Workspace 或设置功能。
- 任务 15 尚未整体完成时不提前标记为已验收或要求提交。

## 中文程序逻辑

1. Refrain 继续从窗口客户区和当前 DPI 得到逻辑宽高。
2. 底部控制区使用固定的 Foobox Basic 默认逻辑高度，不随窗口高度按比例放大。
3. 进度行和按钮行在这段高度内使用经过验证的固定偏移；横向布局及所有真实命令来源不变。
4. 每次窗口大小或 DPI 改变都重新计算绘制和命中矩形，因此画面和点击位置同步移动。
5. 当客户区过小时，主内容区截到零，不生成负高度；窗口关闭和重建不需要新增持久化状态。
6. 五套自绘列表继续以现有项目数和可见容量判断是否溢出；只有溢出列表在滚轮、键盘翻页、自动滚动或直接操作滑块时显示自己的滑块，并在最后一次活动后 1000 毫秒隐藏。
7. Playlist View 永久从表头和正文扣除 12 DIP 滚动栏位；表头方格使用相同表面色且不参与列排序、列拖动或列宽调整。Playlist Browser、Album Grid、Album Track List 与 Playback Queue 不保留方格，滑块覆盖内容边缘。
8. Playlist View 与右侧展示区共用一条 1 DIP 竖线和 8 DIP 隐形热区；拖动时把右侧宽度限制在 280–440 DIP，把合法比例写入带稳定 GUID 的版本 3 设置。ESLyric 子窗口为该热区留出空间，歌词模式与歌曲信息模式都能拖动。
9. Now Playing 封面不再使用固定左右和顶部边距；边长由右栏宽度与上区扣除信息区后的高度共同决定，信息区跟随实际封面底边。
10. Track Details 在当前下区视口内重新钳制滚动位置并硬裁剪；ESLyric 伪透明背景由 Refrain 父窗口提供与 Now Playing 一致的背景色。
11. Album Cover Browser 使用唯一的无间距正方形封面墙；默认无文字，Hover/Selected 才在封面内部底部显示单行 Album 标题，Selected 保持撞色描边。
12. Refrain 不再提供 Playlist Filter：删除输入控件、清除按钮、筛选状态、可见行筛选模型、专用拖放限制和布局占位；`Playlists` 标题下直接显示完整列表。foobar2000 `Ctrl+F` 与原生 Album List 入口不变。
13. Lyrics View 不能只依赖父窗口伪透明；ESLyric 播放时主动绘制的背景通过其可选 Automation 接口同步为 Refrain 当前背景色。接口不可用时保持原 ESLyric 行为，不影响歌词功能。

## 可检查步骤

### 步骤 1：冻结宏观比例范围

- 输入：任务 14 最终截图、Foobox `主界面.jpg`、用户最新决定。
- 动作：确认只有底部控制区比例不协调，冻结其他宏观比例。
- 产物：默认视觉规格 0.1 的底栏章节。
- 用户检查方法：核对规格没有顺带改变其他区域。
- 通过标准：改动边界只包含底栏纵向高度和其内部纵向位置。
- 状态：已完成

### 步骤 2：实现紧凑底部控制区

- 输入：当前 104 逻辑像素底栏和参考图约 68 像素底栏。
- 动作：改为 68 逻辑像素，并重新安置进度行和按钮行。
- 产物：不裁切、不重叠且命中一致的紧凑底栏。
- 用户检查方法：对照任务 14 截图观察主内容增高和底栏变薄，逐个操作底部控件。
- 通过标准：视觉比例接近参考，全部控制仍正常。
- 状态：已完成并经用户确认

### 步骤 3：自动验证与隔离部署

- 输入：底栏比例实现。
- 动作：运行相关测试、Debug/Release 构建并只部署到 `foobar-dev`。
- 产物：供用户人工检查的新组件。
- 用户检查方法：最大化、还原并完成底部功能回归。
- 通过标准：构建测试通过，开发实例加载的新 DLL 与构建产物哈希一致。
- 状态：已完成

### 步骤 4：继续核对任务 15 其余视觉阶段

- 输入：用户验收后的底栏比例和后续视觉意见。
- 动作：依次核对内容密度、文字、状态表面和图标，不把未讨论项混入当前阶段。
- 产物：逐步完善的默认视觉规格和实现。
- 用户检查方法：每一阶段单独对照截图。
- 通过标准：任务 15 全部视觉层级完成后再进行整体验收。
- 状态：已完成并经用户整体确认

### 步骤 5：实现滚动可见性与右栏宽度

- 输入：用户批准的固定滚动栏位、1 秒隐藏和右栏宽度规则。
- 动作：为五套自绘列表增加独立活动期限，为 Playlist View 增加固定栏位，为右栏增加可保存的纵向分隔热区。
- 产物：内容不会被滑块遮盖、无溢出时不出现滑块、停止滚动后自动收起且可拖动保存的右侧展示区。
- 用户检查方法：分别滚动 Playlist View、Playlist Browser、Playback Queue、Album Grid 和 Album Track List；等待一秒；用短列表检查无滑块；拖动右栏并重启。
- 通过标准：各滑块只在自己的列表活动时出现；Playlist View 方格和正文栏位始终保留；Album Workspace 与 Playlist Browser 无方格；右栏范围、光标、拖动和重启恢复均正确。
- 状态：已完成并经用户确认

### 步骤 6：实现 Now Playing 自适应内容

- 输入：用户标注的封面、信息区和 Metadata 截图，以及 Lyrics View 同色要求。
- 动作：移除封面固定留白，按宽高共同约束正方形封面；让其下信息区跟随；钳制并裁剪 Track Details；为 ESLyric 伪透明模式提供共同父背景。
- 产物：窄栏封面可顶格、宽栏动态留白、信息不交叠、Metadata 不越界且歌词背景可随共同主题表面变化的右侧展示区。
- 用户检查方法：把右栏分别拖到最窄、居中和最宽，并上下拖动横向分隔；切换 Lyrics / Track Details，滚到 Metadata 最底；将 ESLyric 背景设为伪透明后检查上下区底色一致。
- 通过标准：封面始终为正方形且无固定左右空白；所有文字留在上区；Metadata 永不越界；Lyrics View 与 Now Playing 背景相同。
- 状态：已完成并经用户确认

### 步骤 7：实现 Album Cover Wall

- 输入：用户确认的 `封面浏览器最终参考.jpg` 和 Album Browser 0.6 规则。
- 动作：把固定 184 DIP 高的封面卡片改成连续正方形单元；封面居中裁切铺满；根据 Hover/Selected 绘制内部单行标题条和选中撞色描边；同步所有滚动、命中和键盘可见范围计算。
- 产物：没有卡片留白、没有常驻两行文字且选择不会意外清除的封面墙。
- 用户检查方法：调整 Workspace 分隔和窗口宽度；悬停多个封面并移出；选择后点击空白、来源和右侧列表；选择另一专辑；检查非正方形图、缺图、滚动及键盘移动。
- 通过标准：封面连续接壤且不变形；Hover 标题即时切换/消失；Selected 标题与撞色描边保持；右侧曲目和既有播放行为无回归。
- 状态：已完成并经用户确认

### 步骤 8：移除 Playlist Filter

- 输入：用户最新决定“Refrain 不要 Filter；不是隐藏”。
- 动作：删除原生 Edit、清除按钮、筛选状态与模型、无匹配提示、筛选期拖放分支、布局占位和相关测试。
- 产物：`Playlists` 标题下直接显示完整播放列表的干净侧栏。
- 用户检查方法：确认没有 Filter 控件或空白占位；测试播放列表选择、重排、右键、拖入和空白建表；确认 `Ctrl+F` 与 Album List 仍正常。
- 通过标准：代码和界面均不存在 Filter；其删除不影响任务 12 的播放列表管理或任务 13 的两个保留入口。
- 状态：已完成并经用户确认

### 步骤 9：同步 ESLyric 背景色

- 输入：用户提供的“播放时白色、无曲目时同色”截图、Foobox `infoArt.js` 只读证据和已开启的 `pref.script.expose`。
- 动作：在 ESLyric 子窗口创建成功后，通过其当前加载模块的 Automation 类工厂调用 `GetAll()` / `SetBackgroundColor()`；不修改字体、歌词颜色或背景类型。
- 产物：有歌词和无歌词时均与 Now Playing 使用相同背景色的 Lyrics View。
- 用户检查方法：播放有歌词和无歌词曲目，暂停、切歌和停止；确认背景不再变白，右键、双击全屏、歌词滚动仍正常。
- 通过标准：播放状态变化不改变背景色；Automation 不可用时不会导致崩溃或让 ESLyric 失效。
- 状态：已完成并经用户确认

### 步骤 10：确认双语歌词默认行为

- 输入：用户希望英文原文与中文译文默认双行显示。
- 动作：检查开发实例实际安装的 ESLyric 1.0.6.7 搜索脚本、KRC 解析器、Automation 接口与持久化配置；不等待外部 Wiki，不改写第三方私有配置。
- 产物：有译文时默认保留原文与译文的产品规则；当前 NetEase 脚本与 KRC 解析器已执行这一行为，因此 Refrain 无需新增虚假的双行开关。
- 用户检查方法：播放一首已知歌词源含中文译文的外语歌曲，选用 NetEase 或含翻译的 KRC 结果，确认同一时间点显示原文和译文两行。
- 通过标准：源提供译文时显示双语；源不提供译文时仍只显示真实可用的单语歌词，不重复或伪造第二行。
- 状态：本机实现证据已确认，无需代码或私有配置变更

## 用户检查点

- [x] 用户已核对本阶段只修改底部控制区比例。
- [x] 用户已决定其他宏观比例保持原样。
- [x] 用户已确认紧凑底栏没有视觉或功能问题。
- [x] 用户已确认五套滑块、固定栏位和右栏拖动没有视觉或功能问题。
- [x] 用户已确认 Now Playing 自适应封面、Metadata 裁剪和 Lyrics 同色没有视觉或功能问题。
- [x] 用户已确认任务 15 的最初需求与测试全部完成，批准整体验收。

## 验证记录

- 2026-07-14：确认任务 14 已提交为 `356f9f3`，工作树干净后开始任务 15。
- 2026-07-14：参考图为 1920×1032，底部区域约 68 像素；代码当前使用 104 逻辑像素，在用户 125% 缩放环境下约为 130 物理像素。用户批准只修改该比例。
- 2026-07-14：新增纯计算布局模型，将底栏设为 68 DIP、进度中心偏移设为 12 DIP、普通按钮顶部偏移设为 29 DIP；编译期和运行时测试覆盖控件不重叠、主按钮不越界及极小窗口主内容不为负。
- 2026-07-14：x64 Debug 与 Release 均在 `/W4 /WX` 下构建成功，两配置各 10/10 自动测试通过。首次 Release 构建发现测试中的局部变量只在 `assert` 中使用，Release 移除断言后触发 C4189；改为编译期断言后完整重跑通过，组件代码没有编译失败。
- 2026-07-14：组件包 `dist/Refrain-0.1.0.fb2k-component` 已生成，SHA-256 为 `D6120FC970C22C78A6E124AF7F53AB474B558E04F83A5582E429C5E183784385`。Release DLL 只部署到 `.local/foobar-dev`，构建与部署 SHA-256 同为 `68EAF45C81B96AEE4CD736C203BA083CD0024B1B9BD3C00334592D982FB7FAD5`，运行进程确认加载该 DLL。
- 2026-07-14：用户检查 68 DIP 底栏后要求控件与容器等比例缩小、进度与按钮及按钮与底边视觉等距、进度条两端各内收 4 DIP，并要求窗口改变时保持固定外边距和三组锚点。具体数值授权 Codex 按参考审美决定。
- 2026-07-14：第二轮实现把普通按钮缩为 24 DIP、主按钮缩为 28 DIP、间距统一为 8 DIP；左组锚定 16 DIP 左边距，Play/Pause 精确居中，右组锚定 16 DIP 右边距，音量轨缩为 96 DIP。代码绘制图标随按钮按约三分之二缩放，进度条两端改为 68 DIP inset。
- 2026-07-14：第二轮 x64 Debug/Release 均在 `/W4 /WX` 下构建成功，两配置各 10/10 测试通过。Release 包 SHA-256 为 `EBC951E2589F5C255C545B539F2A66177EF912B93D9C2CA4D6B9A08053D907C1`；Release DLL 只部署到 `.local/foobar-dev`，构建与部署 SHA-256 同为 `7AD66EDAEA48A81B6D73B4A944657F15B65C6D64344780ECB09F351117F229B5`，运行进程确认加载该 DLL。
- 2026-07-14：用户要求下一轮移除内部粗边框：所有可拖分隔只显示灰色细线且拖动不高亮；所有列表滚动条改为无可见轨道的悬浮灰色滑块；范围覆盖 Playlist、Now Playing 上下分隔、Playback Queue 及 Album Workspace 两套滚动条和中央分隔。
- 2026-07-14：第三轮实现将分隔线统一为 8 DIP 隐形热区中的 1 DIP 灰线，滚动条统一为 12 DIP 隐形热区中的 3 DIP 灰色滑块；轨道不再绘制，内容延伸到边缘，悬停、按下和拖动均不切换强调色。右侧上下区移除重复横边框，只保留单一分隔线。
- 2026-07-14：第三轮 x64 Debug/Release 均在 `/W4 /WX` 下构建成功，两配置各 10/10 测试通过。Release 包 SHA-256 为 `5045FBADBC778248FD741EC1666A09FECCC0B57212C31BBC820169CAA9DE3BB7`；Release DLL 只部署到 `.local/foobar-dev`，构建与部署 SHA-256 同为 `9C862B808543A8073838435B2458027328071BDB3B2ED96D5EE7663FFA04E7B9`。首次 15 秒模块确认窗口超时，但开发进程持续正常响应，随后枚举进程模块确认同哈希 `foo_refrain.dll` 已加载；没有崩溃或组件拒绝记录。
- 2026-07-14：用户要求 Playlist View 表头和正文永久保留 12 DIP 滚动栏位，表头最右显示同色固定方格；Playlist Browser 与 Album Workspace 不显示方格。所有自绘滑块在滚动停止 1 秒后隐藏，内容不足时从不出现。用户同时要求 Playlist View 与 Now Playing 之间的细灰竖线可拖动，最小宽度由 Codex 决定。
- 2026-07-14：第四轮实现为 Playlist、Playlist Browser、Playback Queue、Album Grid 和 Album Track List 建立互不联动的活动期限；滚轮、键盘导致的视口变化、拖放自动滚动及滑块操作会显示对应滑块，最后活动后 1000 毫秒只隐藏绘制。继续复用既有 `count > capacity`/`maximumTopRow`，未引入系统滚动条或第二套滚动位置。
- 2026-07-14：Playlist View 的列宽计算与曲目裁剪永久止于最右 12 DIP 栏位，表头方格使用同一 surface 并从命中测试排除。Playlist Browser 新增覆盖式滑块但无方格；Album Workspace 两套滑块和 Playback Queue 同样保持覆盖式。
- 2026-07-14：右侧展示区新增默认 230‰、280–440 DIP 限制的可拖宽度；设置版本从 2 升到 3，并使用独立稳定 GUID 保存。竖线仍只画 1 DIP，热区为 8 DIP，ESLyric 子窗口右移半个热区，保证歌词子窗口不会截获竖线拖动。
- 2026-07-14：边界审计发现旧滑块尺寸函数在内容未溢出时返回整条轨道高度；绘制虽已跳过，但直接用矩形判断会留下不可见命中区。最终实现改为只有 `maximumTopRow > 0` 才创建滑块矩形，因此短列表既不绘制也不吞鼠标。
- 2026-07-14：最终第四轮 x64 Debug/Release 均在 `/W4 /WX` 下构建成功，两配置各 10/10 测试通过，`git diff --check` 通过。Release 包 SHA-256 为 `0B5C23347FBCB56D6E90970DB0FA39DDD27104355E4AED965121F8B85EEC0456`；Release DLL 只部署到 `.local/foobar-dev`，构建与部署 SHA-256 同为 `073E6C5BEC3A6FC2FED9730B8DCEDD3C6CBCFC755C3D8ABFF7E757224E3F4468`，进程确认正常加载且响应正常。
- 2026-07-14：第一次部署命令使用了旧参数名，被保护脚本拒绝且没有写入；随后按脚本实际 `-Instance dev -ComponentDll` 参数完成部署。强制结束 dev 留下 foobar2000 异常终止提示，读取提示后仅选择一次 `Start foobar2000 normally`，未勾永久跳过；C 盘日常实例始终保持运行且未触碰。
- 2026-07-14：用户确认第四轮滚动可见性、固定栏位和右栏拖动完全正确，随后批准 Now Playing 自适应内容：封面取消固定左右留白，Metadata 永不越界，Lyrics View 与 Now Playing 使用共同背景。
- 2026-07-14：第五轮移除封面固定 16 DIP 左右/顶部留白，新增纯计算的宽高共同约束模型；封面在高度允许时填满最窄右栏，宽栏受高度约束时自动居中。评分及三行信息从实际封面底边继续排布。
- 2026-07-14：Track Details 在每次绘制前按当前视口重新钳制滚动位置，并对整个下区使用 Direct2D 硬裁剪。Refrain 同时处理父窗口 `WM_ERASEBKGND`/`WM_PRINTCLIENT`，从单一背景色值为 ESLyric 伪透明模式提供与 Now Playing 相同的背景。
- 2026-07-14：第五轮 x64 Debug/Release 均在 `/W4 /WX` 下构建成功，两配置各 10/10 测试通过，`git diff --check` 通过。Release 包 SHA-256 为 `01699AD21A195C3F8BD018CB9CFCA7539320578439A505AF5E4A8000600300AC`；Release DLL 只部署到 `.local/foobar-dev`，构建、部署和进程加载 SHA-256 同为 `CF09A83EAEDA53A6AD410E57973F1C07B24F4F7E13BD5B250DE8BC8E1499D77C`，开发进程正常响应。
- 2026-07-14：用户提供 `封面浏览器最终参考.jpg` 并确认封面墙硬规则：无卡片边距、封面铺满正方形、Hover/Selected 才显示内部单行标题、Selected 保持撞色描边且点击其他区域不清除。
- 2026-07-14：第六轮把旧 184 DIP 卡片行与 132 DIP 封面替换为按可用宽度计算的连续正方形单元；封面使用居中正方形源裁切，滚动容量、滑块、命中、键盘可见范围与绘制共用同一单元尺寸。Hover 变化和移出立即重绘，Selected 标题和描边独立保持。
- 2026-07-14：第六轮 x64 Debug/Release 均在 `/W4 /WX` 下构建成功，两配置各 10/10 测试通过，`git diff --check` 通过。Release 包 SHA-256 为 `35FAA591EA18D638CF6C42B76EF1BE190D2E70021ECAB1A778B1565C774B06B5`；Release DLL 只部署到 `.local/foobar-dev`，构建、部署和进程加载 SHA-256 同为 `2A689325CDF62FF152EC36C8837BB168E79454BFFFE74765251094674868EACC`，开发进程正常响应。
- 2026-07-14：用户首次检查确认白色多行信息框来自 Refrain 旧 Album Grid tooltip，并要求删除；封面墙此后只使用内部单行 Hover/Selected 标题条，其他按钮 tooltip 不受影响。
- 2026-07-14：tooltip 修正版 x64 Debug/Release 各 10/10 测试通过，`git diff --check` 通过。Release 包 SHA-256 为 `7B1963137C02D0CBCEC86252A62F6E8D221A93577B64CA6E209C84E4BB7E40E2`；Release DLL 只部署到 `.local/foobar-dev`，构建、部署和进程加载 SHA-256 同为 `C78F85AFA22C75C7253D59E7228A2F8BA84FE57C5ACEBEFC339A5C7D6D102D74`。
- 2026-07-14：Filter 收尾保留原生单行 Edit 和 IME 路径，移除 `WS_BORDER` 系统白底粗框，改用 Playlist Browser 同色 GDI 背景、1 DIP Direct2D 灰框和 24 DIP 外高；字符版 `×` 暂用同色 owner-draw 按钮，SVG 替换已写入规格。
- 2026-07-14：Filter 收尾 x64 Debug/Release 各 10/10 测试通过，`git diff --check` 通过。Release 包 SHA-256 为 `BEC47CDED835A6C74E5EB46EC67FFC18C5E5F7483C0557B49D2E1543B3BAFEFE`；Release DLL 只部署到 `.local/foobar-dev`，构建、部署和进程加载 SHA-256 同为 `907F141A2FAE22A87DE7DFBE1DA1B11E8AEA04123FAE80379D08924816DE9906`。
- 2026-07-14：用户认为 24 DIP Filter 外框可以先保留，但原生文字区域偏上；按用户指定顺序先只把 18 DIP Edit 文字窗口垂直置于外框中央，部署后停止等待视觉判断，不提前把整个 Filter 改为 18 DIP。
- 2026-07-14：用户检查确认仅移动内部文字窗口没有改善，批准第二方案：整个 Filter 与 `×` 按钮改为 18 DIP，Edit 在灰框内上下各留 1 DIP。
- 2026-07-14：用户随后作出覆盖此前方案的最终决定：Refrain 不提供 Playlist Filter，且不是隐藏。实现完整删除控件、状态、筛选模型、布局占位、专用交互分支及测试；此前 Filter 视觉记录只作为决策历史保留，SVG 清除图标计划一并取消。
- 2026-07-14：Filter 移除版 x64 Debug/Release 均在 `/W4 /WX` 下构建成功，两配置各 10/10 自动测试通过，`git diff --check` 通过。组件包 SHA-256 为 `4A645E56FFCA7C602E9D74599BC7FB946089A8EFE54A108582C7084091AA72C6`；Release DLL 只部署到 `.local/foobar-dev`，构建、部署和进程加载 SHA-256 同为 `E86FD490EF7D9A3F29B83198C7A7F2558F6A9E04739193E7F322EE99C3BEE8A2`，开发进程正常响应。
- 2026-07-14：用户提供截图确认白色来自 ESLyric 播放态自绘，并在开发实例将 `pref.script.expose` 设为 `True`；原始配置字节确认当前值已持久化为 `1`、默认值仍为 `0`。Foobox `infoArt.js` 与 ESLyric 1.0.6.7 内嵌类型库共同证明 `GetAll()` / `SetBackgroundColor(color)` 调用路径。
- 2026-07-14：Refrain 新增可选背景同步适配：从已托管 ESLyric 子窗口取得实际加载模块，经导出 `DllGetClassObject` 创建内嵌 Automation 控制器，并以 ARGB `#FFF4F1EA` 同步当前背景。失败只记录兼容提示，不改变托管、歌词或 Track Details；不读取安装路径、不写 ESLyric 配置，也不覆盖背景类型、字体或歌词颜色。
- 2026-07-14：ESLyric 背景同步版 x64 Debug/Release 均在 `/W4 /WX` 下构建成功，两配置各 10/10 自动测试通过。组件包 SHA-256 为 `B656594D5BB9BC68E9296AF500399B62C9C4C804F2EC12D5C18D240468530F8C`；Release DLL 只部署到 `.local/foobar-dev`，构建、部署和进程加载 SHA-256 同为 `D2D36B4FEDF7C6607C32FCBD61A902CB01DCCD6AC538EB06A1AB8E36BFA2155C`，开发进程正常响应。
- 2026-07-15：本机 ESLyric 1.0.6.7 脚本确认 NetEase 搜索源会将 `tlyric` 译文追加到原歌词，KRC 解析器会将内嵌译文写为同时间戳下一行；Automation 没有双语或翻译语言设置。因此固定“源有译文则默认双行保留”的规则，不增加无效开关，不改写 ESLyric 私有配置或用户歌词源顺序。
- 2026-07-15：用户确认任务 15 虽在执行中扩展了若干收尾决策，但最初需求和测试已全部完成，批准任务整体验收。

## 改动文件

- `specs/modules/FOOBOX_BASIC_DEFAULT_VISUAL.md`：任务 15 的默认视觉规则和已批准底栏比例。
- `specs/modules/ALBUM_COVER_BROWSER.md`：0.6 封面墙显示、Hover 和 Selected 规则。
- `tasks/001-Foobox-Basic交互取证/screenshots/封面浏览器最终参考.jpg`：用户指定的封面墙视觉证据。
- `tasks/015-实现Foobox-Basic默认视觉/README.md`：分阶段步骤、中文逻辑、验证与用户检查入口。
- `src/playback_layout.h`：底栏纵向逻辑尺寸和可独立验证的边界计算。
- `src/playback_panel.cpp`：底栏尺寸、细分隔/覆盖式滑块、1 秒独立隐藏、Playlist 固定栏位和可拖右栏。
- `src/lyrics_host.h`、`src/lyrics_host.cpp`：ESLyric 可选 Automation 背景同步与失败隔离。
- `src/playlist_browser_model.h`、`tests/playlist_browser_model_tests.cpp`：删除 Playlist Filter 模型和专用回归测试，保留播放列表管理模型。
- `src/settings_model.h`、`src/settings.cpp`：保存右栏宽度的稳定配置、合法值校验和版本 3 迁移。
- `src/preferences.cpp`：从 Preferences 保存其他设置时保留用户拖动后的右栏宽度。
- `tests/playback_layout_tests.cpp`：底栏尺寸、极小窗口和控件边界回归测试。
- `tests/settings_model_tests.cpp`：右栏默认值、非法值回退、合法值保留和设置版本迁移。
- `CMakeLists.txt`：把布局模型加入组件工程并注册第十项测试。
- `tasks/README.md`、`tasks/TODO.md`：把当前工作切换到任务 15。

## 决策与未决问题

已决定第一轮宏观比例只压缩底栏，其他宏观比例保持任务 14 状态。底栏普通按钮采用 24 DIP、主按钮 28 DIP、间距 8 DIP、外边距 16 DIP、音量轨宽 96 DIP，进度条 inset 为 68 DIP；左、中、右三组分别锚定左边、窗口中心和右边。内容密度、文字、表面状态和最终矢量图标仍需后续分阶段核对。

内部结构线采用“宽热区、细视觉”规则：分隔热区 8 DIP 但只画 1 DIP 灰线；滚动热区 12 DIP 但只画 3 DIP 灰色滑块；轨道透明且任何交互状态都不切换强调色。选中、焦点和拖放语义反馈不属于此次删除范围。

滚动滑块按各列表独立记录最后活动时间，停止 1000 毫秒后只隐藏绘制；Playlist View 永久保留表头方格及正文滚动栏位，其余列表使用覆盖式滑块。右侧展示区采用默认 230‰、最小 280 DIP、最大 440 DIP 的可拖宽度，并通过设置版本迁移保存。

Now Playing 封面采用宽高共同约束的正方形布局，不再保留固定左右留白；Track Details 以硬裁剪保证不越过下区。ESLyric 的背景类型属于外部组件私有设置，Refrain 不改写其私有配置；集成使用 ESLyric 自带伪透明模式，并由 Refrain 提供和 Now Playing 相同的父背景。
