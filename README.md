# WellSpan — Wearable Health Monitoring System

- **Presentation**: [`docs/EL2205-WellSpan-Presentation-EN.pdf`](docs/EL2205-WellSpan-Presentation-EN.pdf) — 20 slides, the English defence deck

- **Coursework report**: [`docs/EL2205-WellSpan-Report.pdf`](docs/EL2205-WellSpan-Report.pdf) — 37 pages, the submitted EL2205 report (student identifiers redacted)

A battery-powered wearable health-monitoring watch and its host-side link, built as Year-2 undergraduate coursework (EL2205 – Electronic System Application).
The device continuously acquires **heart rate / blood oxygen**, **body motion**, and **ambient temperature / humidity**, displays them on a menu-driven graphical UI, and streams them wirelessly to a host computer for a carer or clinician.

The repository contains the firmware of **two display generations** of the same product: an OLED-based prototype and the final colour-TFT version with a touch interface.

## Features

| Subsystem | Device | What the firmware does |
|---|---|---|
| **Heart rate / SpO₂** | MAX30102 (I²C PPG sensor) | FIFO configuration (rollover, `A_FULL = 15`), SpO₂ mode (RED + IR), 50 sps / 411 µs / 2048 nA range, LED currents, and the ported Maxim reference algorithm for heart rate and SpO₂ |
| **Motion / posture** | MPU6050 (6-axis IMU on I²C2) | Raw accelerometer + gyroscope reads, attitude angles, step counting and fall detection, plus a gyroscope "spirit level" screen |
| **Environment** | DHT11 | Bit-banged single-wire humidity + temperature acquisition |
| **Display (v1)** | Monochrome OLED | Graphics driver implementing the SSD1306 command set, with ASCII 6×8 and 8×16 font tables and a multi-screen UI (dashboard, heart rate, gyroscope) |
| **Display (v2, final)** | ILI9341 240×320 colour TFT + resistive touch | Full graphical menu with bitmap assets and button icons, dedicated heart-rate / gyroscope / alert screens, and two mini-games (Dino and Maze) |
| **Touch** | Resistive panel, bit-banged SPI (XPT2046-class command set `0xD0` / `0x90`) | `touch.c` implements the scan and the touch-point calibration; `App/LCD/LCD.h` selects the resistive mode with `USE_TP_TYPE = RTP` |
| **Wireless link** | HC-05 Bluetooth Classic on USART1 | `bt_manager` task that frames the measurements and streams them every 500 ms |
| **System structure** | — | Application layer (`App/`) with shared types (`app_types.h`) and one module per peripheral/task, on top of CubeMX-generated `Core/`, driven by cooperative time-slice polling in the main loop |

## Repository layout

```
firmware/
  tft-lcd/            final version — colour TFT + resistive touch, App-layer architecture
    App/
      app_types/      shared application types
      bt_task/        Bluetooth manager (HC-05 link + data framing)
      DHT/            DHT11 temperature & humidity driver
      LCD/            TFT driver, font and bitmap UI assets (menu, heart, gyro, games, alerts)
      max30102/       PPG heart-rate / SpO₂ driver
      mpu6050/        IMU driver, step counter and fall detection
      oled/           OLED driver (still compiled) and its font data
      TOUCH/          touchscreen: resistive driver (compiled) and an unused
                      capacitive FT6336 driver (ctp.c / ctiic.c, not in the build)
      TEST/           bring-up / self-test code
    Core/             CubeMX-generated startup and peripheral initialisation.
                      Note: Core/Src also holds stale copies of LCD.c, max30102.c,
                      mpu6050.c and touch.c that are NOT part of the Keil build —
                      the compiled sources are the ones under App/
    MDK-ARM/          Keil MDK project files (new.uvprojx, max30102_dht.uvprojx)
    new.ioc           STM32CubeMX configuration — regenerate the full project from this

  oled-versions/      four development iterations of the OLED prototype (V03 → V05 → V06 → V7),
                      each an independent CubeMX + Keil project, kept to show the design evolution
```

## Hardware and toolchain

- **MCU:** STM32L476RG (`startup_stm32l476xx.s`, `stm32l4xx_hal*`); the exact part and pin map are pinned in `firmware/tft-lcd/new.ioc`.
- **Sensors:** MAX30102, MPU6050, DHT11.
- **Display:** monochrome OLED (prototype) → ILI9341 colour TFT with a resistive touch panel (final).
- **Wireless:** HC-05 Bluetooth Classic module on USART1.
- **Toolchain:** STM32CubeMX + Keil MDK-ARM (ARM Compiler), STM32 HAL.

## How to build

