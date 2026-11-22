# 第三方仓库与可用性指南

这份清单按本项目 **Nikon Z6II C 1.70** 的实际使用情况整理。“可用性”指它对本项目的作用，不是对上游项目的质量评价。**不要把其他机型的固件或模块直接刷入 Z6II。**

## 最重要的两个项目

| 仓库 | 本项目用到了什么 | 可用性与限制 |
|------|------------------|--------------|
| [JKWTCN/z7ii-extensions](https://github.com/JKWTCN/z7ii-extensions) | SD 原生模块加载器、模块容器、录像时长运行时方案、竖屏 UI 和视频方向元数据，是本次移植的主要基础 | **Z7II C 1.70 可直接参考其发布包；Z6II 不能直接使用。** 两机型的代码和 RAM 地址不同，需要逐项映射。本项目的 Z6II 移植位于 `z62_extensions/`。 |
| [abaeyens/qemu-expeed](https://github.com/abaeyens/qemu-expeed) | 实际执行 Z6II C 1.70 固件，验证加载器、读卡、模块初始化、回调安装，并截取左右竖屏和 i 菜单画面 | **当前最贴近本项目目标的上游模拟器。** 上游快速入门以 Linux/Docker 为主，本地另有 macOS 构建适配。能显示取景和菜单，但视频编码、硬件计时、完整传感器/镜头行为并不完整，不能代替真机验收。 |

如果朋友只想理解当前成果，优先看这两个仓库，再看本项目的 `docs/research-notes.md`。

## 固件格式、签名和模块机制参考

| 仓库 | 主要价值 | 可用性与限制 |
|------|----------|--------------|
| [horshack-dpreview/NikonPictureControlsDev](https://github.com/horshack-dpreview/NikonPictureControlsDev) | NCP 容器、基础 TRC 与用户 256 项 LUT 的域说明，提供 RawLinear 数学模板 | 用于生成实验性 NLOG8；不是尼康官方工具，Neutral 补偿为近似，导入和色调仍需 Z6II 真机验证。 |
| [simeonpilgrim/nikon-firmware-tools](https://github.com/simeonpilgrim/nikon-firmware-tools) | NikonHacker 经典工具集；`Firmware.cs` 提供旧式固件 XOR 解码表和格式线索，`dumpncp.pl` 可独立解析 NCP。`qemu-expeed` 的解包脚本明确以其为基础 | 面向 D5100/D7000 等老机型，**不是 Z6II 的现成补丁器**。对当前项目主要是解码、NCP 校验和格式研究基础，不能单独解决新固件签名。 |
| [XinmiaoZhao/NikonFirmwarePatch](https://github.com/XinmiaoZhao/NikonFirmwarePatch) | Z8 功能补丁、输入校验、K8 签名/验证和发布清单的实现参考 | 当前上游工具面向 **Z8 C 3.11** 的 ProRes、预拍摄、像素位移、开放门/N-RAW 等功能。**不是通用 Nikon 补丁工具，也不支持直接生成 Z6II 固件。** |
| [youyli03/z5mod](https://github.com/youyli03/z5mod) | 展示“固件宿主 + SD 卡 C 模块”的两层结构、模块校验和构建方法 | 目标是 **Z5 C 1.50**，其 `Z5APP.BIN`、A 盘路径和 MY MENU 激活机制不能直接套到 Z6II。适合学习架构；当前 Z6II 加载器实际以 Z7II 项目为主要移植基础。 |

## 其他 Z6II 模拟研究仓库

这两个仓库对早期研究有参考价值，但**不参与当前组合模块的最终 QEMU 验证**；本次验证使用的是上面的 `qemu-expeed`。

| 仓库 | 内容 | 可用性与限制 |
|------|------|--------------|
| [new-Beginner/z62-qemu](https://github.com/new-Beginner/z62-qemu) | Z6II C 1.70 系统仿真、候选固件加载、机身输入、虚拟场景、存储和大量诊断工具 | 功能很多，但上游明确说明完整相机仿真尚未完成。公开仓库不包含固件、NOR、虚拟卡和完整本地工具链；更偏 Windows 研究环境。适合参考设备建模和诊断，不要凭模拟结果判断刷机安全。 |
| [xtd1145/z6ii-fw-lab](https://github.com/xtd1145/z6ii-fw-lab) | Z6II 单/双处理器实验 machine、浏览器实验台、启动稳定性与补丁启动测试脚本 | 早期用于理解补丁锚点、容器和启动验证。需要自行准备固件并整合对应 QEMU 源码；菜单/HUD 可运行不代表完整录像、AF 或真机安全。当前成果不依赖其运行环境。 |

## PTP 工具：能连接，但不是破解方案

- [gphoto2](https://github.com/gphoto/gphoto2) 是命令行前端。
- [libgphoto2](https://github.com/gphoto/libgphoto2) 包含 Nikon PTP 属性、录像触发和会话实现。

本项目用它们理解 PTP 会话和 `movie=1` 的真实调用序列。本地工具为 `gphoto2 2.5.32`。

**最终真机实测表明：虽然 gphoto2 触发录像时屏幕显示 `--m--s`，后台原厂计时仍然运行，到 29:59 仍会自动停止。** 因此它可用于控制和协议研究，不能解除录制限制。USB 连接、会话切换和内存读取还可能影响相机显示/操作，不应在重要拍摄中实验。

## 本地核对的仓库基线

以下提交号用于说明本项目当时参考的源码快照，不代表上游最新状态；部分本地工作树另有适配或实验修改。

```text
JKWTCN/z7ii-extensions       9ddc447789cf（本地基线）；e507523a8bc0（视频方向参考）
abaeyens/qemu-expeed         0d8476d245f9
horshack-dpreview/NikonPictureControlsDev 1777cbe31572
simeonpilgrim/nikon-firmware-tools  89ad7354b6d7
XinmiaoZhao/NikonFirmwarePatch      cd1e46332c5e
youyli03/z5mod               8a1bfdca4a06
new-Beginner/z62-qemu        f3c05d8ee7d9
xtd1145/z6ii-fw-lab          fc693ad51376
```

## 给朋友的最短建议

1. 想直接了解功能实现：读 `z7ii-extensions`，但 Z6II 请使用本项目移植后的源码和模块。
2. 想离线运行 Z6II 固件：优先读 `qemu-expeed`，准备合法取得的 Z6II C 1.70 更新包。
3. 想研究解码和签名：从 `nikon-firmware-tools`、`NikonFirmwarePatch` 入手，但注意它们的机型范围。
4. 不要用 gphoto2 的 `--m--s` 显示推断录像计时已解除，也不要把任何模拟器结果当作真机安全证明。
