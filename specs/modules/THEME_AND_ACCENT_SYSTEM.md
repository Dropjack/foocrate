# 深浅主题与封面配色系统规格

- 状态：已验收
- 版本：0.4
- 最后更新：2026-07-15
- 所属产品规格：[`../PRODUCT_SPEC.md`](../PRODUCT_SPEC.md)
- 对应路线：`16 实现深浅主题与强调色系统`

## 1. 目标与设计来源

FooCrate 提供四套完整预设：两套浅色、两套深色。设计直接参考 `D:\Tools\ComicPlate\src\ComicPlate.App\Themes\ColorTokens.axaml` 的 Light mist 色板及其语义 token 方法：布局和控件不持有零散颜色，全部从可替换的语义角色取色。

ComicPlate 当前参考目录只有一套 Light mist，并没有四套可直接复制的主题。因此“漫雾青”尽量保留参考色板；其余三套沿用同一明暗层级和语义关系，为 FooCrate 的播放、列表、评分、歌词和默认播放列表状态补齐颜色。

## 2. 四套预设

| 语义角色 | 漫雾青（浅） | 暖纸橙（浅） | 松影深色（深） | 靛夜琥珀（深） |
| --- | --- | --- | --- | --- |
| `BackgroundBase` 主背景 | `#EEF4F7` | `#F4F1EA` | `#151B1A` | `#111820` |
| `SurfacePanel` 面板 | `#F3F5F2` | `#EEE9DF` | `#1D2623` | `#18232E` |
| `SurfaceMuted` 选中/轨道表面 | `#E4EAE7` | `#DDD7CC` | `#27322E` | `#21303D` |
| `SurfaceElevated` 浮层/按钮悬停 | `#FFFFFF` | `#FFFCF7` | `#303C37` | `#2B3A48` |
| `TextPrimary` 正文 | `#1F2A2A` | `#252A2E` | `#EDF4F0` | `#F0F4F7` |
| `TextSecondary` 次要文字 | `#63706E` | `#6F6B64` | `#AEBBB5` | `#AAB7C2` |
| `TextDisabled` 禁用文字 | `#A7B1AE` | `#A5A099` | `#707C77` | `#6F7E89` |
| `TextOnAccent` 强调色上文字/图标 | `#FFFFFF` | `#252A2E` | `#102019` | `#241A09` |
| `BorderSubtle` 分隔/滚动滑块 | `#C9D4D0` | `#C9C1B4` | `#3B4843` | `#394958` |
| `Accent` 播放/评分/进度 | `#4F7F6A` | `#E7A43B` | `#6FA58C` | `#E4A94F` |
| `AccentHover` 强调悬停 | `#5F927B` | `#F0B34F` | `#80B89F` | `#F0B95F` |
| `StateHover` 普通悬停 | `#E8EFEC` | `#ECE6DB` | `#23302B` | `#202D39` |
| `StateSelected` 选中背景 | `#DCE6E2` | `#E2DBCF` | `#2C4338` | `#314254` |
| `Focus` 键盘焦点/拖放定位 | `#2E5FA8` | `#B66D27` | `#8EC2AA` | `#F4C46F` |
| `DefaultPlaylist` 默认列表语义 | `#2E5FA8` | `#586D8F` | `#7FA8D9` | `#7FB1E3` |
| `Error` 错误/失效状态 | `#B34444` | `#A8443A` | `#E27878` | `#E27972` |

“漫雾青”的次要文字从参考 `#667371` 轻微加深为 `#63706E`，使普通字号在主背景上的对比度达到 4.5:1 以上。“暖纸橙”的默认播放列表色从当前 `#5F7495` 加深为 `#586D8F`，原因相同。

## 3. 唯一颜色模式

Preferences 的 `Appearance` 区只提供两行：

1. `Colour mode`：`Follow Windows`、`FooCrate preset`、`Follow album artwork`。
2. `Preset`：漫雾青、暖纸橙、松影深色、靛夜琥珀；仅在 `FooCrate preset` 模式启用。

不再存在独立的 `Base colours`、`Accent source`、`Custom colour` 或 Columns UI 颜色集成。Columns UI 只负责承载组件，不参与 FooCrate 配色，也不会被 FooCrate 写入颜色配置。

颜色优先级从高到低固定为：

1. Windows 高对比度：使用系统 `COLOR_*` 语义色并保留线框、图标和文字标记。
2. 用户选择的 `Colour mode`。
3. 当前模式读取或计算失败时使用漫雾青安全回退。

## 4. 三种模式语义

### Follow Windows

