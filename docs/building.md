# 构建与发布指南

本文说明如何按功能组合构建一个 Z6II C 1.70 SD 原生模块，以及如何构建加载器升级包和整理 GitHub Releases 附件。

> [!WARNING]
> 构建成功只说明代码、格式和签名校验通过，不等于真机功能或稳定性通过。当前模块仍有 README 中列出的已知问题。不要为其他机型或其他固件版本直接修改目标名称后刷入。

## 1. 构建产物

| 产物 | 构建入口 | 需要的额外输入 |
| --- | --- | --- |
| 可配置 SD 模块 `Z62APP.BIN` | `z62_extensions/tools/build.py` | 无 |
| 加载器升级包 `Z6_2_0170_loader.bin` | `z62_extensions/tools/build_package.py` | 官方 C 1.70 更新包、解密表、K8 |
| 单独的加载器主模块 | `z62_extensions/tools/build_host.py` | 从官方更新包解出的原始主模块 |

`build.py` 每次只生成一个 `Z62APP.BIN` 和一个同名 `.json` 配置清单。切换功能时使用 `--force` 替换这两个文件，不再为 LCD-only、movie-only、diagnostics 等配置保留独立预编译副本。

只修改 `plugins/` 中的模块代码或功能组合时，通常只需替换 SD 卡根目录的 `Z62APP.BIN`，不必重刷加载器。只有修改 `host/`、加载 ABI 或宿主地址映射时才需要重建升级包。

## 2. 环境准备

需要以下工具：

- Python 3 和 NumPy；
- 支持 ARM Thumb 的 Clang；
- GNU `arm-none-eabi-ld`；
- LLVM `llvm-objcopy` 和 `llvm-nm`（构建宿主时使用）；
- Git（获取源码）和 `zip`（仅发布打包时使用）。

NumPy 只用于固件解密和升级包构建；仅构建 SD 模块不需要它。

### macOS / Homebrew

```sh
brew install python llvm arm-none-eabi-binutils
export PATH="$PATH:$(brew --prefix llvm)/bin"

python3 -m venv .venv
source .venv/bin/activate
python -m pip install numpy
```

Homebrew LLVM 是 keg-only，`llvm-objcopy`、`llvm-nm` 默认可能不在 `PATH` 中，因此建议执行上面的 `export`。该环境变量只对当前终端有效；新开终端后需要重新执行，并重新激活虚拟环境。

这里刻意把 LLVM 目录追加到 `PATH` **末尾**，以保留系统 Clang。当前候选版已使用 Apple Clang 17 构建；Homebrew Clang 23 会对固定宽度多语言字符串数组触发 `-Wunterminated-string-initialization`，并因构建参数中的 `-Werror` 而停止。不要为了消除错误而随意扩大这些固定宽度数组。

其他操作系统安装同名工具即可，建议先使用 Clang 17 这一已验证版本。构建脚本会在 `PATH` 中查找工具；Apple Silicon Homebrew 的构建脚本还会检查 `/opt/homebrew/opt/llvm/bin`，但不要依赖这一回退路径完成全部构建。

### 检查工具

在仓库根目录执行：

```sh
python --version
python -c "import numpy; print(numpy.__version__)"
clang --version
arm-none-eabi-ld --version
llvm-objcopy --version
llvm-nm --version
```

记录这些版本，并在发布候选版时随测试结果保存。不同编译器版本可能生成不同但同样有效的机器码，因此不能仅凭 SHA-256 不同就判断构建损坏。

## 3. 选择功能并构建 SD 模块

### 功能参数

