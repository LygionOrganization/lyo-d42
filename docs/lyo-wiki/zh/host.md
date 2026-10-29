# 上位机开发：Python 与 Rust SDK

适用于Lyo-D42固件 **0.4.0**。先查询状态，再明确启用低速例程。速度上限0.32m/s；所有新运动例程同时发送m/s²加速度。默认心跳3秒，同一时刻只使用一个控制源。

## 下载

- [完整项目源码（含两个SDK及例程）](assets/Lyo-D42-0.4.0-source.zip)
- [Python SDK](assets/Lyo-D42-0.4.0-python-sdk.zip)
- [Rust SDK](assets/Lyo-D42-0.4.0-rust-sdk.zip)
- [固件包](assets/Lyo-D42-0.4.0-firmware.zip)
- [校验与构建清单](assets/manifest.json) · [SHA256SUMS](assets/SHA256SUMS.txt)

这些是本次构建的发布候选文件，不代表已完成实机验收；具体检查见源码内docs/RELEASE_VALIDATION.md。固件包按清单地址烧录，不能将firmware.bin写到0x0。

## 按通信方式开始

| 方式 | Python | Rust | 教程 |
|---|---|---|---|
|USB CDC|SerialTransport|SerialTransport|[USB CDC教程](usb-cdc.md)|
|UART0|SerialTransport(uart=True)，1Mbps|SerialTransport(...,true)|[UART说明](usb-cdc.md)|
|HTTP|HttpTransport|HttpTransport|下文|
|WebSocket|WebSocketTransport|WebSocketTransport（ws://）|下文|
|CAN|python-can：SocketCAN/PCAN/Vector/SLCAN等|Linux SocketCAN、跨平台SLCAN、CanBus扩展|[CAN教程](can.md)|

完整字段、CAN字节表、ACK语义见[JSON/CAN接口](api.md)。CAN不支持任意通用JSON命令，反馈也不包含完整JSON字段。

## Python最小程序

完整源码解压到Lyo-D42目录，在该目录执行：

```powershell
python -m pip install "./sdk/python[all]"
python sdk/python/examples/control.py usb COM7
python sdk/python/examples/control.py http http://192.168.4.1
python sdk/python/examples/control.py ws ws://192.168.4.1/ws
```

SDK独立包解压后在包含pyproject.toml的目录执行`python -m pip install ".[all]"`，例程路径为`examples/control.py`。

```python
from lyo_d42 import Client, HttpTransport
with Client(HttpTransport('http://192.168.4.1')) as robot:
    print(robot.status())
    # Uncomment only when motion is intended:
    # robot.drive_for(0.03, 0, 0.2, 1)
```

`drive_for`有时限并在finally请求强制停车。它不是柔性停车示例。自主控制应用应把导航输入的新鲜度与输出命令绑定；SDK没有独立永续心跳。

## Rust最小程序

安装Rust和平台C++工具链后：

```powershell
cargo test --manifest-path sdk/rust/Cargo.toml
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- usb COM7
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- http http://192.168.4.1
```

项目依赖可写 `lyo-d42 = { path = "../Lyo-D42/sdk/rust" }`，独立包直接使用其解压目录。SDK内README列出所有构造器和控制方法，AGENTS.md规定协议兼容和测试要求。

## 控制方法与生命周期

Python/Rust均提供twist、左右轮速度、定距、定角、心跳、驻坡、灯光、超时配置、强制停止和状态查询。Python定距名move，Rust为move_distance。运动目标与加速度同时传入。普通例程默认只读，添加`--drive`明确运行1秒0.03m/s。

- HTTP POST返回202表示入队，SDK再用rid轮询共享结果；多个客户端同时写会导致结果被覆盖。
- Serial/WS忽略无关消息并匹配rid；CAN匹配node、seq、ACK和完整分片。
- 无自动重发运动；错误时先读状态。关闭transport不会自动发送停止，防止只读客户端退出影响别的控制源。
- 定距、转角和柔性停止要持续heartbeat，直到完成。CAN没有JSON state名称，可结合enabled、轮速和驻坡策略处理完成，不伪造地面里程。
- `current_raw`是驱动反馈原始值，不是A；`current_valid=false`不能解读为无电流。

## 与ROS2/导航应用连接

T804的v/w分别对应Twist.linear.x/angular.z。若同时平移/转向导致任一轮超过0.32m/s，SDK拒绝；应用可先同比缩放左右轮再调用wheels。输入消息过期时发送stop，不要让旧速度被后台心跳永久续期。这里提供SDK和示例，不提供融合里程计或完整ROS2节点。
