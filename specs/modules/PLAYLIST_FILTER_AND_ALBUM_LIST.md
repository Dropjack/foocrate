# Playlist Filter 与 Album List 入口模块规格

- 状态：已验收
- 版本：0.1
- 最后更新：2026-07-14
- 所属产品规格：[`../PRODUCT_SPEC.md`](../PRODUCT_SPEC.md)
- 对应路线：`13 实现搜索筛选与 Album List 入口`

## 1. 已核准范围

- Refrain 不实现歌曲搜索框，也不接管 `Ctrl+F`；用户继续使用 foobar2000 正式搜索功能查找歌曲。
- `Playlist Filter` 位于 Playlist Browser 顶部，只按播放列表名称实时筛选左栏行。它不搜索曲目，不创建搜索播放列表，也不修改任何播放列表内容或规则。
- `Album List` 是 foobar2000 原生媒体库树形浏览器，不是播放列表管理器。入口位于底部左侧，与 Workspace 按钮同组，但点击后只打开正式 Album List，不切换 Refrain Workspace。
- Album List 按钮本轮使用代码绘制的临时图标；最终形状、间距和配色在步骤 14–16 统一调整。
- 不加入 ReFacets、JS Smooth Browser、搜索历史、字段范围菜单或 Refrain 自建媒体库。

## 2. Playlist Filter 行为

- 使用原生 Windows 单行编辑框，支持中文输入法组合、粘贴、选择和常规文本编辑。
- 匹配采用 Unicode 文本的大小写不敏感包含关系；中文按原字符匹配。空字符串显示全部播放列表。
- 输入变化立即重新建立可见行，但播放列表真实顺序、活动列表、正在播放列表、Default 和内容完全不变；管理选择按当前可见 GUID 安全收窄，避免批量命令误作用于已经隐藏的行。
- 可见行继续支持激活、右键管理和曲目拖入普通列表；被过滤掉的行只是暂时不可见。
- Filter 非空时禁止侧栏播放列表重排，因为隐藏行仍参与 foobar2000 全局顺序；清空后恢复重排，避免产生不可见的插入结果。
- Filter 非空时，拖到列表行仍投递到该真实列表；拖到筛选结果空白处显示禁止反馈且不自动创建 `Dragged Items`，防止把“无匹配”误解为正常侧栏空白。
- 无匹配时显示明确的 `No matching playlists`；只能点击 `×` 清除筛选并恢复全部行，Esc 不清除筛选。
- 播放列表由外部新建、删除或改名时，以当前 Filter 重新计算结果。

## 3. Album List 入口行为

- 点击左下按钮后，通过 foobar2000 正式主菜单命令服务查找并执行 `Album List`。
- Refrain 不读取、保存或覆盖 Album List 的树形视图、分组、过滤和窗口配置；这些全部由 foobar2000 持有。
- 正式命令不存在时显示明确提示，其他播放与浏览功能保持可用。
- Album List 是否有内容取决于 foobar2000 Media Library；Refrain 不扫描文件夹补偿空媒体库。

## 4. 参考证据

- Foobox `base.js` 的底部 `LibBtn` 在当前配置下执行 `Library/Album List`，证明它是底部媒体库入口而非 Playlist Browser 的搜索控件。
- Foobox `jsspm.js` 分别维护播放列表名称 Filter 与歌曲搜索，证明两者语义独立。
- foobar2000 SDK `mainmenu_commands::g_find_by_name()` 与 `g_execute()` 提供正式命令解析和执行；命令缺失可以返回失败。

## 5. 决策记录

- 2026-07-14：用户确认歌曲搜索继续使用 foobar2000 `Ctrl+F`，接受 Playlist Browser 名称 Filter，并要求 Album List 入口保留且放在左下角；临时图标由 Codex 自定。
- 2026-07-14：为保证筛选下全局播放列表顺序安全，Filter 非空时禁用侧栏重排和空白建表；这不影响对可见真实列表的激活、菜单与行目标拖放。
- 2026-07-14：实现完成。用户提供的开发实例截图确认 Filter 位于左栏顶部，底部左侧依次为 Playlist Workspace、Album Workspace、Album List、Settings，未遮挡现有内容；具体图标视觉仍留给步骤 14–16。
