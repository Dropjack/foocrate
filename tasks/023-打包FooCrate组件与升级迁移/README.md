# 023-打包 FooCrate 组件与升级迁移

- 状态：实现完成待验收
- 对应规格：[`../TODO.md`](../TODO.md) 步骤 23
- 前置任务：[`../022-建立ESLyric推荐默认设置/README.md`](../022-建立ESLyric推荐默认设置/README.md)
- Fork 提交标题：`打包 FooCrate 组件与升级迁移`
- 最后更新：2026-07-16

## 任务目标

提供可单独安装、升级、卸载和回退的 FooCrate `.fb2k-component`，并把 Columns UI、ESLyric、Playback Statistics 等外部组件的必要性和缺失行为说清楚。FooCrate 不捆绑未经允许的第三方组件；用户可以按需要分别升级它们。

## 明确不做

- 不把 ESLyric、Playback Statistics、Columns UI 或 Beefweb 二进制塞进 FooCrate 组件包。
- 不把用户放在 `third_party` 的本地组件提交到 Git。
- 不覆盖已有 Columns UI 布局、快捷键或 ESLyric 私有配置。
- 本步骤生成版本号为 1.0.1 的正式候选组件与安装资料；公开发布确认、全量人工回归和 Git tag 仍属于步骤 24。

## 中文程序逻辑

1. FooCrate 启动时通过正式接口检查可选组件能力，不通过文件名猜测安装状态。
2. ESLyric 可用时，播放中按设置显示歌词；不可用或创建失败时，右下区域自动显示 Metadata，不留下空白面板，也不反复尝试切回歌词。
3. Playback Statistics 不可用时，播放、浏览、Metadata 和歌词等功能继续工作；评分入口保持明确的不可用反馈。
4. Playlist View 的五星绘制和鼠标命中共用同一个五格几何模型。五星按 Foobox 的连续方格布局放大、居中，点击位置与实际可见星形一致。
5. Follow album artwork 模式切歌时，在新封面异步颜色完整算出前保留上一张封面和上一套配色，并把当前颜色重新推送给会在新曲时重置画面的 ESLyric；新结果到达后封面与颜色一起替换。新曲无封面时才在加载结束后回退默认色，快速切歌的过期结果继续由现有代次检查丢弃。
6. 在 foobar2000 SDK 返回的用户 Profile 下使用 `foocrate/cache` 保存可重建数据：`artwork` 保存 320px 缩略图，`theme-colors` 保存已提取配色。缓存身份包含媒体文件、外部封面路径、文件大小、修改时间和封面类型；源文件变化会生成新身份。
7. 缓存总上限为 256 MB，按最近使用时间清理；文件采用版本化格式、校验和与临时文件原子替换。目录不可写、远程路径、格式损坏或任何 I/O 失败时，退回现有内存加载，不影响播放。
8. Release 打包只收录 FooCrate DLL；构建后审计包内容并生成校验和。FCL 作为可选布局文件单独提供，不与组件二进制混装。
9. 用户手动安装、升级、卸载或回退；Codex 不自动写入日常 foobar2000。
10. FooCrate 提供三档启动行为：继续播放上次曲目并恢复绝对时间、只恢复上次曲目及 Playlist View 相对位置、停止播放并回到 Default Playlist 首页。首次安装和全局重置使用第三档；旧版已开启 Playlist View 恢复的配置迁移到第二档。

## 可检查步骤

### 步骤 1：缺失依赖降级

- 输入：未安装 ESLyric 的 foobar2000。
- 动作：载入 FooCrate，开始播放，并切换右下区域。
- 产物：右下区域自动显示 Metadata。
- 用户检查方法：确认没有白板或 “ESLyric not detected” 占据内容区；中键切换也不会进入不可用歌词页。
- 通过标准：缺少 ESLyric 不影响其余界面和播放。
- 状态：实现完成待验收

### 步骤 2：Playlist Rating 命中修正

