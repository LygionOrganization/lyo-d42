# USB CDC 与 UART 控制教程

## 选择接口

- **Type-C USB**：ESP32-S3原生USB CDC，运行固件后用于上位机。Windows通常为COM口，Linux常为`/dev/ttyACM0`。SDK可填写115200；原生CDC传输不是按这个波特率输出物理UART。
- **Type-C UART**：板载USB转串口，连接UART0，固件JSON通信波特率 **1000000**。该口也用于自动下载。UART0默认不输出反馈，SDK的uart模式会先发送T604开启回复。
- 不要混淆主控USB/UART和电机TTL总线（UART1 TX4/RX5）。不要在连接上位机时占用电机总线针脚。烧录期间关闭上位机和串口监视器。

## 安装与只读检查

从[上位机页面](LYO_D42_HOST.md)下载源码/SDK，解压后进入项目目录：

```powershell
python -m pip install "./sdk/python[all]"
python -m serial.tools.list_ports
python sdk/python/examples/control.py usb COM7
# UART端口使用1Mbps并自动开启回复：
python sdk/python/examples/control.py uart COM8
```

Linux将COM7替换为`/dev/ttyACM0`，确保账号具有串口访问权限（常见为dialout组）。端口名称以实际枚举为准。状态查询应显示T808、ok:true、两轮valid及当前配置；没有驱动器也应能连接，但运动会被拒绝。SDK默认只读，不会使能电机。

## 原始JSON通信

每条指令末尾加换行`\n`；最长255字节，不含换行。输出中可能夹杂启动日志和其它结果，必须按rid匹配，不能假定下一行就是回复。

```python
import json
import time
import serial

with serial.Serial('COM7', 115200, timeout=0.1, write_timeout=0.5) as port:
    port.write(b'{"T":808,"rid":42}\n')
    end = time.monotonic() + 2
    buffer = bytearray()
    while time.monotonic() < end:
        buffer.extend(port.read(1))
        if len(buffer) > 8192:
            raise RuntimeError('Reply too large')
        if buffer.endswith(b'\n'):
            try:
                response = json.loads(buffer)
            except (ValueError, UnicodeError):
                response = None
            buffer.clear()
            if isinstance(response, dict) and response.get('rid') == 42:
                print(response)
                break
    else:
        raise TimeoutError('No matching response')
```

UART0模式将端口波特率设为1000000，并在查询前写入`{"T":604,"echo":0,"uart":1,"usb":1}\n`。此设置为运行期设置。UART电平连接仅使用兼容3.3V接口并共地，禁止接传统RS232电压。

## 有限时长运动

确认轮子可安全转动后明确添加`--drive`：

```powershell
python sdk/python/examples/control.py usb COM7 --drive
```

示例以0.03m/s、0.2m/s²运行1秒，每100ms更新，最后发送T800。T800没有JSON执行ACK；发送成功并不等于机械已停。

```python
from lyo_d42 import Client, SerialTransport
with Client(SerialTransport('COM7')) as robot:
    try:
        robot.twist(0.05, 0, 0.2)
        # The application must keep updating valid commands while motion is intended.
        # Use drive_for for a complete bounded demo.
    finally:
        robot.stop()
```

不要启动与应用输入无关的永续心跳线程。导航输入过期应主动停车；程序卡死/拔线后由固件3秒看门狗兜底。柔性停止需要继续心跳直到结束，不能发一个零速度便立即关闭进程。详细字段见[JSON/CAN协议](LYO_D42_API.md)。

## Rust

```powershell
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- usb COM7
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- uart COM8
```

默认只读；明确添加`--drive`才执行同样的有限时长运动。

## 排查

- 无端口：检查数据线、USB/UART口选择、设备管理器与USB转串口驱动。
- 有日志但无回复：检查换行、rid、UART波特率和T604回复配置。
- 超时后先查询T808；不要自动重发T805/T806，以免执行两次。
- 电流raw为空：当前驱动固件可能不支持寄存器69–70；不能将空值当0A。
