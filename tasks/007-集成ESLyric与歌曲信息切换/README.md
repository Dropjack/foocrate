# 007 集成 ESLyric 与歌曲信息切换

- 状态：已验收
- TODO 步骤：`07 集成 ESLyric 与歌曲信息切换`
- Fork 提交标题：`集成 ESLyric 与歌曲信息切换`
- 前置提交：`ab0994d 实现封面基本信息与五星评分`
- 对应规格：[`../../specs/modules/LYRICS_AND_TRACK_DETAILS.md`](../../specs/modules/LYRICS_AND_TRACK_DETAILS.md)
- 最后更新：2026-07-13

## 任务目标

完成 `Now Playing Panel` 右下区域：默认托管用户已安装的 ESLyric Columns UI 面板，并允许用鼠标中键在歌词和 Refrain 原生 `Track Details View` 之间切换。歌词搜索、下载、解析、右键菜单和双击全屏继续由 ESLyric 自己负责。

## 已冻结边界

- 不复制或重写歌词引擎，不把 ESLyric QuickJS 当作 Refrain UI 宿主。
- 不捆绑、不替换、不静默下载 ESLyric；缺失或实际不可托管时只禁用右下歌词功能。
- 不截获 ESLyric 内部双击和右键交互。
- 歌曲信息是 Refrain 原生只读字段名—字段值视图；编辑只调用 foobar2000 Properties。
- 本步骤不实现播放列表、左侧浏览器、最终三栏比例或最终主题视觉。

## 当前取证问题

1. Columns UI 正式接口如何由 Refrain 创建、持有、显示、隐藏并销毁 ESLyric 窗口，能否稳定按扩展 GUID 寻址。
2. Foobox 的中键命中范围、切歌/停止自动切换、双击全屏前提和保存方式中，哪些是已观察行为，哪些只是脚本配置。
3. `Track Details View` 的字段顺序、技术字段范围、空字段规则、滚动/选择/复制与 Properties 目标冻结方式。
4. ESLyric 缺失、创建失败、运行中失效、窗口重建和 Refrain 销毁时的降级与生命周期。

## 中文程序逻辑（待规格核准后冻结）

1. Refrain 在右下宿主区域按正式 Columns UI 扩展身份查找 ESLyric，而不是按 DLL 路径或窗口标题猜测。
2. 宿主只在主线程创建和转移子窗口；Refrain 保存宿主所有权、当前显示模式和窗口生命期，切换只改变可见子项，不销毁歌词状态。
3. 中键事件只在核准的右下命中范围内切换；交给 ESLyric 的双击、滚轮、右键和键盘消息不被 Refrain 重解释。
4. 歌曲信息针对冻结的 Now Playing/停止焦点句柄生成只读快照；元数据变化时在主线程更新，复制和 Properties 始终作用于用户当前看到的目标。
5. ESLyric 缺失或创建失败时显示明确恢复入口，其他播放摘要和控制保持可用；窗口销毁先解除宿主关系和回调，拒绝晚到刷新。
6. 经核准需要持久化的视图模式写入现有版本化 Refrain 设置，并具有合法值校验、迁移和恢复默认。

## 计划步骤

### 步骤 1：参考与接口取证

- 状态：完成
- 产物：Foobox 行为证据、Columns UI/ESLyric 接口证据和差异清单。

### 步骤 2：模块规格核准

- 状态：完成
- 产物：`LYRICS_AND_TRACK_DETAILS.md` 与集中待确认问题。

### 步骤 3：ESLyric 正式托管与降级

- 状态：完成
- 产物：右下歌词宿主、缺失提示和生命周期处理。

### 步骤 4：Track Details 与切换

- 状态：完成
- 产物：只读字段视图、复制、Properties、中键切换和状态保存。

### 步骤 5：构建、部署与人工验收

- 状态：完成
- 产物：Debug/Release、回归测试、隔离部署和人工检查。

## 验证记录

