# Now Playing Header 模块规格

- 状态：规格核对中
- 版本：0.1
- 最后更新：2026-07-13
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

1. 从当前批准的目标曲目取得封面和元数据；播放/停止时目标选择规则在步骤 06 补齐。
2. 异步准备封面，返回 UI 前重新核对曲目身份，防止旧封面覆盖新曲目。
3. 从 Playback Statistics 读取评分，并通过其正式命令写入或取消评分；默认不写音频文件标签。
4. 同时计算 `Track Title`、`Artist and Album`、`Codec and Bitrate` 三行文字。
5. 任一字段缺失或格式失败时，只对该字段使用经批准的回退，不隐藏其他行，也不让布局跳动。
6. 状态变化只更新对应内容；右下 `Lyrics View`/`Track Details View` 切换不改变本区域。

## 5. 尚待步骤 06 细化

- 播放、暂停、停止和焦点变化时跟随哪一首曲目；
- 缺封面、损坏封面和多封面的选择规则；
- 缺少 Artist、Album、Codec 或 Bitrate 时的英文回退文案；
- 网络流和动态元数据；
- 长文本截断、提示和字体缩放；
- 五星悬停、取消评分和禁用视觉；
- 默认高度、间距和不同 DPI 下的尺寸。

这些细化项不能移除五类必需内容或恢复 Foobox 的 12 秒信息轮换。

## 6. 决策记录

- 2026-07-13：用户要求封面、评分、歌名、`Artist | Album`、`Codec | Bitrate` 在右上同时完整出现。
- 2026-07-13：最新决定覆盖早期的 `Codec | Codec Profile | Bitrate | Sample Rate` 摘要；Codec Profile 与 Sample Rate 不再属于右上必需信息。
- 2026-07-13：用户允许未来扩展或反悔；修改必须通过版本化规格完成，不静默改变已批准内容。

