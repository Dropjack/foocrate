# Lyrics and Track Details 模块规格

- 状态：0.4 已验收
- 版本：0.4
- 最后更新：2026-07-15
- 所属产品规格：[`../PRODUCT_SPEC.md`](../PRODUCT_SPEC.md)
- 对应路线：`07 集成 ESLyric 与歌曲信息切换`
- 对应任务：[`../../tasks/007-集成ESLyric与歌曲信息切换/README.md`](../../tasks/007-集成ESLyric与歌曲信息切换/README.md)

## 1. 用户愿景

`Now Playing Panel` 右下默认显示 ESLyric。用户直接在歌词区域双击时，由 ESLyric 自己进入和退出其全屏界面；用户用鼠标中键把同一位置切换成 Refrain 原生歌曲信息，再次中键返回歌词。

Refrain 只负责稳定托管、切换和失败隔离，不复制 ESLyric 的搜索、下载、保存、解析、歌词源、右键菜单或全屏实现。

## 2. 已批准产品边界

- 右侧内部顺序固定为上部 `Now Playing Header`、下部 `Lyrics View`/`Track Details View`；二者不能拖走、浮动或交换上下关系。
- ESLyric 作为独立安装的 Columns UI 组件使用，不进入 Refrain 安装包，不由 Refrain 更新。
- Refrain 不截获 ESLyric 子窗口内的左键双击、右键、滚轮、键盘和歌词源交互。
- `Track Details View` 是只读字段名—字段值列表；编辑只调用 foobar2000 Properties，不直接写标签。
- ESLyric 缺失或创建失败不能影响右上摘要、播放控制或以后实现的播放列表。
- 本模块不实现 Biography、频谱、第二套歌词数据库或脚本 UI 宿主。

## 3. 参考行为证据

### 3.1 已观察或文件直接证明

- `foobox8 basic.fth` 把 ESLyric 放在右侧组合面板下部，参考默认上下比例为 `0.5`。
- `infoArt.js` 通过 `window.GetPanel('ESLyric')` 获得现有面板；中键切换只调用同一实例的显示/隐藏，不销毁面板。
- Foobox 的中键回调挂在整个旧右侧组合脚本上，因此封面上方和歌曲信息下方均会触发切换。
- Foobox 默认开启 `Auto.switch.esl.prop`：播放新曲显示 ESLyric；正常停止显示歌曲信息；因开始另一首而产生的过渡停止不切换。
- `properties.js` 的歌曲信息默认启用 Metadata、Location、Tech Info、Playback Statistics，默认关闭 ReplayGain；Metadata 排除歌词字段和 MusicBrainz 字段。
- 参考歌曲信息可以滚动，长值悬停显示全文；Properties 针对当前冻结曲目。

### 3.2 参考行为不自动等于 Refrain 决定

- 中键覆盖整个右侧容易在封面或五星区域误切换；用户最初描述的是让“歌词部分”切换成歌曲信息。
- 自动切换会覆盖用户刚手动选择的页面，是否保留 Foobox 默认仍需批准。
- 参考脚本的字段点击会创建自动播放列表，不属于用户提出的歌曲信息目标，不进入 Refrain。
- 参考以脚本属性保存开关，Refrain 必须使用已有版本化原生设置和迁移。

## 4. 正式托管方案

1. 启动实现前在隔离实例中枚举 Columns UI `uie::window` 服务，确认 ESLyric 的稳定扩展 GUID、名称、类型和多实例可用性；实现以 GUID 寻址，名称只用于诊断文案。
2. Refrain 为右下区域创建专用 `uie::window_host`，在主线程调用 ESLyric 的 `create_or_transfer_window()`，并保存扩展对象、子 HWND 与宿主所有权。
3. 切换页面只改变同一 ESLyric 子窗口的可见性；不在每次切换时销毁/重建，因此 ESLyric 自己的滚动、搜索和全屏状态得以保留。
4. Refrain 调整尺寸时只把核准的右下客户区传给 ESLyric；把系统颜色、时间和设置变化等 Columns UI 要求的宿主消息转发给可用子窗口。
5. Refrain 销毁或 ESLyric 转移所有权时，先断开宿主回调，再按 Columns UI 生命周期销毁/释放；任何失败都回到明确占位状态。
6. 找不到 GUID、`is_available()` 为假、创建返回空窗口或子窗口意外失效时显示 `ESLyric unavailable`、检测到的依赖状态和恢复指引；不按 DLL 路径、窗口类名或标题猜测。

