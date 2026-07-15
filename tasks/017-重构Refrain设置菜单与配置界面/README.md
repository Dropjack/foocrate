# 017-重构 Refrain 设置菜单与配置界面

- 状态：已验收
- 对应规格：[`../../specs/modules/SETTINGS_INFORMATION_ARCHITECTURE.md`](../../specs/modules/SETTINGS_INFORMATION_ARCHITECTURE.md) 0.4（已批准）
- 前置任务：[`../016-实现深浅主题与强调色系统/README.md`](../016-实现深浅主题与强调色系统/README.md)（已提交为 `bfcd5a4 实现深浅主题与强调色系统`）
- Fork 提交标题：`重构 Refrain 设置菜单与配置界面`
- 最后更新：2026-07-15

## 任务目标

单独重新审视 Refrain 的全部设置入口和设置页，不把它当作 DPI 修补。用户可以逐项决定哪些设置保留、新增、减少、合并或改名，再共同确定导航、分区、操作按钮和编辑区域布局。最终页面在常用窗口尺寸和 100%/125%/150% 缩放下都能完整阅读和操作，并保持既有设置数据、Apply/Cancel/Reset 和主题预览语义。

## 当前截图证据

2026-07-15 用户提供当前 Playlist View 设置页截图（574×538），已观察到：

- `Grouping` 与 `Columns` 使用固定左右分栏，可用宽度不足；
- `Collapse groups by default` 文案被右栏遮挡；
- Group key、Sort、Title、Subtitle 等标题格式只能看到开头，无法判断完整内容；
- Columns 清单同时出现纵向和横向滚动，下面的属性编辑区仍然狭窄；
- 分组模板操作只有 Copy/Delete，新增、减少、排序和危险操作层级需要重新核对；
- 列的显隐、选择、属性编辑和宽度设置挤在同一区域，缺少清楚的主次关系。

原图来自本轮对话临时附件；任务 17 正式启动时应把用户批准的“当前状态”和“目标布局”截图一并归档到任务目录。当前只记录问题，不根据一张截图猜定最终布局。

## 明确不做

- 不在任务 16 验收期间顺带修改设置页，避免主题功能与设置重构混成一次无法独立验收的改动。
- 不通过仅增大 Preferences 默认窗口解决裁切；必须建立最小尺寸、响应式重排和必要的滚动策略。
- 不删除稳定 GUID 或静默丢弃旧配置；任何设置删除、合并或语义改变都要先批准迁移规则。
- 不新增没有真实行为的设置项、占位按钮或第二套保存机制。
- 不把系统级屏幕阅读器和完整键盘无障碍验证提前吞并；任务 17 完成本页结构和基础键盘顺序，任务 18 做全产品系统级验收。

## 中文程序逻辑

1. 程序从现有主 Refrain、Playlist View 和 Playlist Browser Preferences 页面建立唯一设置项清单，记录字段身份、默认值、当前入口、依赖关系和保存位置。
2. 用户逐项决定保留、新增、删除、合并和改名后，规格为每个设置确定所属页面、分区、编辑控件、启用条件和帮助说明。
3. 页面打开时读取已保存值形成草稿；跨分区导航只切换可见编辑区域，不丢失草稿，也不提前持久化。
4. 新增、复制、删除、排序分组或列时只修改草稿。删除内置或正在使用对象时按批准规则禁止或确认，取消后恢复原状态。
5. 长标题格式使用足够宽的编辑区或独立编辑方式，输入错误在原处显示，不让无效格式覆盖已应用配置。
6. 页面根据当前客户区和 DPI 计算单栏、双栏或可滚动布局；标签、输入框和按钮共用布局模型，绘制位置与键盘 Tab 顺序同步。
7. Apply 校验并保存草稿，Cancel/关闭丢弃未应用修改，Reset page 恢复当前页面批准的默认草稿；主题即时预览仍按任务 16 的独立预览层恢复。
8. 升级读取旧 GUID 和旧序列化配置，迁移到新结构；未知未来值和局部损坏只回退受影响项，不重置其他设置。

## 可检查步骤

### 步骤 1：盘点全部设置与当前问题

