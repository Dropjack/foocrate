# 002 配置隔离测试依赖

- 状态：已验收
- TODO 步骤：`02 配置隔离测试依赖`
- Fork 提交标题：`配置隔离测试依赖`
- 前置提交：`9a32c60 完成 Foobox Basic 交互取证`
- 对应规格：[`../../specs/PRODUCT_SPEC.md`](../../specs/PRODUCT_SPEC.md)
- 环境记录：[`../../docs/DEVELOPMENT_SETUP.md`](../../docs/DEVELOPMENT_SETUP.md)
- 最后更新：2026-07-13

## 任务目标

让 `foobar-dev` 与 `foobar-test` 拥有完全一致、来源可追溯的 Columns UI、ESLyric 和 Playback Statistics 运行依赖，并明确开发环境与最终日常使用之间的边界。完成后，本轮开发可以安全验证依赖；正式安装后的 Refrain 不依赖 D 盘项目继续存在，也不要求用户追随第三方每次更新。

## 明确不做

- 不创建 CMake 工程或编写 Refrain C++。
- 不修改 `D:\Dev\foobar2000` 或 C 盘日常 foobar2000。
- 不把 ESLyric、Playback Statistics 或 Columns UI 二进制加入仓库或 Refrain 安装包。
- 不由 Refrain 静默联网、下载或自动替换第三方组件。
- 不因为上游出现新版本就未经回归自动宣称兼容。

## 中文程序逻辑

本任务没有 Refrain 运行代码，建立的是后续实现必须遵守的依赖逻辑：

1. 开发环境从两个隔离实例的真实 DLL 和 foobar2000 Components 元数据读取已安装状态。
2. 仓库只保存组件名称、版本、来源、哈希和验证结果，不保存第三方安装包或 DLL。
3. Refrain 后续运行时查找歌词面板、评分命令和 `%rating%` 数据能力，不把“文件版本必须等于某个字符串”作为唯一判断。
4. 需要的能力存在时正常启用；组件缺失或接口行为不兼容时，只禁用对应区域并提示用户，不影响播放列表、封面和播放控制。
5. 上游发布更新本身不触发 Refrain 开发、测试或重新打包；用户可以继续使用原有稳定组合。
6. 用户主动更新后，只要所需能力仍存在，已安装的 Refrain 直接继续工作；若发生真实故障，再恢复开发环境复现和修复。
7. 只有准备发布新的 Refrain，或处理已发生的兼容故障时，才在 `foobar-dev` 做开发检查、在 `foobar-test` 做回归并更新验证记录。
8. 正式用户通过 foobar2000 或组件作者的发布渠道自行选择是否更新依赖，Refrain 只显示检测结果和恢复提示。

## 组件更新策略

