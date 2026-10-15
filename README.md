# WellSpan — Wearable Health Monitoring System

A battery-powered wearable health-monitoring watch and its host-side link, built as Year-2 undergraduate coursework (EL2205 – Electronic System Application).
The device continuously acquires **heart rate / blood oxygen**, **body motion**, and **ambient temperature / humidity**, displays them on a menu-driven graphical UI, and streams them wirelessly to a host computer for a carer or clinician.

The repository contains the firmware of **two display generations** of the same product: an OLED-based prototype and the final colour-TFT version with a touch interface.

## Features

| Subsystem | Device | What the firmware does |
|---|---|---|
| **Heart rate / SpO₂** | MAX30102 (I²C PPG sensor) | FIFO-driven sampling, LED current and sample-rate configuration, rolling-average heart-rate estimation and SpO₂ calculation |
| **Motion / posture** | MPU6050 (6-axis IMU) | Raw accelerometer + gyroscope reads, zero-offset calibration, gyroscope "level" screen and motion/impact detection |
| **Environment** | DHT11 / DHT20 | Bit-banged and I²C humidity + temperature acquisition with checksum validation |
| **Display (v1)** | 0.96″ SSD1306 OLED | Graphics driver plus ASCII/Chinese font tables, multi-screen UI (dashboard, heart rate, gyroscope) |
| **Display (v2, final)** | Colour TFT LCD + resistive touch | Full graphical menu with bitmap assets and button icons, dedicated heart-rate / gyroscope / alert screens, and two built-in mini-games used as rehabilitation motivation |
| **Wireless link** | HC-05 Bluetooth Classic | `bt_manager` task that pairs with the host, frames the measurements and streams them continuously |
| **System structure** | — | Explicit application layer (`App/`) with shared types (`app_types.h`) and one module per peripheral/task, on top of CubeMX-generated `Core/` |

## Repository layout

```
firmware/
  tft-lcd/            final version — colour TFT + resistive touch, App-layer architecture
    App/
      app_types/      shared application types
      bt_task/        Bluetooth manager (HC-05 link + data framing)
      DHT/            temperature & humidity driver
      LCD/            TFT driver, fonts and bitmap UI assets (menu, heart, gyro, games, alerts)
      max30102/       PPG heart-rate / SpO₂ driver
      mpu6050/        IMU driver
      oled/           OLED driver kept alongside for the early UI
      TOUCH/          resistive touchscreen handling
      TEST/           bring-up / self-test code
    Core/             CubeMX-generated startup and peripheral initialisation
    MDK-ARM/          Keil MDK project files (new.uvprojx, max30102_dht.uvprojx)
    new.ioc           STM32CubeMX configuration — regenerate the full project from this

  oled-versions/      four development iterations of the OLED prototype (V03 → V05 → V06 → V7),
                      each an independent CubeMX + Keil project, kept to show the design evolution
```

## Hardware and toolchain

- **MCU:** STM32L4 series (STM32L476 class) — the exact part is pinned in `firmware/tft-lcd/new.ioc`.
- **Sensors:** MAX30102, MPU6050, DHT11 / DHT20.
- **Display:** SSD1306 OLED (prototype) → colour TFT LCD with resistive touch (final).
- **Wireless:** HC-05 Bluetooth Classic module.
- **Toolchain:** STM32CubeMX + Keil MDK-ARM (ARM Compiler), STM32 HAL.

## How to build

1. Open `firmware/tft-lcd/new.ioc` in STM32CubeMX and **generate code** — this recreates the ST HAL / CMSIS `Drivers/` and `Middlewares/` trees, which are deliberately not committed (they are vendor-generated and would dwarf the actual project).
2. Open `firmware/tft-lcd/MDK-ARM/new.uvprojx` in Keil MDK-ARM, build and flash to the board.
3. The earlier `firmware/oled-versions/*/new/` projects build the same way, each from its own `new.ioc`.

## Notes

- Build outputs (`Objects/`, `Listings/`, `DebugConfig/`), vendor HAL/CMSIS trees and course reports/presentations are intentionally **not** committed — see `.gitignore`.
- `firmware/oled-versions/` intentionally keeps several near-duplicate snapshots: they document how the UI and the sensor pipeline evolved, and several later ones are nested copies of the previous stage.
- The Bluetooth host-side tool that pairs with the watch is not part of this repository.

---

# WellSpan —— 可穿戴健康监测系统

