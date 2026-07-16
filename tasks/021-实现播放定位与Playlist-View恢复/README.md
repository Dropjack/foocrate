# 021-实现播放定位与 Playlist View 恢复

- 状态：可实现
- 对应规格：[`../../specs/modules/PLAYBACK_LOCATION_AND_RESTORE.md`](../../specs/modules/PLAYBACK_LOCATION_AND_RESTORE.md) 0.1（已批准）
- 前置任务：[`../020-建立Refrain临时图标系统/README.md`](../020-建立Refrain临时图标系统/README.md)（已验收）
- Fork 提交标题：`实现播放定位与 Playlist View 恢复`
- 最后更新：2026-07-15

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
5. 保存上次播放列表、曲目、分组与滚动锚点；设置默认开启，可在 Refrain Preferences 关闭。
6. 启动时只恢复 Playlist View；无效身份逐层放弃，不创建或播放替代内容。

## 可检查步骤

### 步骤 1：定位模型与命令入口

- 输入：真实播放位置、任意冻结曲目、Default Playlist。
- 动作：实现两个命令、Track Title 双击、右键和稳定快捷键入口。
- 产物：跨播放列表确定定位。
- 用户检查方法：从普通、自动和 Album Browser 来源逐项测试。
- 通过标准：只改变查看位置，不改变播放或数据。
- 状态：待开始

### 步骤 2：恢复设置与迁移

- 输入：上次播放位置和现有版本化设置。
- 动作：新增默认开启设置、保存锚点、启动恢复与旧值迁移。
- 产物：只影响 Playlist View 的稳定恢复。
- 用户检查方法：开启/关闭、重启、删除原列表、切换 Album Browser。
- 通过标准：恢复可预测，Album Browser 和实际播放会话不受影响。
- 状态：待开始

## 用户检查点

- [x] 用户已核对功能范围和双击入口。
- [x] 用户已认可中文逻辑与默认开启范围。
- [ ] 用户已核对实现后的可见反馈。
- [ ] 用户已按人工验收步骤确认结果。

## 验证记录

- 2026-07-15：用户批准 `Show Now Playing`、`Show in Default Playlist` 和默认开启的 Playlist View 恢复；快速入口固定为双击 Artwork View 下方的歌曲名字。

## 改动文件

实现后填写。

## 决策与未决问题

- 已批准：双击 Track Title，不双击 Artwork View。
- 已批准：实际播放时间恢复服从 foobar2000，Refrain 设置只恢复 Playlist View。
