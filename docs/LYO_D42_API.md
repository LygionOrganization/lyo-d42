# Lyo-D42 JSON / CAN API · 0.4.0

## 单位、限制与共同规则

- 左轮ID1、右轮ID2；所有公开轮速均以前进为正。v单位m/s，w单位rad/s（逆时针为正），左右轮速度为 `v ± w × 0.159 / 2`。
- **轮速硬上限0.32m/s**。超过配置max_speed的连续速度同比缩放；T809不接受大于0.32的配置，定距速度也不能超限。驱动寄存器上限向下取整为489，对应约0.31963m/s，避免量化超过0.32。
- `accel_mps2`：0.01..5m/s²；随每条目标速度发送。两轮按最大速度差同比插值，控制任务执行斜坡。实际地面加速度受负载、总线时序和失步影响。
- 旧`acc`（1..254，默认15）仍为驱动寄存器加速度，与`accel_mps2`互斥。新应用应明确使用物理单位字段。定距/定角会将物理加速度换算为各轮原生寄存器，量化并限制到1..254。
- JSON一行一条，UTF-8，最多255字节，不含换行；T必须为整数。可选rid为uint32。
- 默认最新指令槽只保留最新一条；`reliable:true`使用8项FIFO。满载拒绝新可靠指令。HTTP 202仅表示入队；429表示未受理。
- 所有通道共享底盘控制权，使用单一驾驶源。无自动重发或幂等去重；定距指令超时后先查询状态，不能盲目重发。

## JSON 指令全集（底盘）

| T | 参数/示例 | 行为 |
|---|---|---|
|800|`{"T":800}`|强制停车停用，清队列、退出任务、关闭驻坡。JSON无执行ACK；CAN只确认接收|
|801|`enable:true/false`|显式初始化/使能，或停用；普通运动会自动使能|
|802|`left,right`（轮rpm），`accel_mps2`|轮转速，输出轮rpm，不是电机轴rpm|
|803|`left,right`（m/s），`accel_mps2`|独立左右轮线速度|
|804|`v,w,accel_mps2`|车体速度，例如`{"T":804,"v":0.1,"w":0,"accel_mps2":0.2}`|
|805|`distance`（−100..100m）、`speed`（0.005..max_speed）、`accel_mps2`|相对定距，开始时必须静止|
|806|`degrees`（−36000..36000°）、`speed`、`accel_mps2`|相对原地旋转，正数逆时针|
|807|`brightness`（0..1）|射灯；0关闭，非零映射到min_duty..100%|
|808|无|读取缓存反馈，不刷新运动心跳|
|809|见配置表|停用状态保存配置，可只提供要修改的字段|
|810|无|已使能底盘心跳，不自动使能、不改变目标|
|811|`hold:true/false`|自动驻坡策略开关，行驶时切换不打断运动|

T802–804可带`soft_stop`布尔值：物理加速度指令默认true；目标双轮零速度时按加速度减速，false则直接停止。非零速度变化始终遵循物理加速度。网页柔性停止开关默认关闭。

```json
{"T":804,"v":0.1,"w":0,"accel_mps2":0.2,"rid":100}
{"T":804,"v":0,"w":0,"accel_mps2":0.1,"soft_stop":true,"rid":101}
{"T":811,"hold":true,"rid":102,"reliable":true}
{"T":809,"timeout_ms":3000,"rid":103,"reliable":true}
```

## 驻坡、释放与心跳

自动驻坡开启时，非零指令正常驾驶；零速/定距完成后位置保持，电流寄存器60。关闭驻坡，停车后关闭驱动扭矩。停用最后写寄存器40=0并读回确认，失败时每100ms重试，`release_pending`为true期间禁止重新运动。读回不能替代实机电流和机械检查。

默认心跳3000ms，可配置100..60000ms。有效使能、运动、驻坡和T810续期；状态、射灯、配置、无效命令不续期。减速、定距和驻坡也需要更新。推荐100ms控制周期；如将超时设为100ms，需要更短周期并考虑传输抖动。T800、故障和心跳超时直接停用，不等待柔性停止。总线断线可能导致停止指令无法抵达驱动器，不能把软件ACK当作机械停止。

原始步进调试T720–726交出底盘控制权，不属于正常底盘运动接口；原始调试输出需主动关闭。底盘API的保护不代表这些原始命令也有同样的看门狗。

## 持久配置 T809

| 字段 | 范围 | 默认 |
|---|---|---|
|can_id|1..127|1|
|max_speed|0.01..0.32m/s|0.32|
|min_duty|0.01..1|0.4|
|timeout_ms|100..60000整数ms|3000|
|current|1..1000，驱动电流参数，不是A|200|

先停止并关闭驻坡再配置。写入NVS；上电不会恢复运动。已有超出0.32的旧速度配置在加载时回退到0.32。Web设置页支持单独保存心跳。