一块电池供电的可穿戴健康监测手表及其上位机链路，大二课程作业（EL2205 电子系统应用）。
设备持续采集**心率 / 血氧**、**体动**、**环境温湿度**，在菜单式图形界面上显示，并无线传送到上位机供护理人员或医生查看。

仓库里包含同一产品的**两代显示方案**固件：OLED 原型版，以及最终带触摸的彩色 TFT 版。

## 功能

| 子系统 | 器件 | 固件做的事 |
|---|---|---|
| **心率 / 血氧** | MAX30102（I²C 光电容积脉搏波传感器） | FIFO 驱动采样、LED 电流与采样率配置、滑动平均心率估计与血氧计算 |
| **体动 / 姿态** | MPU6050（六轴 IMU） | 加速度计与陀螺仪原始读取、零偏标定、陀螺仪"水平仪"界面与运动/撞击检测 |
| **环境** | DHT11 / DHT20 | 软件时序（bit-bang）与 I²C 两种方式读温湿度，带校验 |
| **显示（第一代）** | 0.96″ SSD1306 OLED | 图形驱动 + ASCII/中文字库，多界面 UI（主界面、心率、陀螺仪） |
| **显示（第二代，最终版）** | 彩色 TFT LCD + 电阻触摸 | 完整图形菜单（位图背景 + 按钮图标）、独立的心率 / 陀螺仪 / 告警界面，还内置两个小游戏作为康复激励 |
| **无线链路** | HC-05 经典蓝牙 | `bt_manager` 任务：与上位机配对、组帧并持续上传测量数据 |
| **系统结构** | — | 明确的应用层 `App/`：共享类型定义（`app_types.h`）+ 每个外设/任务一个模块，叠在 CubeMX 生成的 `Core/` 之上 |

## 仓库结构

```
firmware/
  tft-lcd/            最终版 —— 彩色 TFT + 电阻触摸，App 分层架构
    App/
      app_types/      应用层共享类型
      bt_task/        蓝牙管理（HC-05 链路与数据组帧）
      DHT/            温湿度驱动
      LCD/            TFT 驱动、字库与位图 UI 资源（菜单、心率、陀螺仪、游戏、告警）
      max30102/       心率 / 血氧驱动
      mpu6050/        IMU 驱动
      oled/           早期 UI 用的 OLED 驱动（保留）
      TOUCH/          电阻触摸处理
      TEST/           调试自检代码
    Core/             CubeMX 生成的启动与初始化代码
    MDK-ARM/          Keil MDK 工程（new.uvprojx、max30102_dht.uvprojx）
    new.ioc           STM32CubeMX 配置 —— 用它重新生成完整工程

  oled-versions/      OLED 原型的四个迭代版本（V03 → V05 → V06 → V7），
                      每个都是独立的 CubeMX + Keil 工程，保留下来展示设计演进
```

## 硬件与工具链

- **主控**：STM32L4 系列（STM32L476 档），具体型号见 `firmware/tft-lcd/new.ioc`。
- **传感器**：MAX30102、MPU6050、DHT11 / DHT20。
- **显示**：SSD1306 OLED（原型）→ 带电阻触摸的彩色 TFT LCD（最终版）。
- **无线**：HC-05 经典蓝牙模块。
- **工具链**：STM32CubeMX + Keil MDK-ARM（ARM Compiler）、STM32 HAL。

## 如何编译

1. 用 STM32CubeMX 打开 `firmware/tft-lcd/new.ioc` 并**生成代码** —— 这会重建 ST HAL / CMSIS 的 `Drivers/` 与 `Middlewares/` 目录树；这两棵目录是厂商生成的，故**有意未入库**（它们会淹没真正的项目代码）。
2. 用 Keil MDK-ARM 打开 `firmware/tft-lcd/MDK-ARM/new.uvprojx`，编译并烧录到板子。
3. 早期的 `firmware/oled-versions/*/new/` 工程同理，各自用自己目录下的 `new.ioc` 生成后编译。

## 说明

- 编译产物（`Objects/`、`Listings/`、`DebugConfig/`）、厂商 HAL/CMSIS 目录树以及课程报告/答辩 PPT **有意未入库**，见 `.gitignore`。
- `firmware/oled-versions/` 里刻意保留了几份近乎重复的快照：它们记录了 UI 与传感器数据链路的演进过程，后面几份是前一阶段的嵌套副本。
- 与手表配对的上位机工具不在本仓库内。
