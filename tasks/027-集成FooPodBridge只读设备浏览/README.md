# 027 集成 FooPodBridge 只读设备浏览

- 状态：实现完成待验收（1.1.0-beta.3 的 Classic 实机只读播放单项已通过；原 beta.2 浏览范围已验收）。
- 日期：2026-09-21。
- 对应 FooPodBridge 任务 006：`D:/dev/foo/FooPodBridge/FooPodBridge/tasks/006-实现FooCrate只读Devices工作区/SPEC.md`。
- 用户已批准顶部/左栏及右下第三页签布局，并明确“就这么做了”；不重复请求同一设计批准。
- 此项为用户主动批准的跨仓库集成，不改变既有任务 023 验收结果，不代表任务 024–026 自动获批。

## 中文程序逻辑与范围

1. FooCrate 枚举公开的 FooPodBridge ABI 1 服务，再查询只读扩展；无服务不创建设备区域，旧服务明确显示不兼容。
2. 左栏保留普通 Playlists，上下分隔可拖动并用独立 GUID 保存比例；下部原生设备树显示真实设备、Library、普通/智能播放列表及歌曲总数，不创建 foobar playlist。
3. 中央使用只读虚拟列表，仅显示合同当前提供的 Title/Artist/Album；保持普通列标题式顶部，不加 Library 大标题、分类数量或分类页签。列表成员按 track ID 对应，保留保存的顺序与重复引用；不存在的成员显示错误。
4. 每个主线程消息最多投影 128 个内存条目，不在 UI 调用磁盘 I/O；提供者刷新后清空过期表，完成的新结果再次核对 token/generation/revision。
5. 右上歌曲封面/播放信息保持原处理。右下只在 iPod 浏览上下文提供 Lyrics / Track details / Device，进入时不自动选 Device；主动选择 Device 后显示概览，退出设备上下文恢复之前的歌词或歌曲信息。
6. 设备行不创建 metadb 播放句柄，不接受删除、粘贴、改名、拖入或双击播放；现有播放工具栏继续控制原播放状态。服务读取与浏览不能写设备。
7. 窗口销毁先令回调 lifetime 失效并取消订阅，再销毁子控件/字体/画刷；晚到消息不引用已经销毁的控件。服务停用隐藏设备区域。
8. 同名设备按 token 区分。已知上游重复项 DUP-001 暂缓，不靠显示名合并或隐藏第二台；本包不声称修复上游发现。

## 代码与来源

- `src/device_browser_model.h`：独立条件页签状态、曲目 ID 索引。
- `src/device_browser.h/.cpp`：公共服务消费者、可取消订阅、分批快照投影、只读原生控件和主题/DPI 同步。
- `src/playback_panel.cpp`：布局/生命周期接入、设备与普通列表命令隔离、右上播放区域保护。
- `third_party/foopodbridge-contract`、`src/foopodbridge_guids.cpp`：仅复制有 0BSD SPDX 的 ABI 1.1 公共声明与 GUID，不复制 LGPL Core 或设备数据库实现。
- `scripts/build-local.py`：使用本机 VS Build Tools 完整路径、子进程 Path 去重及关闭 MSBuild 节点复用，不修改系统 PATH。

## 构建与验证

版本预留 `1.1.0-beta.1`，旧组件不可覆盖；尚未生成新包，不能交付旧 DLL 冒充本次实现。

- 根 AGENTS/开发环境记录的 Community 路径不存在。本机有 VS 2022 Build Tools、MSVC 14.44、SDK 10.0.26100；新构建树为 `build/devices-1.1.0-beta.1`。
- 首次完整 Debug 构建在原有 SDK `shared/filedialogs_vista.cpp` 失败，错误 C1083：缺少 `atlbase.h`。未修改第三方源码或移除依赖以绕过。
- 新增和现有 FooCrate 源码以 MSBuild `ClCompile` 独立编译，Debug/Release 均通过，0 警告/错误；此项不是完整 DLL 链接成功。
- 不依赖 ATL/shared.dll 的 Debug/Release 各 14 项测试通过，包含新增 device_browser；artwork_cache 与完整组件待 ATL 补齐后验证。
- 用户允许补装 ATL，但要求说明对同时运行 Unity 项目的影响。只读发现 3 个 Unity 编辑器运行，尚未启动安装器，等待用户选定时机。安装计划只添加 Microsoft.VisualStudio.Component.VC.ATL，禁止 --force 和自动重启；若需要关闭其他项目或扩大更新范围先停止。
- 未启动/控制 foobar2000、Unity 或任何测试应用，未读写 iPod，未部署和提交/推送。自动测试不代替人工检查。