### 4.1 背景色同步

- ESLyric 播放时会主动绘制自己的客户区；只给 Refrain 父窗口提供同色背景不能改变歌词面板，因此“父背景伪透明即可解决”不是完整方案。
- Refrain 按 Foobox 已验证的路径使用 ESLyric 自带 Automation 类型库：取得当前 foobar2000 进程内的 ESLyric 面板集合，并调用 `SetBackgroundColor` 同步 Refrain 当前主题背景色。
- Automation 仅在 ESLyric Advanced 中 `pref.script.expose = True` 时可用。Refrain 不直接改写 ESLyric 私有配置；接口关闭、缺失或版本不兼容时安全跳过，不影响歌词托管、搜索、全屏或 Track Details。
- 本同步只设置背景颜色，不强制改变 ESLyric 的背景类型、背景图片、字体、普通歌词颜色、高亮颜色、间距或其他私有设置。当前实现与 Foobox 一样作用于同一 foobar2000 进程内全部 ESLyric 面板。
- Refrain 不按安装路径加载 ESLyric；从已托管子窗口定位它当前实际加载的模块，再查询该模块导出的类工厂和内嵌类型接口，避免触碰其他 foobar2000 实例或磁盘配置。

### 4.2 双语歌词默认规则

- 当歌词源返回原文与译文时，Lyrics View 默认保留两者；具有相同时间戳的原文和译文由 ESLyric 按双行显示。
- 当前 ESLyric 1.0.6.7 随带的 NetEase 搜索脚本已将 `tlyric` 译文并入原歌词；KRC 解析器也已将内嵌译文写为同时间戳的下一行。这是已安装歌词源的默认行为，不需要 Refrain 再添加一个“双行”开关。
- 双语可用性依赖歌词源是否拥有对应译文；没有译文时，Refrain 不重复原文、不机器翻译，也不伪造第二行。
- ESLyric Automation 1.0.6.7 没有公开设置翻译语言或强制双语的方法。Refrain 不改写 ESLyric 私有配置，不在组件包内覆盖用户的歌词源顺序和搜索设置。

## 5. Track Details View

### 5.1 目标与刷新

- 与步骤 06 使用同一目标规则：播放/暂停时冻结 Now Playing；完全停止后使用活动播放列表焦点；无目标时显示 `Nothing selected`。
- 元数据或动态播放信息改变时重建当前目标的只读快照；绘制、滚动和复制不执行文件 I/O。
- 打开 Properties 前再次核对当前可见快照仍对应冻结句柄，避免焦点或切歌回调把操作投递给另一首。

### 5.2 推荐字段与顺序

默认按 Foobox 用户习惯提供以下分区：

1. `Metadata`：文件中的普通元数据，保留 iTunes 等自定义字段；排除内嵌歌词、MusicBrainz 大批标识和二进制图片字段。
2. `Location`：File name、Folder、File path、Subsong、File size、Last modified。
3. `Technical`：Duration 及 foobar2000 `file_info` 暴露的技术字段。
4. `Playback Statistics`：Played、First played、Last played、Added、Rating；依赖缺失时整个分区明确不可用而不是伪造空值。
5. `ReplayGain`：Album/Track gain 与 peak；默认隐藏，可在 Refrain 设置中开启。

空字段不生成无意义行；分区顺序固定，字段名与值分别可读。歌词全文不进入歌曲信息，避免巨大文本破坏滚动和复制。

### 5.3 交互

