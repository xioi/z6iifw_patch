# Z6II Firmware Patch

Nikon Z 6II C 1.70 的 SD 原生模块加载器，以及录像时长和竖屏拍摄 UI 模块。移植自 [JKWTCN/z7ii-extensions](https://github.com/JKWTCN/z7ii-extensions)，针对 Z6II 逐项重新映射并验证地址。

## 功能

| 功能 | 当前实现 | 状态 |
|------|----------|------|
| SD 原生模块加载 | 启动时读取并执行 `Z62APP.BIN` | QEMU 全链路通过 |
| 后屏竖拍 UI | 拍摄信息和 4×3 i 菜单旋转，根界面触控热区同步；普通二级选项区横向铺满；菜单内方向变化自动重排 | 两类菜单、两种方向及触控坐标的 QEMU 验证通过；真机已知问题见下文 |
| EVF 竖屏拍摄信息 | 旋转拍摄信息，LCD↔EVF 切换时刷新；EVF 的 i 菜单保持原厂方向 | **实验功能，当前 QEMU 无法验证 EVF，需真机确认** |
| 手动对焦指示 | 已确认实时对焦位置属性 `0x2000`；标尺和清除生命周期继续在原生控件层调查 | 此前坐标补丁已撤回；距离显示待实现 |
| 解除普通录像 29:59 | 运行时同步 writer、父录制任务和 UI 时长 | 回调安装通过；待真机录像验证 |

当前竖屏默认开启，尚未移植扩展设置菜单和配置文件。实体 MENU、回放界面及 EVF 的 i 菜单保持原厂方向。场景图像本身不会被旋转。顶部信息分行时保持原字号，行起点间隔为 64 像素；[布局截图](docs/images/z62-portrait-rows.png)。

## 文件

- `firmware/Z6_2_0170_loader.bin`：从官方 C 1.70 生成的模块加载宿主固件，不含旧的 ROM 录像表补丁。
- `sd/Z62APP.BIN`：EVF 实验 + 双列 DISP + 解除普通录像时长的组合模块。
- `sd/lcd-only/Z62APP.BIN`：仅 LCD 竖屏的回退版本，不包含 EVF 实验路径。
- `sd/movie-only/Z62APP.BIN`：仅解除录像时长的回退模块。
- `sd/diagnostics/Z62APP.BIN`：对焦属性诊断模块，生成 `FOCUSPROP.BIN`，[使用方法](docs/focus-distance.md)。
- `z62_extensions/`：宿主、模块和构建工具源码。
- `firmware/Z6_2_0170_loader.bin.json`：加载器构建校验报告。

**仅适用于 Z6II C 1.70，不可用于其他机型或固件版本。**

## 验证范围

- 固件解密、模块目录、各级 CRC、K8 重签及回读比对通过。
- QEMU 已跑通宿主启动、读卡、CRC/重定位、模块入口和回调安装。
- 正式组合模块在原生方向读取路径下保持正常横屏，加载器返回 `LOADED=11`、模块返回 0；绘图、触控、菜单更新与录像 Hook 同时安装且地址逐项吻合。对焦诊断模块也已通过加载检查。
- 临时方向注入模块验证了左右竖屏、双列顶部信息及 DISP 清除，并进一步验证 i 根菜单、普通二级页和 Picture Control 在菜单内左右旋转及恢复横屏；临时模块未进入发布目录。
- 通过运行时计算模块入口，在 ARM 上逐点验证触屏开关、快门速度、光圈、i 按钮和二级选项的触控逆映射；这验证坐标与回调逻辑，不代替真机点击测试。
- **真机 EVF、方向感应、触控、录像超过 29:59 及长期稳定性仍需验证。** QEMU 不提供可用 EVF 画面，录像编码和硬件计时也不完整。
- **DISP 历史问题**：自制直方图/水平仪叠加在视频和照片页面均出现过花屏、假死和快门禁用。当前所有模块已完全停用该叠加功能，DISP 与水平仪交还原厂实现；视频和照片切换测试通过。不要继续使用旧版。
- 旧的 ROM 直改表固件会立即报告“录制已中断”，已移除，不应再刷。

## 竖屏候选版已知问题

以下问题来自提交 `7dab6f1` 对应组合模块（校验值见下文）的真机测试，尚未修复：

1. 竖屏时，快门速度、ISO 等拍摄参数按钮可以触发，但弹出的调整页面仍为横屏。
2. 竖屏时，按下曝光补偿或 ISO 物理按钮，或放大画面后，出现的相关 UI 仍为横屏。
3. 进入 i 菜单的二级子菜单后，底部「i 键返回，OK 确认」的按钮形状与文字错位。

前两项涉及的参数调整页和临时曝光/放大覆盖层尚未纳入现有竖屏适配。第三项的根因尚未确认，后续将优先检查底部提示文字与背景是否采用了不同的缩放路径。

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
cedba0235aa94153be215d0c6d3aa008dc8d8b30cbc6f6ce7f9c8dfe6239421b  sd/Z62APP.BIN
a99f8357afca95428a85a79dc1b47c5569adaf297ce671fa03a27a0b543fb14f  sd/lcd-only/Z62APP.BIN
eeb23e7423797de003c34ccc7a2ae355c17413a48199e445f78cb22ee1fbead7  sd/movie-only/Z62APP.BIN
521f9313af1b7c0de63b1c460bae4d17b5bf3bdf030e6886e3a76631106f24ab  sd/diagnostics/Z62APP.BIN
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