## 恢复入口与交付

用户确认可安装后，补齐 ATL，重新完成 Debug/Release 全构建及全部测试，再生成 `dist/FooCrate-1.1.0-beta.1.fb2k-component` 并审计唯一 `foo_crate.dll`。若候选已交付后再改源码，使用下一个 beta 号。

首个人工检查在 `D:/dev/foo/FooCrate/.local/foobar-test`：手动安装 FooCrate 新包与 FooPodBridge beta.4，打开左侧设备 Library，核对真实列表、右上播放不变、右下 Device 入口只在设备上下文出现且不抢歌词。一次检查一个点，等待反馈后再验证刷新/切换/移除/缺失服务和主题/DPI。

## 2026-09-21 ATL 补齐后的完整验证

用户关闭 Unity 并手动安装 ATL，已检测到 MSVC 14.44.35207/atlmfc/include/atlbase.h。此前一次自动补装被已有安装器锁阻挡，未强制关闭或重启任何进程。

FooCrate x64 Debug/Release 全构建成功，各 15/15 测试通过，包括 device_browser 与 artwork_cache。Release 仅有任务 003 已记录的第三方 SDK 五处 C4996 弃用警告，FooCrate 自有代码无新增警告。日志为 build/devices-1.1.0-beta.1/full-debug.log、full-release.log 与各配置 Testing/Temporary/LastTest.log。

候选包：D:/dev/foo/FooCrate/dist/FooCrate-1.1.0-beta.1.fb2k-component，425789 字节。已检查 ZIP CRC、仅含根目录 foo_crate.dll、包内 DLL 与 Release DLL 完全一致、PE x64、ProductVersion 1.1.0-beta.1。包 SHA-256：1246D005C19E2AB8AAFFC7B327CC037FD32D6ABF83A206820F108627C20B50A9。

首次打包已完成归档及结构检查，但最后 Get-FileHash 因嵌套 PowerShell 环境缺失命令而退出；改用 .NET SHA256，既有候选不覆盖，独立 Python 审计已通过。该修改不改变组件二进制。

未部署、启动或操作应用，未访问真实设备文件，未提交或推送。人工组件加载、布局、播放隔离、刷新/移除、服务缺失、主题/DPI 仍待用户逐项验证。第一检查点：在 foobar-test 手动安装 FooCrate 本包及 FooPodBridge 0.1.0-beta.4，点击设备 Library，反馈界面截图；核对真实曲目表与右下条件 Device 入口。DUP-001 仍暂缓。


## 2026-09-21 用户修订：隐藏页面入口

用户明确要求恢复 FooCrate 细滚动条，右下不主动显示页签，沿用鼠标中键切换。此决定覆盖此前可见 Lyrics / Track details / Device 页签设计：普通上下文保持原来的两页；iPod 上下文中键依次切换歌词、歌曲信息、设备概览，歌词不可用时跳过。进入设备浏览不自动切页，退出时仍恢复此前普通页面。无需为隐藏入口预留高度。设备树、曲目表、概览滚动条使用主题色的 3 DIP 细滑块并在空闲时隐藏，保留滚轮、拖动和轨道点击。

## beta.2：细滚动条与隐藏页面入口

用户确认 beta.1 设备内容已显示，截图指出原生粗滚动条，随后明确要求恢复细样式，右下仅用中键切换。此反馈不等同于整体任务验收。

- 新增 device_scrollbars.h：设备树、虚拟表与概览的原生滚动区域使用共享 3 DIP 滑块宽度、1000 ms 隐藏时限和主题色；保留滚轮/键盘滚动，单独处理滑块拖动、轨道翻页、捕获释放、DPI、销毁与空滚动范围。原生非客户区命中宽度保留，视觉去除粗轨道及箭头。
- 删除右下 TabControl 和 28 DIP 高度预留；中键经原歌词宿主、父面板或概览文本入口统一轮换，普通上下文仍是原两页。新增模型测试覆盖三页轮换、歌词缺失跳过、退出恢复与普通两页。
- FooCrate 1.1.0-beta.2：Debug/Release 全构建成功，各 15/15 测试通过，无新增编译警告。沿用现有构建树 build/devices-1.1.0-beta.1；日志 beta2-debug-final.log / beta2-release-final.log。包在 D:/dev/foo/FooCrate/dist/FooCrate-1.1.0-beta.2.fb2k-component，ZIP CRC、仅含 foo_crate.dll、x64、与 Release 字节一致均通过。旧 beta.1 保留。
- 没有更新 FooPodBridge 二进制，没有部署/启动应用、访问或写入设备。待用户在 foobar-test 手动更新 FooCrate，先检查 Library 界面已无可见右下页签及粗滚动条；之后再逐项检查中键轮换和滚动拖动。不能用模型测试替代实际 UI 验证。

