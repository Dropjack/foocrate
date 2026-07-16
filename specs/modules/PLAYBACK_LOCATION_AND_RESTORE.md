# 播放定位与 Playlist View 恢复规格

- 状态：已验收
- 版本：0.2
- 最后更新：2026-07-16
- 所属产品规格：[`../PRODUCT_SPEC.md`](../PRODUCT_SPEC.md)
- 对应路线：`21 实现播放定位与 Playlist View 恢复`

## 1. 用户目标

用户平时可以播放任意普通或自动播放列表，同时把包含完整曲库的 `all` 等列表设为 Default Playlist。Refrain 必须既能找到“当前到底从哪里播放”，也能把同一首歌带到 Default Playlist 中，从而自然浏览它所在的完整专辑。

这是一套通用的“播放来源定位”和“资料库目的地定位”，不是复制曲目、创建临时列表或重建媒体库。

## 2. 正式命令

### 2.1 `Show Now Playing`

1. 读取 foobar2000 的真实播放列表和曲目索引。
2. 切换 Playlist Workspace 到该播放列表。
3. 展开曲目所属分组，设置焦点与单项选择，并滚动到可见位置。
4. 不重新开始播放、不 Seek、不改变播放顺序和列表内容。
5. 如果播放来自隐藏的 `Refrain Album View` 桥接列表，不把该内部列表暴露给用户；优先在原 Album Source 播放列表查找，找不到再在 Default Playlist 查找，仍找不到时给出明确提示。

入口包括：

- 双击 Now Playing Header 中的 `Track Title`；
- 稳定的 Refrain 主命令，供 foobar2000 快捷键系统绑定。

双击 `Artwork View` 不执行本命令。

### 2.2 `Show in Default Playlist`

1. 冻结用户右击时的曲目句柄，不能在菜单打开后因切歌作用到另一首。
2. 通过已保存的 Default Playlist GUID 找到目标列表。
3. 按文件身份与子曲目索引查找精确引用；存在重复引用时定位第一项，并保持行为确定。
4. 切换目标列表、展开所属分组、设置焦点与单项选择并滚动到可见位置。
5. 不向 Default Playlist 插入曲目，不改变自动列表规则，不触发播放。
6. 目标列表消失时先服从现有 Default Playlist 修复规则；曲目不在目标列表时显示 `Track is not in the Default Playlist`。

入口为 foobar2000 原生 Track Actions 上下文命令。该命令以独立分组显示在 `Add to playback queue` 上方；Refrain 的 Now Playing Header、Playlist View 和 Album Track List 均打开完整的原生 Track Actions 菜单，不以自定义菜单隐藏原有命令。

## 3. Playlist View 恢复

Refrain Preferences 新增默认开启的：

`Restore Playlist View to last played track`

Refrain 保存：

- 播放列表稳定 GUID；
- 文件路径与子曲目身份；
- 所属分组的稳定键；
- 曲目相对于视口的滚动锚点；
- 为显示该曲目所必需的分组展开状态。

启动或重建面板时：

1. 如果 foobar2000 已恢复真实播放，优先显示当前真实播放项。
2. 如果没有播放，只恢复 Playlist View 到上次曲目附近，不擅自开始播放。
3. 实际播放时间点由 foobar2000 自己的恢复能力负责；Refrain 不自动 Seek，也不维护第二套播放恢复开关。
4. 此设置不改变 Album Cover Browser 的来源、专辑选择或滚动位置。
5. 列表、曲目或分组已不存在时安全放弃对应层级，保留当前有效视图，不创建替代数据。

## 4. 状态与失败边界

- 没有正在播放曲目时，`Show Now Playing` 禁用并说明原因。
- 网络流没有可定位的播放列表引用时不伪造目的地。
- 自动播放列表允许被激活和查看；定位不尝试重排其内容。
- 定位期间若列表被删除或重建，提交前重新用 GUID 和曲目身份核对。
- 大型 Default Playlist 的查找不得在绘制阶段执行；结果返回时重新确认窗口和目标命令仍有效。

## 5. 决策记录

- 2026-07-15：用户批准把“定位正在播放”和“前往 Default Playlist”统一为播放定位系统。
- 2026-07-15：用户指定快速入口为双击 `Track Title`，不双击 Artwork View。
- 2026-07-15：用户批准 Playlist View 恢复默认开启，并明确不影响 Album Browser；实际播放时间恢复继续由 foobar2000 核心负责。
- 2026-07-16：实现使用设置版本 7 和独立稳定 GUID 保存播放列表 GUID、文件路径、子曲目、分组键及视口锚点；旧版本升级默认开启恢复。右键菜单冻结本地曲目句柄，提交前重新核对播放列表 GUID 与曲目身份。
- 2026-07-16：按用户复查修正右键集成：右键中移除 `Show Now Playing`，恢复完整原生 Track Actions；`Show in Default Playlist` 注册为 `Add to playback queue` 上方的独立分组。
- 2026-07-16：用户确认没有更多任务 21 测试并明确要求划勾，规格按最终实现验收。
