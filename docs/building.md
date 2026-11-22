# 构建与发布指南

本文说明如何从源码构建 Z6II C 1.70 的 SD 原生模块、加载器升级包，以及如何整理 GitHub Releases 附件。

> [!WARNING]
> 构建成功只说明代码、格式和签名校验通过，不等于真机功能或稳定性通过。当前模块仍有 README 中列出的已知问题。不要为其他机型或其他固件版本直接修改目标名称后刷入。

## 1. 构建产物

构建分为两条相对独立的路径：

| 产物 | 构建入口 | 需要的额外输入 |
| --- | --- | --- |
| SD 原生模块 `Z62APP.BIN` | `z62_extensions/tools/module.py` | 无 |
| 加载器升级包 `Z6_2_0170_loader.bin` | `z62_extensions/tools/build_package.py` | 官方 C 1.70 更新包、解密表、K8 |
| 单独的加载器主模块 | `z62_extensions/tools/build_host.py` | 从官方更新包解出的原始主模块 |

只修改 `plugins/` 中的模块代码时，通常只需重建并替换 SD 卡根目录的 `Z62APP.BIN`，不必重刷加载器。只有修改 `host/`、加载 ABI 或宿主地址映射时才需要重建升级包。

## 2. 环境准备

需要以下工具：

- Python 3 和 NumPy；
- 支持 ARM Thumb 的 Clang；
- GNU `arm-none-eabi-ld`；
- LLVM `llvm-objcopy` 和 `llvm-nm`（构建宿主时使用）；
- Git（获取源码）和 `zip`（仅发布打包时使用）。

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

其他操作系统安装同名工具即可，建议先使用 Clang 17 这一已验证版本。构建脚本会在 `PATH` 中查找工具；Apple Silicon Homebrew 的模块构建脚本还会检查 `/opt/homebrew/opt/llvm/bin`，但不要依赖这一回退路径完成全部构建。

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

## 3. 构建 SD 模块

所有命令均从仓库根目录执行。以下示例使用新路径 `artifacts/`；若输出已经存在，构建器会拒绝覆盖，确认不再需要旧文件后才添加 `--force`，或换一个新的输出目录。

### 组合模块

包含竖屏 UI、EVF 实验路径和普通录像时长扩展：

```sh
python z62_extensions/tools/module.py \
  z62_extensions/plugins/extensions.c \
  --output artifacts/Z62APP.BIN
```

### LCD-only 回退模块

不包含 EVF 实验路径：

```sh
python z62_extensions/tools/module.py \
  z62_extensions/plugins/lcd_only.c \
  --output artifacts/lcd-only/Z62APP.BIN
```

### 仅录像模块

不包含竖屏和 EVF 路径：

```sh
python z62_extensions/tools/module.py \
  z62_extensions/plugins/movie_limit.c \
  --output artifacts/movie-only/Z62APP.BIN
```

### 对焦诊断模块

```sh
python z62_extensions/tools/module.py \
  z62_extensions/plugins/focus_probe.c \
  --output artifacts/diagnostics/Z62APP.BIN
```

每次构建会输出 JSON 报告，包含镜像尺寸、BSS、入口偏移、重定位表和符号。模块构建器会检查 ELF 格式、Thumb 入口、未定义符号、支持的重定位类型和 1 MiB 文件上限，并写入带 CRC32 的 `Z62M` 模块头。

四种模块都以 `Z62APP.BIN` 为最终文件名，但**一次只能在 SD 卡根目录放置一种**。保留上面的分目录可以避免互相覆盖。

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

macOS 使用 `shasum`，Linux 可换成 `sha256sum`：

```sh
shasum -a 256 \
  artifacts/Z6_2_0170_loader.bin \
  artifacts/Z62APP.BIN \
  artifacts/lcd-only/Z62APP.BIN \
  artifacts/movie-only/Z62APP.BIN \
  artifacts/diagnostics/Z62APP.BIN
```

README 中列出了现有真机候选版的校验值。使用相同源码和工具链时可以据此检查文件是否一致；更换编译器后，应同时检查构建报告并重新完成加载与真机验证，而不是把旧哈希复制到新产物上。

至少确认：

- 加载器报告的 `stock_package_sha256` 和 `stock_sha256` 与上述原厂值一致；
- `only_master_changed` 和 `roundtrip_verified` 均为 `true`；
- 正式功能模块与真机实际测试的模块逐字节一致；诊断等未验证模块已明确标记；
- 没有把 `eg1850_mas_01700_loader.bin` 错当成完整升级包发布；
- 没有把旧版自制直方图模块或 ROM 直改录像表固件重新混入发布包。

## 6. 整理 GitHub Release