## 状态 T808 / GET /api/chassis

顶部字段：`ok,ready,enabled,state,release_pending,hill_hold,holding,debug,motor_error,motor_id,left_mps,right_mps,remaining_m,led,max_speed,min_duty,can_id,timeout_ms,current,sta_rssi,ap_clients,wheels,tilt`。

- `hill_hold`为自动驻坡开关，`holding`为正在保持；`left_mps/right_mps`为目标，不是实际测量。
- `ready`只表示上次初始化结果。`state`可能为disabled、idle、velocity、decelerating、position、completed、hill_hold、stopped、watchdog或错误状态。
- 每轮包含`id,valid`，有效时含`mps,voltage,temperature_c,status,current_valid`；电流有效时提供`current_raw`。
- 普通反馈运动时约100ms、停用时500ms采样，超过1000ms无效；故障位或必需反馈失败停用。
- 电流通过库寄存器69–70读取，约500ms一次。当前资料未确认安培换算，不得标成校准后的A值；不支持/失败时current_valid=false，该可选读取不影响运动。
- `tilt`为valid/roll/pitch，度；交换原Roll/Pitch，新Pitch减90°，仅重力倾角，没有Yaw积分。IMU调试T740保留原始六轴数据和网页相对Yaw。
- OLED电池电压使用缓存值，失效显示`-- V`。底盘停用且控制通信安静至少1秒时后台刷新，首次空闲可刷新，随后最多每60秒一次；忙碌持续顺延，不额外查询驱动总线。

JSON结果用rid匹配。HTTP用`GET /api/result`（只保存最新带rid结果）或WS；USB默认输出结果。UART0需先开启回复：`{"T":604,"echo":0,"uart":1,"usb":1}`，再发带rid查询。

## CAN 指令集

Classic CAN 2.0B，500kbit/s，29位扩展数据帧，DLC严格为8，不能使用RTR或CAN FD。

```text
request ID = 0x18EF0000 | (node << 8) | op
op bit7 = reliable queue (1) / latest slot (0)
bytes0..1 = uint16 sequence, little endian
```

| op低7位 | JSON T | bytes2..7 |
|---|---|---|
|0|800|全0；停车停用|
|1|801|byte2 bool enable，其余0|
|2|802|i16 left×100 rpm，i16 right×100 rpm，u8 acceleration，u8 flags|
|3|803|i16 left mm/s，i16 right mm/s，u8 acceleration，u8 flags|
|4|804|i16 v mm/s，i16 w mrad/s，u8 acceleration，u8 flags|
|5|805|i16 distance mm，i16 speed mm/s，u8 acceleration，u8 flags|
|6|806|i16 angle×100°，i16 speed mm/s，u8 acceleration，u8 flags|
|7|807|u16 brightness 0..1000，其余0|
|8|808|全0；状态查询|
|9|809|u8 node，u8 max_speed（单位0.002m/s），u8 min_duty（%），u8 timeout（10ms），u16 current|
|10|810|全0；心跳|
|11|811|byte2 bool hold，其余0|
|12|809|u16 timeout_ms，其余0；新超时配置，支持100..60000ms|

op2..6：flags bit7=1时acceleration单位0.01m/s²（1..254，即0.01..2.54），bit0为soft_stop，bit1..6必须0。flags=0兼容旧寄存器acc。所有多字节字段均小端。CAN定距范围−32.768..32.767m，转角−327.68..327.67°；超过编码范围用分次任务或JSON接口。op9的timeout最高2550ms，3秒超时请用op12；固件0.4.0不改变旧op9字节布局。

### CAN 反馈

ACK ID=`0x18EE0000 | node<<8 | op低7位`，DLC3：seq u16、code u8（0成功，1拒绝）。op0仅确认收到停车请求。op12结果使用12，不是9。

op8还返回三条DLC8状态帧，均以seq u16开头：

| ID低字节 | bytes2..7 |
|---|---|
|`0x18EDnn00`|ready, enabled, ID1 valid, ID2 valid, ID1 voltage×10, light%|
|`0x18EDnn01`|i16 ID1 mm/s, i16 ID2 mm/s, ID1 status, ID2 status|
|`0x18EDnn02`|i16 ID1 current_raw, i16 ID2 current_raw, ID1 current_valid, ID2 current_valid|

只接受正确node和seq的帧，帧顺序不固定。当前CAN反馈没有完整JSON的温度、姿态、配置、state字符串。SDK需要0.4.0的第三条状态帧，旧固件会报不完整超时。收到不匹配/错误帧应忽略，不得将其当作本次结果。

例：node1、seq42、v=0.1m/s、w=0、a=0.2m/s²、柔性停止与可靠槽开启：ID `18EF0184`，数据 `2A 00 64 00 00 00 14 81`。设置3000ms超时：ID `18EF018C`，数据 `2B 00 B8 0B 00 00 00 00`（先停用）。
