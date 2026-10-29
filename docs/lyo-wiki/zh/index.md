# Lyo-D42 双驱移动底盘

Lyo-D42 基于 ESP32-S3 / RoboCore S3，配备 ID1、ID2 两个步进驱动器，支持手机网页、USB CDC、HTTP、WebSocket 和 CAN 控制。标准产品没有 TTL Node；航模接收机和 TTL Node 仅用于独立遥控实验程序。

| 使用目标 | 阅读入口 |
| --- | --- |
| 新手，让底盘动起来 | [快速开始与网页控制](quickstart.md) |
| 接线、扩展设备 | [硬件与供电](hardware.md) |
| 查询指令、CAN 帧 | [JSON / CAN 协议](api.md) |
| Python / Rust / ROS2 集成 | [上位机开发](host.md) |
| 修改固件与网页 | [进阶二次开发](development.md) |
| 无法运动、OLED、驻坡 | [故障排查](faq.md) |

本文对应 Lyo-D42 0.4.0 固件。几何参数与默认电流来自项目配置，负载、停车距离和坡面保持能力需按实机验证。

速度硬上限 **0.32 m/s**，默认心跳3秒。新增独立教程：[USB CDC / UART](usb-cdc.md)、[CAN 总线通信](can.md)。下载及 SDK 例程见[上位机开发](host.md)。