- 输入：三个现有 Preferences 页面、设置 GUID/序列化模型、用户截图和实际开发实例。
- 动作：列出每个设置的名称、用途、入口、默认值、依赖、保留/新增/减少候选和当前布局问题。
- 产物：用户可以逐项批注的单一设置矩阵。
- 用户检查方法：逐行决定保留、新增、减少、合并、改名或移动。
- 通过标准：没有设置项只存在于代码而未进入矩阵，也没有无来源的新设置。
- 状态：已完成

### 步骤 2：核对信息架构与线框

- 输入：获批设置矩阵和常用 Preferences 可用尺寸。
- 动作：提出页面导航、单/双栏断点、分组/列管理和长格式编辑线框。
- 产物：目标布局截图或线框、交互规格和迁移决定。
- 用户检查方法：核对添加、删除、排序、编辑、取消和最小窗口流程。
- 通过标准：用户明确批准后才进入代码实现。
- 状态：已完成

### 步骤 3：实现完整设置重构

- 输入：获批规格、稳定设置身份和现有配置。
- 动作：实现响应式布局、编辑行为、校验、保存、取消、恢复默认和迁移。
- 产物：三个入口一致、无裁切且功能完整的设置体验。
- 用户检查方法：按设置矩阵逐项操作并比较应用前后状态。
- 通过标准：没有占位控件、丢配置或只完成一半的增删流程。
- 状态：已完成

### 步骤 4：构建、部署与多尺寸验收

- 输入：完整实现。
- 动作：Debug/Release、自动测试、配置迁移测试并部署到 `foobar-dev`。
- 产物：人工验收版本。
- 用户检查方法：在 100%/125%/150% 和最小/常用尺寸检查全部页面与操作。
- 通过标准：自动验证通过且用户明确验收。
- 状态：已完成

## 用户检查点

- [x] 用户已完成全部设置项的保留、新增、减少、合并和改名决定。
- [x] 用户已批准设置导航、分区和响应式布局线框。
- [x] 用户已批准分组/列的新增、删除、排序和长格式编辑行为。
- [x] 用户已按多尺寸人工验收步骤确认结果。

## 验证记录

