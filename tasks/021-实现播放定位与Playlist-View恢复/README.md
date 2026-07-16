# 021-实现播放定位与 Playlist View 恢复

- 状态：已验收（待提交）
- 对应规格：[`../../specs/modules/PLAYBACK_LOCATION_AND_RESTORE.md`](../../specs/modules/PLAYBACK_LOCATION_AND_RESTORE.md) 0.2（已验收）
- 前置任务：[`../020-建立FooCrate临时图标系统/README.md`](../020-建立FooCrate临时图标系统/README.md)（已验收）
- Fork 提交标题：`实现播放定位与 Playlist View 恢复`
- 最后更新：2026-07-16

## 任务目标

用户可以从任何正常播放来源定位当前歌曲，也可以把歌曲带到包含完整曲库的 Default Playlist 中查看所属专辑。重启或重建面板后，Playlist View 默认回到上次播放曲目附近，但不改变 Album Browser，也不建立第二套播放恢复。

## 明确不做

- 不向 Default Playlist 自动插入缺失曲目。
- 不创建“完整专辑”临时列表或自建媒体库。
- 不替代 foobar2000 的实际播放启动和时间点恢复。

## 中文程序逻辑

1. 命令冻结播放项或右击曲目的稳定身份。
2. 用播放位置或 Default Playlist GUID 找目标列表；隐藏 Album Bridge 转译为用户可见来源。
3. 找到精确文件/子曲目后切换活动列表，展开分组、设置焦点和选择并滚动到可见位置。
4. 每次提交前重新核对列表 GUID、曲目和窗口生命周期。
5. 保存上次播放列表、曲目、分组与滚动锚点；设置默认开启，可在 FooCrate Preferences 关闭。
6. 启动时只恢复 Playlist View；无效身份逐层放弃，不创建或播放替代内容。

## 可检查步骤

### 步骤 1：定位模型与命令入口

- 输入：真实播放位置、任意冻结曲目、Default Playlist。
- 动作：实现两个命令、Track Title 双击、右键和稳定快捷键入口。
- 产物：跨播放列表确定定位。
- 用户检查方法：从普通、自动和 Album Browser 来源逐项测试。
- 通过标准：只改变查看位置，不改变播放或数据。
- 状态：已完成

### 步骤 2：恢复设置与迁移

- 输入：上次播放位置和现有版本化设置。
- 动作：新增默认开启设置、保存锚点、启动恢复与旧值迁移。
- 产物：只影响 Playlist View 的稳定恢复。
- 用户检查方法：开启/关闭、重启、删除原列表、切换 Album Browser。
- 通过标准：恢复可预测，Album Browser 和实际播放会话不受影响。
- 状态：已完成

## 用户检查点

- [x] 用户已核对功能范围和双击入口。
- [x] 用户已认可中文逻辑与默认开启范围。
- [x] 用户已核对实现后的可见反馈。
- [x] 用户已按人工验收步骤确认结果。

## 验证记录

- 2026-07-15：用户批准 `Show Now Playing`、`Show in Default Playlist` 和默认开启的 Playlist View 恢复；快速入口固定为双击 Artwork View 下方的歌曲名字。
- 2026-07-16：实现 `View > FooCrate: Show Now Playing` 稳定命令和 Track Title 双击；Now Playing Header、Playlist View 与 Album Track List 均使用完整的 foobar2000 原生 Track Actions 菜单。`Show in Default Playlist` 是 `Add to playback queue` 上方的独立分组，右键中不显示 `Show Now Playing`。命令执行时冻结首个选中曲目句柄；定位前后均按播放列表 GUID 和文件＋子曲目身份重新核对，重复项固定取第一项。
- 2026-07-16：隐藏 `FooCrate Album View` 播放优先回查原 Album Source 播放列表，再回查 Default Playlist；普通和自动播放列表只被激活查看，不播放、不 Seek、不排序、不插入。
- 2026-07-16：设置版本由 6 升至 7，新增默认开启的 `Restore Playlist View to last played track`；保存播放列表 GUID、路径、子曲目、分组键和视口锚点。启动时优先真实播放项，无播放时恢复最后播放位置；关闭设置时不自动恢复。
- 2026-07-16：x64 Debug/Release 串行构建成功；两种配置各 13 项自动测试全部通过；严格 UTF-8、无 BOM 与 `git diff --check` 通过。
- 2026-07-16：最终 Release 手动导入组件生成于 `dist/FooCrate-0.1.0.fb2k-component`，大小 367237 字节，SHA-256 为 `4D63DF0571DF527A479C90C65CB5FEAA3E29F1CF997EC575E284B898EE490DA5`；内容审计仅含 `foo_crate.dll`。
- 2026-07-16：按用户复查恢复完整原生 Track Actions，右键中移除 `Show Now Playing`，并将 `Show in Default Playlist` 置于 `Add to playback queue` 上方的独立分组；修正版 Debug/Release 各 13 项测试通过，组件重新生成并审计。
- 2026-07-16：用户确认没有更多测试并要求给任务 21 划勾；任务已验收，等待用户在 Fork 中提交。

## 改动文件

- `src/playback_panel.cpp`：定位命令、双击入口、原生 Track Actions 集成、桥接转译、确定性定位、恢复与生命周期处理。
- `src/playback_location_model.h`：视口锚点恢复的纯模型边界。
- `src/settings_model.h`、`src/settings.h`、`src/settings.cpp`：设置版本 7、恢复开关和稳定恢复状态。
- `src/preferences.cpp`：默认开启的 Playlist View 恢复复选框。
- `tests/playback_location_model_tests.cpp`、`tests/settings_model_tests.cpp`：锚点边界与版本迁移测试。
- `CMakeLists.txt`：注册定位模型和测试目标。
- `specs/modules/PLAYBACK_LOCATION_AND_RESTORE.md`、本任务文档及任务索引：同步规格和验证证据。

## 决策与未决问题

- 已批准：双击 Track Title，不双击 Artwork View。
- 已批准：实际播放时间恢复服从 foobar2000，FooCrate 设置只恢复 Playlist View。

## 人工验收

1. 手动导入 `dist/FooCrate-0.1.0.fb2k-component` 并重启 foobar2000。
2. 从普通播放列表播放一首歌，再切换到另一播放列表；双击右侧 Track Title，确认回到真实播放列表、目标曲目被单选且可见，播放时间没有跳变。
3. 在 `View` 菜单执行 `FooCrate: Show Now Playing`，并在 foobar2000 快捷键设置中确认该命令可绑定；结果应与双击一致。
4. 分别右击 Now Playing Header、Playlist View 曲目和 Album Track List 曲目，确认原有 Track Actions 完整保留、右键中没有 `Show Now Playing`，并确认 `Show in Default Playlist` 作为独立分组紧邻显示在 `Add to playback queue` 上方；该命令只定位 Default Playlist 中第一条精确引用，不插入、不播放。
5. 从 Album Workspace 播放专辑后执行 `Show Now Playing`，确认不会显示隐藏的 `FooCrate Album View`，而是回到原 Album Source 或 Default Playlist。
6. 对不在 Default Playlist 的曲目执行定位，确认出现 `Track is not in the Default Playlist`，列表内容不变。
7. 在 FooCrate Preferences 确认 `Restore Playlist View to last played track` 默认勾选；播放、滚动后重启，确认只恢复 Playlist View 位置。取消勾选并重启，确认不再自动定位，Album Workspace 的来源和选择不受影响。
