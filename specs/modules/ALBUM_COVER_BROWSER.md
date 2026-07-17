# Album Cover Browser 模块规格

- 状态：实现完成待验收（0.7 混合封面兼容修订）
- 版本：0.7
- 最后更新：2026-07-17
- 所属产品规格：[`../PRODUCT_SPEC.md`](../PRODUCT_SPEC.md)
- 对应路线：`11 实现专辑封面浏览器与专辑曲目列表`
- 参考证据：[`../../tasks/001-Foobox-Basic交互取证/screenshots/封面浏览器最终参考.jpg`](../../tasks/001-Foobox-Basic交互取证/screenshots/封面浏览器最终参考.jpg)

## 1. 用户愿景

提供一个类似 iTunes 10 专辑视图的专辑封面浏览页面。用户以专辑作为唯一浏览维度，在左侧浏览封面；点击一张封面后，该专辑的歌曲显示在右侧。用户可以选择数据来自整个媒体库或某个播放列表。

本节保存产品意图，后续允许用户修改；修改时更新本规格版本和决策记录，不要求用户重新描述原始愿景。

## 2. 产品限制

- 唯一浏览维度是 Album。
- 不提供 Album Artist、Artist、Folder、Library Directory、Genre 或其他分类模式。
- 不提供分类模式切换按钮。
- 不照搬 Foobox JS Smooth Browser 的通用浏览器能力。
- 页面级工具只保留来源选择；不保留 Foobox 的分类工具栏、通用过滤框或其他模式按钮。
- 本模块不是第二套媒体库。媒体文件、媒体库成员关系和播放列表仍由 foobar2000 管理。
- 本模块不替代左侧 `Playlist Browser` 中调用 foobar2000 原生 Album List 的入口。
- 页面拓扑固定，不能把封面网格或歌曲列表拖走、交换、浮动或嵌入其他页面。

## 3. 正式命名与固定结构

| 英文名称 | 中文解释 | 职责 |
| --- | --- | --- |
| `Album Cover Browser Workspace` | 专辑封面浏览工作区 | 顶栏与底栏之间的完整页面模式 |
| `Album Source Selector` | 专辑来源选择器 | 显示并选择 `Media Library` 或某个播放列表 |
| `Album Cover Browser` | 专辑封面浏览器 | 左侧大区域的专辑浏览容器 |
| `Album Cover Grid` | 专辑封面网格 | 显示来源中按专辑归组后的封面项目 |
| `Album Track List` | 专辑曲目列表 | 右侧显示当前选中专辑的歌曲 |
| `Album Track Header` | 专辑曲目表头 | 显示当前专辑身份和曲目数量 |

```text
┌──────────────── Album Source Selector ────────────────┐
│ Album Cover Browser                 │ Album Track List │
│ ┌──────────── Album Cover Grid ───┐ │ ┌ Track Header ┐ │
│ │ [cover] [cover] [cover] ...     │ │ │ Album / count│ │
│ │ [cover] [cover] [cover] ...     │ │ ├──────────────┤ │
│ └─────────────────────────────────┘ │ │ tracks       │ │
└─────────────────────────────────────┴──────────────────┘
```

`Top Bar` 和 `Playback Bar` 位于本工作区之外，页面切换前后始终存在。

### 3.1 Workspace 入口

- `Workspace Switcher` 位于底部 `Control Bar` 左侧，与设置齿轮同属左下功能组；现在正式包含独立的 `Playlist Workspace` 和 `Album Workspace` 两个入口，不能使用一个按钮往返切换。
- 两个入口本步骤先使用 Direct2D 代码绘制的临时列表/专辑图标，不新增位图或 SVG 资产；最终图标在步骤 14/15 统一替换。
- 当前 Workspace 对应的入口必须显示灰色底作为稳定的互斥选中状态；悬停和按下仍需与选中状态可区分。
- 未来新增 Workspace 也继续加入左下 `Workspace Switcher`，不能散落到顶栏、右键菜单或独立窗口作为主要入口。
- 入口不能隐藏 `Top Bar`、`Seek Bar` 或 `Control Bar`，只替换它们之间的 Workspace 内容。