1. Open `firmware/tft-lcd/new.ioc` in STM32CubeMX and **generate code** — this recreates the ST HAL / CMSIS `Drivers/` and `Middlewares/` trees, which are deliberately not committed (they are vendor-generated and would dwarf the actual project).
2. Open `firmware/tft-lcd/MDK-ARM/new.uvprojx` in Keil MDK-ARM, build and flash to the board.
3. The earlier `firmware/oled-versions/*/new/` projects build the same way, each from its own `new.ioc`.

## Notes

- Build outputs (`Objects/`, `Listings/`, `DebugConfig/`), vendor HAL/CMSIS trees and course reports/presentations are intentionally **not** committed — see `.gitignore`.
- `firmware/oled-versions/` intentionally keeps several near-duplicate snapshots: they document how the UI and the sensor pipeline evolved, and several later ones are nested copies of the previous stage.
- The Bluetooth host-side tool, the MATLAB cross-check script, the schematics, PCB layouts, Gerber files, report and presentation are not part of this repository.
- Measured performance figures quoted below come from the project report and bench testing; they cannot be reproduced from the source code alone.

## Team and attribution

A two-person team project (EL2205, October 2025 – April 2026). **With thanks to Mo Yan, the project's hardware lead.**

| Role | Who | Content |
|---|---|---|
| **Software and algorithms** | **Ni Youjia** | Module-per-peripheral application layer with cooperative time-slice scheduling; sensor bring-up and driver debugging (MPU6050, MAX30102, DHT11, ILI9341 TFT, resistive touch panel, HC-05); attitude-angle solving, step counting, fall detection and the alarm loop, PPG conditioning and heart-rate / SpO₂ computation; the Python PC host GUI. |
| **Hardware** | **Mo Yan** | Two-board architecture (main board + 2.8″ SPI TFT touch board); component selection, schematics and PCB layout, Gerber production files, hardware bring-up and testing, pin/bus allocation and the 3D-printed enclosure. |
| Joint | both | System integration, functional testing and the English technical report. |

### Selected implementation and results

- **Cooperative time-slice scheduling** in the main loop, all driven by `HAL_GetTick()`: 8 ms for the MAX30102, 100 ms for the MPU6050 *and* the UI refresh (30 ms on the spirit-level screen, every iteration on the two game screens), 2000 ms for the DHT11, 50 ms for the touch scan and 500 ms for the Bluetooth upload.
- **MAX30102 configuration** proven on the module: `0x08 = 0x1F` (FIFO rollover, `A_FULL = 15`), `0x09 = 0x03` (SpO₂ mode, RED + IR), `0x0A = 0x23` (50 sps, 411 µs pulse, 2048 nA range), `0x0C` / `0x0D = 0x2F` (LED currents).
- **PPG chain:** the official Maxim reference algorithm (R. Fraczkiewicz, MAX30102 EV-kit / MAXREFDES117) ported over a 2 s, 100-sample window at 50 Hz — DC removal with linear-trend removal, a Pearson correlation gate between RED and IR (≥ 0.8) for motion rejection, autocorrelation periodicity tracking (≥ 0.5, harmonic-safe) with heart rate clamped to 40–180 bpm, and SpO₂ from the official quadratic fit **SpO₂ = (−45.060·R + 30.354)·R + 94.845**. Finger presence is decided from the raw IR level (below 30 000 for 20 consecutive samples) and a 3 s staleness timeout invalidates the reading.
- **Step counting** on the filtered acceleration peak (`STEP_ACC_THRESHOLD = 1.6`, minimum interval 400 ms); **fall detection** triggers an alarm screen plus the buzzer and holds an alarm loop until the button is pressed for 2 s, and is disabled on the second game screen.
- **Mini-games:** Dino and Maze, both driven by a fixed 30 ms game tick — jump impulse −8 px/tick, gravity 0.9 px/tick², obstacle speed 4 px/tick rising by 1 per 5 points up to a cap of 10.
- **Attitude angles** solved with `atan2` from the accelerometer axes.
- *(From the report, not verifiable in this repository)*: bench comparison against 20 laboratory samples and a Samsung Galaxy Watch4 Classic — heart-rate error down to **±5 bpm**, SpO₂ error down to **±2%**, fall false-alarm rate **< 5 %** with alarm response **< 500 ms**; the PPG waveform and peak detection were cross-checked in MATLAB, and the Python host displays six parameters live over HC-05 and exports a TXT log.

---

- **课程报告**：[`docs/EL2205-WellSpan-Report.pdf`](docs/EL2205-WellSpan-Report.pdf) —— 提交版 EL2205 报告，37 页（已脱敏个人信息）

