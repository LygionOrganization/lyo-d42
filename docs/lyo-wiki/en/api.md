# Lyo-D42 JSON / CAN API ·0.4.0

## Shared rules

Left wheel is ID1; right is ID2. Both public wheel speeds are positive forward. v is m/s; w is rad/s, positive counterclockwise. Wheel speeds are `v ± w ×0.159/2`. The hard cap is **0.32m/s**, enforced after conversion; register quantization is capped at489 (about0.31963m/s). Continuous overspeed targets are proportionally scaled to configured max_speed. T809 rejects max_speed above0.32; position speed must be within the configured limit.

`accel_mps2` is0.01–5m/s². Continuous motion uses a shared two-wheel software ramp. Position commands convert acceleration to native driver units. Actual ground acceleration depends on mechanics and missed steps. Legacy `acc` is a register value1–254 (default15) and cannot be combined with accel_mps2. New applications should provide physical acceleration explicitly.

JSON is one UTF-8 object per line, at most255bytes excluding newline. T is an integer; optional rid is uint32. By default only the latest queued command survives; `reliable:true` selects an8-entry FIFO. A full queue rejects new reliable commands. HTTP202 means queued,429 means not accepted. Match execution results using rid; HTTP exposes only the latest result at `/api/result`. Use one active controller. There is no automatic replay protection or motion retry.

## Chassis JSON commands

|T|Fields|Operation|
|---|---|---|
|800|none|Hard stop/disable, clear queues and mission, cancel hold. No JSON execution ACK; CAN confirms receipt only|
|801|enable bool|Enable/initialize or disable; motion auto-enables|
|802|left,right output-wheel rpm; accel_mps2|Wheel RPM targets|
|803|left,right m/s; accel_mps2|Wheel linear targets|
|804|v,w,accel_mps2|Body velocity|
|805|distance −100..100m; speed0.005..max_speed; accel_mps2|Relative distance, start at rest|
|806|degrees −36000..36000; speed; accel_mps2|Relative rotation, start at rest|
|807|brightness0..1|Light; nonzero maps from min_duty to100%|
|808|none|Cached status, does not renew watchdog|
|809|partial configuration below|Save while disabled|
|810|none|Renew enabled chassis watchdog; never auto-enable|
|811|hold bool|Auto hill-hold policy; does not interrupt driving|

T802–804 accept soft_stop bool, default true with physical acceleration. A zero target ramps down when true and stops directly when false. Nonzero changes always use the physical ramp. Web Soft stop defaults off.

```json
{"T":804,"v":0.1,"w":0,"accel_mps2":0.2,"rid":100}
{"T":804,"v":0,"w":0,"accel_mps2":0.1,"soft_stop":true,"rid":101}
{"T":811,"hold":true,"rid":102,"reliable":true}
{"T":809,"timeout_ms":3000,"rid":103,"reliable":true}
```

## Hold, release and watchdog

Auto hold allows driving, then holds position at current register60 after a zero target or completed position task. With hold off, stopping disables torque. Firmware writes register40=0 last, reads it back and retries failures every100ms. release_pending blocks new motion. A register ACK is not proof of mechanical stopping.

Default timeout is3000ms, configurable100–60000ms. Valid enable/motion/hold/T810 renew it; queries, light, config and invalid commands do not. Position tasks, deceleration and holding need heartbeat too. A100ms host period is suitable for the default; shorter configured timeouts require a shorter period and jitter margin. T800, fault and expiry disable immediately. Bus failure may prevent a disable reaching the motor driver.

T720–726 raw stepper debugging transfers control out of the chassis state machine. Explicitly disable raw outputs; those operations do not inherit the chassis watchdog guarantee.

## T809 persistent settings

|Field|Range|Default|
|---|---|---|
|can_id|1..127|1|
|max_speed|0.01..0.32m/s|0.32|
|min_duty|0.01..1|0.4|
|timeout_ms|integer100..60000ms|3000|
|current|register1..1000, not amperes|200|

Only supplied fields change. Stop and disable hold first. Values persist in NVS but motion never resumes at startup. Old speed settings above0.32 fall back to0.32.