- 2026-07-15：路线审计确认任务 05 只建立设置入口和迁移基础，任务 09 实现当前分组/列配置，原任务 17 只做系统级 DPI/无障碍；没有独立环节能覆盖设置项新增、减少和整体布局重构。
- 2026-07-15：用户要求把设置菜单作为独立任务处理；新增任务 17，原 DPI/无障碍、稳定性、打包和发布顺延为任务 18–21。任务 16 状态保持实现完成待验收。
- 2026-07-15：确认任务 16 已提交为 `bfcd5a4 实现深浅主题与强调色系统`，工作树干净，任务 17 正式进入规格核对。用户已重置 `D:\Dev\foobar2000` 的 Foobox 范例，允许继续只读取证。
- 2026-07-15：只读盘点 Foobox 主设置五页、右侧 InfoArt/Properties、Album Cover Browser、Playlist Manager 和 Search 的持久属性及菜单选项。确认 Foobox 同时暴露产品行为与脚本内部参数，不能逐项照搬。
- 2026-07-15：创建设置规格 0.1，完整记录 Foobox 设置、Refrain 已有设置和建议处理。A 级建议包括响应式 Playlist View、完整 Group/Column 管理、详情区段、右栏跟随对象、列表密度、激活动作、封面 Fit/Fill 和专辑网格密度；Layout profiles、多封面轮播和顶部摘要格式等待用户决定。
- 2026-07-15：用户批准 Group/Column 的完整管理操作：New、Duplicate、Move up、Move down、Delete；内置项保持不可删除，用户项删除前明确确认。
- 2026-07-15：用户批准长标题格式大编辑区与原位错误；Track Details 五类区段显隐；Activate track 先实现后体验决定；Album tile size 三档由实现确定；Front/Back/Disc/Artist 多封面轮播必须加入。
- 2026-07-15：用户最初冻结全局封面规则为等比 Fill；随后在 Playlist View 实机校准时明确把组封面改为完整等比显示。当前规则为 Album Browser/右侧大封面 Fill，Playlist View Cover 完整显示；全部绝不拉伸且不暴露开关。
- 2026-07-15：曲目行布局暂定 Compact / Standard；若实现 Foobox 式 Secondary display format，则第三档改为 Two-line。右栏跟随和响应式重排等待解释后决定；分组展开时 2 DIP 分隔线与当前详细组头作为候选，不增加任意高度数值。
- 2026-07-15：用户批准 `Track row layout: Compact / Standard / Two-line`，Two-line 的默认附加字段与 Foobox 一致并由 Refrain C++ 独立实现；批准 Playlist View 按宽度响应式重排；批准 Right panel follows 的完整行为并固定动态主题继续跟随播放项。
- 2026-07-15：用户用已归档的 `tasks/001-Foobox-Basic交互取证/screenshots/compact expanded分割线预览.jpg` 明确 Group header 两档：Detailed 展开/折叠均用当前 26 DIP 详细组头；Compact line 展开/折叠均以组文字开头、右字段结尾，中间绘制 2 DIP 横线，同时保留正常命中高度。
- 2026-07-15：用户批准多封面轮播的行为和设置：Front/Back/Disc/Artist、默认开启、20 秒、缺失跳过；主题固定从 Front 或第一个可用来源取色。用户确认此前提出的七项说明已经全部回答。
- 2026-07-15：用户批准完整 Layout profiles 与每播放列表分配；批准在 Advanced 区编辑 Now Playing summary format；批准 Album Browser 始终显示专辑总数而不增加开关。设置功能清单至此闭合，进入页面树与两层 profile 模型核对。
- 2026-07-15：用户批准五页 Preferences 树、Playlist View 三标签页和“全局定义库 + profile 引用”两层模型，并授权开始实现；首轮先完成功能，随后依据用户截图校准超出页面部分的比例和尺寸。
- 2026-07-15：完成首轮实现：设置版本升级到 6，Playlist View 配置升级为可迁移的 RPV2；Preferences 树拆为 Refrain、Now Playing & Details、Playlist View、Browsers、Dependencies 五页，Playlist View 提供 Layout profiles / Groups / Columns 三标签页及完整增删、复制、排序、确认删除和原位格式错误。
- 2026-07-15：运行行为已接入 Play / Add to queue、右栏跟随、五类 Track Details 显隐、Compact / Standard / Two-line、Detailed / Compact line、每播放列表 profile、三档专辑网格、专辑总数、多封面轮播与可编辑摘要格式；封面均保持比例，Playlist View 的完整显示例外由后续实机校准补充，播放中主题取色不随右栏选择改变。
- 2026-07-15：Debug 与 Release 均构建成功；两种配置下 11/11 自动测试通过。Release DLL 已部署到 `.local/foobar-dev`，等待用户提供设置页溢出截图后校准比例和尺寸。
- 2026-07-15：按实机反馈取消独立 `Browsers` Preferences 子页，并取消 Playlist View 的独立 Columns 标签页；`Default playlist` 与 `Album tile size` 改放到 Cover Browser 正式命名的 `Album Source Selector` 行最右侧，以固定宽度连续下拉栏直接显示和修改当前值。
- 2026-07-15：用户截图确认 Compact line 的横线会穿过长标题和右侧 Genre/Year。只读核对 Foobox `WSHplaylist.js` 后确认其核心是先测量左右字段再把横线填入中间剩余空间；Refrain 移除“字符数 × 7、最多 300 DIP”和“右栏固定 82 DIP”两项估算，改用 DirectWrite 精确测量、固定两端留白、空间不足不画线，并把左附加字段改为次要文字色。
- 2026-07-15：按用户首轮观感把 Compact line 横线改为严格 2 DIP 实心矩形；专辑名由 11 增至 13 DIP，专辑表演者由 11 增至 12 DIP，右侧辅助字段保持 11 DIP。若后续层级过强，用户预定回退方案为专辑名 12 DIP、表演者 11.5 DIP。
- 2026-07-15：用户确认 DirectWrite 使用 DIP 字号后重新校准视觉层级：Compact line 专辑名增至 17 DIP、专辑表演者增至 13 DIP，横线减至严格 1 DIP，右侧辅助字段仍为 11 DIP，使专辑名而非横线成为视觉中心。
- 2026-07-15：用户把 Playlist View Cover 明确为固定宽度的独立合并区域：右侧数据区的组头、曲目、焦点和拖放横线不得进入 Cover；两区使用贯穿表头与列表体的纵向分隔线。Playlist View Cover 同时成为全局裁切规则的明确例外，使用完整等比显示、居中和主题底色留白，不能裁切或拉伸。
- 2026-07-15：用户实机截图确认 Cover 目标矩形的 Surface 底色与列表背景形成色块，并要求去掉右侧边框。实现取消 Cover 目标矩形填色，让留白直接透出 Playlist View 背景；同时移除表头和列表体的 Cover 右侧纵向线，保留所有横线只从右侧数据区开始的边界。
- 2026-07-15：用户第二轮截图要求进一步模仿 Foobox：专辑名和专辑表演者所在组头恢复从列表最左侧开始；Cover 仍从第一首曲目行开始，但完整等比图像由槽位垂直居中改为贴顶，使封面上沿与第一首曲目主文字处于同一视觉高度。
- 2026-07-15：用户第三轮截图要求正常窗口下尽可能完整显示 Playlist View 专辑标题。实现修正 DirectWrite 测量框缺少字形边缘余量导致的错误省略，并把组头空间优先级固定为“专辑标题最高、表演者最多约 22%、右侧字段最多约 18%、横线最低”；整体页面真正缩窄时才逐级裁切。
- 2026-07-15：用户开始校准 Now Playing Panel Artwork View：右键可在 Front Cover / Back Cover / Disc / Artist 间手动切换，缺失项禁用；`Pin current artwork` 暂停轮播，取消后恢复设置中的开关、来源和默认 20 秒间隔。后台改为探测全部来源，自动轮播仍只使用设置勾选项。Track Title 行同时锁定为歌名/文件名回退，原 Advanced 两行摘要兼容迁移为只控制第二行 Artist/Album。
- 2026-07-15：用户验收清单第 1、2 项仍不通过：Now Playing 的 Playback statistics 越界，Groups 单行过厚；Layout profiles 使用下拉框导致排序结果不可见，删除与页面下滚冲突。规格升级到 0.4：详情区段改为 3+2 两行；Groups 使用更紧凑的 34 DIP 行节距；Profile 改为最多显示 5 行、可独立滚轮滚动的真实列表，并增加独立名称编辑框；整个设置页在高度不足时仍可外层滚动。Group 系统行为问题另行复现，本轮不猜测修复。
- 2026-07-15：完成上述界面收敛。Debug/Release 完整构建成功，两种配置各 11/11 自动测试通过；最新 Release 包写入 `dist/Refrain-0.1.0.fb2k-component`，SHA-256 为 `F40B95F33E2AF19CC41037C284C9F1E7E25CF3EF04254180B819E8D67A46E727`。等待用户实机检查两层滚动、五行 Profile 列表和紧凑 Groups 页面；Group 系统行为仍按用户决定留待后续单独复现。
- 2026-07-15：用户完成最终实机检查，确认越界、Profile 五行列表与双层滚动、Move/Delete、Groups 紧凑布局及本任务其余功能均无问题，并明确宣布任务 17 验收通过。任务状态划为已验收，等待用户以 `重构 Refrain 设置菜单与配置界面` 提交 Fork。

## 改动文件

- `tasks/017-重构Refrain设置菜单与配置界面/README.md`：新增任务范围、问题证据和未来核对步骤。
- `specs/modules/SETTINGS_INFORMATION_ARCHITECTURE.md`：Foobox/Refrain 设置矩阵、推荐范围和页面信息架构草案。
- `tasks/README.md`：加入任务 17 入口。
- `tasks/TODO.md`：插入任务 17 并顺延系统级质量与发布路线。
- `src/now_playing_preferences.cpp`、`src/dependencies_preferences.cpp`：新增独立设置子页。
- `src/playlist_config_model.h`、`src/settings_model.h`：新增 RPV2 profiles 与设置版本 6 迁移。
- `src/playback_panel.cpp`、`src/artwork_loader.*`：接入获批的列表、详情、封面和右栏运行行为。

## 决策与未决问题

设置清单、精简后的 Preferences 导航、Cover Browser 内联选择栏、响应式编辑框架、Layout profiles 数据边界及全部运行行为均已批准。继续依据实机截图做视觉校准。