- **答辩 PPT**：[`docs/EL2205-WellSpan-Presentation-EN.pdf`](docs/EL2205-WellSpan-Presentation-EN.pdf) —— 英文答辩稿，20 页

# WellSpan —— 可穿戴健康监测系统

一块电池供电的可穿戴健康监测手表及其上位机链路，大二课程作业（EL2205 电子系统应用）。
设备持续采集**心率 / 血氧**、**体动**、**环境温湿度**，在菜单式图形界面上显示，并无线传送到上位机供护理人员或医生查看。

仓库里包含同一产品的**两代显示方案**固件：OLED 原型版，以及最终带触摸的彩色 TFT 版。

## 功能

| 子系统 | 器件 | 固件做的事 |
|---|---|---|
| **心率 / 血氧** | MAX30102（I²C 光电容积脉搏波传感器） | FIFO 配置（rollover、`A_FULL = 15`）、SpO₂ 模式（RED + IR）、50 sps / 411 µs / 2048 nA 量程、LED 电流，以及移植的 Maxim 官方参考算法求心率与血氧 |
| **体动 / 姿态** | MPU6050（六轴 IMU，接 I²C2） | 加速度计与陀螺仪原始读取、姿态角解算、计步与跌倒检测，另有陀螺仪"水平仪"界面 |
| **环境** | DHT11 | 软件时序（单总线 bit-bang）读温湿度 |
| **显示（第一代）** | 单色 OLED | 实现 SSD1306 命令集的图形驱动，配 ASCII 6×8 与 8×16 字库，多界面 UI（主界面、心率、陀螺仪） |
| **显示（第二代，最终版）** | ILI9341 240×320 彩色 TFT + 电阻触摸 | 完整图形菜单（位图背景 + 按钮图标）、独立的心率 / 陀螺仪 / 告警界面，以及两个小游戏（Dino 与 Maze） |
| **触摸** | 电阻屏，软件 SPI（XPT2046 类命令 `0xD0` / `0x90`） | `touch.c` 实现扫描与触摸点校准；`App/LCD/LCD.h` 以 `USE_TP_TYPE = RTP` 选定电阻模式 |
| **无线链路** | HC-05 经典蓝牙，接 USART1 | `bt_manager` 任务组帧，每 500 ms 上传一次测量数据 |
| **系统结构** | — | 应用层 `App/`（共享类型 `app_types.h` + 每个外设/任务一个模块）叠在 CubeMX 生成的 `Core/` 之上，主循环用协作式时间片轮询调度 |

## 仓库结构

```
firmware/
  tft-lcd/            最终版 —— 彩色 TFT + 电阻触摸，App 分层架构
    App/
      app_types/      应用层共享类型
      bt_task/        蓝牙管理（HC-05 链路与数据组帧）
      DHT/            DHT11 温湿度驱动
      LCD/            TFT 驱动、字库与位图 UI 资源（菜单、心率、陀螺仪、游戏、告警）
      max30102/       心率 / 血氧驱动
      mpu6050/        IMU 驱动、计步与跌倒检测
      oled/           OLED 驱动（仍在编译清单里）及其字库数据
      TOUCH/          触摸：电阻驱动（参与编译）+ 一份未使用的
                      电容 FT6336 驱动（ctp.c / ctiic.c，不在编译清单里）
      TEST/           调试自检代码
    Core/             CubeMX 生成的启动与初始化代码。
                      注意：Core/Src 下另有 LCD.c、max30102.c、
                      mpu6050.c、touch.c 的陈旧副本，它们**不参与 Keil 编译** ——
                      真正编译的是 App/ 下的那一份
    MDK-ARM/          Keil MDK 工程（new.uvprojx、max30102_dht.uvprojx）
    new.ioc           STM32CubeMX 配置 —— 用它重新生成完整工程

  oled-versions/      OLED 原型的四个迭代版本（V03 → V05 → V06 → V7），
                      每个都是独立的 CubeMX + Keil 工程，保留下来展示设计演进
```

## 硬件与工具链

- **主控**：STM32L476RG（`startup_stm32l476xx.s`、`stm32l4xx_hal*`）；具体型号与引脚映射在 `firmware/tft-lcd/new.ioc` 里。
- **传感器**：MAX30102、MPU6050、DHT11。
- **显示**：单色 OLED（原型）→ ILI9341 彩色 TFT + 电阻触摸屏（最终版）。
- **无线**：HC-05 经典蓝牙模块，接 USART1。
- **工具链**：STM32CubeMX + Keil MDK-ARM（ARM Compiler）、STM32 HAL。

## 如何编译

