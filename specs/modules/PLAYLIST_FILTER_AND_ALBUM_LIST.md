# Playlist 搜索边界与 Album List 入口模块规格

- 状态：0.3 实现完成待验收
- 版本：0.3
- 最后更新：2026-07-14
- 所属产品规格：[`../PRODUCT_SPEC.md`](../PRODUCT_SPEC.md)
- 对应路线：`13 实现搜索筛选与 Album List 入口`、`15 实现 Foobox Basic 默认视觉`

## 1. 已核准范围

- FooCrate 不提供 Playlist Filter，也不提供歌曲搜索框。这里的“不提供”是删除该功能，而不是在界面上隐藏仍然存在的实现。
- Playlist Browser 的 `Playlists` 标题下方直接显示完整的普通/自动播放列表，不为 Filter 输入框、清除按钮或筛选状态保留空间。
- FooCrate 不接管 `Ctrl+F`；用户继续使用 foobar2000 正式搜索功能查找歌曲。
- `Album List` 是 foobar2000 原生媒体库树形浏览器，不是播放列表管理器。入口位于底部左侧，与 Workspace 按钮同组，但点击后只打开正式 Album List，不切换 FooCrate Workspace。
- 不加入 ReFacets、JS Smooth Browser、搜索历史、字段范围菜单或 FooCrate 自建媒体库。

## 2. Playlist Browser 搜索边界

- 不创建编辑框、清除按钮、查询文本、可见行副本、无匹配提示或筛选专用持久化状态。
- 播放列表外部新建、删除、改名或重排后，侧栏始终按 foobar2000 当前完整顺序刷新。
- 播放列表重排、空白处创建 `Dragged Items`、对普通列表投递曲目等行为不再存在“Filter 非空”分支，只服从播放列表类型、锁和任务 12 的权限规则。
- 后续视觉或主题任务不得重新加入 Filter 占位、隐藏开关或只剩外观但没有产品语义的控件。若未来反悔，必须作为新的产品决定重新设计和验收。

## 3. Album List 入口行为

- 点击左下按钮后，通过 foobar2000 正式主菜单命令服务查找并执行 `Album List`。
- FooCrate 不读取、保存或覆盖 Album List 的树形视图、分组、过滤和窗口配置；这些全部由 foobar2000 持有。
- 正式命令不存在时显示明确提示，其他播放与浏览功能保持可用。
- Album List 是否有内容取决于 foobar2000 Media Library；FooCrate 不扫描文件夹补偿空媒体库。
- Album List 按钮当前使用代码绘制的临时图标；最终形状、间距和配色在图标阶段统一调整。

## 4. 参考证据

- Foobox `base.js` 的底部 `LibBtn` 在当前配置下执行 `Library/Album List`，证明它是底部媒体库入口而非 Playlist Browser 的搜索控件。
- Foobox 曾把播放列表名称 Filter 与歌曲搜索分开；这只能证明二者语义独立，不要求 FooCrate 复制 Filter。
- foobar2000 SDK `mainmenu_commands::g_find_by_name()` 与 `g_execute()` 提供正式命令解析和执行；命令缺失可以返回失败。

## 5. 决策记录

- 2026-07-14：任务 13 曾实现并验收 Playlist Filter，同时保留 foobar2000 `Ctrl+F` 与原生 Album List 入口。
- 2026-07-14：任务 15 视觉调整期间，用户作出更新且优先级更高的产品决定：FooCrate 不再提供 Playlist Filter。该决定覆盖此前所有 Filter 行为、视觉和 SVG 清除图标计划。
- 2026-07-14：删除范围包括输入控件、清除按钮、筛选模型、查询状态、无匹配提示、布局占位、筛选期拖放限制和相关自动测试；`Ctrl+F` 与 Album List 入口保持不变。
