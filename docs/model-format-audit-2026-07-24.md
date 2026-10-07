# Tauri / Bongo-Cat-Mver 模型格式审计

日期：2026-07-24
原样结构支持更新：2026-07-25
产品：BongoCat 0.1.0

## 结论

BongoCat 现在以同一条事务式导入流水线支持三类输入：

- Tauri/普通 Live2D v3 模型目录。
- 带 `config.json` 与 `img/<mode>` 的完整 Bongo-Cat-Mver 模型包。
- 只有图片差异的 Mver patch 包，例如“Z-无按键显示”和“Z-有按键显示”。
- 放在可执行文件同目录（或其一级子目录）的完整 Mver 原生目录。

真实 Tauri、完整 Mver 和两个 Z patch 均会安装 `standard`、`keyboard`、
`gamepad` 三个模型，并逐个通过 Cubism 原生运行时加载。导入后的 `payload` 是用户模型目录的
逐文件原样副本，BongoCat 生成的预览、输入贴图、媒体和元数据只写入并列的 `adapter` 目录；不会向
用户源目录或 `payload` 增删、改名、合成或覆盖文件。无法安全跨机器迁移的固定画布坐标、
工作区坐标和设备精灵几何会写入
`.bongo-cat-import-report.json`，不再被静默忽略。

项目尚未发布，因此用户数据目录中的自定义模型不提供旧统一目录布局兼容或自动迁移；没有当前
版本旁路描述文件的目录仍会被扫描器忽略。作为明确的生态兼容入口，可执行文件旁的完整 Mver
目录会直接作为只读源加载，BongoCat 只在用户数据的 `portable-mver` 中缓存合成图片、行为元数据和
兼容报告，不复制、重命名或修改原目录。内置模型仍由只读的预设扫描路径直接加载。

## 格式契约

| 类型 | 必要结构 | 结果 |
|---|---|---|
| Tauri / Live2D | 唯一合法 `.model3.json` 及其必需引用 | 导入一个或多个模型 |
| 完整 Mver | `config.json`、`img`、合法 mode 配置、编号资源和 Live2D manifest | 每个合法 mode 导入一个模型 |
| Mver patch | 唯一 patch `img` 根及唯一同名完整基础包 | 分别原样保存 base/patch，由 adapter 按文件覆盖语义解析 |
| 运行目录 Mver | 可执行文件旁的完整 Mver 根，或包含完整包/图片补丁的模型容器 | 直接引用 Live2D 源文件，只缓存 BongoCat adapter |

同目录多 manifest、多个 patch 根、无基础包或多个基础包都被拒绝。图片 patch 不会被伪装成
可独立运行的模型。

## 已映射能力

- Live2D v3 MOC、纹理、Physics、Pose、DisplayInfo、Expressions 和 Motions。
- Tauri 与 Mver 的预览图、背景和三种 mode。
- Windows VK 到 BongoCat 跨平台快捷键名称的转换，包括组合键和左右修饰键。
- `input_mode=1` 的 Mver 手柄按钮；`input_mode=0` 保持键盘协议。
- standard/keyboard/gamepad 的左右手编号图片，支持稀疏编号并报告缺项。
- `CAT_motion`、`CAT_motion_lock` 和 expression 的逐索引严格映射。
- Mver `face` 图片表情及 `emoticonKeep` / `emoticonClear`。
- Mver 独立音频及 `soundKeep` / `soundClear`；关闭 keep 时松键停止。
- 不同尺寸 straight-alpha 图片合成，不再按最小画布裁切。
- Z patch 的逐文件覆盖和基础资源继承。

## 明确降级

以下字段依赖 Mver 固定 1400x1400 SFML 画布、Windows 桌面坐标或其专用手臂曲线，不能直接
持久化到跨平台模型：

- `window_size`、`topWindow`、`workarea`。
- `offsetX/Y`、`scalar`、`hand_offset`。
- `l2d_offset`、`l2d_correct`。
- `leftHanded`、`mouse_force_move`。
- standard 的 `arm/up/mouse/tablet` 动态设备精灵几何。
- gamepad 的 `stick_offset_L/R`。

BongoCat 对应功能继续由用户的窗口、鼠标镜像、模型缩放和手柄轴设置控制。导入报告会逐模型记录
这些降级项、原因、能力布尔值、声明/可用/缺失资源数以及可选 motion Sound 缺失数。

