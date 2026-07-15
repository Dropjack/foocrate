# 016-实现深浅主题与强调色系统

- 状态：已验收
- 对应规格：[`../../specs/modules/THEME_AND_ACCENT_SYSTEM.md`](../../specs/modules/THEME_AND_ACCENT_SYSTEM.md)
- 前置任务：[`../015-实现Foobox-Basic默认视觉/README.md`](../015-实现Foobox-Basic默认视觉/README.md)（已提交为 `b6ad891 实现 Foobox Basic 默认视觉`）
- Fork 提交标题：`实现深浅主题与强调色系统`
- 最后更新：2026-07-15

## 任务目标

提供两套浅色、两套深色的完整预设，并把颜色选择收敛成 `Follow Windows`、`Refrain preset`、`Follow album artwork` 三种互斥模式。封面模式生成完整语义主题；Refrain 不读取或改写 Columns UI 配色。

## 明确不做

- 不改变任务 15 已验收的布局比例、字体、图标和命中区域。
- 不建立独立 Accent source、自定义颜色或 Columns UI 颜色来源。
- 不改写 Windows、Columns UI 或 ESLyric 配置。
- 不增加运行时依赖，不联网下载主题，不建立主题商店。

## 中文程序逻辑

1. 程序先检查 Windows 高对比度；高对比度始终覆盖普通模式。
2. `Follow Windows` 按系统浅深状态生成完整基底，并使用经过可读性校验的系统高亮色。
3. `Refrain preset` 使用四套固定主题之一；只有此模式允许选择 Preset。
4. `Follow album artwork` 在现有封面后台任务中抽样像素，以整体亮度决定深浅，以代表色生成背景、表面、文字、边框、状态和强调色的完整语义色表。
5. 缺图、损坏、停止播放、提取失败或旧 generation 晚到时立即回退漫雾青，不保留上一首的主题。
6. Preferences 草稿即时预览；`Apply` 持久化，`Cancel` 恢复，`Reset page` 回到 `Refrain preset + 漫雾青`。
7. 主题变化只重建颜色画刷并同步 ESLyric 背景，不重置播放、列表、滚动、分隔、Workspace 或封面缓存。
8. 设置版本 5 确定性迁移版本 4：旧 Windows 保留为 Follow Windows，旧 Refrain 和 Columns UI 都迁移为 Refrain preset；旧 Accent source 和 Custom colour 停用。

## 用户人工验收

1. 打开 `Preferences > Display > Refrain`，确认 Appearance 只有 `Colour mode` 和 `Preset` 两行。
2. 选择 `Refrain preset`，确认 Preset 可用，四套名称完整；逐一切换并检查浅色两套、深色两套。
3. 选择 `Follow Windows`，确认 Preset 禁用；切换 Windows 应用浅/深色后确认 Refrain 跟随。
4. 选择 `Follow album artwork`，确认 Preset 禁用；连续播放几张色调明显不同的封面，确认背景、表面、文字和强调色整体变化，而非只变强调色。
5. 测试彩色、灰度、缺图和快速切歌，确认文字可读、灰度主题中性、缺图回退漫雾青、旧封面不覆盖新曲目。
6. 分别测试 Apply、Cancel、关闭未应用页面、Reset page 和重启，确认预览与持久化正确。
7. 检查播放、选择、焦点、评分、进度、歌词、禁用、错误和默认播放列表状态，确认主题切换不重置布局或交互状态。

## 验证记录

- 2026-07-15：严格读取 ComicPlate Light mist 色板，以相同语义 token 方法建立漫雾青、暖纸橙、松影深色、靛夜琥珀四套完整预设；用户批准保留四套名称、两浅两深，默认漫雾青。
- 2026-07-15：初版曾实现基础来源与独立强调色来源。用户在验收前进一步统一语义，最终批准三种互斥 Colour mode，并要求删除 Accent source、自定义色和 Columns UI 颜色层；本任务以最终版本 5 模型为准。
- 2026-07-15：封面算法在现有后台 BGRA 数据上有界抽样约 4096 像素；整体亮度决定深浅，代表色生成完整主题，灰度封面生成中性色；generation 校验阻止旧结果覆盖新曲目。
- 2026-07-15：Preferences 已收敛为两行；Preset 只在 Refrain preset 模式启用。高对比度仍拥有最高优先级。
- 2026-07-15：设置架构升级到版本 5，复用旧基础来源 GUID 保存 Colour mode，并覆盖旧 Refrain、Windows、Columns UI 的迁移测试。
- 2026-07-15：最终语义调整后的 x64 Debug/Release 均在 `/W4 /WX` 下构建成功，两配置各 11/11 自动测试通过。组件包 SHA-256 为 `183C57569BE331CEAA2F8ACF0C84CF3F0A44CE2962654956D5A7870033741ADF`；Release DLL 只部署到 `.local/foobar-dev`，构建、部署和进程实际加载 SHA-256 同为 `1992CAF786AD0EB4E04345553F5129D81CCBC741BA406F84D37146411FD458B3`。隔离进程正常响应并在确认后关闭。
- 2026-07-15：用户在重新启动并加载最新组件后完成三种 Colour mode、四套预设、封面完整主题和设置行为检查，确认没有问题并明确宣布任务 16 验收通过。

## 改动文件

- `specs/modules/THEME_AND_ACCENT_SYSTEM.md`：最终三模式与完整封面主题规格。
- `specs/modules/SETTINGS_AND_MIGRATION.md`：版本 5 设置身份与迁移。
- `src/theme_model.h`、`src/theme_model.cpp`：四套语义主题、Windows 派生和完整封面主题算法。
- `src/settings_model.h`、`src/settings.h`、`src/settings.cpp`：Colour mode、版本 5 迁移和预览层。
- `src/preferences.cpp`：两行 Appearance 设置与条件启用。
- `src/playback_panel.cpp`：三模式解析、语义画刷、ESLyric 同步和封面完整主题接入。
- `tests/theme_model_tests.cpp`、`tests/settings_model_tests.cpp`：封面主题与版本迁移测试。
- `CMakeLists.txt`、`cmake/Foobar2000Sdk.cmake`：主题源码和测试目标。

## 决策

- 四套预设固定为两浅两深，默认漫雾青。
- Appearance 只保留 Colour mode 与 Preset。
- 封面模式控制整套 base colours；没有单独 accent source。
- Refrain 不保留 Columns UI 颜色集成。