- 输入：包含 Rating 列的 Playlist View。
- 动作：分别点击五颗星的左侧、中间、右侧，并移动鼠标预览。
- 产物：五颗较大的连续星形，预览和实际评分准确对应鼠标所在星位。
- 用户检查方法：特别检查第一颗和第五颗边缘，不应出现鼠标位于一颗星却点中相邻星的情况。
- 通过标准：绘制、Hover 和点击使用同一位置；窄列会整体缩放，不错位。
- 状态：实现完成待验收

### 步骤 3：正式包与升级迁移

- 输入：干净 test 实例、旧 FooCrate 配置、可选 FCL。
- 动作：安装、升级、卸载、回退并审计包内容。
- 产物：Release 组件、SHA-256、依赖说明和迁移记录。
- 用户检查方法：按最终验收单在 test 实例执行。
- 通过标准：包内只有获准文件；配置按规格保留或安全迁移；回退路径可执行。
- 状态：实现完成待验收

### 步骤 4：封面配色无中间闪色

- 输入：Follow album artwork 模式下颜色差异明显的连续曲目。
- 动作：连续切歌，并包含一首无封面的曲目。
- 产物：加载新封面期间保持上一套完整配色；新配色准备好后直接切换。
- 用户检查方法：观察中栏、右栏和歌词背景，不应在两套封面配色之间短暂闪成 Mist 默认色。
- 通过标准：有封面曲目只发生一次完整换色；无封面曲目加载结束后一次性回退；快速切歌不出现过期颜色。
- 状态：实现完成待验收

### 步骤 5：用户 Profile 持久封面缓存

- 输入：Album Browser、带分组封面的 Playlist View 和 Follow album artwork。
- 动作：浏览一批专辑并切歌，退出 foobar2000 后重新启动，再次访问相同内容。
- 产物：当前 Profile 下的 `foocrate/cache/artwork` 与 `foocrate/cache/theme-colors`。
- 用户检查方法：第二次访问时缩略图和主题色应更快出现；替换一张外部 cover 后应生成新结果而不是继续显示旧图。
- 通过标准：缓存跨重启生效，最大 256 MB；删除整个 `foocrate` 目录后可以无损重建；目录不可写或文件损坏时界面仍正常加载。
- 状态：实现完成待验收

### 步骤 6：三档启动行为

- 输入：分别选择 `Resume last track and position`、`Restore last track without playing`、`Start at home`。
- 动作：播放到一首歌的中段并滚动 Playlist View，正常退出 foobar2000 后重新打开。
- 产物：第一档自动播放同一首歌并 seek 到保存的绝对秒数；第二档保持停止但定位同一首歌和原视口锚点，按 Play 即从该曲开始；第三档保持停止并回到 Default Playlist 顶部。
- 用户检查方法：三档各重启一次；再执行 FooCrate 设置 Reset，确认回到第三档。
- 通过标准：首次安装/Reset 不自动播放；旧版 `Restore Playlist View` 开启值升级为第二档；无效或已删除的曲目安全回到首页，不播放替代内容。
- 状态：实现完成待验收

## 用户检查点

- [x] 用户已要求 ESLyric 缺失时自动回退 Metadata。
- [x] 用户已要求 Playlist Rating 采用 Foobox 风格的大五星并修正命中。
- [ ] 用户已核对正式发布依赖策略。
- [x] 用户已确认默认 FCL 可导入并形成 FooCrate 布局。
- [ ] 用户已检查三档启动行为和设置 Reset 默认值。

## 验证记录

