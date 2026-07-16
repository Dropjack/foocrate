# 022-建立 ESLyric 推荐默认设置

- 状态：已验收
- 对应规格：[`../../specs/modules/ESLYRIC_RECOMMENDED_DEFAULTS.md`](../../specs/modules/ESLYRIC_RECOMMENDED_DEFAULTS.md) 0.3
- 前置任务：[`../021-实现播放定位与Playlist-View恢复/README.md`](../021-实现播放定位与Playlist-View恢复/README.md)
- Fork 提交标题：`建立 ESLyric 推荐默认设置`
- 最后更新：2026-07-16

## 任务目标

为 FooCrate 托管的 ESLyric 建立安全的主题协调默认，同时保证已有用户配置和之后的右键修改不会被 FooCrate 反复覆盖。正式实例配置无法承载完整显示预设后，任务以动态背景/歌词高亮同步和明确保留个性化设置完成。

## 明确不做

- 不直接读写 `foo_uie_eslyric.dll.cfg`。
- 不捆绑 ESLyric，不接管歌词源、歌词目录或网络行为。
- 不导入完整 ESLyric 预设，不覆盖字体、布局、显示规则和用户快捷键。

## 中文程序逻辑

1. 使用正式 `export_config()` 验证用户候选设置是否可移植；实例仅导出 6 字节后，明确停止完整快照方案。
2. 不解析或复制 ESLyric 私有全局配置；用户右键设置继续由 ESLyric 管理。
3. FooCrate 运行时同步背景、普通歌词与当前歌词高亮色：普通歌词至少保持 4.5:1，高亮至少保持 7:1；封面模式优先选择色相不同的第二候选，无候选时安全回退。
4. ESLyric 接口缺失时只降级高亮同步，不影响其他设置和 FooCrate 功能。
5. 快捷键保持用户全局配置，不自动应用 dev 的私有 blob。
6. 新安装及未分配 profile 的 Playlist 默认采用 `Album Artist / Album / Disc`；不持续强制重排手工列表。

## 可检查步骤

### 步骤 1：正式实例接口验证

- 输入：干净 dev/test、已有 Foobox 风格配置、ESLyric 1.0.6.7。
- 动作：从用户已调整实例调用正式 `export_config()`，与 ESLyric 全局私有配置边界对照。
- 产物：6 字节实例快照证据，以及“不发布完整快照”的安全结论。
- 用户检查方法：核对三态可见结果，不检查二进制内部结构。
- 通过标准：不覆盖已有设置，失败可恢复。
- 状态：完成

### 步骤 2：动态颜色与 Playlist 默认

- 输入：用户高亮要求、FooCrate 四套预设、封面取色结果和现有 Playlist profile。
- 动作：同步背景、普通歌词与高亮；固定预设选择第二对比色；封面模式选择色相差至少 60°的第二候选，普通歌词保证至少 4.5:1、高亮保证至少 7:1；默认 Playlist profile 迁移到 Album Artist / Album / Disc。
- 产物：可测试的动态高亮和 RPV3 默认 profile。
- 用户检查方法：切换预设与多张封面，观察当前歌词高亮；新建 Playlist 检查默认分组。
- 通过标准：高亮明显可读；无第二候选安全回退；手工 Playlist 不被持续强制重排。
- 状态：完成

### 步骤 3：安全回归

- 输入：已有 dev 配置、缺失/旧版 ESLyric、RPV2 Playlist 设置。
- 动作：验证不导入私有配置、不复制快捷键、高亮接口缺失时降级、RPV2 一次性迁移和完整回归测试。
- 产物：Release 组件和人工验收清单。
- 用户检查方法：加载组件后修改一个非颜色 ESLyric 选项并重启；检查该选项保留。
- 通过标准：用户其他设置不被回写；缺接口不阻断 FooCrate；Debug/Release 与包审计通过。
- 状态：完成

## 用户检查点

- [x] 用户已批准调查完整默认设置的可行性。
- [x] 用户已授权 Codex 判断通用推荐边界与动态高亮方案。
- [x] 已确认不导入完整快照并保留已有个性化设置。
- [x] 用户已按人工验收步骤确认结果。

## 验证记录