## beta.2 人工外观检查通过

2026-09-21：用户提供 beta.2 的 Library 界面截图并明确表示满意。截图确认真实设备列表/曲目显示、滚动条细样式、右下不显示页签栏。此项外观检查通过；中键轮换、退出设备上下文恢复、滚动条拖动和其他生命周期检查尚未由本次截图验证，整体任务仍待验收。下一检查：在 foobar-test 保持设备 Library 上下文，在右下区域用中键轮换，确认能进入并离开 Device overview。

## beta.2 人工中键轮换检查通过

2026-09-21：用户在要求检查设备 Library 下右下区域中键来回切换后回复没有任何问题，并提供 Device overview 截图。确认设备上下文中键轮换通过，概览显示 Read-only、119.00 GiB 容量和 94.67 GiB 可用空间。未据此扩展为播放隔离、退出恢复、移除或服务缺失检查通过。下一检查：概览显示时点击左上普通 Library (full)，确认恢复歌曲信息/歌词，且普通上下文中键不再进入设备概览。整体任务仍待剩余人工验收。

## beta.2 人工退出设备上下文检查通过

2026-09-21：用户按上一检查要求，从显示 Device overview 的状态点击普通 Library (full)，并确认没有任何问题。退出设备浏览恢复歌曲信息/歌词、普通上下文中键不再进入 Device overview，均记录为通过。下一检查：播放测试实例中已有的普通曲目时浏览设备 Library/playlist 并切换右下概览，确认播放不中断、不换曲，右上仍显示原播放歌曲。播放隔离及其他未执行检查仍待验收。

## beta.2 人工播放隔离检查通过

2026-09-21：用户确认播放普通本地歌曲期间浏览设备 Library/playlist、切换右下设备概览的检查已经一起测试通过。记录播放不中断、不换曲、右上维持原歌曲信息。此前已通过外观、中键轮换和退出设备上下文恢复；不把此确认扩大为尚未执行的滚动条拖动、设备移除、服务缺失和主题/DPI 检查。下一检查为设备 Library 纵向细滚动条拖动，确认曲目跟随且松开后停止。

## 跨仓库验收记录同步（2026-09-21）

以下为 FooPodBridge 任务 006 的逐项人工反馈留档，历史的未同步说明已由本次同步解除。

## beta.2 人工纵向滚动条拖动检查通过

2026-09-21：用户针对设备 Library 中央列表纵向细滚动条上下拖动、曲目跟随及松开停止的检查回复没有任何问题，记录为通过。下一检查：在 foobar-test 保持设备 Library 打开，通过 Windows 安全弹出设备并断开，确认旧曲目清除、状态反映断开且普通播放列表可用。设备移除、服务缺失和主题/DPI 等未执行项仍待验证，整体任务不标记已验收。本轮记录在 FooPodBridge；FooCrate 文档尚未同步此条。

## beta.2 人工安全弹出与拔线状态检查通过

2026-09-21：用户提供两张截图，明确第一张为 Windows 安全弹出后、第二张为拔线后，并反馈有效设备项消失及时。第一张旧曲目已清除，剩余设备项显示 Not mounted / Windows has not exposed an accessible storage volume；第二张设备树显示 No iPod devices detected，中央显示 Device disconnected。此状态响应与清除失效曲目检查通过。安全弹出后仍留一项的具体来源未证明，不能据此认定 DUP-001 已修复。截图不证明普通 playlist 后续操作、重新连接恢复或服务缺失场景通过。下一检查：重新连接同一 iPod，等待读取后选择 Library，确认真实曲目及播放列表恢复，无需重启 foobar。本条仅更新 FooPodBridge 记录。

## beta.2 人工重新连接恢复检查通过

2026-09-21：用户确认重新插入同一设备后，资源管理器出现时 foobar 也及时恢复设备内容，无需重启；截图显示设备播放列表选中及其真实曲目，右上歌曲信息与右下歌词仍显示。重新连接恢复检查通过，不把用户所说的出现推断为程序主动抢焦点。已通过外观、中键轮换、退出恢复、播放隔离、纵向滚动条拖动、安全弹出/拔线及重连恢复。服务缺失、主题/DPI 等未执行项仍待验证，DUP-001 仍暂缓。下一检查优先验证 FooCrate 无 FooPodBridge 时可独立使用：仅在 foobar-test 手动移除 FooPodBridge 并重启，确认 Devices 消失且普通播放/歌词可用；不能操作日常安装。本条仅更新 FooPodBridge 记录。

