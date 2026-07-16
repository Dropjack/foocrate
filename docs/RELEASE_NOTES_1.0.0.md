# FooCrate 1.0.0 发布说明

FooCrate 1.0.0 是面向 foobar2000 x64 与 Columns UI 的首个正式版本。它以原生 C++、Win32、Direct2D 和 DirectWrite 提供完整的 Foobox Basic 风格整合界面，不依赖 JavaScript UI 宿主。

## 主要功能

- 原生播放控制、进度、音量、输出设备与七种 Playback Order。
- Playlist View 分组、封面、列、排序、队列、拖放、评分和上下文菜单。
- Playlist Browser、Album Cover Browser、Album List 入口与播放定位。
- Now Playing 封面、歌曲信息、ESLyric 托管和 Metadata 安全降级。
- 深浅主题、四套预设、Follow Windows 与 Follow album artwork。
- 版本化设置、三档启动行为、DPI、高对比度和生命周期处理。
- 用户 Profile 下有容量上限、可删除重建的封面与主题色缓存。

## 已验证基线

- Windows 10/11 x64。
- foobar2000 2.25.10 stable x64。
- Columns UI 3.5.0。
- ESLyric 1.0.6.7。
- Playback Statistics 独立安装。

## 依赖与限制

- Columns UI 是必要宿主，不包含在 FooCrate 包内。
- ESLyric 和 Playback Statistics 是可选组件，不包含在 FooCrate 包内。
- FooCrate 不覆盖已有 Columns UI 布局、快捷键或 ESLyric 私有配置。
- 可选 `.fcl` 必须单独导入；ESLyric 字体、布局、歌词源和其他私有设置仍由 ESLyric 自己保存。
- 完整键盘对象树、屏幕阅读器支持、正式图标资产、Mini Player 和 CoverFlow 独立窗口不属于 1.0.0。

安装、升级、卸载和回退步骤见 `INSTALLATION_AND_UPGRADE.md`。
