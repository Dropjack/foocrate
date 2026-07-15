# Now Playing Header 模块规格

- 状态：已验收
- 版本：0.3
- 最后更新：2026-07-15
- 所属产品规格：[`../PRODUCT_SPEC.md`](../PRODUCT_SPEC.md)
- 对应路线：`06 实现封面基本信息与五星评分`
- 参考证据：[`../../tasks/001-Foobox-Basic交互取证/screenshots/主界面.jpg`](../../tasks/001-Foobox-Basic交互取证/screenshots/主界面.jpg)

## 1. 用户愿景

右上区域应当是一块标准、稳定的当前播放摘要：无需等待轮换或执行额外操作，就能同时看到封面、评分、歌名、艺术家与专辑，以及编码与码率。

本节保存产品意图，后续允许用户增加、删除或重排信息；修改时更新本规格版本和决策记录，不要求用户重新描述原始愿景。

## 2. 产品限制

- `Now Playing Header` 固定在 `Now Playing Panel` 上部。
- 五类内容必须同时存在，不能用定时轮换、分页或中键切换互相替代。
- 必需摘要不包含 Codec Profile、Sample Rate 或其他扩展技术字段。
- 扩展技术字段可以进入右下 `Track Details View`，不能挤掉本规格规定的五类内容。
- 本区域不承载歌词、Biography、频谱或通用标签编辑器。
- 区域可以随已批准分隔线改变高度，但不能拖走、浮动或改变与右下视图的上下顺序。

## 3. 正式命名与内容顺序

| 顺序 | 英文名称 | 必须显示的内容 |
| --- | --- | --- |
| 1 | `Artwork View` | 当前目标曲目的封面 |
| 2 | `Rating Control` | Playback Statistics 五星评分 |
| 3 | `Track Title` | 歌名 |
| 4 | `Artist and Album` | `Artist | Album` |
| 5 | `Codec and Bitrate` | `Codec | Bitrate` |

```text
┌────────────── Now Playing Header ──────────────┐
│                 Artwork View                   │
│                 ☆ ☆ ☆ ☆ ☆                     │
│                 Track Title                    │
│              Artist | Album                    │
│              Codec | Bitrate                   │
└────────────────────────────────────────────────┘
```

“完整展示”在本规格中表示五类信息同时占有稳定位置，而不是承诺任意长度文本永不截断。长文本的截断、提示和无障碍读取规则在步骤 06 与步骤 17 中批准，但不能删除任何一类信息。

## 4. 执行方案

1. 播放或暂停时以 foobar2000 的 Now Playing 为唯一目标；完全停止后改为活动播放列表的焦点曲目。停止且没有焦点时显示无目标状态，不擅自选择列表第一首。
2. 异步准备封面，返回 UI 前重新核对曲目身份，防止旧封面覆盖新曲目。
3. 从 Playback Statistics 读取评分，并通过其正式命令写入或取消评分；默认不写音频文件标签。
4. 同时计算 `Track Title`、`Artist and Album`、`Codec and Bitrate` 三行文字。
5. 任一字段缺失或格式失败时，只对该字段使用经批准的回退，不隐藏其他行，也不让布局跳动。
6. 状态变化只更新对应内容；右下 `Lyrics View`/`Track Details View` 切换不改变本区域。

## 5. 步骤 06 细化规则

### 5.1 目标与刷新

- 播放、暂停和可恢复的播放状态均跟随 Now Playing；改变列表焦点不抢走播放摘要。
- 完全停止后立即读取活动播放列表焦点；焦点、活动列表或目标元数据变化时刷新。
- 网络流使用播放动态信息刷新标题、编码和码率；没有稳定封面时显示占位，不把上一首封面留下。

### 5.2 封面

