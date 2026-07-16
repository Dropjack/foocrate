# ESLyric 推荐默认设置规格

- 状态：已验收
- 版本：0.3
- 最后更新：2026-07-16
- 所属产品规格：[`../PRODUCT_SPEC.md`](../PRODUCT_SPEC.md)
- 对应路线：`22 建立 ESLyric 推荐默认设置`

## 1. 目标

让首次使用 FooCrate 的用户获得一套与 FooCrate 布局、主题和双语歌词方向协调的 ESLyric 面板设置，同时继续允许用户通过 ESLyric 自己的右键菜单自由修改。FooCrate 不接管歌词下载、歌词源、歌词文件和 ESLyric 的全局用户数据。

## 2. 2026-07-15 调查证据

### 2.1 已确认

- [ESLyric 官方 README](https://github.com/ESLyric/release)说明面板右键菜单可以自定义显示/隐藏，按住 Shift 右键显示完整菜单，并支持自定义面板布局；官方发布仓库在调查时把 1.0.6.7 标为最新版。
- 仓库只读依赖 `third_party/columns_ui-sdk-8.1.0/base.h` 中的 `uie::extension_base` 正式提供 `set_config/get_config`、`import_config/export_config`、`have_config_popup/show_config_popup`；实例配置必须在 `create_or_transfer_window()` 之前导入。
- FooCrate 当前的颜色同步通过 ESLyric 运行时 `IDispatch` 接口调用 `GetAll`、`SetBackgroundColor`、`SetTextColor` 与 `SetTextHighlightColor`。这只更新已创建面板的运行时颜色，不等于修改 ESLyric 的完整设置。
- `foobar-dev`、`foobar-test` 和只读 Foobox 参考实例的 `foo_uie_eslyric.dll.cfg` 长度与哈希都不同，证明该文件包含实例/用户状态，不能把任何一份整文件当作通用默认覆盖：

| 只读样本 | 字节数 | SHA-256 |
| --- | ---: | --- |
| `foobar-dev` | 3800 | `3B1B17668E059CCFBAF9245D8C547E962C85D81C70568AD11B97E6FFCDE25153` |
| `foobar-test` | 3520 | `D7DF8D7C4C808A1CFE2F6A118D4D272C84013A5391BFCDD276E80D1CBF62621C` |
| Foobox 只读参考 | 2738 | `2AB324FC2829D39BC73D1DC63EEA548B22896931C5AD719FA11F527AE24ABDCA` |
- ESLyric 1.0.6.7 发布仓库只提供发布物、布局和语言资源，没有公开逐字段配置结构。组件二进制可见 `RENDER_BACKGROUND_PESU_TRANSPARENT`、布局等内部名称，但字符串证据不足以授权解析私有二进制配置。

### 2.2 当前结论

2026-07-16 已在 `.local/foobar-dev` 使用临时 Debug 探针调用正式 `export_config()`。用户调整后的 ESLyric 实例只导出 6 字节，SHA-256 为 `40DB3CDFB62FD41F8E898A6CCA8C10D7E69B4B7A088BA1ABC40A9F7870410D1A`；而同一实例的 ESLyric 全局私有配置为 3844 字节。由此确认正式实例配置不承载完整字体、布局和显示设置，不能制作原计划中的完整可移植快照。

因此不批准自动应用完整 ESLyric 预设，也不继续尝试解析或复制私有全局配置。三态边界结论为：

1. 全新实例保留 ESLyric 自己的默认和用户后续选择，仅接受 FooCrate 运行时颜色同步；
2. 已配置实例不导入、不覆盖，只在 FooCrate 托管期间同步获批准的背景、普通歌词与高亮色；
3. 升级实例同样不迁移私有配置；运行时接口缺失时保持 ESLyric 当前设置。

## 3. 预定安全行为

1. FooCrate 不导入 ESLyric 私有配置，不提供会误导用户的“完整预设”按钮。
2. 用户通过 ESLyric 右键修改的字体、布局、歌词源、路径和显示规则继续由 ESLyric 保存，FooCrate 不回写。
3. 背景颜色继续通过正式运行时控制接口同步；普通歌词通过 `SetTextColor` 同步为至少 4.5:1、接近可读门槛的弱对比色；当前歌词高亮与主题/封面变化一起更新。
4. 固定 FooCrate 预设使用各自与主强调色区分的第二对比色，高亮色相对背景必须达到至少 7:1。
5. Follow album artwork 从封面候选颜色中优先选择与主强调色色相差至少 60°的第二颜色，并调整明度确保至少 7:1；没有可靠第二颜色时使用深浅主题安全回退色，仍不足时使用主文字色。
6. ESLyric 运行时普通歌词或高亮接口缺失、版本不兼容或组件缺失时保留对应的 ESLyric 当前颜色，不阻止其他颜色同步和 FooCrate 其他区域工作。
7. FooCrate 保留通过 ESLyric 正式实例接口转发不透明配置的代码边界，但任务 22 不承诺 Columns UI `.fcl` 已携带该数据。最终导入/导出和干净实例恢复验证转入任务 23；FooCrate 仍不得解析或复制 ESLyric 全局私有配置。

## 4. 不自动应用的个性化设置

- 面板布局、文本对齐、字体、字号、行距；
- 原文、翻译、音译、无歌词、纯音乐和搜索中的显示规则；
- 自动搜索、保存位置、歌词源和全屏歌词设置；
- 用户在 dev 实例中调整的其他 ESLyric 右键选项。

这些设置可能不适合所有人，且正式实例接口不能安全携带，因此只作为用户本地配置保留。

`Show reference lines` 也属于此边界。ESLyric 官方说明该开关由面板歌词和桌面歌词分别通过右键控制；1.0.6.7 的运行时接口没有对应 setter。FooCrate 不解析私有配置，也不通过模拟菜单强制开启；用户在各面板开启后的状态继续由 ESLyric 自己保存。

## 5. 实现与回归验证

- 自动测试普通歌词至少 4.5:1，以及固定预设和封面第二候选的色相差、至少 7:1 高亮对比度与安全回退。
- 在 ESLyric 1.0.6.7 验证主题/封面变化时背景、普通歌词和当前歌词高亮同步。
- 验证 ESLyric 右键修改的其他设置跨重启保留。
- 验证缺少 `SetTextHighlightColor` 时只跳过高亮同步，不反复重试或覆盖私有配置。
- 任务 23 验证 Columns UI `.fcl` 是否同时包含 FooCrate 面板配置和 ESLyric 正式实例配置，并在干净实例测试导入；当前手动导出的 FCL 只确认包含 FooCrate 面板 GUID。

## 6. 2026-07-16 用户候选默认

- 候选设置来源为用户已调整的 `.local/foobar-dev`，Codex 负责判断通用性，不把所有个人设置原样复制给其他用户。
- ESLyric 普通歌词不能与背景融为一体，至少保持 4.5:1 对比度并弱于当前歌词；歌词高亮不冻结为单一颜色：固定 FooCrate 预设使用与背景/主色形成明显区分且至少 7:1 的对比色；Follow album artwork 从封面候选色中选择色相或明度差异充分且满足增强文字对比度的颜色；没有可靠候选时使用保证可读的主题回退色。
- 用户修改过的 foobar2000 快捷键先作为候选清单审计。快捷键是全局用户数据，除非能通过正式接口提供明确的可选应用与冲突确认，否则不纳入自动默认。
- 核对所有新建 Playlist 是否已经采用 `Album Artist | Date | Album Title | Disc Number` 默认排序/分组预设；已有正确实现时只记录验证，不重复增加逻辑。

## 7. 相关但不自动覆盖的设置

- dev 的 `core.keyboardShortcuts` 是 240 字节全局私有配置，test 实例没有对应 blob。FooCrate 不解析、不复制、不自动覆盖快捷键；后续只可提供人类可读的可选建议清单。
- Playlist 已有 `Album Artist / Album / Disc` 分组和 `Album Artist | Date | Album | Disc | Track | Title` 排序表达式，FooCrate 创建的 AutoPlaylist 也已使用该表达式。本任务只把新安装及未单独分配 profile 的默认分组改为该预设，不在每次添加曲目时强制重排手工 Playlist。