- 2026-07-15：完成第一轮只读调查。确认背景同步是运行时颜色接口，不是完整设置写入；Columns UI 提供正式实例配置导入/导出接口；ESLyric 未公开逐字段配置结构；三份实例私有配置文件均不同，禁止整文件覆盖。下一步需要实际执行三态实例导入验证。
- 2026-07-16：用户已在 `.local/foobar-dev` 调整 ESLyric 和快捷键，授权 Codex 判断哪些适合作为通用推荐；歌词高亮要求在固定预设中使用对比色，在专辑取色模式中从封面颜色选择差异明显的颜色，无可靠候选时使用保证可读的回退色。用户同时要求核对新建 Playlist 是否已经默认采用 `Album Artist | Date | Album Title | Disc Number` 排序/分组预设。
- 2026-07-16：临时 Debug 探针通过 Columns UI 正式 `export_config()` 导出用户调整后的 ESLyric 实例，仅得到 6 字节；探针随后移除，dev 正常退出并按 SHA-256 恢复原 DLL。确认完整显示设置不可通过实例快照安全发布，任务收敛为运行时背景/高亮同步并保留用户其他设置。
- 2026-07-16：实现 ESLyric `SetTextHighlightColor` 运行时同步；四套预设使用独立第二对比色，封面主题优先选择色相差至少 60°且与背景对比度至少 7:1 的第二候选，无候选时使用安全回退。
- 2026-07-16：补充 ESLyric `SetTextColor` 普通歌词同步；普通歌词至少保持 4.5:1 的弱对比，高亮提高到至少 7:1，避免深色主题下普通歌词与背景融为一体。
- 2026-07-16：Columns UI `.fcl` 导出验证发现 FooCrate 内部托管的 ESLyric 不会自动进入布局配置；补充 FooCrate 面板配置转发边界，通过 ESLyric 正式 `export_config()` / `import_config()` 处理不透明实例配置，不复制全局私有配置。最终 FCL 导入/导出回归转入任务 23。
- 2026-07-16：Playlist View 设置升级为 RPV3；新安装和未分配 profile 默认采用 `Album Artist / Album / Disc`，RPV2 默认 profile 一次性迁移。AutoPlaylist 原有排序表达式已确认符合 `Album Artist | Date | Album | Disc | Track | Title`，手工列表不持续强制重排。
- 2026-07-16：dev 的 240 字节 `core.keyboardShortcuts` 确认为 foobar 全局私有配置，未解析、未复制、未自动应用。
- 2026-07-16：核对 `Show reference lines`：官方只提供面板/桌面歌词各自右键开关，本地运行时接口没有对应 setter。test 布局未创建可导出的 FooCrate/ESLyric 面板，按重试规则停止进一步探针；不通过私有配置或模拟菜单强制默认开启，用户开启后的状态由 ESLyric 保存。
- 2026-07-16：x64 Debug/Release 构建成功，两种配置各 13 项自动测试全部通过；临时探针未进入最终源码与组件。
- 2026-07-16：Release 组件生成于 `dist/FooCrate-0.1.0.fb2k-component`，大小 369034 字节，SHA-256 为 `0331DFB8A7BFA1BFE6B3A490009FB56604550FB3350862411EFE2C51290C3D9E`；包内容审计仅含 `foo_crate.dll`。
- 2026-07-16：用户确认普通歌词、高亮歌词与其他任务 22 行为测试无问题，明确要求任务 22 划勾。最终组件 SHA-256 为 `7F1209D8307F808DC56575B9B0C6CF6422FBDD2F096C3FC2E759FAE834616E25`。
- 2026-07-16：用户手动通过 Columns UI 导出 `dist/foocrate-0.1.0.fcl`，SHA-256 为 `5D94BF89759218AF1B76D87CAC88DC5E51C8B7E50EF7CDB80175C14B7B8BB9B2`。文件包含 FooCrate 面板 GUID，但当前未包含 ESLyric 实例配置块；该项不阻挡任务 22，转入任务 23 的可选布局导入验证。

## 改动文件

- `src/lyrics_host.h`、`src/lyrics_host.cpp`：ESLyric 背景、普通歌词与当前歌词高亮运行时同步、缺接口降级及正式实例配置转发边界。
- `src/theme_model.h`、`src/theme_model.cpp`：普通歌词 4.5:1、高亮 7:1、固定预设第二对比色、封面第二候选选择与安全回退。
- `src/playback_panel.cpp`：把解析后的歌词普通色与高亮色随主题一起交给 ESLyric，并提供 FooCrate 面板配置容器。
- `src/playlist_config_model.h`：RPV3 默认 profile 与 RPV2 一次性迁移。
- `tests/theme_model_tests.cpp`、`tests/playlist_grouping_model_tests.cpp`：动态高亮和默认 profile 回归测试。
- 本任务、任务索引、TODO 与模块规格：记录接口证据、最终安全边界和验收步骤。

## 决策与未决问题

- 正式实例快照不承载完整显示设置，因此取消完整预设导入，不再以猜测方式区分全新/已有/升级实例。
- 用户其他 ESLyric 右键选项和全局快捷键始终保留；本任务只接管获批准的运行时背景/当前歌词高亮。
- Columns UI 的正式导出格式是 `.fcl`，不是 `.fth`。FCL 携带 FooCrate 内部 ESLyric 实例配置的完整导入/导出回归留给任务 23，不再扩张任务 22。

## 人工验收

1. 手动导入 `dist/FooCrate-0.1.0.fb2k-component` 并启动 `.local/foobar-dev`。
2. 依次切换 Mist、Paper、Pine、Ink，播放带逐行歌词的歌曲；确认当前歌词使用与主强调色不同的第二对比色，普通歌词仍清楚。
3. 切到 Follow album artwork，连续切换色彩丰富、单色、灰度和缺封面的歌曲；确认高亮随封面变化，任何情况下都能读清且不会残留上一张封面的颜色。
4. 在 ESLyric 右键修改一个非颜色选项，重启后确认它仍保留；FooCrate 不应恢复整套 ESLyric 设置。
5. 新建普通 Playlist，确认默认显示为 `Album Artist / Album / Disc`；新建 AutoPlaylist，确认默认排序表达式为 `Album Artist | Date | Album | Disc | Track | Title`。手工调整曲目顺序后，确认 FooCrate 不会在每次添加时强制重排。
6. 确认你现有的 foobar 快捷键仍保持不变。