## 4. 执行方案

1. 从 `Album Source Selector` 取得当前来源：整个 `Media Library` 或一个现有播放列表。
2. 读取该来源中的真实曲目，不复制或接管 foobar2000 的媒体库/播放列表数据。
3. 只按经批准的专辑身份规则归组，并为每组建立一个专辑封面项目。
4. 在 `Album Cover Grid` 中显示专辑封面；选择变化不改变页面结构。
5. 用户点击专辑后，将该组的真实歌曲交给右侧 `Album Track List`。
6. 来源、媒体库或播放列表变化时重新核对专辑身份；异步返回的旧封面不能覆盖新项目。
7. 左右分隔宽度按版本化设置保存；重启后的来源由步骤 12 新增的 `Default Playlist` 设置决定，不把“上次浏览来源”作为最终启动规则。

## 5. 当前已决定的显示要求

- 来源选择器必须明确显示 `Current source: Media Library` 或 `Current source: <playlist name>` 的等价英文文案。
- 左侧以封面网格为主要内容，不改成树、普通文本列表或 Artist/Genre 浏览页。
- 点击封面后，歌曲内容出现在右侧，不在封面下展开，也不跳转到主 `Playlist View` 才能查看。
- 右侧必须能明确看出当前对应哪个专辑；具体列、播放操作和菜单在步骤 11 实现前补齐并由用户核对。

## 6. 步骤 11 执行细则草案

### 6.1 来源与刷新

- `Album Source Selector` 是工作区顶部的单一英文下拉入口，列出 `Media Library` 和当前全部播放列表；不在此处加入搜索或分类按钮。
- Media Library 使用 `library_manager::get_all_items()` 的真实快照，并通过正式 library callback 合并刷新；播放列表来源使用稳定 playlist GUID，并响应创建、删除、改名、重排和内容变化。
- 来源消失时回退到 `Media Library`；媒体库未启用时显示空状态和打开 foobar2000 Media Library Preferences 的按钮，不扫描文件夹替代。

### 6.2 专辑身份与排序（已核准）

- 专辑身份只使用歌曲自身的 foobar2000 元数据语义：规范化后的 `%album% + %album artist%`。不读取、不要求也不优先使用 MusicBrainz、在线数据库或 FooCrate 私有 ID。
- `%album artist%` 采用 foobar2000 标准字段/回退语义；同一来源中 Album 与 Album Artist 相同的曲目就是同一专辑。Date/Year 不进入身份，因此不同年份或版本若这两个字段相同会按用户指定规则合并。
- Disc Number 不进入专辑身份，因此多碟专辑合并为一张专辑；右侧按 Disc Number、Track Number、Title 排序。
- 缺少 Album 的曲目不全部合并为一个未知专辑，而是各自形成 `Single` 项，身份附加真实 metadb 位置以避免碰撞。
- 专辑网格默认按 Album 标题不区分大小写升序，重名时按 Album Artist、Date/Year 排序；这与 Foobox Album-only 模式和归档截图的标题顺序一致。
- 本步骤不增加排序按钮；若未来需要可在不增加浏览维度的前提下版本化扩展。

### 6.3 网格显示

