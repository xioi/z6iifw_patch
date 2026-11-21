# Z6II Firmware Patch

Nikon Z 6II C 1.70 的 SD 原生模块加载器，以及录像时长和竖屏拍摄 UI 模块。移植自 [JKWTCN/z7ii-extensions](https://github.com/JKWTCN/z7ii-extensions)，针对 Z6II 逐项重新映射并验证地址。

## 功能

| 功能 | 当前实现 | 状态 |
|------|----------|------|
| SD 原生模块加载 | 启动时读取并执行 `Z62APP.BIN` | QEMU 全链路通过 |
| 后屏竖拍 UI | 拍摄信息、4×3 i 菜单及实时直方图旋转，触控和方向键跟随；密集顶部信息使用双列布局 | 两种方向、i 菜单和 DISP 切换的 QEMU 截图验证通过 |
| EVF 竖屏拍摄信息 | 旋转拍摄信息，LCD↔EVF 切换时刷新；EVF 的 i 菜单保持原厂方向 | **实验功能，当前 QEMU 无法验证 EVF，需真机确认** |
| 手动对焦指示 | 已确认实时对焦位置属性 `0x2000`；标尺和清除生命周期继续在原生控件层调查 | 此前坐标补丁已撤回；距离显示待实现 |
| 解除普通录像 29:59 | 运行时同步 writer、父录制任务和 UI 时长 | 回调安装通过；待真机录像验证 |

当前竖屏默认开启，尚未移植扩展设置菜单和配置文件。实体 MENU、回放界面及 EVF 的 i 菜单保持原厂方向。场景图像本身不会被旋转。顶部信息分行时保持原字号，行起点间隔为 64 像素；[布局截图](docs/images/z62-portrait-rows.png)。

## 文件

- `firmware/Z6_2_0170_loader.bin`：从官方 C 1.70 生成的模块加载宿主固件，不含旧的 ROM 录像表补丁。
- `sd/Z62APP.BIN`：EVF 实验 + 双列 DISP + 解除普通录像时长的组合模块。
- `sd/lcd-only/Z62APP.BIN`：仅 LCD 竖屏、无 EVF 的回退版本，包含视频模式直方图保护。
- `sd/movie-only/Z62APP.BIN`：仅解除录像时长的回退模块。
- `sd/diagnostics/Z62APP.BIN`：对焦属性诊断模块，生成 `FOCUSPROP.BIN`，[使用方法](docs/focus-distance.md)。
- `z62_extensions/`：宿主、模块和构建工具源码。
- `firmware/Z6_2_0170_loader.bin.json`：加载器构建校验报告。

**仅适用于 Z6II C 1.70，不可用于其他机型或固件版本。**

## 验证范围

- 固件解密、模块目录、各级 CRC、K8 重签及回读比对通过。
- QEMU 已跑通宿主启动、读卡、CRC/重定位、模块入口和回调安装。
- 正式组合模块在原生方向读取路径下保持正常横屏，且竖屏与录像 Hook 同时安装；对焦诊断模块也已通过加载检查。
- 临时强制方向模块验证了左右竖屏、竖屏 i 菜单、双列顶部信息及切换到精简 DISP 后的清除；临时模块未进入发布目录。
- **真机 EVF、方向感应、触控、录像超过 29:59 及长期稳定性仍需验证。** QEMU 不提供可用 EVF 画面，录像编码和硬件计时也不完整。
- **视频 DISP 历史问题**：旧模块曾在水平仪页面造成假死和粉色像素块。当前主模块与 LCD-only 均已禁止视频模式执行照片直方图采样/绘制，QEMU 连续切换通过；真机仍需回归。不要继续使用旧版。
- 旧的 ROM 直改表固件会立即报告“录制已中断”，已移除，不应再刷。

## 使用

1. 备份数据，确保相机电量充足，使用正常工作的 SD 卡。
2. 将 `firmware/Z6_2_0170_loader.bin` 放到 SD 卡根目录，通过「设定菜单 → 固件版本 → 升级固件」安装。
3. 将 `sd/Z62APP.BIN` 复制到 SD 卡根目录。可删除固件升级文件，但必须保留模块文件。
4. 等待存储卡访问结束，关机后再开机。宿主会依次尝试 `B:\Z62APP.BIN` 和 `A:\Z62APP.BIN`。
5. 分别测试横屏、左右竖屏、i 菜单、不同 DISP 页面及 EVF，再进行短录像；确认正常后才测试超过 29:59 的录像。

如果已经刷入同校验值的加载器固件，只需替换 SD 卡中的 `Z62APP.BIN`，完整断电后重开，不必重复刷机。

若新布局或 EVF 在真机上有问题，可将 `sd/lcd-only/Z62APP.BIN` 作为根目录的 `Z62APP.BIN`，恢复旧 LCD 竖屏行为。只需要录像功能则使用 `sd/movie-only/Z62APP.BIN`。完全停用模块可删除根目录文件；恢复官方固件可刷回官方 `Z6_2_0170.bin`。

## 限制与风险

- 100/120p 慢动作等特殊录像路径的独立限制不以当前模块为目标。
- 卡容量、文件数量、电池和过热保护仍然有效；预计可录时间不等于保证录制时间。
- 修改版固件和原生模块可能导致死机、数据丢失、硬件损坏或无法启动。首次真机测试由使用者自行承担风险。
- 只加载可信的 `Z62APP.BIN`；模块是具有相机权限的原生代码，不是沙箱程序。

## 校验

```sh
shasum -a 256 firmware/Z6_2_0170_loader.bin sd/Z62APP.BIN \
  sd/lcd-only/Z62APP.BIN sd/movie-only/Z62APP.BIN sd/diagnostics/Z62APP.BIN
```

期望：

```text
aa5e2e8599f9126e92476c64e7de7c966d662d101e7d9ab19a358d1bb19ceacf  firmware/Z6_2_0170_loader.bin
31a2ba9fbc1fd592b6f220c0b300506e29d173ee04d431123592619e9ccbf3bb  sd/Z62APP.BIN
97139063f4a67d6843b9c03308f9ae52cd146cf248c6214f7f4751f5298a2eda  sd/lcd-only/Z62APP.BIN
eeb23e7423797de003c34ccc7a2ae355c17413a48199e445f78cb22ee1fbead7  sd/movie-only/Z62APP.BIN
8f5154eb148bb816299fd8b638668650883f30cb9c9344bcb70edfaecb6c1ebe  sd/diagnostics/Z62APP.BIN
```

## 重新构建

需要 Python、NumPy、LLVM `clang`/`llvm-objcopy`/`llvm-nm`，以及 `arm-none-eabi-ld`。还需要官方 C 1.70 更新包和该机型的 `key.json`、K8 签名 JSON；密钥文件不随发布目录自动提供。

构建组合模块：

```sh
python3 z62_extensions/tools/module.py \
  z62_extensions/plugins/extensions.c \
  --output z62_extensions/artifacts/Z62APP.BIN --force
```

仅录像模块可将源文件换成 `z62_extensions/plugins/movie_limit.c`。

重建加载器固件：

```sh
python3 z62_extensions/tools/build_package.py \
  --stock-package /path/to/Z6_2_0170.bin \
  --key /path/to/key.json --sig /path/to/sig_z6ii.json \
  --output z62_extensions/artifacts/Z6_2_0170_loader.bin --force
```

宿主构建器锁定原版主模块的 SHA-256、Hook 原指令和 BSS 边界；输入不符会拒绝生成。

## 目录

```text
z62fw_patch/
├── firmware/                    # 加载器固件与校验报告
├── sd/Z62APP.BIN                # EVF 实验 + 双列布局 + 录像
├── sd/lcd-only/Z62APP.BIN       # 旧 LCD 竖屏回退模块
├── sd/movie-only/Z62APP.BIN     # 仅录像的回退模块
├── sd/diagnostics/Z62APP.BIN    # 对焦属性诊断模块
├── z62_extensions/              # 可复现源码
├── patches/movie-recording-limit/ # 原理、地址映射与失败方案记录
├── tools/                       # PTP/真机研究工具
└── docs/                        # 研究笔记、截图与第三方仓库指南
```

第三方仓库的用途、适用范围和可用性见 [第三方仓库与可用性指南](docs/third-party-repos.md)。

## 致谢

加载器、模块格式、录像限制和竖屏 UI 的原始实现来自 JKWTCN 的 Z7II 开源项目。Z6II 地址映射、最小宿主、双路径加载、组合模块和 QEMU 验证在本项目中完成。许可证见 `LICENSE`。
