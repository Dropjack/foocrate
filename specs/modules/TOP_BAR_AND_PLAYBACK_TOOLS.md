# 顶部摘要与底部播放工具模块规格

- 状态：已验收
- 版本：0.1
- 最后更新：2026-07-14
- 所属产品规格：[`../PRODUCT_SPEC.md`](../PRODUCT_SPEC.md)
- 对应路线：`14 还原顶部摘要与底部功能按钮`

## 1. 目标外壳

目标界面从上到下固定为：

1. Windows 原生标题栏与系统窗口按钮；
2. 单行 Columns UI 工具栏区：`Main Menu | Refrain Now Playing Summary`；
3. Refrain 内容工作区；
4. Refrain 自己的 `Playback Bar`。

目标布局隐藏 Columns UI 原生播放按钮、Seek Bar、Playback Order (`Default`) 下拉框、Playlist Switcher、频谱工具栏、Main Menu 下方独占一整行的 Playlist Tabs（例如只显示 `Library (full)` 的标签栏）以及底部状态栏。左侧 Refrain `Playlists` 侧栏不受此规则影响。Refrain 不隐藏 Windows 标题栏，不自绘最小化、最大化或关闭按钮。

Refrain 注册正式的 Columns UI 顶栏扩展 `Refrain Now Playing Summary`，让它能够与 `Main Menu` 放在同一行。组件不能在未经许可时重写用户整个 Columns UI 布局，因此任务 14 的开发实例验收包含一次明确的工具栏整理；任务 19 可以再提供可选布局导入，但不能覆盖已有布局。

## 2. 顶部播放摘要

- 播放时显示播放状态、标题、艺术家、专辑和技术摘要，字段次序按 Foobox Basic 参考：`Title - Artist | Album | Codec | Codec Profile（存在时） | lossless/lossy | Channel Mode | Bit Depth（存在时） | Bitrate | Sample Rate`。
- 暂停时保留全文并用暂停状态替换播放状态；停止后清空 Refrain 摘要，不用旧曲目信息冒充当前播放。
- 缺失字段直接省略，相邻分隔符随之消失；网络流和未知技术字段只显示真实可得信息。
- 文本超过可用宽度时单行省略，不挤压或破坏 Main Menu；摘要本身不接管窗口拖动、Alt 菜单或系统快捷键。
- 本任务不让摘要承担搜索、跳转、评分或上下文菜单；这些能力已经属于各自区域或 foobar2000 正式菜单。

## 3. 底部固定排列

### 3.1 左侧 `Workspace Switcher`

从左到右固定为：

1. `Playlist Workspace`；
2. `Album Workspace`；
3. `Album List`；
4. `Settings`（遵守已有的显示/隐藏设置）。

两个 Workspace 按钮互斥显示选中状态。Album List 调用 foobar2000 正式命令而不改变 Workspace；Settings 打开 foobar2000 Preferences 中的 Refrain 页面。

### 3.2 中央 `Transport Controls`

从左到右固定为：

1. `Open`；
2. `Previous`；
3. `Play/Pause`；
4. `Next`；
5. `Stop`。

Open 调用 foobar2000 正式 `Open...` 命令，不实现第二套文件选择或导入逻辑。其他四项继续使用已验收的核心播放控制和禁用条件。

### 3.3 右侧 `Playback Tools`

从左到右固定为：

1. `Playback Queue`；
2. `Playback Order`；
3. `Output Device`；
4. `Mute`；
5. `Volume`。

Playback Queue 继续整栏切换现有队列视图。Playback Order 取代目标布局中被隐藏的 Columns UI `Default` 下拉框：点击后列出 foobar2000 当前正式播放顺序，标记当前项，选择后写回同一核心状态，并订阅外部菜单、快捷键或配置变化。它不是第二套播放规则。

Output Device 点击后列出 foobar2000 当前可用输出设备、标记活动设备并通过正式输出服务切换；设备枚举失败或切换失败时保持原设备并给出可理解的反馈。Mute 和 Volume 沿用已验收的同步与音量曲线。

## 4. 通用交互

- 所有图标按钮使用稳定命中区，并具有英文 tooltip、正常、悬停、按下、选中和禁用状态。
- 鼠标按下后移出命中区再松开不执行；菜单按钮只在菜单命令真实成功时改变状态。
- 简单命令按钮不创造无意义的右键菜单；Playback Order 和 Output Device 的选择菜单由左键打开，队列曲目的管理菜单仍在队列视图内部。
- 外部通过 foobar2000 菜单、快捷键或其他组件改变播放、顺序、输出、静音或音量后，Refrain 必须在主线程刷新到真实状态。
- 组件缺少正式命令或服务时对应按钮禁用或显示明确错误，不伪造成功，也不影响其余播放控制。

## 5. 与后续任务的边界

- 任务 14 确定完整控件集合、位置、命令和交互状态，并使用清楚可用的代码绘制图标。
- 任务 15 再统一最终图标造型、字体、间距、默认比例和 Foobox Basic 默认视觉。
- 任务 16 再实现深浅主题、强调色和完整颜色来源策略。
- 不增加 Biography、Video、ReFacets、内嵌频谱或独立播放顺序预测列表。

## 6. 证据与决定

- [`../../tasks/001-Foobox-Basic交互取证/screenshots/主界面.jpg`](../../tasks/001-Foobox-Basic交互取证/screenshots/主界面.jpg) 直接证明单行顶部摘要、底部三组按钮及相对顺序。
- Foobox `base.js` 只读证据证明 Open 使用正式 `Open...` 命令，播放顺序和输出设备是核心状态菜单，Mute/Volume 使用正式播放接口。
- Columns UI SDK 8.1.0 明确支持 `type_toolbar` 扩展；`cui::toolbars::guid_playback_order` 证明当前 `Default` 是 Columns UI 正式播放顺序工具栏。
- 2026-07-14：用户决定除 Main Menu 所在单行外隐藏所有 Columns UI 原生工具栏和状态栏，把原生 `Default` 的功能迁入 Refrain 右下角，并明确同意本规格所述整体外壳。
- 2026-07-14：用户进一步确认 Main Menu 下方的原生 Playlist Tabs 整行也必须隐藏；播放列表入口只保留 Refrain 左侧 `Playlists` 侧栏及各 Workspace 自身入口。
- 2026-07-14：用户批准 Foobox 式顶部字段、中央 `Open | Previous | Play/Pause | Next | Stop` 和右侧 `Playback Queue | Playback Order | Output Device | Mute | Volume`，规格进入可实现状态。
- 2026-07-14：完整实现、Debug/Release、两配置 9/9 测试、组件打包及隔离 dev 部署均已完成。
- 2026-07-14：用户在隔离开发实例中移除原生工具栏、Playlist Tabs 和状态栏，确认最终外层为 Windows 标题栏、单行 `Main Menu | Refrain Now Playing Summary`、Refrain 根面板与底部三组控制；随后明确确认任务 14 验收通过。
