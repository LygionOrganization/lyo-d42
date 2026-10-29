# CAN 总线控制教程

## 硬件与接线

使用支持 **Classic CAN、500kbit/s、29位扩展帧** 的USB-CAN适配器，或带CAN控制器和收发器的Linux主机。将底盘物理通信选择开关拨到CAN，连接CAN-H到CAN-H、CAN-L到CAN-L，并建立合适的参考地。总线两端各120Ω终端电阻；断电测H/L之间通常约60Ω。不要在每个中间节点重复加终端。底盘CAN控制器TX14/RX13是主控到收发器的信号，不应直接接CAN-H/L。

CAN 2+2接口中的电源触点是电池电压。不要将电池正极接入适配器信号地或主机USB供电端。以产品接口标注核对极性。

## Linux SocketCAN

适配器驱动成功后先配置500kbit/s；替换can0为实际接口：

```sh
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000
sudo ip link set can0 up
ip -details -statistics link show can0
candump can0
```

USB-CAN是否支持SocketCAN取决于型号/固件。Windows PCAN、Vector等需厂商驱动；使用python-can对应backend。普通USB串口不等于CAN适配器。SLCAN只适用于LAWICEL兼容固件。

## 先读状态，不运动

默认node=1。以下使用seq=42，扩展帧ID `18EF0108`，8字节数据：

```sh
cansend can0 18EF0108#2A00000000000000
```

应收到ACK `18EE0108`（`2A 00 00`）以及`18ED0100/0101/0102`三条状态帧。核对序号、节点、valid、enabled和速度。完整字段见[CAN指令集](LYO_D42_API.md)。状态查询不会刷新运动心跳。

## Python SDK（跨适配器）

解压[项目源码](LYO_D42_HOST.md)后：

```powershell
python -m pip install "./sdk/python[all]"
python sdk/python/examples/control.py can can0 --interface socketcan
python sdk/python/examples/control.py can PCAN_USBBUS1 --interface pcan
```

Vector、SLCAN等需在应用中按python-can该backend的参数构造Bus：

```python
import can
from lyo_d42 import Client, CanTransport
bus = can.Bus(interface='socketcan', channel='can0', bitrate=500000)
with Client(CanTransport(bus, node=1)) as robot:
    print(robot.status())
    # Explicit opt-in after checking wiring and wheel clearance:
    # robot.drive_for(0.03, 0, 0.2, 1)
```

`--drive`明确执行1秒低速运动并停车。SDK校验轮速<=0.32m/s、CAN加速度0.01..2.54m/s²，并匹配所有反馈帧的序号。SDK不自动重发。

## 手工构造运动、心跳与停车

仅在确认可运动的条件下使用：

```text
node=1, seq=42, v=0.1m/s, w=0, acceleration=0.2m/s²
ID: 18EF0184
data: 2A 00 64 00 00 00 14 81
```

- op4对应T804，op bit7=1选择可靠槽。
- 数据中的bit7（byte7）是另一处独立标志：1代表物理加速度编码，byte6=0x14=20即0.2m/s²；byte7 bit0=1允许柔性停止。
- 每次控制使用递增seq；建议每100ms发送新目标，或者在仍希望保持目标时发op10心跳。
- 心跳示例：`18EF010A#2B00000000000000`。
- 强制停车示例：`18EF0100#2C00000000000000`，不经过渐停。ACK仅表示接收。
- 柔性停车：保持op4但v/w设0，保留物理加速度和soft_stop标志，持续心跳至反馈enabled=false（驻坡开启时会保持使能）。

以上单帧示例不会自动循环；不要用无限发送器掩盖应用失效。默认3秒没有新有效运动/心跳，固件停用。

## 心跳与驻坡配置

停车后用op12设置3000ms：

```sh
cansend can0 18EF018C#2D00B80B00000000
```

结果是`18EE010C`，不是op9。旧op9只有1字节×10ms的超时字段，最大2550ms，新程序不要用它设置3秒。驻坡使用op11，byte2=1开启、0关闭；开关开启仍可驾驶，停下后电流参数60保持。

## Rust适配器

```sh
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- can can0
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- slcan COM9
```

SocketCAN示例仅Linux；SLCAN跨平台，并将适配器设为500kbit/s。Windows厂商二进制适配器通过`CanBus` trait接入厂商SDK，或使用Python SDK，不能直接套用SLCAN。

## 排障与限制

无ACK先检查终端、500kbit/s、扩展帧、DLC8、node以及拨动开关。bus-off检查物理总线后再恢复。超时不等于未执行；先读状态，不盲目重发定距动作。CAN定距/角度字段较JSON窄，SDK超范围会拒绝。CAN状态是精简反馈，不含完整JSON温度、倾角和配置。固件0.4.0以前缺少第三条电流状态帧，升级后再使用本SDK。