## 审计 TODO

### P0：协议和行为正确性

- [x] M01 固化三类格式契约和稳定的失败规则。
- [x] M02 将 Z 目录实现为显式 patch 工作流，禁止独立伪模型。
- [x] M03 校验编号资源、损坏 PNG 和稀疏资源，并输出数量诊断。
- [x] M04 用 `input_mode` 决定手柄/键盘协议，覆盖低 VK 值和组合键。
- [x] M05 精确映射 `CAT_motion` / `CAT_motion_lock`，校验配置行数。
- [x] M06 统一 motion Sound、独立音频、全局开关和 keep/clear 语义。

### P1：Mver 可见行为

- [x] M07 支持图片 `face`、保持、清除和 momentary 松键恢复。
- [x] M08 审计鼠标/数位板资源；固定画布设备几何改为显式降级报告。
- [x] M09 对可移植状态做映射，对机器相关状态做逐字段报告。
- [x] M10 预检全部 manifest 必需引用；缺失 motion Sound 作为可选诊断。
- [x] M11 同目录多 manifest 明确拒绝，递归候选去重并稳定排序。
- [x] M12 限制向上找包边界，patch 基础包必须唯一。
- [x] M13 修复不同尺寸合成裁切，并测试半透明与透明空白像素。
- [x] M14 保持 staging/rename/rollback 事务，启动时安全清理本应用临时目录。
- [x] M15 元数据只接受版本 1，只补缺失默认绑定并保留用户覆盖。

### P2：测试、跨平台和组织

- [x] M16 增加纯 C 的 Mver chord/device 协议测试，并保留端到端安装测试。
- [x] M17 增加环境变量驱动的可选真实样本测试入口。
- [x] M18 对 Tauri、完整 Mver、两个 Z patch 逐模型执行 Cubism smoke。
- [x] M19 统一 CTest 命令、测试计数和审计文档。
- [x] M20 曾将当时的本地审计脚本纳入 300 行门禁并拆分超长脚本；这些脚本现已移出仓库。
- [x] M21 将 discovery、patch、shortcut、motion、effect、metadata、report、storage 分层。
- [x] M22 将 Mver 的 Windows VK 解析限制在 adapter，运行时只接收 BongoCat 输入名。
- [x] M23 清理历史命名噪声；旧 schema 测试变量不再使用 `legacy_path`。
- [x] M24 将原模型与 BongoCat 适配产物分离为 `payload` / `adapter`，并验证源目录未变化。
- [x] M25 移除未发布旧自定义目录布局的兼容扫描，只接受当前旁路描述文件。
- [x] M26 按 Mver 原版相对路径习惯自动发现运行目录模型（包括嵌套的完整包与图片补丁），并保持源目录只读。
- [x] M27 对原版常见的“配置绑定多于 manifest 入口”仅截取可用入口；手动导入仍保留严格校验。

## 自动化证据

- `model-import-unit`：VK、组合键、`input_mode`、手柄映射，以及运行目录 Mver
  直接/嵌套容器、图片补丁发现和 adapter 缓存测试。
- `model-import-formats`：32 个合成端到端场景，包括歧义、回滚、patch、稀疏资源、
  momentary、清除、缺失可选音频、非法 chord 和不同尺寸合成。
- `model-import-real-samples`：由 `BONGO_CAT_TAURI_SOURCE`、
  `BONGO_CAT_MVER_SOURCE`、`BONGO_CAT_MVER_PATCH_SOURCES` 驱动；未配置时明确 Skip。
- 合成与真实样本审计会比较导入前后源目录签名，并验证安装 payload 的相对路径和 SHA-256
  与对应 Tauri、Mver、base、patch 源目录完全一致。
- Windows 多配置生成器的标准命令为：
  `ctest --test-dir build-cubism -C Release --output-on-failure`。

真实样本结果：Tauri 3/3、A-露西亚 3/3、Z-无按键显示 3/3、Z-有按键显示 3/3，
安装、报告生成和逐模型 Cubism 加载均通过。

## 外部门禁

本次 Windows 审计不能替代 Linux/macOS 的原生构建，也不能模拟真实手柄热插拔、不同桌面缩放
和声卡故障。相关代码只使用 SDL、标准 C 和 yyjson 的跨平台接口；这些环境仍应由对应系统的 CI
和真实硬件回归承担。
