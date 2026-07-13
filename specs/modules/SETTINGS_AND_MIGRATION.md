# Refrain 设置与配置迁移规格

- 状态：已验收
- 对应任务：`05 实现 Refrain 设置与配置迁移`
- 最后更新：2026-07-13

## 1. 用户入口

Refrain 使用 foobar2000 的正式 Preferences 体系，不占用主界面中央内容区。页面固定注册在 `File > Preferences > Display > Refrain`。底部齿轮和稳定主菜单命令 `View > Refrain Settings...` 都打开同一页面；它们不是第二套设置系统。

齿轮默认显示，但可以在设置中隐藏。隐藏后，Preferences 树和 `View > Refrain Settings...` 命令继续存在，因此用户始终能重新显示齿轮。命令使用稳定 GUID，也可由 foobar2000 自带的 Keyboard Shortcuts 绑定；Refrain 不维护独立快捷键。

## 2. 本步骤设置

| 设置 | 类型 | 默认值 | 可见结果 |
| --- | --- | --- | --- |
| `Right-side time` | 枚举：`Total` / `Remaining` | `Total` | 底部右侧显示总时长，或显示带负号的剩余时间 |
| `Show control tooltips` | 布尔值 | 开 | 控制栏悬停提示启用或关闭 |
| `Show settings button` | 布尔值 | 开 | 显示或隐藏底部齿轮，不影响其他设置入口 |

页面编辑的是草稿值：`Apply` 才持久化并立即通知所有已创建 Refrain 面板；`Cancel` 销毁草稿，不产生保存；`Reset page` 只把草稿恢复默认，仍需 `Apply`。这三项均不要求重启 foobar2000。

## 3. 配置身份、校验与迁移

- Preferences 页面、主菜单命令、配置版本和每个设置分别使用已经冻结的稳定 GUID；发布后不得因为重构、改名或升级而更换。
- 当前配置架构版本为 `1`。首次读取版本 `0` 时，把可识别值校验后迁移到版本 `1`；不存在的值使用默认值。
- 枚举只接受已定义值。无效时间模式在运行时安全回退到 `Total`，并在当前版本配置中修正持久值。
- 若旧版 Refrain 读取到高于自身认识范围的未来配置版本，只使用能够安全解释的字段，不降低版本号、不删除未知数据。
- 应用已知设置时只写各自稳定字段，不覆盖 Columns UI 布局、foobar2000 全局快捷键、播放列表或第三方组件配置。

| 身份 | 稳定 GUID |
| --- | --- |
| Preferences page | `0b88eb5f-579d-477c-8b16-2241340025b8` |
| `Refrain Settings...` command | `ab4e6816-2610-4286-862e-c6ec6857bbc9` |
| Configuration version | `6b7e5304-cd2a-4d7e-a228-d066ef9d0190` |
| Right-side time | `f2ae2fe9-a2cc-4960-9f93-bcee16249d29` |
| Show control tooltips | `39608393-f962-46b0-86bf-8ce92e46bcd1` |
| Show settings button | `70e58186-a678-4b86-a570-1e7f2f583984` |

## 4. Dependencies 状态区

Preferences 页面只读枚举 foobar2000 已加载组件公开的名称与版本，显示 Columns UI、ESLyric 和 Playback Statistics 的 `Detected <version>` 或 `Not detected`。这里的版本是诊断信息，不把“高于已测试版本”误报为不兼容。

每项提供明确的 `Open official page` 按钮。只有用户点击按钮才调用系统浏览器；Refrain 不后台联网、不下载、不安装、不替换第三方组件。05 只报告组件是否被检测；步骤 06、07 在真正使用评分和歌词能力时再报告实际接口或命令不兼容。

## 5. 生命周期与失败行为

- Preferences 页面可以反复创建、取消和关闭；窗口先销毁、服务对象后释放，不保留失效窗口句柄。
- 设置应用通知只投递给仍存在的 Refrain 面板；面板销毁时注销。
- 打开 Preferences 或官方页面失败时不改变配置，也不影响播放。
- 本步骤不实现主题、封面、歌词、评分、播放列表和最终截图按钮排列。
