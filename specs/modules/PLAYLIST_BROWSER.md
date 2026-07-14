# Playlist Browser 模块规格

- 状态：已验收
- 版本：0.2
- 最后更新：2026-07-14
- 所属产品规格：[`../PRODUCT_SPEC.md`](../PRODUCT_SPEC.md)
- 对应路线：`12 实现播放列表管理侧栏`
- 参考证据：[`../../tasks/001-Foobox-Basic交互取证/screenshots/主界面.jpg`](../../tasks/001-Foobox-Basic交互取证/screenshots/主界面.jpg)

## 1. 已冻结的产品边界

- 固定位置是 `Playlist Workspace` 左侧，顺序为 `Playlist Browser | Playlist View | Right Side Panel`。
- 面板不能拖走、交换、浮动或隐藏，只能调整与 Playlist View 的左右边界。
- 显示 foobar2000 的真实普通/自动播放列表、名称和项目数；区分当前活动、正在播放、管理选中和锁定/保留状态。
- FavPop、FavScore 等现有自动列表的名称、查询和内容继续由用户及 foobar2000 管理，Refrain 不重建或覆盖。
- `Refrain Album View` 是步骤 11 已批准的唯一保留工作列表，使用稳定 GUID 识别，不能仅靠名称判断。
- 步骤 13 才实现当前列表搜索、播放列表名称筛选和原生 Album List 入口；步骤 12 不显示不可用假控件。
- 不增加 Foobox 的电台、搜索结果列表管理器或强制置顶 `Library` 自动列表。用户在 0.1 核对中明确要求加入 Foobox 原理的预设自动列表；具体模板范围仍见第 8 节。

## 2. 只读取证结论

### 2.1 Foobox 可观察/文件行为

- 主界面左栏逐行显示图标、名称和当前项目数，活动列表与正在播放列表使用不同状态。
- `jsspm.js` 使用正式自动列表判断，不靠名称；自动列表拒绝曲目拖入，并提供 Properties 与 Convert to Normal。
- 单击列表会激活；支持 Ctrl/Shift 多选、批量重排、内联重命名和删除确认。
- Foobox 双击播放列表名称会取某个焦点索引直接播放；空白区域接收曲目会创建 `Dragged Items`。
- Foobox 还会创建/置顶 `Library` 自动列表，并提供预设自动列表、电台、搜索历史和内容清理菜单；这些超出 Refrain 已批准范围。

### 2.2 SDK 能力与限制

- `playlist_manager_v5::playlist_get_guid()` / `find_playlist_by_guid()` 可在改名、重排和异步确认期间稳定定位目标。
- `autoplaylist_manager::is_client_present()` / `query_client()` 可可靠识别自动列表；客户端可报告属性 UI 是否可用并提供配置。
- 自动列表复制查询需要按客户端 GUID 找到对应 factory 并重建配置；第三方 provider 缺失时不能可靠复制，必须禁用并说明。
- playlist lock 的 filter mask 决定是否允许添加、移除、重排、替换、改名和删除列表；UI 不能只按“自动/普通”二分后假定权限。
- 播放列表全局顺序由 foobar2000 持有；Refrain 的拖动排序必须提交完整 permutation，并用回调结果刷新。

## 3. 推荐的类型与状态模型

每行至少包含稳定 GUID、当前索引、名称、项目数、类型、锁权限，以及四个互不混淆的状态：

| 状态 | 含义 |
| --- | --- |
| Active | 中央 Playlist View 当前显示的列表 |
| Playing | foobar2000 当前播放上下文所属列表 |
| Selected | 侧栏正在用于批量管理的列表 |
| Reserved/Locked | Refrain 保留或其他组件限制修改的列表 |

自动列表是类型；锁定是权限。二者可能重叠，但不能视为同一概念。

Default Playlist 还需要独立于上述状态的持久视觉标识。任务 12 暂用灰蓝色
`#5F7495` 绘制其名称与项目数；Active、Playing 和 Selected 仍分别使用背景、播放三角
和选择边框，因此 Default 可以与这些状态叠加。该色值是当前浅色界面的临时语义色，
步骤 14–16 建立完整主题系统时可以调整色值，但必须保留“默认列表可直接辨认”的语义。

## 4. 任务 12 与步骤 13 的界面边界

任务 12 不在左栏表面放 `+` 或其他新建按钮。新建、载入与保存入口统一进入右键菜单；下面直接显示播放列表虚拟列表。步骤 13 再把最终的搜索输入、名称 Filter 和 Album List 入口接入已预留的顶部结构。本步骤不画不可点击的搜索框或媒体库占位。

## 5. 自动列表安全原则

- 激活、查看、重命名和删除只改变播放列表容器，不修改查询；删除仍需明确确认。
- Properties 只调用该自动列表客户端的正式 UI，不自制查询编辑器。
- Duplicate 必须复制查询/排序/flags；不能悄悄生成同名普通快照。provider 无法重建时禁用并说明。
- Convert to Normal 明确生成当前内容快照；只有新普通列表创建成功后才移除原自动列表。
- 自动或禁止添加的锁定列表永远拒绝曲目拖入。

