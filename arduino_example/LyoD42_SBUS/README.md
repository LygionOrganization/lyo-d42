# Lyo-D42 S.BUS 独立教学例程

此 `.ino` 从 lymino 双轮遥控程序迁移，轮距改为159mm，串口明确使用Serial0。不修改原lymino工程。它需要ID30 TTL Node(A)和相匹配的S.BUS接收机，不包含主固件网页、JSON、CAN或IMU功能；上传后会替换主固件。

Arduino IDE：ESP32S3 Dev Module；ESP32 Arduino3.3.0；8MB Flash，OPI PSRAM；USB CDC On Boot Disabled。将本仓库 `lib/lygion_devs` 安装到 Arduino libraries。使用Type-C UART下载与115200串口监视。

ID1左轮正转前进，ID2右轮负转前进；TX4/RX5，1Mbps。CH2前后、CH1转向；CH5先低位停车，再摇杆回中并拨高使能。CH7沿用旧教学接线，控制 **Node PWM1**，不是Lyo-D42 GPIO21射灯。接收机端点282/1002/1722、通道方向、失控位解释均沿用原设备，换遥控器必须修改并实测，不能作为通用出厂控制。

示例最大原始速度500（对应25000步/s，约0.327m/s），电流寄存器200、加速度15、制动50。节点读取失联200ms停车停用；停止零速只发送一次。没有源时间戳，无法识别Node持续返回旧通道数据的情况。应在接收机设置CH5低位作为失控输出，并架空验证无线断开后失控标志真实生效。