| 参数 | 功能 | 依赖 |
| --- | --- | --- |
| `--portrait` | LCD 拍摄信息、触控和已适配菜单的竖屏路径 | 无 |
| `--evf` | 实验性 EVF 竖屏拍摄信息 | 自动启用 `--portrait` |
| `--movie` | 实验性普通录像时长扩展 | 无，可独立构建 |
| `--video-rotation` | 普通内录 MOV/MP4 开档锁姿并写入视频轨方向矩阵 | 无，可独立构建 |
| `--focus-probe` | 对焦属性和原生绘制诊断，生成 `FOCUSPROP.BIN` | 自动启用 `--portrait` |
| `--raw12-dual-frame` | CPU 双帧诊断；真机第二帧复制超时撕裂，不再作为功能候选 | 无；保留失败诊断 |
| `--raw12-rawout-single` | RAWOUT 写入诊断；真机中途启用未覆盖私有缓冲 | 无；不再作为功能候选 |
| `--raw12-dma-single` | XDMAC 通道 3 与 CPU 各复制半帧，停止后导出 | 无；QEMU 及真机 1080p30 单帧已通过 |
| `--raw12-dma-dual` | 两块独立缓冲连续捕获两帧，停止后一起导出 | 无；QEMU 及真机 1080p30 双帧已通过 |
| `--raw12-dma-burst4` | 四个独立缓冲连续捕获四帧，停止后一起导出 | 无；真机采集/导出通过，末两帧重复；已排除 `0x1002` 替代事件并恢复 `0x1000` |
| `--raw12-dma-stream8` | 8 个独立槽位和独立写入任务，有限 8 帧连续写入 | 无；QEMU 已通过，真机待验证；不包含无限连续录像 |
| `--output PATH` | 指定模块输出路径 | 默认 `artifacts/Z62APP.BIN` |
| `--force` | 替换旧模块及其配置清单 | 切换配置时显式指定 |

至少选择一个功能参数。`--evf` 或 `--focus-probe` 自动包含竖屏依赖，但不会自动包含录像功能。`--video-rotation` 独立于 `--movie` 和 `--portrait`，只负责视频文件方向元数据。旧版自制直方图/水平仪已确认不兼容，不提供构建开关。

### 默认发布组合

```sh
python z62_extensions/tools/build.py --portrait --evf --movie --video-rotation
```

默认生成：

```text
artifacts/Z62APP.BIN       # 复制到相机 SD 卡的唯一模块
artifacts/Z62APP.BIN.json  # 配置、编译定义、符号、重定位和 SHA-256 清单
```

终端会输出简要 JSON，确认所选配置、尺寸和哈希。完整报告保存在 `.json` 清单中；其中 `camera_verified` 始终为 `false`，真机测试需要另行记录。

### 其他组合

以下是**互斥示例**，只执行需要的一条。`--force` 表示用新配置替换默认输出，而不是另外保留一份变体：

```sh
# LCD 竖屏 + 录像，不含 EVF
python z62_extensions/tools/build.py --portrait --movie --force

# 仅 LCD 竖屏
python z62_extensions/tools/build.py --portrait --force

# LCD + EVF 竖屏，不含录像
python z62_extensions/tools/build.py --evf --force

# 仅录像时长扩展
python z62_extensions/tools/build.py --movie --force

# 仅视频方向元数据
python z62_extensions/tools/build.py --video-rotation --force

# 对焦诊断，自动包含 LCD 竖屏，不含 EVF 和录像
python z62_extensions/tools/build.py --focus-probe --force
```

每次切换后都应检查新的 `.json` 清单和终端哈希，再把同一个输出文件复制到 SD 卡。不要根据旧的文件名猜测当前配置。

底层 `z62_extensions/tools/module.py` 仍可用于编译指定 C 入口和调试模块格式；正式组合构建统一使用 `build.py`，避免绕过功能依赖和配置清单。

## 4. 构建加载器升级包

### 必需输入

| 输入 | 说明 |
| --- | --- |
| 官方 `Z6_2_0170.bin` | Z6II C 1.70 官方更新包，必须保持原始内容 |
| `key.json` | 与该机型的旧版固件容器匹配的 `T1`、`T2`、`T3` 及自检哈希 |
| `sig_z6ii.json` | 与该官方包匹配的 8 字节 `K8` 及可选自检哈希 |

仓库不包含官方固件、恢复密钥或 K8，也不会自动获取它们。缺少这些输入时仍可构建 SD 模块，但不能生成新的签名升级包。不要把真实密钥文件提交到 Git、贴到 Issue 或上传到 Release。

构建器锁定的原厂输入哈希为：