## 6. 拖放安全原则

- 列表名称内部拖动只改变播放列表全局顺序；曲目 OLE 拖到列表名称才改变目标内容，两类手势不得混淆。
- 目标必须用 GUID 冻结并在提交前复核，不能因外部重排而投递到原索引对应的另一个列表。
- 任务 12 已核准为 Copy-only：目标成功插入引用后来源列表仍完全不变；Shift/Ctrl 不得把该操作改成 Move。
- 继续遵守步骤 10 的精确重复项策略，不能因跨列表落点出现而重新允许意外重复。
- 自动、保留或锁拒绝目标显示禁止反馈，不创建替代列表掩盖失败。

## 7. Default Playlist 已核准基础

Playlist Workspace 与 Album Workspace 共用一个永久 `Default Playlist`。用户可在播放列表行右键选择 `Set as Default Playlist`，也可在 Refrain Preferences 的播放列表子页通过下拉框设置；两处写入同一个播放列表 GUID，而不是名称或索引。每个 foobar2000 进程只应用一次：启动进入 Playlist Workspace，中央活动列表与 Album Workspace 首次来源都使用它；重新创建 Refrain 面板不能再次强制切换。

外部 `.m3u8` 或 `.fpl` 必须先通过 foobar2000 载入为内部播放列表，随后即可设为 Default。Refrain 不修改外部文件加标签，也不保存一个会在每次启动时重复导入的文件路径。只要该内部列表仍存在，其 GUID 就持续有效；删除并重新载入后需重新设置。

Default 被删除时回退到第一个非保留播放列表；若完全没有可用列表则创建一个普通 `Default Playlist` 并更新设置。

## 8. 已核准的选择记录

### 8.1 Default Playlist 的作用范围（已核准）

- A（用户核准）：每次 foobar2000 进程只应用一次；启动到 Playlist Workspace，中央活动列表与 Album Workspace 首次来源都使用同一个 Default Playlist。重新创建面板不再次强制切换。设置入口采用右键命令与 Preferences 下拉框。
- B：只决定 Album Workspace 来源，中央活动列表继续由 foobar2000 恢复。
- C：分别设置 Playlist Default 与 Album Default。

### 8.2 Default Playlist 消失时（已核准）

- A（用户核准）：回退到第一个非保留播放列表；完全没有可用列表时创建一个普通 `Default Playlist`，并更新设置。
- B：回退到 Media Library（只适用于 Album Workspace），Playlist Workspace 保持 foobar2000 当前状态。
- C：显示错误并等待用户手动选择，不自动回退。

### 8.3 新建入口（部分核准）

- A（被用户修改后核准）：不显示 `+`。右键 `Add Playlist…` 子菜单提供 `New Playlist`、`New Autoplaylist…` 和 `Preset Autoplaylists`。自动列表由 Refrain 提供查询/排序模板，再调用 foobar2000 正式 autoplaylist API 创建和持续计算；Refrain 不自建查询引擎。
- B：任务 12 只允许新建普通列表，自动列表继续从 foobar2000 其他入口创建。

### 8.4 点击、多选与双击（已核准）

- A（用户核准）：普通单击激活并单选；Ctrl/Shift 只改变批量管理选择而不反复切换中央列表；右键未选中行只改变管理选择；双击没有额外播放行为。F2/菜单重命名，Enter 提交，Esc 恢复原名。
- B：完全复制 Foobox，双击列表名称按该列表焦点索引开始播放。
- C：不支持多列表选择，所有管理操作一次只针对一个列表。

### 8.5 自动列表操作（已核准）

- A（用户核准）：命令使用音乐管理语义：`Edit Rules…` 编辑查询与排序模板；`Duplicate Rules` 复制规则供用户修改；`Freeze as Manual Playlist` 把当前结果冻结为普通引用列表；另提供 Rename、Set as Default 和 Delete。所有操作只改变播放列表定义或曲目引用，不修改音频文件。
- B：自动列表只允许激活和 Properties，其他管理必须去 foobar2000 原生界面。
- C：允许 Rename/Delete，但不提供 Duplicate/Convert。

### 8.6 `Refrain Album View`（已核准）

- A：在侧栏可见，显示保留/锁图标和项目数；允许激活查看，但禁止设为默认、改名、复制、删除、重排和接收曲目。
- B（用户核准）：完全隐藏，不占 Playlist Browser 主视觉空间；Album Workspace 继续通过来源下拉切换用户播放列表。底层桥接列表仍可能出现在 foobar2000 原生播放列表 UI 中。
- C：可见但不能激活，只作状态说明。

### 8.7 删除规则（已核准）