- 2026-07-16：用户在正式打包前决定将产品由旧名改为 `FooCrate`，以 `Crate` 保留对 Foobox 视觉与交互来源的致意。源码 namespace、显示名、Preferences、面板类名、CMake target、测试 target、缓存目录、DLL、组件包、FCL 名称和项目维护文档全部改名；稳定组件/面板/设置 GUID 数值保持不变，保证旧安装的布局与设置继续识别。旧名缓存属于可重建数据，不迁移，FooCrate 改用 `foocrate/cache`。
- 2026-07-16：FooCrate 改名后的 Debug/Release 构建成功，两种配置各 14/14 自动测试通过；模拟安装布局审计通过。最终组件为 `dist/FooCrate-0.1.0.fb2k-component`，大小 401438 字节，SHA-256 `2C4F8B40293EC4C9E285D83168B03D5AD897336D646829E92BD4F4E8E83645DE`；包内仅 `foo_crate.dll`，DLL SHA-256 `FE298E907A704CAF06650312876F2E8B0694D9EF7EC7738A25FD3F8480F3CA1C`。Windows 版本信息的 ProductName、FileDescription 和 OriginalFilename 均已更新；项目维护文本和文件名扫描确认无旧产品名残留，UTF-8 无 BOM 验证通过。
- 2026-07-16：实现 ESLyric 创建失败、播放切歌和设置刷新三条路径的 Metadata 自动回退。
- 2026-07-16：Playlist Rating 改为五个连续的原生五星；绘制与 Hit Test 共用 `PlaylistRatingStrip`，默认每颗 16 DIP，列宽不足时等比缩放。
- 2026-07-16：x64 Debug 和 Release 组件构建成功；两种配置各 13 项自动测试全部通过。
- 2026-07-16：生成 `dist/FooCrate-0.1.0.fb2k-component`，包内容审计仅含根目录 `foo_crate.dll`；当前修正版等待用户手动导入检查缺失 ESLyric 降级与五星命中。
- 2026-07-16：修正 Follow album artwork 切歌时先清空主题造成的中间闪色；加载期间复用已经存在的上一套主题，不增加缓存或异步资源，新主题完成后一次性替换。
- 2026-07-16：封面配色修正版完成 x64 Debug/Release 构建，两种配置各 13 项自动测试通过；重新生成 Release 组件等待人工检查。
- 2026-07-16：用户复验发现取消默认主题刷新后，ESLyric 新曲初始化会直接闪白，且 Now Playing 封面仍被提前释放。二次修正为切歌时重新应用当前 ESLyric 颜色并继续显示上一张封面，新封面与配色完成后统一替换。
- 2026-07-16：二次闪白修正版完成 x64 Debug/Release 构建，两种配置各 14 项自动测试通过，并重新生成 Release 组件供人工复验。
- 2026-07-16：用户再次确认同一曲目重新播放也会闪白，证明白帧来自 ESLyric 自身的播放事件重绘，而非封面加载。Foobox 证据显示其同曲重载延迟 10 ms 取色、新图到达后才更新 ESLyric，并通过旧图/动画覆盖过渡。FooCrate 改为在歌词子窗口上方短暂显示当前背景色遮罩，并在 16/48/96/160 ms 多次重推颜色，约 192 ms 后撤掉遮罩。
- 2026-07-16：歌词遮罩修正版完成 x64 Debug/Release 构建，两种配置各 14 项自动测试通过，并重新生成 Release 组件供同曲重播人工复验。
- 2026-07-16：用户确认遮罩版仍会整界面闪白。重新对照 Foobox 后确认其颜色更新只修改状态并重绘，而 FooCrate 每次换色会释放全部 Direct2D 画刷后重建；同时遮罩版还反复调用 ESLyric 的全局 `GetAll()` 颜色接口。撤销遮罩/重试，改为相同配色零更新、不同配色仅更新实际变化的 ESLyric 字段，并原地调用 `ID2D1SolidColorBrush::SetColor`，不再销毁整套绘图资源。
- 2026-07-16：原地画刷更新修正版完成 x64 Debug/Release 构建，两种配置各 14 项自动测试通过，并重新生成 Release 组件供人工复验。
- 2026-07-16：用户提供决定性对照：`Right Panel = Playlist Selection` 完全不闪，只有 `Playing Track` 必闪。定位到 foobar 切歌回调之间存在 Now Playing 句柄暂时为空的过渡窗口；FooCrate 把该空值当成真正停止，短暂切到焦点曲目或清空目标。新增播放切换状态，过渡期间 Now Playing 为空时保留旧目标，只有正常停止后才回到焦点曲目。
- 2026-07-16：Playing Track 过渡状态修正版完成 x64 Debug/Release 构建，两种配置各 14 项自动测试通过，并重新生成 Release 组件供人工复验。
- 2026-07-16：新增 Profile 下的持久缩略图和主题色缓存；缓存不保存原始高清封面或不可恢复配置。缓存身份以 foobar2000 实际引用的本地封面源路径、大小、修改时间和封面类型生成；远程/不可识别来源不落盘。
- 2026-07-16：x64 Debug/Release 构建成功，两种配置各 14 项自动测试全部通过；新增缓存格式、损坏回退和容量上限测试。
- 2026-07-16：按用户的新决定扩展任务 21 的恢复范围：设置版本升至 8，FooCrate Preferences 用三选一下拉框替代旧复选框；恢复状态新增绝对播放秒数。续播通过主线程消息启动目标曲目，并最多检查三次 seek 能力；旧版开启恢复迁移为“只定位不播放”，首次安装和 Reset 默认“Start at home”。
- 2026-07-16：三档启动行为完成 x64 Debug/Release 构建，两种配置各 14/14 自动测试通过。Release 手动导入包为 `dist/FooCrate-0.1.0.fb2k-component`，大小 400577 字节，SHA-256 `68DBB049632E3320F288C6BDA65820A555E4518ED13342F4024023C42AA8C640`；包内仅 `foo_crate.dll`，且条目哈希与 Release DLL 一致。
- 2026-07-16：用户实测发现首次实现会把上次曲目的时间应用到列表第一首，且“只恢复不播放”无法让 FooCrate Play 从目标曲目开始。只读检查 Foobox `jsplaylist.js` / `WSHplaylist.js`：其列表播放始终使用 `ExecutePlaylistDefaultAction(playlist, exact item index)`，定位则同步 Active Playlist、Focus 和 Selection。FooCrate 据此冻结组件加载前读到的恢复记录，等待 foobar 初始播放停止完成后再恢复，并在启动阶段禁止回调用第一首覆盖记录；恢复状态新增精确 playlist item index，身份变化时才回退路径/子曲目查找。停止状态下的 FooCrate Play 改为执行当前焦点的精确 item；启动模式下拉框宽度缩为原来的 4/5 并受页面可用宽度限制。
- 2026-07-16：启动恢复竞态修正版完成 x64 Debug/Release 构建，两种配置各 14/14 自动测试通过。新包 `dist/FooCrate-0.1.0.fb2k-component` 大小 401136 字节，SHA-256 `1738272988B07877518C0AC43B1A9099D136F0957B5A022020B6B65F29D69D0B`；包内仅 `foo_crate.dll`，条目哈希与 Release DLL 一致。
- 2026-07-16：用户再次复测仍回到 favpop 第一首。复核发现启动保护在恢复目标不匹配、seek 失败等路径没有解除，且曲目身份只在 `on_playback_new_track` 回调内读取；这会让旧 playlist GUID/item 长期保留，而新歌曲的时间继续写入旧记录。修正为所有退出路径都结束保护，切歌后通过窗口消息延迟读取 Playing Location，并在每 5 秒、暂停、seek 和关闭时重新保存完整 playlist GUID + item index + 曲目身份。恢复秒数同时限制在实际曲长以内。Debug/Release 各 14/14 测试通过；新包大小 401154 字节，SHA-256 `E24BAF68D8D0EBD5D4138827A4BF96EEB5118A2DE68A1EF46B5CF8472F556D48`，内容审计仍仅含 `foo_crate.dll`。
- 2026-07-16：为避免前两版已污染的 `favpop/第一首 + 其他曲目时间` 继续影响复测，恢复记录增加独立格式版本；旧的无版本记录自动视为无效，升级后首次启动安全回首页，随后播放才建立新格式记录。最终 Debug/Release 各 14/14 通过；组件大小 401392 字节，SHA-256 `2A5069998F8CB1718D04CBDEC04D90D3E907113F1074AC5E171F3160FE8C9A94`，包内仅 `foo_crate.dll`。
- 2026-07-16：完成 FooCrate 1.0.0 发布元数据审计。CMake、foobar2000 组件版本、稳定身份常量、Windows VERSIONINFO、测试和打包文件名统一为 `1.0.0`；产品总规格同步三档启动行为。改名后的维护文本通过严格 UTF-8 无 BOM 检查。
- 2026-07-16：在全新 `build/release-1.0.0` 构建树完成 x64 Debug/Release；两种配置各 14/14 自动测试通过。正式候选包为 `dist/FooCrate-1.0.0.fb2k-component`，大小 401443 字节，SHA-256 `B0F266C27175C4E0C2456B84E4947D255C56233374AEE15A582F9F94B61C620A`。包内仅根目录 `foo_crate.dll`；条目与 Release DLL 的 SHA-256 均为 `6E7A5703FB1654D81CB9B51196CD7ED7FA018C9918D932803AAC9840B9725F33`。Windows FileVersion、ProductVersion、ProductName、FileDescription 与 OriginalFilename 审计通过。
- 2026-07-16：`dist` 已整理为 1.0.0 组件、安装升级说明、发布说明和 SHA-256 清单；旧 0.1.0 组件及嵌有 `Refrain` 名称的旧 FCL 已从交付目录移除。当前仍需用户在改名后的 Columns UI 中重新导出 `.fcl` 并在 `foobar-test` 导入验证；仓库尚未声明项目许可证，公开发布前必须由用户决定许可证或明确保留全部权利。
- 2026-07-16：用户完成默认 Columns UI 布局的导入验证并重新导出 `dist/FooCrate-1.0.0.fcl`。文件大小 9821 字节，SHA-256 `9D62D4133F6E27D6C6BE6F83C2419D9FD4109F7ACB28A98B192566655EE6B1FC`；可见字符串审计包含 `FooCrate` 且不含 `Refrain`。布局保持通用默认，不覆盖 ESLyric 私有个性化配置。
- 2026-07-16：发布候选人工检查发现部分 Apple M4A/AAC/ALAC 文件内嵌 `rating=0` 时，正在播放项目的动态标题格式会优先返回文件标签，遮蔽 Playback Statistics 数据库评分；正式右键 Rating 命令仍可写入数据库，因此表现为 FooCrate 不显示、点击后也像未生效。修正为右上五星、Playlist View Rating 列及 Track Details 的 Playback Statistics 区段始终使用目标 metadb 句柄的静态显示字段，让 Playback Statistics 的字段提供器决定结果；动态码率等真正需要播放动态信息的字段保持原路径。FooCrate 不读取或改写文件 `RATING` 标签。
- 2026-07-16：M4A 评分修正版在 `build/release-1.0.0` 完成 x64 Debug/Release 构建，两种配置各 14/14 自动测试通过。手动测试组件为 `dist/FooCrate-1.0.0.fb2k-component`，大小 401907 字节，SHA-256 `93E5D16F105491257457980CE276135DED965138FBFDE1572A64E88EDA60154F`；包内仅根目录 `foo_crate.dll`，条目与 Release DLL 的 SHA-256 均为 `D2875A136CD2AB45D7E8165B436133A4A044526ACE9AC2F3C58B86552B3DF393`。本轮保持版本 1.0.0 供人工复验；用户确认后再升至 1.0.1 并更新正式发布材料。
- 2026-07-16：用户确认 Apple M4A/AAC/ALAC 评分显示、写入和清除均通过人工复验，FLAC 回归正常；批准将修复正式发布为 1.0.1。版本元数据、安装说明、README 和独立 1.0.1 Release Notes 同步更新；1.0.0 Release Notes 作为首版历史保留，1.0.0 FCL 因稳定 GUID 和布局格式未变继续兼容。
- 2026-07-16：在全新 `build/release-1.0.1` 构建树完成 x64 Debug/Release，两种配置各 14/14 自动测试通过。正式组件为 `dist/FooCrate-1.0.1.fb2k-component`，大小 401905 字节，SHA-256 `46EACA4DB76DCA8435C06BF87508AAA082DA23332D867E497B0C735080973E6F`；包内仅根目录 `foo_crate.dll`，条目与 Release DLL 的 SHA-256 均为 `7DFC878AA3DAA30690C7EC1F4F4AC8F9D0220ABE92E9EB596A3DFC4463430511`。Windows FileVersion/ProductVersion 均为 1.0.1，ProductName、FileDescription 和 OriginalFilename 审计通过。`dist/FooCrate-1.0.1-SHA256SUMS.txt` 仅列本版组件，`dist/FooCrate-1.0.1-RELEASE-NOTES.md` 与仓库正式说明内容一致。

