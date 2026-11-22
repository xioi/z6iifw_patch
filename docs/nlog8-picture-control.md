# NLOG8 v01：实验性仿 N-Log Picture Control

NLOG8 是面向 Z6II 8bit 内录的自定义 Picture Control 原型。它只改变亮度响应，不是尼康真正的 Rec.2020/N-Log 色调模式，也不保证可直接套用官方 N-Log 还原 LUT。

当前状态：**NCP 文件结构和全部 256 项数学转换已验证；Z6II 真机导入、照片和视频色调尚未验证。**

## 测试文件

```text
sd/PICCON10.NCP
内部名称：NLOG8 v01
大小：638 字节
SHA-256：10843b02ab1ffab75bc9d776170cee33d80849f537de2df437298299823d7770
```

配置以 Neutral/RawLinear 补偿为基础，锐化为 0，饱和度为 0，对比度和亮度使用自定义曲线。`PICCON10.NCP.json` 记录模板哈希、数学参数和代表性采样值。

## 曲线生成方法

使用 [NikonPictureControlsDev](https://github.com/horshack-dpreview/NikonPictureControlsDev) 的 `Horshack_RawLinear.NCP` 作为数学模板。该模板用于补偿 Neutral 基础曲线及 sRGB 1/2.4 gamma；生成器保留其容器和输入响应，把每个线性 RAW 输出值送入尼康官方 N-Log 正向公式。

为对齐中间灰，线性值先乘以 `0.18 / 0.088`，把尼康测光的 8.8% 场景亮度映射到 N-Log 的 18% 反射率。然后计算：

```text
y < 0.328:
x = 650 × (y + 0.0075)^(1/3)

否则:
x = 150 × ln(y) + 619
```

`x` 为 10bit N-Log 全范围码值，再量化到 NCP 的 256 项、15bit 输出 LUT。18% 灰对应 `x=372.032`，相当于 PC 8bit 全范围码值 93。模板的最大线性 RAW 输入映射到 PC 码值约 181，因此未调色画面会偏暗、黑位抬高，这是本原型的预期表现。

参考：[N-Log 官方参数](https://download.nikonimglib.com/archive5/x8yZX00KKy8505VK2Hn94bPDap74/N-Log_Specification_(En)01.pdf)、[Picture Control 曲线说明](https://nikonimglib.com/npcu/onlinehelp/en/pc010000.html)。

## 导入相机

不需要更新加载器或替换 `Z62APP.BIN`。

1. 在相机第一存储卡上创建 `NIKON/CUSTOMPC/`。
2. 将测试文件复制为 `/NIKON/CUSTOMPC/PICCON10.NCP`。如果已有同名旧文件，先删除再复制。
3. 进入「管理优化校准 → 载入/保存 → 复制到照相机」，选择 `NLOG8 v01`，保存到一个空闲自定义槽位。
4. 在视频拍摄菜单的「设定优化校准」中选择 `NLOG8 v01`。
5. 第一轮测试使用 1080p24/25、普通画质、固定白平衡和手动曝光；关闭自动 D-Lighting，先不要使用自动 ISO。

如果相机拒绝导入，请从这台 Z6II 导出一个本机生成的自定义 NCP，保留为后续适配模板；不要直接认定曲线内容无效。

## 真机验证步骤

1. 先拍摄静态灰阶或灰卡，确认照片和视频均可正常生成文件。
2. 用相同曝光分别录制原厂 Flat 和 NLOG8，检查灰阶是否单调、是否出现异常断层或色彩偏移。
3. 再拍高光渐变、暗部渐变和 ColorChecker，观察暗部色带、饱和度高光及肤色。
4. 在电脑波形图上检查中间灰和高光位置，不要只凭相机屏幕的暗、平画面判断失败。
5. 暂时不要直接套官方 N-Log 还原 LUT 作为合格标准；当前配置没有实现 Rec.2020 原色和完整 N-Log 色彩管线。

测试时保留相机固件版本、NCP 哈希、视频规格、ISO、曝光、白平衡和原始短片。若画面过暗、饱和度异常或暗部色带明显，应基于这些样片调整灰点增益和曲线，而不是继续增加曲线幅度。

## 限制

- 不能恢复基础 SDR 管线已经裁切的高光，也不能凭空增加传感器动态范围。
- 8bit 全范围 Log 容易出现暗部色带和渐变断层。
- 自定义曲线会改变饱和度；当前只固定饱和度参数，没有做三维色彩拟合。
- RawLinear 模板对原厂 Neutral 曲线的补偿是第三方近似，最终效果必须真机校准。
- 当前二进制以第三方模板生成；正式公开分发前，建议改用 Z6II 本机导出的模板重新生成并验证。

## 重新生成

```sh
python3 tools/generate_nlog8_pc.py \
  --template /path/to/Horshack_RawLinear.NCP \
  --output sd/PICCON10.NCP \
  --report sd/PICCON10.NCP.json
```

已有输出时，确认可以替换后添加 `--force`。生成器会锁定已审阅模板的 SHA-256，检查记录布局、LUT 位置和单调性。