Git 源码仓库会忽略 `firmware/`、`sd/`、`artifacts/` 和 `*.bin`。GitHub 自动生成的 Source code 压缩包因此只包含源码，**必须另外上传包含预编译产物的 Release ZIP**。

以下 macOS 示例把已构建产物整理为带版本号的发布目录。先设置自己的版本号，并确保使用全新的构建产物：

```sh
VERSION=0.1.0-rc1
STAGE="artifacts/release-$VERSION"

mkdir -p "$STAGE/firmware" \
  "$STAGE/sd/lcd-only" \
  "$STAGE/sd/movie-only" \
  "$STAGE/sd/diagnostics"

cp artifacts/Z6_2_0170_loader.bin \
  artifacts/Z6_2_0170_loader.bin.json \
  "$STAGE/firmware/"
cp artifacts/Z62APP.BIN "$STAGE/sd/Z62APP.BIN"
cp artifacts/lcd-only/Z62APP.BIN "$STAGE/sd/lcd-only/Z62APP.BIN"
cp artifacts/movie-only/Z62APP.BIN "$STAGE/sd/movie-only/Z62APP.BIN"
cp artifacts/diagnostics/Z62APP.BIN "$STAGE/sd/diagnostics/Z62APP.BIN"
cp README.md LICENSE "$STAGE/"
cp -R docs "$STAGE/"
```

在发布目录内生成校验文件并打包：

```sh
(
  cd "$STAGE"
  shasum -a 256 \
    firmware/Z6_2_0170_loader.bin \
    sd/Z62APP.BIN \
    sd/lcd-only/Z62APP.BIN \
    sd/movie-only/Z62APP.BIN \
    sd/diagnostics/Z62APP.BIN > SHA256SUMS.txt
  zip -r "../z62fw_patch-$VERSION.zip" \
    README.md LICENSE docs firmware sd SHA256SUMS.txt
)

shasum -a 256 "artifacts/z62fw_patch-$VERSION.zip" \
  > "artifacts/z62fw_patch-$VERSION.zip.sha256"
```

发布时建议：

1. 先提交源码并创建与 `VERSION` 对应的 Git 标签，记录实际测试提交。
2. 上传 `z62fw_patch-$VERSION.zip` 及其 `.sha256`，不要只依赖自动 Source code 附件。
3. 在 Release Notes 中链接 README 的已知问题，列出工具链版本、QEMU 范围和真机测试范围。
4. 未验证的 EVF、录像超过 29:59 和长期稳定性不得标为已完成。
5. 不上传官方固件、`key.json`、`sig*.json`、解密后的原厂模块或包含这些材料的构建目录。

## 7. 常见问题

| 错误 | 原因与处理 |
| --- | --- |
| `ModuleNotFoundError: No module named 'numpy'` | 尚未激活虚拟环境或未安装 NumPy；执行 `source .venv/bin/activate` 和 `python -m pip install numpy`。 |
| `Missing build tool: ...` | 工具未安装或不在 `PATH`；macOS 检查 `export PATH="$PATH:$(brew --prefix llvm)/bin"`，并确认 `clang` 仍是已验证的系统版本。 |
| `-Wunterminated-string-initialization` 被 `-Werror` 提升为错误 | Homebrew Clang 23 与当前固定宽度字符串数组的已知兼容性问题；改用已验证的 Apple Clang 17，不要通过扩大数组来改变数据布局。 |
| `Output already exists; use --force to replace it` | 构建器正在防止覆盖旧产物；改用新输出路径，或确认后添加 `--force`。 |
| `Requires the exact original Z6II C 1.70 master` | 更新包、主模块或版本不正确；核对上面的原厂 SHA-256，不要绕过检查。 |
| `The supplied K8 does not verify the stock package` | K8 与机型或官方更新包不匹配；检查输入，不要提交或公开密钥。 |
| `Undefined module symbol` | 模块引用了未提供的外部符号；模块是 freestanding 原生镜像，不能直接依赖主机 libc。 |
| 重建后 SHA-256 不同 | 可能是工具链变化；记录版本、检查构建报告，并重新验证加载和真机行为。 |

## 8. 真机验证边界

构建和哈希检查之后，仍应分别验证：

- 无 `Z62APP.BIN` 时相机正常启动；
- 每种候选模块单独加载，横屏、左右竖屏和恢复横屏的行为；
- 照片/视频模式、各 DISP 页面、i 根菜单和二级菜单；
- 参数调整页、曝光补偿、ISO、放大及 LCD/EVF 切换；
- 短录像和正常停止，再在具备散热与存储条件时测试长录像。

任何真机失败都应保留模块哈希、操作步骤和恢复结果。QEMU 成功或模块返回 0 不能替代这些检查。