## beta.2 人工服务缺失与独立使用检查通过

2026-09-21：用户针对 foobar-test 移除 FooPodBridge、保留 FooCrate 并重启后的检查明确确认没有问题。记录 Devices 区域隐藏、普通播放列表/播放/歌词正常，FooCrate 可不安装 FooPodBridge 独立使用的原则已通过人工验证。已确认的其他项目保持原记录；主题/DPI 检查仍未执行，不将整体任务提前标记已验收。下一检查：恢复 FooPodBridge beta.4 后，在测试实例使用 FooCrate 深色主题检查设备 Library、细滚动条与右下概览的可读性。本条仅更新 FooPodBridge 记录，FooCrate 文档尚待同步。

## beta.2 人工深色主题检查通过

2026-09-21：用户针对恢复 FooPodBridge beta.4 后切换 FooCrate 深色主题、检查设备 Library 文字/选中行/细滚动条可读性的要求回复没有问题。按用户报告记录深色主题检查通过；本条没有附加截图，不声称已目视核验。DPI 缩放检查仍待执行，整体任务尚未标记已验收。下一检查：若有不同缩放比例的显示器，将 foobar-test 移至该屏，检查设备列表、滚动条与概览无裁切或错位；没有该环境则明确记录未测，不要求更改系统缩放。本条仅更新 FooPodBridge 记录。

## beta.2 人工 DPI 检查与本轮验收结论

2026-09-21：用户针对不同缩放显示器下设备列表、细滚动条和右下概览无裁切/错位的检查回复没有问题，按用户报告记录 DPI 检查通过。

已逐项获用户确认：外观、隐藏入口中键轮换、退出设备上下文恢复、播放隔离、纵向滚动条拖动、安全弹出与拔线响应、重新连接恢复、无 FooPodBridge 时 FooCrate 独立使用、深色主题、DPI。任务 006 的本轮已批准只读浏览范围据此标记已验收，候选为 FooCrate 1.1.0-beta.2 与 FooPodBridge 0.1.0-beta.4。不声称所有硬件、系统主题或任意故障场景均已覆盖。

任务 005 的 DUP-001（一台设备重复显示）按用户决定继续暂缓；本结论不代表 005 全部验收、写入授权或任务 007 实施授权。未生成稳定版、未提交或推送。FooCrate 仓库当前不在可写范围，本轮后续人工记录保存在此处，其任务文档尚待同步。

## beta.3：设备只读播放（实现完成待验收）

用户要求在等待 iPod 拷贝期间先实现播放入口。本轮将设备 Library/playlist 的双击动作接到 foobar 播放：FooPodBridge 只读快照提供挂载根目录，FooCrate 将曲目相对路径解析为本地挂载文件，创建/复用专用 `FooPodBridge playback` 播放列表并执行默认播放动作。该流程只读设备文件，不执行 iPod 数据库写入、同步、删除或重命名。

- 版本：FooCrate 1.1.0-beta.3；FooPodBridge 0.1.0-beta.10。
- 自动检查：FooCrate Debug/Release 各 15/15；FooPodBridge Debug/Release 各 13/13。
- 包路径：`D:/dev/foo/FooCrate/dist/FooCrate-1.1.0-beta.3.fb2k-component` 与 `D:/dev/foo/FooPodBridge/FooPodBridge/dist/FooPodBridge-0.1.0-beta.10.fb2k-component`。
- 尚未进行用户操作验收；不得把自动测试当作实际播放成功证据。

回家后在 `foobar-test` 手动安装这两个新包，打开设备 Library，双击一首已确认存在的歌曲。通过标准：foobar 开始播放该歌曲，设备仍保持只读，原有普通播放列表没有被删除或改写；若失败，请记录设备是否仍挂载、双击的曲目标题和 foobar 状态栏/控制台错误。

## 2026-09-24 beta.3 实机只读播放反馈

用户在 `foobar-test` 接入此前那台 Classic 后反馈“能放歌了！显示了！”，并提供 FooCrate 截图。设备 Library 显示 1862 首；`Space Song` 正在播放，右上显示 Beach House / Depression Cherry，底部播放进度在前进；左侧出现含 1 首的 `FooPodBridge playback` 专用列表，原普通 Library (full) 仍显示 1703 首。记录这首曲目的设备读取和播放单项通过。截图不能单独证明普通列表内容逐项未变或设备完全无写入；尚未检查连接期间的 FooPodBridge 恢复状态。左侧仍有第二条同名 Classic 项，DUP-001 保持待查，不能以此次播放成功解除写入门禁。