- 2026-07-13：步骤 06 已提交为 `ab0994d`，用户在修正根布局与评分识别后明确验收通过。
- 2026-07-13：开始步骤 07 时仅有用户勾选 TODO 06 的已知修改；该修改已保留并并入任务状态同步。
- 2026-07-13：只读确认 Foobox 使用同一 ESLyric 面板显隐完成切换，默认新曲显示歌词、正常停止显示歌曲信息；歌曲信息默认分区与字段过滤已记录。
- 2026-07-13：Columns UI 8.1.0 SDK 确认可以通过稳定扩展 GUID 创建 `uie::window` 并由 Refrain `uie::window_host` 托管；ESLyric 私有 ActiveX 接口不进入实现。
- 2026-07-13：建立 `LYRICS_AND_TRACK_DETAILS.md` 0.1，集中保留三个影响用户习惯的核准项，未开始功能代码。
- 2026-07-13：用户选择 Foobox 兼容方案 F，接受默认字段分区，并要求立即实现可保存的右侧上下分隔线；规格 0.2 获准，开始实现。
- 2026-07-13：实现专用 `uie::window_host`、同一 ESLyric 子窗口显隐、子窗口中键转交、缺失占位和销毁/所有权转移清理；双击、右键、滚轮和键盘仍由 ESLyric 子窗口处理。
- 2026-07-13：实现右侧全区中键切换、新曲/正常停止自动切换、关闭自动切换后的页面保存，以及 25%–80% 范围内可拖动并保存的右侧上下分隔线。
- 2026-07-13：实现 Metadata、Location、Technical、Playback Statistics 和可选 ReplayGain 详情分区，支持滚轮/Page/Home/End、Ctrl/Shift 多选、Ctrl+C、右键复制/全选/Properties。
- 2026-07-13：设置版本升至 2，增加自动切换、默认页面、ReplayGain 和分隔比例的迁移与校验；Debug/Release 构建及各 4 项自动测试通过。
- 2026-07-13：生成 `dist/Refrain-0.1.0.fb2k-component`，SHA-256 为 `7DACE4715BCB6A8D09ECC423AA96CD919B3D365B1C8C20B395A6B889DC181E1C`，并将 Release DLL 部署到隔离 `foobar-dev`；尚未替用户启动可见验收窗口。
- 2026-07-13：用户确认歌词托管、中键与自动切换、ESLyric 原生交互、Track Details 和设置均通过；发现分隔线松手时跳到异常比例。根因为 `ReleaseCapture()` 同步触发 `WM_CAPTURECHANGED`，提前清除了拖动暂存值；现已改为先冻结最终比例再释放捕获和保存。修复后 Debug/Release 及各 4 项测试再次通过，新包 SHA-256 为 `9BE7516ADACFF9E6EBA68F22FD3BAF4532B5220166FE3B6F4A33CB6B7DFD8707`，等待用户只复验分隔线松手与重启保存。
- 2026-07-13：用户复验分隔线修复后明确确认步骤 07 通过；提交已存在为 `b88251c 集成 ESLyric 与歌曲信息切换`。用户同时指出当前封面加载观感和右上布局并非最终满意状态，批准把纯视觉、尺寸和主题协调归入步骤 15/16，不改变步骤 06/07 已验收的数据与交互边界。

## 用户检查点

- [x] 用户核准 07 的交互与字段规格。
- [x] 用户检查 ESLyric 搜索、下载、右键和双击全屏未被破坏。
- [x] 用户检查中键切换、歌曲信息、复制、Properties 和状态保存。
- [x] 用户接受缺失/失败降级及步骤 06 回归结果。
- [x] 用户明确回复验收通过。

## 人工验收

1. 关闭可见开发实例后安装最新 `dist/Refrain-0.1.0.fb2k-component`，重启 `foobar-dev`；确认日常 C 盘实例未被关闭或修改。
2. 播放一首 ESLyric 能找到歌词的本地歌曲：右下应显示 ESLyric，搜索、右键菜单、滚轮和双击全屏均仍由 ESLyric 正常工作。
3. 在右侧封面、摘要或歌词区域按中键：应在 `Lyrics` 与 `Track Details` 间切换；中键以外的歌词操作不应触发 Refrain 切换。
4. 播放下一首：自动回到 Lyrics；按 Stop 正常停止：自动转到 Track Details；暂停不切换。
5. 打开 Preferences > Display > Refrain，关闭自动切换并选择默认页面；Apply 后手动中键切换，重启确认最后页面保存。重新开启自动切换后按第 4 项工作。
6. 拖动右上摘要与右下内容之间的横向分隔线，确认范围受限、ESLyric 同步改变大小；重启确认比例保存。
7. 在 Track Details 检查普通/iTunes 自定义标签存在，歌词和 MusicBrainz 大批字段被过滤；Location、Technical、Playback Statistics 存在，ReplayGain 默认隐藏并可从设置开启。
8. 单击、Ctrl/Shift 多选详情行；用滚轮、Page Up/Down、Home/End 滚动；Ctrl+C 和右键 Copy value/Copy row/Select all 可用；Properties 打开当前可见曲目的正式属性窗口。
9. 临时从隔离开发实例移走 ESLyric（不要动 C 盘或参考目录）后启动：歌词页显示不可用提示，Track Details、右上信息和播放控制仍可用；恢复 ESLyric 后重新检查。
10. 快速切歌、停止、反复切换、调整窗口大小并关闭/reopen Refrain，确认无崩溃、旧歌词窗口、错误曲目信息或步骤 06 评分回归。
