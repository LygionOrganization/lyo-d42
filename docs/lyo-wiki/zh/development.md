# 进阶二次开发与固件构建

## 环境与构建

使用配套 Lyo-D42 源码，在 VS Code / PlatformIO 打开含 platformio.ini 的目录。项目文件夹的旧名称不影响固件产品名称。开发环境安装步骤见[PlatformIO 开发教程](../../../tutorials/platformio-development/index.md)。

```powershell
python tools/embed_web.py
pio run
pio run --target upload --upload-port COM7
```

上传端口必须换成实际 Type-C UART 端口。构建环境为 esp32-s3-devkitc-1-n8r8，8 MB Flash / OPI PSRAM，平台和依赖版本由 platformio.ini 固定。产物为 `.pio/build/esp32-s3-devkitc-1-n8r8/firmware.bin`。不要将仅应用程序镜像当作从地址 0 烧录的完整镜像，使用 PlatformIO 自动选择偏移。

上传前记录网络与底盘设置。升级后检查 `/api/info` 产品名与版本，先做只读反馈，再架空测试低速、T800、超时和驻坡。不要把 RoboCore 通用下载包当成底盘专用固件。

## 源码地图

| 文件 | 修改内容 |
| --- | --- |
| src/LyoD42.cpp / .h | 双轮状态机、自动使能、反馈、驻坡、失联保护 |
| include/ChassisMath.h | 轮径、轮距、减速比和单位换算 |
| src/main.cpp | HTTP/USB/WebSocket 入口、OLED 行、调试放行 |
| src/ScreenCtrl.cpp | OLED 初始化与不换行显示 |
| src/Can.h | CAN 编码与接收 |
| web/lyo_d42.js | 驾驶、驻坡开关和反馈显示 |
| web/index.html | 页面结构；执行 embed_web.py 生成 Web_page.h |
| src/BusDevices.cpp | T720–726 原始步进设备调试 |

电机总线使用 BusGuard 串行访问，状态使用独立互斥锁；保持先总线后状态的锁顺序。运动看门狗独立运行，不要用长时间脚本占用总线。驻坡使用位置模式保持当前反馈位置，target current=60；原始调试进入 debug 状态，避免后台控制与手动指令互相覆盖。

## 验证与标定

Windows 主机回归需要 Visual Studio C++ Build Tools 和已安装的 PlatformIO 依赖：

```powershell
python test/run_chassis_tests.py
python test/run_chassis_tests.py --screen
python test/run_chassis_tests.py --can
```

先验证只有 ID1/ID2，OLED/IMU 缺失仍可移动；再验证相位 bit4=0/1 都可驾驶且零速轮关闭输出、零速释放、驻坡电流 60、故障退出保持和原始调试。主机模拟不能代替实机电流、温升、停车距离和坡面保持测试。

轮速寄存器单位为 50 步/s。理论每米约 76495.31 步；轮胎打滑和齿隙需要地面测量标定。定距/定角每段最多 30000 步且段间停顿。扩展导航应加入外部定位，而非把驱动步数当作真实地面位置。

## 航模遥控实验

`arduino_example/LyoD42_SBUS` 是独立教学程序，依赖 ID30 TTL Node 和匹配的 S.BUS 接收机，上传会替换网页主固件。它不是标准产品依赖，不能拿它的接线要求判断默认底盘是否齐全。

## SDK 与发布检查

项目根目录和两个 SDK 目录均有 AGENTS.md。先阅读其不变量。运行 Python SDK 单元测试、`cargo test --all-targets --manifest-path sdk/rust/Cargo.toml` 和 UI 回归，再执行 `python tools/package_release.py`。发布候选包含源码、固件、两个 SDK、SHA256 清单及验证记录。工具不会自动烧录、上传包仓库或部署网站。

`tools/sync_lyo_wiki.py --wiki <wiki目录>` 同步经过校验的本地资料和下载包。`docs/RELEASE_VALIDATION.md` 区分主机测试与实机验收；没有实机结论时不能宣称温升、力矩或停止距离已经达标。
