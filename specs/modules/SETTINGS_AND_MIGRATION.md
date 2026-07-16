# FooCrate 设置与配置迁移规格

- 状态：已验收
- 对应任务：`05 实现 FooCrate 设置与配置迁移`、`16 实现深浅主题与强调色系统`
- 最后更新：2026-07-15

## 1. 用户入口

FooCrate 使用 foobar2000 的正式 Preferences 体系，不占用主界面中央内容区。页面固定注册在 `File > Preferences > Display > FooCrate`。底部齿轮和稳定主菜单命令 `View > FooCrate Settings...` 都打开同一页面；它们不是第二套设置系统。

齿轮默认显示，但可以在设置中隐藏。隐藏后，Preferences 树和主菜单命令继续存在。命令使用稳定 GUID，也可由 foobar2000 自带的 Keyboard Shortcuts 绑定；FooCrate 不维护独立快捷键。

## 2. 当前设置

| 设置 | 类型 | 默认值 | 可见结果 |
| --- | --- | --- | --- |
| `Right-side time` | 枚举：`Total` / `Remaining` | `Total` | 底部右侧显示总时长，或显示带负号的剩余时间 |
| `Show control tooltips` | 布尔值 | 开 | 控制栏悬停提示启用或关闭 |
| `Show settings button` | 布尔值 | 开 | 显示或隐藏底部齿轮，不影响其他设置入口 |
| `Colour mode` | 枚举：Follow Windows / FooCrate preset / Follow album artwork | FooCrate preset | 决定整套基础色、表面、文字、状态与强调色来源 |
| `Preset` | 枚举：漫雾青 / 暖纸橙 / 松影深色 / 靛夜琥珀 | 漫雾青 | 仅在 FooCrate preset 模式启用并决定完整主题 |

页面编辑的是草稿值：主题字段变化时立即预览；`Apply` 持久化并通知所有已创建 FooCrate 面板；`Cancel`、关闭页面或销毁页面恢复已应用主题；`Reset page` 把草稿恢复为新安装默认，仍需 `Apply`。这些设置均不要求重启 foobar2000。

## 3. 配置身份、校验与迁移

- Preferences 页面、主菜单命令、配置版本和每个现存设置分别使用冻结的稳定 GUID；发布后不得因重构、改名或升级更换。
- 当前配置架构版本为 `5`。版本 0–3 迁移为 `FooCrate preset + 漫雾青`。
- 版本 4 的旧 FooCrate、Windows、Columns UI 基础来源分别迁移为 FooCrate preset、Follow Windows、FooCrate preset。
- 版本 4 的 Accent source 与 Custom colour 已被最终三模式设计取代，不再读取或写入。Colour mode 复用旧 Base colour source GUID，以便确定性迁移。
- 枚举只接受已定义值；损坏值回退安全默认，并在当前版本配置中修正持久值。
- 若旧版 FooCrate 读取到高于自身认识范围的未来配置版本，只使用能够安全解释的字段，不降低版本号、不删除未知数据。
- 应用设置不覆盖 Columns UI 布局或配色、foobar2000 全局快捷键、播放列表或第三方组件配置。

| 身份 | 稳定 GUID |
| --- | --- |
| Preferences page | `0b88eb5f-579d-477c-8b16-2241340025b8` |
| `FooCrate Settings...` command | `ab4e6816-2610-4286-862e-c6ec6857bbc9` |
| Configuration version | `6b7e5304-cd2a-4d7e-a228-d066ef9d0190` |
| Right-side time | `f2ae2fe9-a2cc-4960-9f93-bcee16249d29` |
| Show control tooltips | `39608393-f962-46b0-86bf-8ce92e46bcd1` |
| Show settings button | `70e58186-a678-4b86-a570-1e7f2f583984` |
| Theme preset | `b2c4668f-124c-4f8b-a028-6aeabbc3dee7` |
| Colour mode | `ac92cbe4-d155-486e-8b7f-d61d7ca165c5` |

版本 4 曾短暂使用下列 GUID；版本 5 保留其历史身份但不再读取或写入，以避免把废弃字段误当作新设置：

| 历史身份 | GUID |
| --- | --- |
| Accent colour source | `78f46e00-7410-4655-97f2-147db76e4340` |
| Custom accent RGB | `8173f5f7-0011-441b-a481-7ea55f202fb8` |

## 4. Dependencies 状态区

Preferences 页面只读枚举 foobar2000 已加载组件公开的名称与版本，显示 Columns UI、ESLyric 和 Playback Statistics 的 `Detected <version>` 或 `Not detected`。这里的版本是诊断信息，不代表 FooCrate 使用 Columns UI 配色。

每项提供 `Open official page` 按钮。只有用户点击才调用系统浏览器；FooCrate 不后台联网、不下载、不安装、不替换第三方组件。

## 5. 生命周期与失败行为

- Preferences 页面可以反复创建、取消和关闭；窗口先销毁、服务对象后释放，不保留失效窗口句柄。
- 设置应用通知只投递给仍存在的 FooCrate 面板；面板销毁时注销。
- 打开 Preferences 或官方页面失败时不改变配置，也不影响播放。
- 任务 16 在任务 05 的草稿、稳定 GUID、迁移和通知机制上扩展主题字段，不建立第二套设置系统。