- 读取 Windows 应用浅色/深色状态，分别选用安全的浅色或深色语义基底。
- 使用系统高亮色作为强调色，并经过对比度校验。
- 不把标题栏颜色直接作为正文背景。
- Windows 颜色或高对比度变化时立即刷新，不保存为 FooCrate 预设。

### FooCrate preset

- 完整使用 `Preset` 所选的 16 个语义角色。
- 四套名称和值保持稳定；默认是漫雾青。
- 这是唯一启用第二行 `Preset` 控件的模式。

### Follow album artwork

- 从当前播放封面生成一整套主题，而不是只替换强调色。
- 封面整体亮度决定浅色或深色基底；代表色用于轻度染色背景、面板、选中面和强调色。
- 正文、次要文字、边框、焦点、错误和默认播放列表等仍由语义规则生成并校验可读性，不直接照搬任意像素。
- 缺图、损坏、停止播放、提取失败或旧封面异步晚到时使用漫雾青；不能保留上一首歌曲的颜色。

## 5. 封面取色逻辑

- 使用现有后台封面解码得到的 BGRA 像素，不重新读取文件、不增加解码器，也不在 UI 绘制阶段扫描图片。
- 每张封面有界抽样约 4096 个像素，忽略透明像素；以整体感知亮度决定深浅基底。
- 排除近黑、近白和低饱和度像素后，将候选颜色量化为色桶，以出现量和饱和度共同选择代表色。
- 彩色封面产生同色相的低饱和背景和可辨强调色；灰度封面产生中性深浅主题，避免强行注入不存在的颜色。
- 文字与背景目标对比度不低于 4.5:1，强调图形与背景目标不低于 3:1；不满足时调整或回退安全 token。
- 结果携带封面 generation 返回 UI；只有 generation 与当前播放目标仍匹配才应用。

## 6. 状态映射与刷新边界

- 主背景、Now Playing、Lyrics View 和 Track Details 共用 `BackgroundBase`。
- 表头、底部控制栏、列表组标题使用 `SurfacePanel`；选中行使用 `SurfaceMuted`；普通悬停使用 `StateHover`。
- 正在播放、五星评分、进度已播放段、音量填充段和主播放按钮使用 `Accent`；其图标使用 `TextOnAccent`。
- 选择、焦点、当前播放、错误和禁用继续保留非颜色线索，颜色不单独承担语义。
- `DefaultPlaylist` 只表达进程启动时采用的默认列表，不复用 `Accent`。
- 主题变化只释放并重建颜色画刷；不重置播放、选择、滚动、分隔、Workspace 或封面位图。ESLyric 接收同一个 `BackgroundBase` ARGB。

## 7. 设置预览、默认与迁移

页面沿用现有草稿模型：编辑立即预览；`Apply` 保存并把当前预览作为新基线；`Cancel` 或关闭未应用页面恢复基线；`Reset page` 恢复新安装默认草稿并继续预览。多个 FooCrate 面板同时更新。

新安装、0.1 配置迁移和 `Reset page` 全部默认 `FooCrate preset + 漫雾青`。配置架构版本为 5：

- 版本 0–3 迁移为 `FooCrate preset + 漫雾青`。
- 版本 4 的旧 `FooCrate` 基础来源迁移为 `FooCrate preset`。
- 版本 4 的旧 `Windows` 基础来源迁移为 `Follow Windows`。
- 版本 4 的旧 `Columns UI` 基础来源迁移为 `FooCrate preset`。
- 版本 4 的强调色来源与自定义色不再参与运行时配色；旧字段不再读取或写入，但不影响其他设置。
- 未识别的未来枚举只回退安全模式，不降低配置版本、不删除其他设置。

## 8. 已批准决定

1. 保留漫雾青、暖纸橙、松影深色、靛夜琥珀四套预设，固定为两套浅色、两套深色。
2. 新安装、现有 0.1 配置迁移和恢复默认全部使用漫雾青。
3. 设置合并为一个三选一的颜色模式；Preset 只在 FooCrate 模式可选。
4. 封面模式生成完整主题，不再作为独立强调色来源。
5. 删除 Accent source、自定义色和 Columns UI 颜色集成；FooCrate 不碰 Columns UI 配色层。

## 9. 设置身份

主题设置随主配置架构版本 5 保存。`Colour mode` 复用版本 4 的基础颜色来源 GUID，以便确定性迁移；四套预设继续使用原稳定 GUID。详细记录见 [`SETTINGS_AND_MIGRATION.md`](SETTINGS_AND_MIGRATION.md)。
