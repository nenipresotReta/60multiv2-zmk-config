# PH60 Multi V2 个人维护分支（HHKB 配列）

这是 [PH Design](https://github.com/ph-design) 的 ZMK 配置的个人维护分支，只服务于我自己的一把 PH60 Multi V2，配列是 HHKB 加 3+1+3 分体空格。

和上游分开维护，是因为这个配列需要自己的默认布局和键位，另外这里也在试验一些针对自己使用场景的稳定性改动。这个分支不追求覆盖上游的其他型号。

## 硬件与配列

- **主控**：PH-Lite（nRF52840），板级定义在 `boards/arm/ph_lite`
- **PCB**：PH60 Multi V2，shield 是 `PH60-Multi_V2`，默认物理布局是 `ph60_hhkb_layout`
- **定位板**：[`hardware/ph60-multi-v2-hhkb-plate.stl`](hardware/ph60-multi-v2-hhkb-plate.stl)（约 285 × 107 × 5 mm）

配列是标准 HHKB 加 3+1+3 分体空格，以 `PH60-Multi_V2-hhkb.dtsi` 为准。底排两侧各留 1.5u 空位：

| Alt | Win | 空格 | Fn | 空格 | Win | Alt |
|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| 1u | 1.5u | 3u | 1u | 3u | 1.5u | 1u |

## 按键层

### Base

<img width="1362" height="598" alt="Base 层" src="https://github.com/user-attachments/assets/c2137521-3f77-4bb9-b46c-058cf124d961" />

Caps Lock 位置是 Ctrl。

### Function（按住 Fn）

<img width="1366" height="600" alt="Function 层" src="https://github.com/user-attachments/assets/7a1184f6-beec-4533-adda-ad5f438b82b2" />

方向键、翻页和媒体键沿用 HHKB 习惯。R 是重启键盘；再按住 B 进入 Bluetooth 层。

### Bluetooth（按住 Fn + B）

<img width="1364" height="599" alt="Bluetooth 层" src="https://github.com/user-attachments/assets/14795566-fc27-49b6-a806-88218d4476f1" />

- **1–5**：切换蓝牙配置
- **`` ` ``**：短按清除当前配对，按住 3 秒清除全部配对
- **Tab**：USB / 蓝牙切换
- **R**：进入 UF2 bootloader（刷固件用）
- **Backspace**：解锁 ZMK Studio
- **Enter**：软关机

hold-tap 预设了 `ht_def`、`ht_fast`、`ht_slow` 和四个用户槽 `ht_user1`–`ht_user4`，另外留了 5 个 combo 槽位，都可以在 ZMK Studio 里调整。

> **注意**：在 ZMK Studio 里保存过的键位会覆盖固件里编译的默认键位。刷新固件后发现键位"没变"，多半是这个原因。

## 下载和刷写固件

1. 打开 **Actions**，选 `v0.3-lts` 上最新一次成功的 **Build ZMK firmware**
2. 在 **Artifacts** 里下载需要的固件：

| 固件 | 用途 |
|---|---|
| `PH60-Multi_V2-Stability` | 日常使用，包含下面的稳定性修复 |
| `PH60-Multi_V2-Stability_Debug` | 同上，外加 USB 日志，用来排查问题 |
| `PH60-Multi_V2-Baseline` | 不带修复的原始版本，用于对比或回滚 |

3. 进入 bootloader（蓝牙层的 R 键），把解压出的 `.uf2` 拖进出现的 U 盘

每个固件包里还附带生效的 Kconfig、锁定的依赖清单、对应的配置 commit 和 SHA-256 校验值。

## 相对上游的修改

### 配列与键位

- 默认物理布局改为 HHKB（`PH60-Multi_V2.dtsi`）
- HHKB 布局的位置映射改为前四排与 ANSI 一致，切换布局时键位槽可以沿用
- 键位按 HHKB 重排：Base 层 Caps Lock 位置改为 Ctrl；Function 层改为 HHKB 式的方向键、翻页和媒体键；Studio 解锁移到 Bluetooth 层
- USB 设备名改为 `Nenip Ph60MV2 HHKB`

### 稳定性修复

修复以补丁形式存放在 `patches/0001-multiv2-stability.patch`，CI 会把它打到锁定版本的 ZMK 上（`ph-design/zmk@76f4eff`）再编译。主要包括：

- **蓝牙发送失败不再丢键**：键盘和媒体键报告失败时保留并按顺序重试，避免松开键的报告丢失导致"按键卡住"
- **主机未就绪时不断连**：刚连接、还没加密或还没订阅时（`-EPERM` / `-EINVAL` / `-EAGAIN`），最多等 5 秒，然后丢弃报告，但不断开连接。其他发送失败持续 1 秒后才主动断开重连
- **扫描队列溢出后恢复真实状态**：队列加大到 75；溢出时按实际按下的键重新同步，先补发松开再补发按下
- **广播失败自动重试**：蓝牙资源暂时不足时退避重试
- **看门狗更精确**：只在确实有未发出的报告、且 30 秒没有任何进展时才复位，长按和闲置都不会触发
- 调整 `zmk_physical_layouts_get_position_map` 中映射表的初始化长度（`memset` 按字节数计算）

完整说明和设计取舍见 [MULTIV2-STABILITY.txt](MULTIV2-STABILITY.txt)。

### 构建

- 实际生效的构建在 `.github/workflows/ble-recovery.yml`，只构建 PH60-Multi_V2 的三个版本
- `build.yaml` 是上游遗留的构建矩阵，现在没有用到
- 编译前会先运行 `tests/` 下的故障注入测试：用模拟的 Zephyr / 蓝牙接口，直接编译补丁里的真实代码

## 依赖版本

固件基于 `ph-design/zmk@76f4eff`（Zephyr `v3.5.0+zmk-fixes`），版本锁定在 `.github/workflows/ble-recovery.yml` 的 `ZMK_REVISION`。

## 许可

沿用上游的 MIT 许可。原始代码版权归 Team PH Design。