- Album Cover Browser 只有一种“封面墙”显示：从网格左上角开始铺设连续正方形单元，单元之间没有卡片外边距或留白，只允许像素对齐产生的最多 1 px 细缝。
- Front Cover 铺满整个单元。非正方形图片保持比例并居中裁切，不能拉伸变形；缺封面与加载状态占据完全相同的正方形，不改变网格尺寸。
- 默认不长期显示 Album 或 Album Artist。鼠标悬停某项时，在该封面内部底部覆盖一条单行标题条，只显示 Album；超出宽度的部分直接裁切，不换行、不增加单元高度。
- 鼠标移到另一项时旧 Hover 标题立即消失、新标题立即出现；移出网格后 Hover 标题立即消失。
- 选中项使用稳定撞色内描边，并持续显示同一标题条。点击空白、来源选择器、右侧曲目区或其他非封面区域不取消选择；点击另一张封面才转移选择。
- Hover 到非选中项时，选中项的描边和标题继续保留，同时 Hover 项临时显示自己的标题；Hover 本身不显示第二种边框，避免和 Selected 混淆。
- Album Grid 不显示系统白色 tooltip；Hover 信息只使用封面内部标题条。完整 Album Artist、Year 和曲目数仍可从右侧 `Album Track Header` 与曲目上下文读取，不建立第二层悬浮信息框。
- 使用行列虚拟化，只为可见区和小范围预取请求封面；缓存有数量和字节上限，来源代次变化后旧结果不得落入新项目。
- 同一 Album + Album Artist 组允许每首曲目使用不同的内嵌单曲封面。FooCrate 先使用 foobar2000 正式组封面结果；组结果缺失或不可解码时，按右侧专辑曲目的稳定排序逐项请求 Front Cover，显示第一张成功解码的图片。缺图或坏图只跳过对应曲目，不要求组内图片哈希、尺寸、压缩或色度采样一致。

### 6.4 Album Track Header/List

- `Album Track Header` 显示 `Album — Album Artist` 和曲目数；多碟专辑在曲目编号中保留 Disc.Track 语义。
- `Album Track List` 是 Album Browser 的只读智能结果视图，不是允许用户编辑顺序的普通播放列表。它永远等于“当前来源 ∩ 当前 Album + Album Artist 身份”：来源播放列表里只有该专辑的 3 首，就只显示这 3 首；来源是 Media Library 时才显示媒体库中该身份的全部曲目。
- 曲目固定按 Disc Number、Track Number、Title 排列，不接受内部重排、文件拖入、删除或手动增补。来源改变或元数据改变后自动重新计算；用户要改变内容必须修改来源播放列表或曲目元数据。
- `Album Track List` 复用步骤 10 的紧凑曲目视图资源，默认列为 `# / Title / Artist / Length`，并复用选择、滚动、键盘、当前播放状态和适用于只读曲目集合的正式上下文菜单；队列数据与专辑数据仍完全分离。
- 单击封面只改变右侧专辑内容；双击封面播放该专辑第一首。曲目双击/Enter 播放对应曲目，右键使用正式上下文菜单。
- UI 中的 `Album Track List` 本身保持上述只读智能视图语义。foobar2000 核心播放仍需要一个真实播放列表上下文；底层桥接方案见第 8 节唯一剩余核准项，不能把桥接列表冒充可编辑的 Album Track List。

### 6.5 布局、状态与失败边界

- 固定结构是 `Album Cover Browser | Album Track List`；只允许拖动中间分隔线改变宽度，建议默认左 78%、右 22%，保存为版本化千分比并设置安全范围。
- 保存左右比例和每个来源在当前会话中的专辑选择；同一进程内再次进入时恢复当前来源状态。
- foobar2000 重启后不恢复上次浏览来源。步骤 12 将提供 `Default Playlist` 设置并把它接到 Album Workspace 启动来源；该设置完成前，进入 Album Workspace 使用当前活动播放列表，若不存在则安全回退到 `Media Library`。
- 切回 Playlist Workspace 不销毁播放、队列或主列表状态；再次进入 Album Workspace 恢复来源、选中专辑和滚动位置。
- 空媒体库、空播放列表、无 Album、无封面、损坏封面、来源删除、元数据变化、异步结果晚到和窗口销毁都必须有真实空/失败状态，不显示假数据。

这些细则不能扩展出非 Album 浏览模式。若未来用户希望增加其他维度，必须先修改本规格的产品限制和版本记录。

## 7. 性能与生命周期门槛

- 元数据归组生成不可在每次绘制中执行；来源变化形成新代次，后台结果回到 UI 线程前核对窗口、来源 GUID/类型和代次。
- 封面加载复用已有 album-art 工作线程思路，但 Album Grid 使用独立有界缓存和请求队列，不能与播放列表组封面或 Now Playing 封面共享可变状态。
- 大媒体库只绘制可见单元；滚动期间不得同步读取图片文件，不因封面尺寸不同改变行高。
- library/playlist/metadb 回调只标记或增量刷新需要变化的来源；切换来源、退出工作区或销毁窗口时取消旧请求。

