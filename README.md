# Lyo-D42 · 0.4.0

ESP32-S3 双步进移动底盘固件，含中英文可切换网页、USB CDC、UART0、HTTP、WebSocket、CAN 控制，以及 Python / Rust 上位机 SDK。

## 快速入口

- [完整 JSON / CAN 指令集](docs/LYO_D42_API.md)
- [USB CDC / UART 教程](docs/LYO_D42_USB_CDC.md) · [CAN 教程](docs/LYO_D42_CAN.md)
- [Python SDK 与例程](sdk/python/README.md) · [Rust SDK 与例程](sdk/rust/README.md)
- [构建与发布](docs/RELEASING.md) · [验证记录](docs/RELEASE_VALIDATION.md) · [实机验收](docs/LYO_D42_VALIDATION.md)
- [English](docs/LYO_D42_EN.md) · [贡献约定](AGENTS.md) · [上游来源](docs/UPSTREAM.md)

## 当前行为

- 硬上限 **0.32m/s**，网页、固件和SDK一致；驱动寄存器向下限制到489，量化后不超限。
- 速度同时携带 `accel_mps2`，单位m/s²；柔性停止按指定加速度减速。强制停止/故障/心跳超时不等待渐停。
- 自动驻坡开启仍可驾驶，停下后电流参数60保持；关闭后停转释放输出并读回确认，失败重试。
- 默认心跳3秒，可用T809或网页设置改为100..60000ms，重启保留。状态查询不续期，定距和渐停也需要心跳。
- wheel反馈包含速度、电压、温度及可选电流raw/valid。电流原始值不是校准后的安培。
- 竖装姿态：交换Roll/Pitch，新Pitch减90°。驾驶页仅倾角，IMU调试页保留相对Yaw。
- OLED从缓存读取电池电压；空闲且控制通信安静1秒后后台刷新，之后最多每60秒一次，繁忙则顺延，不占用主命令循环。

## 硬件

左轮ID1正转向前，右轮ID2电机负转向前；公开API左右轮均以前进为正。轮距159mm，轮径69mm，减速比5.1818181818182，电机3200微步/圈。TTL驱动总线UART1 TX4/RX5、1Mbps。CAN TX14/RX13、500kbit/s并将物理拨动开关选到CAN。OLED128×32，SDA6/SCL7；GPIO21射灯。主程序不依赖TTL Node或遥控器。

Type-C USB是原生CDC上位机端口；Type-C UART用于下载及UART0 JSON，UART0为1Mbps且需T604打开回复。总线电源触点为电池电压，按硬件标注核对接线。

## 构建、连接与例程

```powershell
# Regenerate after changing web sources, then build:
python tools/embed_web.py
pio run
python -m pip install "./sdk/python[all]"
python sdk/python/examples/control.py usb COM7
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- http http://192.168.4.1
```

示例默认只读；明确添加`--drive`才以0.03m/s执行1秒演示并停车。不要让独立后台心跳无限延续失效应用的旧速度。

默认Wi-Fi为Robot / 12345678，打开http://192.168.4.1；右上角切换语言。松手按柔性停止/驻坡设置处理；空格、失焦、离开页面请求强制停用。固件上电和故障恢复不会自动运动。

T800没有JSON执行ACK；HTTP202仅确认入队，需匹配rid执行结果。CAN有ACK及状态帧，详见协议。定距/定角使用驱动位置模式，长距离分段，不能代替地面定位。

## 发布状态

本目录提供可构建发布候选源码、可复现打包工具和测试；**主机模拟与编译不等于实机验收**。未验证的电流寄存器支持、噪声、实际减速、断线停车和长时间温升应在实机验收表中记录。发布包标记hardware_validated=false。原始步进调试输出不享有底盘API的相同看门狗，调试后主动关闭。

许可及来源见[LICENSE](LICENSE)和各库随附许可。继承的RoboCore示例/工具保留作参考，不作为Lyo-D42协议规范；本产品发布使用tools/package_release.py，不使用旧RoboCore发布脚本。

产品名称为 `Lyo-D42`，项目标识为 `lyo-d42`。NVS 新配置写入 `lyo-d42`；仅在新命名空间不存在时读取旧命名空间，以兼容升级。工作目录可自行命名，不影响产品标识。
