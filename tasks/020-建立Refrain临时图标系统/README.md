# 020-建立 Refrain 临时图标系统

- 状态：已验收（待提交）
- 对应规格：[`../../specs/modules/FOOBOX_BASIC_DEFAULT_VISUAL.md`](../../specs/modules/FOOBOX_BASIC_DEFAULT_VISUAL.md)
- 前置任务：[`../019-完成稳定性性能与生命周期验证/README.md`](../019-完成稳定性性能与生命周期验证/README.md)（已提交为 `76b8c47`）
- Fork 提交标题：`建立 Refrain 临时图标系统`
- 最后更新：2026-07-16

## 任务目标

保留当前可用的代码绘制临时图标，冻结稳定语义映射，并让七种标准 Playback Order 各有独立图形。后续若功能开发需要新的临时图标，统一以 Remix Icon 的现有语义库为来源参考；正式图标由用户在项目完成、生成组件版本后自行替换，不再作为 Refrain 1.0 路线任务。

## 明确不做

- 不购买、绘制或接入正式图标资产。
- 不把 Remix Icon 整套字体加入组件，也不新增运行时字体依赖；只在需要临时图标时选取合规语义并转换为项目原生绘制或独立资源。
- 不实现 Mini Player 或 CoverFlow，也不提前为它们制作图标。
- 不改变任何图标对应的命令、tooltip、命中区或状态行为。

## 中文程序逻辑

1. 每个图标按功能语义识别，不按按钮坐标或文件顺序识别。
2. 现有代码绘制图标继续随 Direct2D 和 DPI 缩放，不增加字体运行时依赖。
3. Playback Order 读取 foobar2000 的活动索引，映射到七种标准临时图形；未知第三方顺序使用明确的通用回退图形。
4. Playlist Workspace 与 Playback Queue 使用不同图形，避免相同形状表达不同命令。
5. 后续功能若缺临时图标，先从 Remix Icon 查找对应语义，再转换为项目可控的单个原生图形；不直接依赖本机安装字体。
6. 用户未来替换正式图标时只替换图形资源或绘制实现，不改变稳定语义映射和命令逻辑。

## 可检查步骤

### 步骤 1：现有语义与来源核对

- 输入：当前 Refrain 绘制代码、Foobox 只读参考和本机 Remix Icon 字体证据。
- 动作：盘点现有图标位置，确认 Playback Order 七种语义和临时图标来源策略。
- 产物：稳定语义清单、参考位置和许可记录。
- 用户检查方法：确认正式图标退出当前路线，临时缺图统一参考 Remix Icon。
- 通过标准：语义、来源和后续替换边界明确。
- 状态：已完成

### 步骤 2：独立临时图形接入

- 输入：七种标准 Playback Order 映射和现有底栏图标。
- 动作：实现七种不同图形，并区分 Playlist Workspace 与 Playback Queue。
- 产物：Debug/Release 可用的临时图标系统。
- 用户检查方法：切换各 Playback Order，确认图形变化且菜单和 tooltip 不变。
- 通过标准：映射独立、未知状态有回退、命令无回归。
- 状态：已完成

## 用户检查点

- [x] 用户已批准临时图标范围和 Remix Icon 来源策略。
- [x] 用户已确认正式图标退出当前路线。
- [x] 用户已要求任务 20 划勾并进入任务 21。

## 验证记录

- 2026-07-15：用户当时确认可以提供散装图标，并接受单独 SVG/透明 PNG 或带编号说明的图标表方案；此正式资产交付计划已由 2026-07-16 的新决定取消，不再阻挡 1.0 路线。
- 2026-07-16：只读检查 Foobox 参考确认，底栏图标字体为 `remixicon`，本机文件位于 `C:\Windows\Fonts\remixicon.ttf`；Foobox 在 `profile/foobox/script/js_panels/base.js` 第 12–13 行为七种标准 Playback Order 建立了字符映射。Refrain 不复制参考字体或字符轮廓，只据此确认七种语义，并使用可替换的原生矢量绘制。
- 2026-07-16：Remix Icon 官方仓库当前使用 Remix Icon License v1.0，允许作为应用功能图标使用且无需购买；本项目把它作为未来临时缺图的统一来源参考，不把整套字体作为 Refrain 运行时依赖。
- 2026-07-16：用户批准把七种 Playback Order 独立图标写入代码，并决定正式图标在项目完成、生成组件版本后由用户自行替换。Refrain 已为七种标准顺序建立稳定映射和不同原生图形；同时把此前与 Playlist Workspace 相同的 Playback Queue 图形改为独立的“列表＋播放标记”。Debug/Release 串行构建成功，两种配置各 12 项自动测试全部通过；用户明确要求任务 20 划勾并推进任务 21。

## 改动文件

- `src/playback_order_icon.h`：七种标准 Playback Order 与未知状态的稳定图标语义。
- `src/playback_panel.cpp`：按活动播放顺序绘制不同图标，并区分 Playlist Workspace 与 Playback Queue。
- `tests/playback_order_icon_tests.cpp`：七种标准映射和未知状态回退测试。
- `CMakeLists.txt`：注册新增源文件与测试目标。
- `specs/modules/FOOBOX_BASIC_DEFAULT_VISUAL.md`：记录 Playback Order 图标状态规则。

## 决策与未决问题

- Playback Order 固定语义清单为：Default、Repeat Playlist、Repeat Track、Random、Shuffle Tracks、Shuffle Albums、Shuffle Folders。
- 后续功能若缺临时图标，统一从 Remix Icon 选择语义参考并转换为项目可控的单个图形；不接入整套字体运行时依赖。
- 正式图标不再是 Refrain 1.0 路线任务，由用户在项目完成、生成组件版本后自行替换。