- 单击行选择；Ctrl/Shift 支持多选；方向键、Page Up/Down、Home/End 与滚轮滚动。
- `Ctrl+C` 复制所选行的 `Field: Value`；右键提供 Copy value、Copy field and value、Select all、Properties。
- 长值保持单行并横向裁切，悬停显示完整值；不把点击字段值改造成自动播放列表查询。
- `Enter` 或右键 Properties 打开 foobar2000 正式 Properties；没有目标时禁用。

## 6. 已批准的切换与持久化规则

- 中键在整个右侧区域切换。
- 自动切换默认开启：新曲强制 Lyrics，正常停止强制 Track Details；提供设置关闭。
- 关闭自动切换后才保存并恢复最后手动页面。
- Preferences 增加 `Lyrics and track details` 设置组：自动切换、默认页面、ReplayGain 分区；不增加另一套歌词设置。
- ESLyric 双击全屏、右键菜单、歌曲信息字段和缺失降级遵循本规格其他章节。

## 7. 尺寸与边界

- 步骤 07 使用当前右栏宽度，右下从 `Now Playing Header` 底边延伸到底部播放控制上边；不会占用中央或左侧未来区域。
- 右侧上下边界在本步骤实现拖动并保存；参考 `0.5` 作为当前默认值，步骤 15 仍可调整视觉默认，但升级不得覆盖用户保存值。
- 固定结构不允许拖走面板。是否在本步骤提前实现可保存的右侧上下分隔拖动，随切换方案一并核准；若延期，步骤 15 必须补齐。

## 8. 失败与生命周期验收

- ESLyric 已安装：创建一次、切换不重建、窗口调整正确、双击全屏和右键功能保持原样。
- ESLyric 缺失/不可用：右下显示明确提示，Track Details 仍可切换使用，其他区域无回归。
- 快速切歌、暂停、正常停止、重建 Refrain、切换主题/字体和关闭 foobar2000 时无悬挂窗口、旧目标或崩溃。
- 配置损坏或旧版本没有新增字段时回到安全默认，并通过现有设置版本迁移。

## 9. 用户核准结果

1. 采用 Foobox 兼容方案 F：整个右侧中键切换，默认自动切换。
2. 接受第 5.2 节的 Foobox 式默认字段分区与 ReplayGain 默认关闭。
3. 右侧上下边界在步骤 07 实现拖动并保存。

## 10. 证据来源

- 只读本地参考：`D:\Dev\foobar2000\themes\foobox8 basic.fth`。
- 只读本地参考：`D:\Dev\foobar2000\profile\foobox\script\js_panels\infoArt.js`、`properties.js`。
- 本地正式 SDK：`third_party/columns_ui-sdk-8.1.0/window.h`、`window_host.h`。
- ESLyric 作者发布页：`https://github.com/ESLyric/release/releases/tag/1.0.6.7`。

## 11. 决策记录

- 2026-07-13：用户明确开始步骤 07；步骤 06 已提交并验收。
- 2026-07-13：确认正式 Columns UI 子窗口托管技术路径可行；未发现 ESLyric 提供供第三方链接的专用 C++ SDK，因此不依赖其私有 ActiveX 接口。
- 2026-07-13：用户批准方案 F、推荐字段分区，并要求本步骤立即实现可保存的右侧上下边界；规格升至 0.2 并进入实现。
- 2026-07-13：按批准规格完成首轮实现；自动构建与模型测试通过，ESLyric 实际窗口行为、复制/Properties 和缺失降级仍等待用户在可见隔离实例中验收。
- 2026-07-13：用户完成可见隔离实例验收；分隔线松手比例问题修复并复验后，步骤 07 正式通过。
- 2026-07-14：任务 15 检查发现有歌曲时 ESLyric 主动绘制白色背景，无歌曲时的同色区域只是 Refrain 占位。只读 Foobox 证据确认其通过 ESLyric Automation `GetAll()` / `SetBackgroundColor()` 同步颜色；用户在开发实例将 `pref.script.expose` 设为 `True`，批准采用同一路径。规格升至 0.3。