1. 用 STM32CubeMX 打开 `firmware/tft-lcd/new.ioc` 并**生成代码** —— 这会重建 ST HAL / CMSIS 的 `Drivers/` 与 `Middlewares/` 目录树；这两棵目录是厂商生成的，故**有意未入库**（它们会淹没真正的项目代码）。
2. 用 Keil MDK-ARM 打开 `firmware/tft-lcd/MDK-ARM/new.uvprojx`，编译并烧录到板子。
3. 早期的 `firmware/oled-versions/*/new/` 工程同理，各自用自己目录下的 `new.ioc` 生成后编译。

## 说明

- 编译产物（`Objects/`、`Listings/`、`DebugConfig/`）、厂商 HAL/CMSIS 目录树以及课程报告/答辩 PPT **有意未入库**，见 `.gitignore`。
- `firmware/oled-versions/` 里刻意保留了几份近乎重复的快照：它们记录了 UI 与传感器数据链路的演进过程，后面几份是前一阶段的嵌套副本。
- 上位机蓝牙工具、MATLAB 交叉验证脚本、原理图、PCB 版图、Gerber 文件、技术报告与答辩 PPT 都不在本仓库内。
- 下面引用的实测性能数字来自项目报告与台上测试，**仅凭源码无法复现**。

## 团队与分工

两人团队项目（EL2205，2025 年 10 月 – 2026 年 4 月）。**特别感谢莫妍（Mo Yan）—— 本项目的硬件负责人。**

| 分工 | 谁 | 内容 |
|---|---|---|
| **软件与算法** | **Ni Youjia** | 每个外设一个模块的应用层 + 协作式时间片调度；传感器调试与驱动（MPU6050、MAX30102、DHT11、ILI9341 TFT、电阻触摸屏、HC-05）；姿态角解算、计步、跌倒检测与告警循环、PPG 信号调理与心率 / 血氧计算；Python 上位机界面。 |
| **硬件** | **莫妍（Mo Yan）** | 双板架构（主控板 + 2.8 英寸 SPI TFT 触摸扩展板）；器件选型、原理图与 PCB 布局布线、Gerber 生产文件、硬件联调与测试、总线/引脚分配、3D 打印外壳结构设计。 |
| 共同完成 | 两人 | 系统联调、功能测试与英文技术报告。 |

### 关键实现与实测结果

- **协作式时间片调度**（全部基于 `HAL_GetTick()`）在主循环里跑：MAX30102 为 8 ms；MPU6050 与 UI 刷新同为 100 ms（水平仪界面 30 ms，两个游戏界面每轮都刷）；DHT11 为 2000 ms；触摸扫描 50 ms；蓝牙上传 500 ms。
- **MAX30102 配置**（在模块上验证过）：`0x08 = 0x1F`（FIFO rollover，`A_FULL = 15`）、`0x09 = 0x03`（SpO₂ 模式，RED + IR）、`0x0A = 0x23`（50 sps、411 µs 脉宽、2048 nA 量程）、`0x0C` / `0x0D = 0x2F`（LED 电流）。
- **PPG 信号链**：移植 Maxim 官方参考算法（R. Fraczkiewicz，MAX30102 EV-kit / MAXREFDES117），在 50 Hz、2 s / 100 样点窗口上运行 —— 去直流 + 线性趋势去除，RED/IR 皮尔逊相关门（≥ 0.8）抑制运动干扰，自相关周期跟踪（≥ 0.5，抗谐波）得到心率并限幅在 40–180 bpm，血氧用官方二次拟合 **SpO₂ = (−45.060·R + 30.354)·R + 94.845**。手指存在由原始 IR 电平判定（连续 20 次低于 30 000），另有 3 s 数据陈旧超时使读数失效。
- **计步**基于滤波后加速度峰值（`STEP_ACC_THRESHOLD = 1.6`、最短间隔 400 ms）；**跌倒检测**触发告警画面与蜂鸣器，并进入告警循环，直到按键被按住 2 s 才解除，且在第二个游戏界面下不生效。
- **小游戏**：Dino 与 Maze，都跑固定的 30 ms 游戏节拍 —— 起跳冲量 −8 px/tick、重力 0.9 px/tick²、障碍速度 4 px/tick 起、每 5 分加 1、上限 10。
- **姿态角**用 `atan2` 由加速度计各轴解算。
- *（以下来自项目报告，无法用本仓库源码验证）*：以 20 组实验室样本、对标 Samsung Galaxy Watch4 Classic 实测 —— 心率误差低至 **±5 bpm**、血氧误差低至 **±2%**，跌倒误报率 **< 5%**、报警响应 **< 500 ms**；PPG 波形与峰检测用 MATLAB 交叉验证；Python 上位机经 HC-05 实时显示六项参数并导出 TXT 日志。