## 8. Album Playback Session（已核准）

用户已经核准两个独立 Workspace 按钮、metadata-only 专辑身份、右侧只读智能结果视图，以及单一真实播放桥接列表。foobar2000 核心播放必须属于一个真实播放列表；SDK 不提供完全隐藏、仅由组件持有的播放列表上下文，因此实现分离“用户看到的只读 Album Track List”和“核心播放桥接列表”。

- 浏览和单击专辑只更新 UI 中的只读 `Album Track List`，不改任何真实播放列表。
- 用户双击封面/曲目或按 Enter 真正开始播放时，才创建或复用唯一真实播放列表 `FooCrate Album View`，填入右侧当前可见曲目并锁定添加、删除和重排，然后从点击曲目开始播放。
- 这个桥接列表会出现在 foobar2000/Columns UI 的原生播放列表 UI 中，但任务 12 的 FooCrate `Playlist Browser` 将它隐藏，不占主视觉空间；它仍由 FooCrate 通过稳定 GUID 识别、锁定并维护，不会按专辑创建多个列表。
- 用户只浏览新专辑时，当前播放和桥接列表不变；真正播放新专辑时才停止/切换到新的有序内容。
- 启动桥接播放后立即把 foobar2000 的活动播放列表恢复为进入 Album Workspace 前的来源 playlist GUID；播放列表与正在播放列表可以不同，因此返回 Playlist Workspace 时仍显示原来源，不显示 `FooCrate Album View`。
- 来源为某播放列表时，桥接内容只包含该来源中匹配 Album + Album Artist 的曲目；来源为 Media Library 时才包含媒体库中的完整匹配专辑。
- Album Workspace 只允许专辑内部顺序或随机播放，不允许随机专辑，也不允许从当前专辑自动跨到整个来源的其他歌曲。
- 顺序模式保持 Disc Number、Track Number、Title 顺序；从第六首开始时前五首仍保留在桥接列表中，上一首可以回到第五首。
- 随机模式只对当前右侧曲目生成一次不重复的会话顺序，点击曲目作为会话起点，其余曲目在本次会话内各播放一次；不写入或污染正式 Playback Queue。
- 模式读取 foobar2000 当前 Playback Order：Random/Shuffle 类入口映射为专辑内随机一次，其他入口在本工作区映射为专辑内顺序。FooCrate 不在步骤 11 增加第二套全局 Playback Order 控件；步骤 14 再决定主界面的正式入口。
- 专辑会话最后一首自然结束后直接停止。不会自动跳回来源播放列表继续播放，因为那会产生不明确的续播索引、重复和跨专辑行为。
- 用户播放期间切回 Playlist Workspace 不终止 Album Playback Session；底部上一首/下一首仍只在桥接专辑范围内工作，直到播放结束或用户从其他真实播放列表启动新播放。

## 9. 决策记录