## 改动文件

- `src/playback_panel.cpp`：ESLyric 缺失回退、Rating 五星绘制和点击命中、M4A 内嵌 Rating 与 Playback Statistics 的读取隔离、封面主题异步切换期间的稳定配色，以及三档启动恢复。
- `src/settings_model.h`、`src/settings.h`、`src/settings.cpp`、`src/preferences.cpp`：启动行为枚举、版本 8 迁移、绝对时间保存与设置界面。
- `tests/settings_model_tests.cpp`：首次安装、旧版恢复开关和新三档值的迁移测试。
- `src/artwork_cache.h`、`src/artwork_cache.cpp`：用户 Profile 缓存目录、版本化文件、校验、原子写入和 256 MB 清理。
- `src/artwork_loader.h`、`src/artwork_loader.cpp`：以媒体/外部封面文件状态生成缓存身份，并复用缩略图和主题色结果。
- `tests/artwork_cache_tests.cpp`：像素/配色往返、损坏文件丢弃和容量上限测试。
- `src/playlist_interaction_model.h`：共享的五星排列与命中模型。
- `tests/playlist_interaction_model_tests.cpp`：五星边缘、连续格和窄列缩放测试。
- `CMakeLists.txt`、`src/component.cpp`、`src/component_identity.h`、`src/foo_crate.rc`、`tests/component_identity_tests.cpp`：1.0.1 版本元数据与自动检查。
- `scripts/package-component.ps1`、`scripts/prepare-release.ps1`：按版本生成正式组件、整理 `dist` 文档并生成 SHA-256 清单。
- `README.md`、`docs/INSTALLATION_AND_UPGRADE.md`、`docs/RELEASE_NOTES_1.0.0.md`、`docs/RELEASE_NOTES_1.0.1.md`：当前发布入口、依赖、安装、升级、卸载、回退、首版历史与 1.0.1 修复说明。
- `.gitignore`：忽略用户本地保存的第三方 `.fb2k-component`。
- 本任务、任务入口和 TODO：记录步骤 23 的范围、状态和人工验收。

## 决策与未决问题

- 五星采用 Foobox 的 5 个连续正方形交互模型，但由 FooCrate 原生 Direct2D 绘制，不复制 Foobox 的字体或位图资产。
- ESLyric 和 Playback Statistics 维持可选独立组件；Columns UI 是 FooCrate 面板运行所需宿主。
- 正式 FCL 已完成改名后导出和导入验证；ESLyric 字体、布局、歌词源等私有个性化配置不进入 FCL，由用户在 ESLyric 右键单独保存。
- 仓库尚未声明项目许可证；这不阻挡第 23 步提交，但阻挡公开发布 GitHub 仓库或 Release。
- 任务 21 原先“实际播放时间恢复服从 foobar2000”的决定已被本任务中的用户新要求取代；FooCrate 现在正式负责上述三档启动行为。
- Foobox 参考证据属于只读脚本观察：`D:\Dev\foobar2000\profile\foobox\script\js_panels\jsplaylist.js` 的 Enter 播放和 `WSHplaylist.js` 的 Now Playing 定位均使用 playlist item index；FooCrate 只复现可观察思路，不复制其 JavaScript 结构。