| 组件 | 谁发布和更新 | 用户如何检查更新 | Refrain 的责任 |
| --- | --- | --- | --- |
| Columns UI | [foobar2000 官方组件仓库](https://www.foobar2000.org/components/view/foo_ui_columns) | `Help > Check for updated components`；只采用用户主动选择的稳定版 | 作为必要 UI 宿主检查可用接口；不要求追随测试版 |
| Playback Statistics | [foobar2000 官方组件仓库](https://www.foobar2000.org/components/view/foo_playcount) | `Help > Check for updated components` 或 Preferences > Components | 检查评分命令和 `%rating%` 能力，维护已测试版本 |
| ESLyric | [ESLyric 作者的 GitHub Releases](https://github.com/ESLyric/release/releases/latest) | 从作者 Releases 下载 `.fb2k-component` 后通过 Preferences > Components 安装 | 检查 Columns UI 面板是否存在并可托管，维护已测试版本 |

Refrain 不需要“保持三个依赖永远最新”。正确目标是稳定兼容：一个已经正常工作的组合可以一直使用；上游更新只是可选项，不是 Refrain 的重新发布事件。

Refrain 自己提供的帮助是 Preferences 中的只读 `Dependencies` 状态区：显示当前检测版本和真实失败原因，并提供正确更新入口。它不在后台检查 GitHub、不下载 DLL、不绕过 foobar2000 的组件安装确认，也不因为存在未验证的新版本就打扰用户。

`foobar-dev`、`foobar-test`、源代码和 SDK 只服务于开发。正式 `.fb2k-component` 安装到日常实例后，它们可以归档或删除；未来确实需要修改时再从 Git 远端或源码备份恢复。

## 来源、许可与再分发结论

| 组件 | 来源与许可证据 | Refrain 发布决定 |
| --- | --- | --- |
| Columns UI | foobar2000 官方组件页明确标为开源，并链接作者源码与发布记录 | 仍由用户独立安装；Refrain 不重复打包其运行时二进制 |
| Playback Statistics | foobar2000 官方组件仓库提供作者发布物，但组件页没有授予 Refrain 再分发二进制的明确许可 | 不捆绑，只链接官方组件页 |
| ESLyric | 作者 GitHub release 仓库提供发布物；仓库首页未列出 LICENSE 文件或明确的二进制再分发许可 | 不捆绑，只链接作者 Releases |

“可以从官方页面下载”不等于“Refrain 有权再次打包发布”。当前三者全部独立安装，因此许可不确定性不会阻挡开发，也不会污染 Refrain 包；若未来希望合包，必须重新取得并记录明确授权。

## 当前安装证据

| 实例 | Columns UI | ESLyric | Playback Statistics | 结论 |
| --- | --- | --- | --- | --- |
| `foobar-dev` | 3.5.0 stable，SHA-256 `CE0AC312C25F1685C0571F567D5D56E961957DF7DA4333B349B62B1DB7C5D368` | 1.0.6.7，SHA-256 `E03FCDC635787610AFFF2406F8B8F14E2BF5BA3002C222DBB0A5C7A27AF07285` | 3.1.10，SHA-256 `4674FC4013B45DB3A004F71A61E449ED08222B46223269DFA71F847934D59F19` | 文件已安装 |
| `foobar-test` | 3.5.0 stable，同上 | 1.0.6.7，同上 | 3.1.10，同上 | 文件已安装且与 dev 一致 |

截至 2026-07-13，上游页面仍将 ESLyric 1.0.6.7 与 Playback Statistics 3.1.10 标为最新版；Columns UI 3.5.0 是当前稳定版，3.6.0-beta.1 是测试版，不构成升级要求。这些结论是会变化的环境事实，不写成永久产品限制。

## 可检查步骤

### 步骤 1：核对文件、版本与两个实例一致性

- 输入：两个隔离实例现有组件目录。
- 动作：读取 DLL 版本/组件元数据，计算 SHA-256，比较 dev/test。
- 产物：本任务“当前安装证据”和开发环境依赖表。
- 用户检查方法：无需手动查看文件；确认两个实例均能启动即可。
- 通过标准：两个实例组件名称、架构、版本与哈希一致。
- 状态：已完成

### 步骤 2：核对来源、更新与再分发边界

- 输入：foobar2000 官方组件页、ESLyric 作者 Releases 和已批准产品边界。
- 动作：记录权威更新渠道、Refrain 不捆绑原则、升级顺序和兼容判定。
- 产物：本任务更新策略、产品规格和开发环境说明。
- 用户检查方法：确认接受“外部更新完全可选，Refrain 正常使用不依赖 D 盘开发环境；只有真实故障或发布新版时才恢复测试”。
- 通过标准：来源和运行边界明确；第三方更新不会自动制造 Refrain 的开发、测试或重新打包义务。
- 状态：已完成

### 步骤 3：完成隔离实例人工验收

- 输入：已安装组件的 `foobar-dev` 与 `foobar-test`。
- 动作：分别启动，检查 Components 版本、ESLyric 面板、双击全屏、Playback Statistics 评分命令和正常关闭。
- 产物：人工验收记录。
- 用户检查方法：按下一节操作并反馈结果。
- 通过标准：两个实例行为一致，无启动/关闭异常；参考与日常安装未改变。
- 状态：已完成

## 用户人工验收

对 `foobar-dev` 和 `foobar-test` 分别执行一次：

1. 启动实例，打开 `File > Preferences > Components`，确认存在 ESLyric 1.0.6.7 和 Playback Statistics 3.1.10。
2. 在 Columns UI 布局中添加或打开 ESLyric 面板，确认面板能显示；播放测试音频后双击歌词区域，确认能进入并退出全屏。
3. 选择测试曲目，确认右键菜单存在 `Playback Statistics > Rating` 的 1–5 星及取消评分命令；设为任意星级后确认 `%rating%` 可见，再取消评分。
4. 正常关闭实例，确认没有错误提示。
5. 确认这次操作只发生在 `.local/foobar-dev` 和 `.local/foobar-test`，没有操作参考目录或 C 盘日常安装。

## 用户检查点

- [x] 用户接受第三方更新可选、Refrain 不随每次更新重发、D 盘工程不是运行依赖的边界。
- [x] 两个实例的 Components 页面版本正确。
- [x] 两个实例的 ESLyric 面板和双击全屏正常。
- [x] 两个实例的评分与取消评分正常。
- [x] 两个实例正常关闭，日常与参考安装未改变。

## 验证记录

- 2026-07-13：严格读取 DLL，确认 dev/test 均安装 ESLyric 1.0.6.7 和 Playback Statistics 3.1.10。
- 2026-07-13：SHA-256 表明同一组件在两个实例完全一致。
- 2026-07-13：核对上游发布页；两者当前均无高于已安装版本的稳定版。
- 2026-07-13：两个实例均已生成 ESLyric 配置文件，证明组件至少被 foobar2000 加载并初始化过；该文件证据不能替代面板、全屏和评分菜单的可见验收。
- 2026-07-13：完成来源与再分发审计；Playback Statistics 与 ESLyric 未找到允许 Refrain 合包的明确授权，因此保持独立安装。
- 2026-07-13：用户明确确认两个实例的面板、全屏、评分、取消评分和正常关闭测试完毕，并提交 `3f5fe39 配置隔离测试依赖-完成`。

## 改动文件

- `tasks/002-配置隔离测试依赖/README.md`：任务过程、版本证据、更新策略和人工验收。
- `docs/DEVELOPMENT_SETUP.md`：本机实际安装状态与可重复升级流程。
- `specs/PRODUCT_SPEC.md`：将固定版本号改为已验证基线，规定 Refrain 的兼容责任。
- `tasks/README.md`：把当前入口切换到任务 002，并记录 000/001 已验收。

## 决策与未决问题

- 2026-07-13：第三方组件由其原作者/foobar2000 更新；是否更新由用户决定，Refrain 不捆绑、不催促、不随每次更新重新发布。
- 2026-07-13：版本号高于基线不自动拒绝；能力正常就继续使用，只有真实故障或 Refrain 新版发布才恢复隔离回归。
- 2026-07-13：正式安装不依赖 D 盘项目、SDK 或两个隔离实例；项目可在完成并备份后从本机删除。
- 待用户完成两个实例的可见行为验收。