- 通过 foobar2000 正式 Album Art Manager 请求 `Front Cover / Back Cover / Disc / Artist`，按用户既有封面搜索配置解析嵌入图或外部图；本模块不自行猜测文件名，也不使用 Genre 图片。
- 文件读取和图片解码在后台完成；每次目标改变都产生新代次并中止上一请求。结果回到 UI 后同时核对窗口生命期和代次。
- 解码统一为预乘 BGRA，最长边限制为 1024 像素；面板只保存当前目标的一张解码图，构成有界单项缓存。设备丢失只重建 D2D 位图，不重新读文件。
- 缺图显示 `No artwork`，读取或解码失败显示 `Artwork unavailable`；二者不改变布局尺寸。
- 右击 Artwork View 显示四种标准来源；不存在的来源禁用，当前来源带勾。选择来源立即显示，但不会自动改变轮播设置。
- `Pin current artwork` 固定当前来源并暂停轮播；取消勾选后重新服从 Preferences 的 Rotate artwork、来源掩码和轮播间隔。固定状态属于当前面板会话，并在切歌时尽量保持同一来源类型；新曲目缺少该来源时回退第一个可用来源。
- 后台可以探测全部四类来源供手动选择；自动轮播只遍历 Preferences 中启用且当前曲目实际存在的来源，默认间隔 20 秒。

### 5.3 文本

- `Track Title` 缺失时回退文件名，再缺失显示 `Unknown title`。
- `Track Title` 行只允许显示 `%title%` 的结果及上述缺失回退，不能被 Artist、Album 或高级摘要格式覆盖。
- Artist、Album、Codec 分别缺失时显示 `Unknown artist`、`Unknown album`、`Unknown codec`；Bitrate 缺失时显示 `Unknown bitrate`，存在时附加 `kbps`。
- Advanced 的 `Artist / album line format` 只控制标题下方第二行；为兼容任务 17 早期保存的两行 `Now Playing summary format`，读取到换行时忽略第一行并继续使用第二行。
- 每行固定为单行并使用尾部省略；完整文本通过悬停提示提供。步骤 17 再补屏幕阅读器信息，但本步骤保证 DPI 字体和命中区域随窗口缩放。

### 5.4 评分

- `%rating%` 只接受 1–5；其他值显示为未评分。
- 悬停第 N 颗星时预览 1–N 颗强调色，移出恢复真实评分。
- 点击非当前星级执行 `Playback Statistics/Rating/N`；点击当前星级执行 `Playback Statistics/Rating/<not set>`。命令针对点击时冻结的目标句柄，不因回调或焦点变化作用到另一首。
- 缺少 Playback Statistics 或正式命令时星级保持可见但禁用，并由提示说明 `Playback Statistics rating command unavailable`；不回退为写文件 `RATING` 标签。

### 5.5 初始尺寸

- 在尚未实现三栏分隔器前，摘要靠内容区右上对齐，宽度按窗口自适应并限制在可读范围；仅绘制已经工作的 Header，不绘制假的歌词面板。
- 封面保持正方形，五类内容位置稳定。该初始几何不冻结步骤 15 的最终截图比例，但 DPI、窄窗口、截断和命中必须可用。

## 6. 决策记录

- 2026-07-13：用户要求封面、评分、歌名、`Artist | Album`、`Codec | Bitrate` 在右上同时完整出现。
- 2026-07-13：最新决定覆盖早期的 `Codec | Codec Profile | Bitrate | Sample Rate` 摘要；Codec Profile 与 Sample Rate 不再属于右上必需信息。
- 2026-07-13：用户允许未来扩展或反悔；修改必须通过版本化规格完成，不静默改变已批准内容。
- 2026-07-13：只读参考 `infoArt.js` 确认点击当前星级调用 `<not set>`、点击其他星级调用 1–5；参考还包含可选写标签与信息轮换，二者不进入 Refrain。
- 2026-07-13：用户要求直接开始步骤 06、发现问题后再反馈，批准按本规格 0.2 的目标、回退和失败规则实现。
- 2026-07-13：首次根布局与评分检查不通过；修正 Columns UI 根元素放置方式和动态评分菜单识别后，用户确认布局、播放摘要与五星均正常并明确验收通过。
- 2026-07-13：步骤 07 验收时用户记录当前封面加载观感和右上位置关系不是最终目标；数据来源、异步安全和评分行为继续视为已验收，纯视觉、尺寸与主题协调明确留到步骤 15/16。
- 2026-07-15：任务 17 实机校准新增 Artwork View 右键来源选择与会话级 `Pin current artwork`；取消固定后恢复设置中的轮播开关、来源与间隔。用户同时冻结 Track Title 行只能显示歌名，高级格式收窄为第二行 Artist/Album。
