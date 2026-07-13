# 000 项目规则与产品规格

- 状态：规格核对中
- 当前提交步骤：步骤 00 已由 `5a0836e 整理项目规格与开发路线` 完成；产品规格等待任务 001 补齐交互证据
- 前置任务：无
- 最后更新：2026-07-13

## 本任务必读

1. [`../TODO.md`](../TODO.md)：提交顺序、步骤范围和验收门槛；
2. [`../../specs/PRODUCT_SPEC.md`](../../specs/PRODUCT_SPEC.md)：唯一产品与界面总规格；
3. [`../../docs/DEVELOPMENT_SETUP.md`](../../docs/DEVELOPMENT_SETUP.md)：本机工具、SDK、实例和部署边界；
4. [`参考盘点.md`](参考盘点.md)：Foobox 只读文件证据与待观察行为。

## 目标与边界

本任务负责封闭长期协作规则、环境、参考证据、产品范围和开发路线，使后续工作不依赖聊天历史。完成前不创建工程、不写 C++、不安装外部依赖、不部署，也不把脚本推断冒充实际观察。

## 已完成

- 建立 ASCII `AGENTS.md`、严格 UTF-8 读取规则、项目总则和任务入口。
- 初始化 Git，并明确 `.local`、构建产物和第三方二进制边界。
- 纳入 foobar2000 SDK 2025-03-07 与 Columns UI SDK 8.1.0 源码和许可证；预编译二进制不进入 Git。
- 核对 Visual Studio、MSVC、Windows SDK、CMake、Ninja、两个 foobar2000 2.25.10 隔离实例和 Columns UI 3.5.0。
- 只读盘点 Foobox 参考文件，记录已确认事实与待观察项。
- 将产品与 UI/UX 规格合并为单一 [`PRODUCT_SPEC.md`](../../specs/PRODUCT_SPEC.md)，按最新用户决定消除旧结论。
- 建立编号 00–19 的项目 [`TODO`](../TODO.md)，每个标题直接作为 Fork 提交信息。

## 当前用户检查

- [x] 产品规格准确表达单一整合组件、Foobox Basic 主布局和原生 Windows 标题栏。
- [x] Album List、ESLyric、Playback Statistics 的归属和版本正确。
- [x] Biography、内嵌频谱、收音机、JS UI 宿主和自建媒体库已正确排除。
- [x] 右上两条固定信息、右下中键切换、ESLyric 双击全屏和评分规则正确。
- [x] TODO 的步骤顺序、粒度、手动检查和提交标题可以直接用于指挥开发。

## 下一步

1. 在任务 001 中解决产品规格第 11 节的观察项。
2. 用户批准最终规格后，本任务才可改为“可实现”。

## 当前环境事实

- Git `main` 当前基线：`3b9b26a 提交基础边界设定`；继续前始终重新检查实际 HEAD。
- `foobar-dev` 与 `foobar-test` 已有 foobar2000 2.25.10 x64 和 Columns UI 3.5.0。
- ESLyric 1.0.6.7 与 Playback Statistics 3.1.10 已批准但尚未安装到两个隔离实例。
- CMake 3.31.6 和 Ninja 1.12.1 已随 Visual Studio 安装；普通 PowerShell `PATH` 未包含 CMake。
- 聊天截图结构已写入产品规格，但原始位图尚无仓库路径；逐像素取证需要用户提供原图文件。

## 验证摘要

- 项目维护文本要求 UTF-8 无 BOM、LF；每次改动后严格复核。
- 参考目录 `D:\Dev\foobar2000` 本任务仅只读，没有部署或保存配置。
- 两个隔离实例中的 Columns UI DLL 哈希一致；实际版本与完整命令见开发环境说明。
- 2026-07-13 目录整理：`docs/specs/tasks` 从 10 个文件、1363 行收敛为 8 个文件、916 行；两份规格合并为一份，重复接续说明删除。
- 步骤 00 提交前审计通过：全部文件严格 UTF-8 无 BOM、LF，本地链接有效，20 个 TODO 标题唯一，旧规格/接续文件名无残留引用；`.local` 被忽略、无受禁二进制被跟踪、暂存区为空，`git diff --check` 通过。
