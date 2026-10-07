# BongoCat 口型同步版 (Lip-Sync Mod)

在 [vladelaina/BongoCat](https://github.com/vladelaina/BongoCat) 基础上添加了**麦克风驱动的实时口型同步**功能。说话时模型嘴巴会跟着动，不需要面捕软件。

## 功能

- 捕获麦克风输入，实时计算音量
- 将音量映射到 Live2D 标准参数 `ParamMouthOpenY`
- 低通平滑处理，口型开合自然不抖
- 噪声门过滤环境底噪
- 可调节灵敏度、阈值、最大张嘴幅度

## 零环境编译（推荐）

完全不用在本地装 CMake、Visual Studio 等工具，用 GitHub 云端编译：

### 准备工作

1. **注册 GitHub 账号**（如果没有）
2. **下载 Cubism SDK for Native**（Live2D 官方 SDK，免费但需注册）
   - 地址：https://www.live2d.com/download/cubism-sdk/download-native/
   - 下载后得到一个 zip 包
3. **把 SDK zip 传到一个能直链下载的地方**
   - 最简单：新建一个 GitHub 仓库，把 zip 上传，然后在 Releases 里上传，拿到下载链接
   - 或者用任何支持直链的网盘

### 编译步骤

1. **Fork 本仓库**（或把修改后的代码上传到你自己的 GitHub 仓库）
2. 进入仓库 → **Actions** → 左侧选 **Build Release with Lip-Sync**
3. 点 **Run workflow**，填写：
   - `cubism_sdk_url`：刚才的 SDK 直链下载地址
   - `build_windows`：勾选（如果你用 Windows）
   - `tag`：随便填个版本号，如 `v1.0-lipsync`（填了会自动发布到 Releases）
4. 点 **Run workflow**，等 3-5 分钟
5. 编译完成后：
   - 如果填了 tag：去仓库 **Releases** 页面下载 zip
   - 没填 tag：去本次 Action 运行页面底部 **Artifacts** 下载

### 替代方案：直接把 SDK 放进仓库

如果你不想搞直链，可以把解压后的 `CubismSdkForNative` 文件夹直接放到仓库的 `vendor/` 目录下（即 `vendor/CubismSdkForNative/Core/include/Live2DCubismCore.h` 存在），然后触发 workflow 时 `cubism_sdk_url` 留空即可。

## 开启口型同步

### 方法一：修改配置文件（推荐）

1. 先运行一次 BongoCat，让它生成配置文件
2. 找到配置文件位置：
   - **Windows**：`%APPDATA%\BongoCat\preferences.json`（或类似路径）
   - **macOS**：`~/Library/Application Support/BongoCat/preferences.json`
   - **Linux**：`~/.config/BongoCat/preferences.json`
3. 用文本编辑器打开，在 `model` 对象里加入：
   ```json
   "model": {
     "lipSyncEnabled": true,
     "lipSyncGain": 1.0,
     "lipSyncThreshold": 0.02,
     "lipSyncMaxOpen": 1.0
   }
   ```
4. 保存，重启 BongoCat
5. 系统会请求麦克风权限，允许即可

### 方法二：默认开启（修改源码默认值）

如果你想让口型同步默认开启，编辑 `src/core/config_defaults.c`，把：
```c
config->model.lip_sync_enabled = false;
```
改成：
```c
config->model.lip_sync_enabled = true;
```
然后重新编译。

## 参数说明

| 参数 | 默认值 | 范围 | 说明 |
|------|--------|------|------|
| `lipSyncEnabled` | `false` | true/false | 口型同步总开关 |
| `lipSyncGain` | `1.0` | 0.0 - 10.0 | 灵敏度乘数。越大越灵敏，小声也能张嘴；太小可能没反应 |
| `lipSyncThreshold` | `0.02` | 0.0 - 1.0 | 噪声门阈值。低于此音量视为静音，过滤环境底噪。环境吵就调大 |
| `lipSyncMaxOpen` | `1.0` | 0.0 - 1.0 | 最大张嘴幅度。1.0 是模型参数最大值，觉得嘴张太大就调小 |

## 调节建议

- **嘴不动**：把 `lipSyncGain` 调大（如 2.0、3.0），或把 `lipSyncThreshold` 调小（如 0.01）
- **嘴一直张着/太敏感**：把 `lipSyncGain` 调小，或把 `lipSyncThreshold` 调大（如 0.05）
- **嘴张太大**：把 `lipSyncMaxOpen` 调小（如 0.7）
- **口型抖**：正常，已经做了平滑。如果还抖，说明阈值太低，调大 `lipSyncThreshold`

## 模型要求

模型必须包含 Live2D 标准口型参数 `ParamMouthOpenY`（范围 0-1，0=闭嘴，1=张嘴）。

绝大多数商用/免费 Live2D 模型都有这个参数。如果你的模型没有，需要在 Live2D Cubism Editor 里添加口型开合的参数和变形。

## 常见问题

**Q：Windows 下运行后没反应，嘴不动？**
A：1) 检查配置文件里 `lipSyncEnabled` 是否为 true；2) 检查系统麦克风是否被其他程序占用；3) 右键 BongoCat 用管理员身份运行；4) 在系统设置里确认 BongoCat 有麦克风权限。

**Q：能不能用系统音频（比如播放的音乐）驱动口型？**
A：目前只支持麦克风输入。如果想用系统音频，可以用虚拟音频线（如 VB-Cable）把系统输出映射成虚拟麦克风。

**Q：会不会很卡/很占资源？**
A：不会。麦克风捕获用 48kHz 单声道 20ms 缓冲，CPU 占用极低。

**Q：和原版 BongoCat 有什么区别？**
A：只加了口型同步功能，其他完全一样。配置文件兼容，模型兼容。

## 技术细节

- 音频捕获：miniaudio（项目已有依赖）
- 音量计算：RMS（均方根）
- 平滑：一阶低通滤波器，系数 0.30
- 参数映射：`mouth_open = volume * gain * max_open`，低于 threshold 则为 0
- 口型参数：硬编码为 `ParamMouthOpenY`（Live2D 标准）

## 许可证

同原项目，BongoCat 源代码采用 AGPL-3.0-only。模型资产遵循各自授权。
