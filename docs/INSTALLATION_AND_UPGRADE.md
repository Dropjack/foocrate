# FooCrate 1.0.1 安装、升级与回退

## 运行环境

- Windows 10/11 x64。
- foobar2000 2.25.10 stable x64 为本次发布验证基线。
- Columns UI 3.5.0 是 FooCrate 根面板的必要宿主，必须独立安装。
- ESLyric 1.0.6.7 为歌词功能的已验证基线，可选并独立安装。
- Playback Statistics 为五星评分功能的已验证依赖，可选并独立安装。

FooCrate 不捆绑、不下载、不替换这些第三方组件。ESLyric 缺失或面板创建失败时，右下区域自动降级为 Metadata；Playback Statistics 缺失时，播放和浏览继续工作，评分入口显示不可用反馈。

## 全新安装

1. 在 foobar2000 的 `File > Preferences > Components` 中确认已经安装 Columns UI。
2. 点击 `Install...`，选择 `FooCrate-1.0.1.fb2k-component`。
3. 确认安装并按 foobar2000 提示重启。
4. 在 Columns UI 布局编辑中添加 `FooCrate` 根面板，或手动导入单独提供并已验证的 FooCrate `.fcl`。
5. 如需歌词或五星评分，分别从各自作者的正式渠道安装 ESLyric 与 Playback Statistics。

首次安装默认使用 `Start at home`：启动后保持停止，并回到 Default Playlist 首页，不自动播放。

## 从 1.0.0 或 0.1.0 升级

1. 正常退出 foobar2000。
2. 通过 `Preferences > Components > Install...` 选择 `FooCrate-1.0.1.fb2k-component`。
3. 确认替换旧版本并重启。

1.0.1 保持组件 GUID、面板 GUID、命令 GUID、设置 GUID 和配置格式不变，因此已有 FooCrate 布局、列、分组、主题和快捷键引用继续识别，不需要重新导入 FCL。由 0.1.0 升级时，旧版已开启 Playlist View 恢复的设置迁移为 `Restore last track without playing`。旧名缓存是可重建数据，不迁移；FooCrate 在用户 Profile 的 `foocrate/cache` 下重新建立缓存。

1.0.1 修复 Apple M4A/AAC/ALAC 文件内嵌 `rating=0` 时遮蔽 Playback Statistics 数据库评分的问题。升级不会修改音频文件标签，也不会重置已有 Playback Statistics 数据。

可选 `.fcl` 只用于用户主动导入布局，不在升级时自动覆盖现有 Columns UI 布局，也不携带或覆盖 ESLyric 的私有全局配置。

## 卸载

1. 打开 `File > Preferences > Components`。
2. 选择 FooCrate，执行移除并重启 foobar2000。

卸载组件不会删除播放列表、媒体库、ESLyric 配置、Playback Statistics 数据或 foobar2000 快捷键。FooCrate 自己的设置可能继续保留在 foobar2000 Profile 中，供重新安装使用。`foocrate/cache` 只含可重建封面缩略图和主题色，可在 foobar2000 完全退出后手动删除。

## 回退

1. 正常退出 foobar2000。
2. 在 Components 页面移除当前 FooCrate，重启完成移除。
3. 再通过 `Install...` 安装此前自行保留的旧 `.fb2k-component`。

1.0.1 保留稳定身份且不新增配置字段。较旧组件不保证理解 1.0.0 已新增的设置；回退前应备份 foobar2000 Profile。若回退后恢复状态异常，可在 FooCrate 设置中执行 Reset，或重新安装 1.0.1。

## 可选布局与 ESLyric

Columns UI `.fcl` 与组件包分开提供。正式布局必须从显示名已经是 FooCrate 的实例重新导出并在干净 `foobar-test` 中验证。ESLyric 的正式实例导出数据不足以携带其完整字体、布局、歌词源和显示规则，因此这些设置继续由用户在 ESLyric 右键菜单中单独调整和保存。
