# 硬件、供电与扩展

| 项目 | 配置 |
| --- | --- |
| 底盘尺寸 | 214.5 × 197.6 × 86.5 mm；顶部两根 1020 型材 |
| 轮距 / 轮径 | 159 mm / 69 mm |
| 电机 / 减速比 | 3200 步/电机轴圈；5.1818181818182 |
| 左右轮 | 左 ID1 正转前进；右 ID2 反转前进 |
| 设备总线 | TTL ×2、RS485 ×1，共用 UART1，TX4 / RX5，1 Mbps |
| CAN | TX14 / RX13，500 kbit/s，物理开关选择 CAN |
| OLED | 128×32，I²C SDA6 / SCL7，地址 0x3C |
| 射灯 | GPIO21，20 kHz PWM；该引脚不用于蜂鸣器 |
| USB | Type-C UART 下载；原生 Type-C USB 提供 CDC 上位机通信 |

推荐使用匹配的 5s 德伟式工具电池（常标 18V / 20V），3s 可用但力矩降低。总线电源触点直接引出电池电压，扩展模块必须匹配电压；不要将低压 TTL 逻辑供电要求和总线电源混淆。

TTL 为 5264-3P，RS485 为 XH2.54-4P（A/B/电源+/−），CAN 2+2 包含信号与 XT30 电源。核对实物标识和极性，断电接线。长线扩展可结合 RS485-TTL Power Bridge 和末端降压。

扩展设备使用不重复的 ID3–253；标准底盘不需要 ID30 TTL Node。OLED、IMU 缺失只影响显示或姿态，不影响基本双轮运动。

更多基础操作见[设备 ID 与波特率](../../../tutorials/device-id-and-baudrate.md)、[供电与接线](../../../tutorials/power-and-wiring-basics.md)。
