# FooCrate 1.0.4 发布说明

FooCrate 1.0.4 修正四个 Playback Statistics 播放历史预设自动列表无法创建的问题。本版本保持组件、面板、命令和设置的稳定身份，可直接从 1.0.0–1.0.3 升级，不需要重新导入 Columns UI 布局。

## 修正

- Never played、History - last week、Played often 和 Recently added - 12 weeks 不再被一次性通用搜索上下文提前拒绝。
- 11 个内置预设的规则统一交给 foobar2000 正式 autoplaylist API 解析和持续计算。
- History 按 `%last_played%` 倒序，Played often 按 `%play_count%` 倒序，Recently added 按 `%added%` 倒序；其他预设保持专辑顺序。
- 缺少 Playback Statistics 时，需要播放历史字段的预设会明确说明依赖，不创建空列表。
- foobar2000 拒绝规则时，FooCrate 会删除刚建立的空容器，并显示预设名称、依赖、回滚结果和底层技术原因。

## 验证

- `1.0.4-beta.1` 已完成 x64 Debug 和 Release 构建，两种配置各 14/14 自动测试通过。
- 用户已在真实媒体库中确认四个原失败预设能正常创建，并批准正式化为 1.0.4。
- 正式包按快速正式化路径仅重建 Release，不重复已通过的 Debug 和全套测试。

## 依赖与升级

- Columns UI 仍是必要宿主。
- ESLyric 和 Playback Statistics 仍为独立可选组件，不包含在 FooCrate 组件包中。
- 评分以及 Never played、History、Played often、Recently added 预设需要 Playback Statistics。
- 安装、升级、卸载和回退步骤见 `INSTALLATION_AND_UPGRADE.md`。