## T808 / GET /api/chassis

Top-level fields: ok,ready,enabled,state,release_pending,hill_hold,holding,debug,motor_error,motor_id,left_mps,right_mps,remaining_m,led,max_speed,min_duty,can_id,timeout_ms,current,sta_rssi,ap_clients,wheels,tilt.

hill_hold is the policy; holding is actual holding. left_mps/right_mps are targets. ready reports the last initialization result. State names include disabled,idle,velocity,decelerating,position,completed,hill_hold,stopped,watchdog and error states.

Each wheel has id,valid and, when valid, mps,voltage,temperature_c,status,current_valid; current_raw appears only when available. Normal feedback is sampled about100ms moving/500ms disabled, stale after1000ms. Required feedback failure or driver fault disables motion. Optional current registers69–70 are read about500ms; unsupported/current-read failures set current_valid=false without faulting motion. The ampere conversion is unverified: raw current is not calibrated A.

Tilt has valid/roll/pitch in degrees: original Roll/Pitch swapped, new Pitch minus90°. No chassis yaw integration. IMU debugging retains raw six-axis readings and relative yaw. OLED uses cached voltage, displays --V when invalid, and only refreshes in quiet disabled periods, at most once per60s after first idle refresh.

USB replies are enabled by default. UART0 needs `{"T":604,"echo":0,"uart":1,"usb":1}` before rid-based requests.

## Complete chassis CAN command set

Classic CAN2.0B,500kbit/s,29-bit extended data frames, DLC exactly8; no RTR or CAN FD. Request ID=`0x18EF0000 | node<<8 | op`. op bit7 selects reliable FIFO. bytes0–1 are uint16 sequence, little endian. All multi-byte fields below are little endian.

|op low7bits|T|bytes2..7|
|---|---|---|
|0|800|zero|
|1|801|byte2 enable bool, rest0|
|2|802|i16 left×100rpm, i16 right×100rpm, u8 acceleration, u8 flags|
|3|803|i16 left mm/s, i16 right mm/s, acceleration, flags|
|4|804|i16 v mm/s, i16 w mrad/s, acceleration, flags|
|5|805|i16 distance mm, i16 speed mm/s, acceleration, flags|
|6|806|i16 angle×100degrees, i16 speed mm/s, acceleration, flags|
|7|807|u16 brightness0..1000, rest0|
|8|808|zero; status|
|9|809|u8 node, u8 max_speed in0.002m/s, u8 min_duty%, u8 timeout in10ms, u16 current|
|10|810|zero; heartbeat|
|11|811|byte2 hold bool, rest0|
|12|809|u16 timeout_ms, rest0; supports100..60000ms|

For op2–6, flags bit7=1 selects physical acceleration in0.01m/s² (byte6=1..254), bit0 selects soft stopping; bits1–6 must be0. flags0 preserves legacy register acceleration. Distance range is−32.768..32.767m; angle−327.68..327.67degrees. Legacy op9 timeout cannot exceed2550ms; use op12 for3seconds. Its byte layout is unchanged.

ACK ID=`0x18EE0000 | node<<8 | op low7bits`, DLC3: seq u16, code u8 (0 success,1 reject). op0 only acknowledges receipt. op12 replies as12.

op8 also returns three DLC8 fragments, each starting with seq u16:

|ID|bytes2..7|
|---|---|
|0x18EDnn00|ready,enabled,ID1 valid,ID2 valid,ID1 voltage×10,light%|
|0x18EDnn01|i16 ID1 mm/s,i16 ID2 mm/s,ID1 status,ID2 status|
|0x18EDnn02|i16 ID1 current_raw,i16 ID2 current_raw,ID1 current_valid,ID2 current_valid|

Collect matching node/seq ACK and all fragments in any order. CAN does not include JSON temperature, tilt, configuration or state name. SDK0.4.0 requires the third fragment and times out against older firmware. Ignore unrelated frames.

Example node1/seq42/v0.1/w0/a0.2/soft/reliable: ID18EF0184, data `2A 00 64 00 00 00 14 81`. Set3000ms while disabled: ID18EF018C, data `2B 00 B8 0B 00 00 00 00`.