- A（用户核准）：Delete 键和菜单都打开确认；列出名称、类型和数量，多选一次确认。当前播放/默认列表给额外警告；保留列表拒绝。确认期间用 GUID 复核目标。删除播放列表只删除容器/规则，不删除音频文件。
- B：只有菜单可以删除，Delete 键不响应。
- C：单个列表不确认，多列表或自动列表才确认。

### 8.8 曲目拖到播放列表名称（已核准）

- A：默认 Copy，Ctrl 仍为 Copy，Shift 为 Move；Move 只删除成功插入项。自动/锁定/保留目标拒绝，精确重复项跳过。
- B（用户核准）：永远 Copy，不提供跨列表 Move。Copy 只把 foobar2000 曲目引用加入目标普通列表；不复制、移动或修改磁盘文件，也不改变来源列表。
- C：默认 Move，Ctrl 才 Copy。

### 8.9 拖到侧栏空白区域（已核准）

- A（推荐）：拒绝并显示“请选择目标播放列表”；只有 `+` 能创建列表。
- B（用户核准）：采用 Foobox 的便捷行为，自动创建普通 `Dragged Items` 并接收曲目引用；命名冲突和是否立即激活仍需在本轮收尾确认。

### 8.10 右键菜单范围（已核准）

- A（用户核准）：只提供生命周期命令。普通列表为 Activate、Set as Default、Rename、Duplicate、Delete；自动列表使用第 8.5 节最终核准的规则管理命令。Clear/去重/死链继续走正式菜单。右键背景另提供第 8.3 节的 Add/Load/Save 入口边界。
- B：复制 Foobox 的 Clear、Remove Duplicates、Remove Dead Items、Load/Save 等完整菜单。

### 8.11 左栏默认尺寸（已核准）

- A（用户核准）：默认约窗口内容宽度 15%，最小 180 DIPs、最大 35%，保存千分比；分隔线可以在该范围内拖动，不能越界或拖到隐藏。

### 8.12 预设自动列表模板（已核准）

采用 Foobox 同义模板：Library (full)、Never played、History（最近一周）、Played often、Recently added（最近 12 周）、Unrated、Rated 1–5；排除 Favorites (mood) 和网络电台。查询完全使用 foobar2000 标准查询语法，默认排序使用 `%album% | %discnumber% | %tracknumber% | %title%`，再由 `autoplaylist_manager::add_client_simple()` 交给核心持续计算。Refrain 不发明“通配符规则”或第二套查询引擎。

### 8.13 右键背景菜单与外部列表（已核准）

背景菜单采用：`Add Playlist…`（New Playlist / New Autoplaylist / Preset Autoplaylists）、`Load Playlist…`、`Save All Playlists…`。单行生命周期菜单另提供 `Save This Playlist…`。载入 `.m3u8/.fpl` 后可通过行菜单 `Set as Default Playlist`，外部文件本身不写入 Refrain 标签。

### 8.14 重复曲目与 `Dragged Items`（已核准）

所有普通播放列表都禁止同一精确曲目引用重复；拖到已有列表和空白新建列表时都先与目标及本批次去重。拖到空白处创建冲突安全名称 `Dragged Items`、`Dragged Items (2)`……，创建成功后立即激活，方便继续拖入和对比。列表只保存 foobar2000 曲目引用，不复制、移动或修改磁盘文件。
- B：给出其他默认比例和最小宽度。

## 9. 决策记录

- 2026-07-14：步骤 11 已提交为 `b924747`，步骤 12 启动。
- 2026-07-14：只读证据表明 Foobox 的 Playlist Manager 范围大于 Refrain 产品边界；0.1 草案保留播放列表生命周期与真实类型管理，把搜索/Filter/Album List 留给步骤 13，并排除电台、预设智能列表和强制 Library 自动列表。
- 2026-07-14：用户核准单一永久 Default、删除回退、多选/无双击播放、删除确认、永远 Copy、空白拖放建表、精简生命周期菜单和 15% 有界宽度；要求所有创建入口进入右键菜单，并新增 Foobox 原理的预设自动列表。`Refrain Album View` 改为在 Refrain 侧栏隐藏。自动列表规则管理、预设范围、Load/Save 菜单和重复项/新表命名仍待收尾核准。
- 2026-07-14：用户接受规则管理菜单；核准非 Mood 的 Foobox 同义预设、foobar2000 标准查询/标题格式语法、背景 Add/Load/Save、单行 Save This、全普通列表精确去重，以及唯一命名并立即激活的 `Dragged Items`。0.2 无未决产品问题，进入实现。
- 2026-07-14：用户要求 Default Playlist 与其他列表颜色不同；任务 12 暂定灰蓝色 `#5F7495`，最终色值留到主题与多套配色阶段调整，但保留独立视觉语义。
- 2026-07-14：用户完成任务 12 人工验收，确认未发现逻辑问题并接受当前 Default Playlist 颜色；模块规格 0.2 标记为已验收，后续只在主题阶段调整视觉参数。