```text
官方 C 1.70 更新包：
5a9640b4c07abe1ecbf5a52655cbec6b893bba045bb2a1a1d21055b9f8f9d06d

更新包内原始 eg1850_mas_ 主模块：
bb83613121f843d65101da936d942acdc513d86dbd3abc26259d0d870d03f5d4
```

### 一步构建可刷写升级包

将示例路径替换为实际输入路径：

```sh
python z62_extensions/tools/build_package.py \
  --stock-package /path/to/Z6_2_0170.bin \
  --key /path/to/key.json \
  --sig /path/to/sig_z6ii.json \
  --output artifacts/Z6_2_0170_loader.bin
```

成功后生成：

```text
artifacts/Z6_2_0170_loader.bin             # 可放入 SD 卡的升级包
artifacts/Z6_2_0170_loader.bin.json        # 构建与验证报告
artifacts/eg1850_mas_01700_loader.bin      # 更新包内的主模块，不是独立升级包
```

构建过程会：

1. 校验并解密官方更新包；
2. 确认提供的 K8 能验证原厂包；
3. 唯一识别并校验 Z6II C 1.70 主模块；
4. 编译宿主、检查 Hook 原指令和 BSS 边界并追加加载器；
5. 重算主模块 CRC、包 CRC 和 K8 签名；
6. 重新解密产物，验证目录、CRC、签名及所有非目标成员逐字节不变。

报告中的 `roundtrip_verified: true` 只代表容器回读验证通过；`camera_verified` 仍为 `false`，不能用构建报告替代真机测试。

### 可选：单独构建加载器主模块

这个入口用于检查宿主编译和地址补丁，**不会生成可直接刷写的完整升级包**：

```sh
python z62_extensions/tools/build_host.py \
  --stock-master /path/to/eg1850_mas_01700.bin \
  --output artifacts/eg1850_mas_01700_loader.bin
```

`build_package.py` 会自行调用同一套宿主构建逻辑，因此正常构建升级包时不需要先执行这一步，也不要把独立主模块当作固件升级文件。

## 5. 校验构建结果

先检查模块清单中的实际配置：

```sh
python -c "import json; print(json.load(open('artifacts/Z62APP.BIN.json'))['configuration'])"
```

macOS 使用 `shasum`，Linux 可换成 `sha256sum`：

```sh
shasum -a 256 \
  artifacts/Z6_2_0170_loader.bin \
  artifacts/Z62APP.BIN
```

README 列出了加载器和默认 `--portrait --evf --movie --video-rotation` 组合的当前候选校验值。使用相同源码和工具链时，默认配置应得到相同哈希；含方向默认模块目前只有构建、QEMU 和 ARM 验证。历史真机模块不包含本次大容量 UI 限幅修正，不能用旧配置名称或旧哈希代替当前产物。其他组合应记录自己的哈希，不要复制默认配置的哈希或真机结论。

至少确认：

- `.json` 中的 `configuration` 和 `defines` 与计划构建的功能一致；
- 加载器报告的 `stock_package_sha256` 和 `stock_sha256` 与上述原厂值一致；
- 加载器的 `only_master_changed` 和 `roundtrip_verified` 均为 `true`；
- 发布 ZIP 中的模块、清单和实际测试文件一致；
- 没有把 `eg1850_mas_01700_loader.bin` 错当成完整升级包发布；
- 没有把旧版自制直方图模块或 ROM 直改录像表固件重新混入发布包。

## 6. 整理 GitHub Release

Git 源码仓库会忽略 `firmware/`、`sd/`、`artifacts/` 和 `*.bin`。GitHub 自动生成的 Source code 压缩包因此只包含源码，**必须另外上传包含预编译产物的 Release ZIP**。

以下 macOS 示例整理一种发布配置。先设置自己的版本号，并确保 `artifacts/Z62APP.BIN` 是准备发布的配置：

```sh
VERSION=0.1.0-rc1
STAGE="artifacts/release-$VERSION"

mkdir -p "$STAGE/firmware" "$STAGE/sd"

cp artifacts/Z6_2_0170_loader.bin \
  artifacts/Z6_2_0170_loader.bin.json \
  "$STAGE/firmware/"
cp artifacts/Z62APP.BIN artifacts/Z62APP.BIN.json "$STAGE/sd/"
cp README.md LICENSE "$STAGE/"
cp -R docs "$STAGE/"
```