- 2026-07-13：用户明确拒绝 Foobox 的 Artist、Folder、Genre 等通用浏览能力，将范围收窄为类似 iTunes 10 的 Album-only 封面浏览器。
- 2026-07-13：页面级控制只保留媒体库/播放列表来源选择。
- 2026-07-13：用户允许未来修改愿景；修改必须通过版本化规格完成，不静默扩大范围。
- 2026-07-14：步骤 10 已提交为 `daaed39`，工作树干净，步骤 11 启动。
- 2026-07-14：用户指定 Album Workspace 入口位于左下，临时图标可先由实现决定，选中状态必须显示灰底；规格升至 0.2。
- 2026-07-14：只读复核 Foobox `jssb.js`，确认 Album-only 模式按 Album 排序，多碟按 Disc/Track 排列；Foobox 为媒体库播放创建/复用 `Library View` 工作列表。FooCrate 是否采用同类保留列表仍待用户明确核准。
- 2026-07-14：用户否决单按钮切换，决定左下同时提供 Playlist/Album 两个互斥按钮，未来 Workspace 入口也统一放在这里。
- 2026-07-14：用户否决 MusicBrainz 优先身份，决定纯用歌曲 metadata 的 Album + Album Artist；右侧结果严格限制为当前来源中的该专辑曲目，固定排序且不可编辑。
- 2026-07-14：用户接受单一 `FooCrate Album View` 核心桥接，但强调 UI 右侧始终是来源交集产生的只读智能视图，返回 Playlist Workspace 不能改成展示桥接列表。
- 2026-07-14：Album Playback Session 范围冻结为当前专辑；顺序模式保留完整有序列表并可从任意曲目开始，随机模式只在专辑内不重复随机。用户把专辑结束行为交由实现决定，FooCrate 选择结束后停止，不自动跨回来源续播。规格升至 0.3 并进入实现。
- 2026-07-14：0.3 已完整实现并进入人工验收：两个左下 Workspace 入口、metadata-only 归组、来源选择/回调、独立虚拟封面缓存、只读多选曲目表、稳定 GUID 的锁定桥接列表、专辑内顺序/随机一次、来源恢复及版本化状态均已落地。
- 2026-07-14：首轮人工验收确认检查点 1–5、8、9 通过；检查点 6 改为步骤 12 的 `Default Playlist` 启动规则，不再要求恢复上次 Album 来源。检查点 7 发现曲目双击不播放但右键 Play 正常；另发现部分封面反复闪烁，规格升至 0.4 并进入验收修正。
- 2026-07-14：双击播放修正为先执行 foobar2000 默认播放动作，成功后立即安装 `FooCrate Album View` 编辑锁，避免锁接管默认动作。封面网格改用 320 px 独立缩略图并按最近使用顺序淘汰，避免 1024 px 封面快速耗尽 64 MiB 缓存后反复请求。实现完成，等待用户复验。
- 2026-07-14：第二轮验收确认封面不再闪烁；双击仍错误播放 Playlist Workspace 的同索引曲目，而右键 Play、上一首和下一首均正确。原因是默认播放动作延迟取活动列表，但代码已提前恢复来源列表；实现改为收到桥接列表的新曲确认后，再通过 UI 消息恢复来源活动列表。
- 2026-07-14：第二轮修正版已实现：默认动作执行期间保持 `FooCrate Album View` 为活动列表；核心报告新曲后只投递窗口消息，离开播放回调后再恢复来源列表。实现完成并重新部署，等待用户复验双击曲目身份。
- 2026-07-14：用户最终复验确认双击播放正确曲目，上一首/下一首维持专辑范围，Playlist Workspace 来源恢复正确；结合已确认的封面不闪烁，模块 0.4 正式验收通过。
- 2026-07-14：任务 12 规格核对时，用户要求 `FooCrate Album View` 不出现在 FooCrate 自己的 Playlist Browser；Album Workspace 继续用来源下拉选择用户列表。底层桥接和原生 foobar2000 UI 可见性不变，规格升至 0.5。
- 2026-07-14：任务 15 视觉核对时，用户以 `封面浏览器最终参考.jpg` 明确将网格改为唯一的无间距封面墙；常驻 Album/Artist 两行被移除，Album 标题只在 Hover 或 Selected 时覆盖于封面底部，Selected 使用撞色描边且不会因点击其他区域取消。规格升至 0.6。
- 2026-07-14：首次封面墙人工检查发现旧 Album Grid tooltip 仍会显示白色多行信息框；用户明确不要该信息框。0.6 最终规则改为只保留封面内部单行 Hover 标题，Album Grid tooltip 永久为空。
- 2026-07-17：用户明确同一专辑可以包含完全不同的逐曲单曲封面，Album Cover Grid 的职责仍是展示一张可用图片。规格升至 0.7：组查询失败时按稳定专辑曲目顺序选择第一张可解码 Front Cover，图片不一致不再导致整张专辑显示 No cover。
