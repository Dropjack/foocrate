# ESLyric 推荐默认设置规格

- 状态：规格核对中
- 版本：0.1
- 最后更新：2026-07-15
- 所属产品规格：[`../PRODUCT_SPEC.md`](../PRODUCT_SPEC.md)
- 对应路线：`22 建立 ESLyric 推荐默认设置`

## 1. 目标

让首次使用 Refrain 的用户获得一套与 Refrain 布局、主题和双语歌词方向协调的 ESLyric 面板设置，同时继续允许用户通过 ESLyric 自己的右键菜单自由修改。Refrain 不接管歌词下载、歌词源、歌词文件和 ESLyric 的全局用户数据。

## 2. 2026-07-15 调查证据

### 2.1 已确认

- [ESLyric 官方 README](https://github.com/ESLyric/release)说明面板右键菜单可以自定义显示/隐藏，按住 Shift 右键显示完整菜单，并支持自定义面板布局；官方发布仓库在调查时把 1.0.6.7 标为最新版。
- 仓库只读依赖 `third_party/columns_ui-sdk-8.1.0/base.h` 中的 `uie::extension_base` 正式提供 `set_config/get_config`、`import_config/export_config`、`have_config_popup/show_config_popup`；实例配置必须在 `create_or_transfer_window()` 之前导入。
- Refrain 当前的背景同步通过 ESLyric 运行时 `IDispatch` 接口调用 `GetAll` 与 `SetBackgroundColor`。这只更新已创建面板的背景颜色，不等于修改 ESLyric 的完整设置。
- `foobar-dev`、`foobar-test` 和只读 Foobox 参考实例的 `foo_uie_eslyric.dll.cfg` 长度与哈希都不同，证明该文件包含实例/用户状态，不能把任何一份整文件当作通用默认覆盖：

| 只读样本 | 字节数 | SHA-256 |
| --- | ---: | --- |
| `foobar-dev` | 3800 | `3B1B17668E059CCFBAF9245D8C547E962C85D81C70568AD11B97E6FFCDE25153` |
| `foobar-test` | 3520 | `D7DF8D7C4C808A1CFE2F6A118D4D272C84013A5391BFCDD276E80D1CBF62621C` |
| Foobox 只读参考 | 2738 | `2AB324FC2829D39BC73D1DC63EEA548B22896931C5AD719FA11F527AE24ABDCA` |
- ESLyric 1.0.6.7 发布仓库只提供发布物、布局和语言资源，没有公开逐字段配置结构。组件二进制可见 `RENDER_BACKGROUND_PESU_TRANSPARENT`、布局等内部名称，但字符串证据不足以授权解析私有二进制配置。

### 2.2 当前结论

技术上可以继续验证 ESLyric 是否完整实现 Columns UI 的可移植实例 `export_config/import_config`。如果验证通过，Refrain 可以保存一份由干净隔离实例导出的“不透明推荐预设”，但不能自行解析其中字段，也不能直接改写 `foo_uie_eslyric.dll.cfg`。

在完成下列三态验证前，不批准自动应用：

1. 全新 foobar2000 配置、全新 ESLyric 面板；
2. 用户已经通过右键改过 ESLyric 的面板；
3. ESLyric 从已验证版本升级后的旧实例配置。

## 3. 预定安全行为

1. Refrain 只对自己将要托管、尚未创建窗口的 ESLyric 实例使用正式实例配置接口。
2. 全新实例可以采用 `Refrain recommended`；已有非空或无法可靠判定来源的配置必须默认保留。
3. Preferences 提供明确的 `Apply Refrain ESLyric preset…` 操作和确认说明，供用户主动重新应用；这不是每次启动强制同步。
4. 用户随后从 ESLyric 右键菜单修改的结果由 ESLyric 保存，Refrain 不在下次启动覆盖。
5. 背景颜色仍由 Refrain 运行时同步，以便 Follow Windows、Refrain preset 和 Follow album artwork 即时更新；推荐预设只负责选择兼容的伪透明背景模式及其他经用户批准的显示项。
6. 导入失败、版本不兼容或 ESLyric 缺失时保持现有设置并显示恢复说明，不阻止 Refrain 其他区域工作。

## 4. 尚需确定的完整默认清单

后续隔离观察需要逐项冻结：

- 面板布局与文本对齐；
- 原文、翻译和音译行的显示规则；
- 字体、字号、行距与高亮方式；
- 伪透明背景模式；
- 无歌词、纯音乐和搜索中的显示；
- 自动搜索、保存位置与歌词源中哪些属于用户数据，必须排除在视觉预设之外；
- 全屏歌词是否共享面板预设，还是保持 ESLyric 自己的独立设置。

## 5. 未决验证

- ESLyric 实例 `export_config()` 是否产生稳定、可跨干净实例导入的非空数据。
- 导入后右键菜单修改是否能正常覆盖推荐值并跨重启保留。
- 能否可靠识别“全新实例”而不误覆盖用户已有配置。
- 1.0.6.7 之后版本拒绝旧预设时的能力检测和降级方式。

只有这些验证完成并由用户核对完整默认清单后，本规格才能标记为“可实现”。