在发布目录内生成校验文件并打包：

```sh
(
  cd "$STAGE"
  shasum -a 256 \
    firmware/Z6_2_0170_loader.bin \
    sd/Z62APP.BIN > SHA256SUMS.txt
  zip -r "../z62fw_patch-$VERSION.zip" \
    README.md LICENSE docs firmware sd SHA256SUMS.txt
)

shasum -a 256 "artifacts/z62fw_patch-$VERSION.zip" \
  > "artifacts/z62fw_patch-$VERSION.zip.sha256"
```

发布时建议：

1. 先提交源码并创建与 `VERSION` 对应的 Git 标签，记录实际测试提交。
2. 上传 `z62fw_patch-$VERSION.zip` 及其 `.sha256`，不要只依赖自动 Source code 附件。
3. 在 Release Notes 中写明模块构建参数，并附上对应 `.json` 清单。
4. 链接 README 的已知问题，列出工具链版本、QEMU 范围和真机测试范围。
5. 未验证的 EVF、录像超过 29:59、Z6II 真机视频方向文件、其他参数组合和长期稳定性不得标为已完成。
6. 不上传官方固件、`key.json`、`sig*.json`、解密后的原厂模块或包含这些材料的构建目录。

## 7. 常见问题

| 错误 | 原因与处理 |
| --- | --- |
| `select at least one feature` | 没有选功能；添加 `--portrait`、`--evf`、`--movie`、`--video-rotation` 或 `--focus-probe`。 |
| `ModuleNotFoundError: No module named 'numpy'` | 构建升级包前尚未激活虚拟环境或安装 NumPy；仅构建 SD 模块不需要 NumPy。 |
| `Missing build tool: ...` | 工具未安装或不在 `PATH`；macOS 检查 `export PATH="$PATH:$(brew --prefix llvm)/bin"`，并确认 `clang` 仍是已验证的系统版本。 |
| `-Wunterminated-string-initialization` 被 `-Werror` 提升为错误 | Homebrew Clang 23 与当前固定宽度字符串数组的已知兼容性问题；改用已验证的 Apple Clang 17，不要通过扩大数组来改变数据布局。 |
| `output or manifest already exists` | 构建器正在防止覆盖旧配置；确认当前输出可以替换后添加 `--force`。 |
| `Requires the exact original Z6II C 1.70 master` | 更新包、主模块或版本不正确；核对上面的原厂 SHA-256，不要绕过检查。 |
| `The supplied K8 does not verify the stock package` | K8 与机型或官方更新包不匹配；检查输入，不要提交或公开密钥。 |
| `Undefined module symbol` | 模块引用了未提供的外部符号；模块是 freestanding 原生镜像，不能直接依赖主机 libc。 |
| 重建后 SHA-256 不同 | 先检查功能参数和 `.json`，再检查源码与工具链版本；不要沿用旧配置的真机结论。 |

## 8. 真机验证边界

构建和哈希检查之后，应针对**当前清单中的配置**验证：

- 无 `Z62APP.BIN` 时相机正常启动；
- 横屏、左右竖屏和恢复横屏的行为；
- 照片/视频模式、各 DISP 页面、i 根菜单和二级菜单；
- 参数调整页、曝光补偿、ISO、放大及 LCD/EVF 切换；
- 短录像和正常停止，再在具备散热与存储条件时测试长录像；
- 启用 `--video-rotation` 时，分别录制横屏和竖屏的短 MOV、MP4，在电脑上检查显示矩阵、实际播放方向及分段文件的方向；
- 对焦诊断结束后重新构建正式配置，不要继续把诊断模块留在卡上。

任何真机失败都应保留构建参数、模块哈希、操作步骤和恢复结果。QEMU 成功或模块返回 0 不能替代这些检查，一个配置通过也不能推导其他组合全部通过。

**所有成功或失败的复现流程细节统一写入[研究笔记](research-notes.md#可复现验证的记录规范)，包括准确的按键、等待时间、状态读取和文件验收方法；不能只留在对话或终端历史中。** 普通录像的 QEMU 基线流程也已记录在该章节。
