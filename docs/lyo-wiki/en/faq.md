# Troubleshooting

|Symptom|Check|
|---|---|
|Driver not responding|ID1/ID2,1Mbps motor bus, wiring and battery; working Wi-Fi does not prove motor power|
|disable_ack_failed or release_pending|Inspect motor power/bus and register40 readback; firmware retries release and blocks motion until confirmed|
|driver_fault|Inspect wheel status against driver documentation; status16 current limiting alone is not a fault|
|feedback_read_failed|Required wheel feedback was lost; inspect connection and power|
|Cannot drive with auto hold|Use0.4.0; auto hold should allow nonzero targets and hold at register60 only after stopping|
|Wheels remain locked with hold off|Inspect holding/release_pending and actual driver response; shutdown must release torque|
|Noise at high speed|UI/firmware cap is0.32m/s; verify mechanics, supply, phase and current if noise persists below the cap|
|watchdog|No valid update before the configured timeout, default3s; queries do not renew it|
|Current unavailable|Optional raw register read may be unsupported; current_valid=false is not zero amperes|
|OLED voltage is old|It updates only in quiet disabled periods, at most once per minute; busy operation postpones it|
|CAN state times out|Require0.4.0 ACK plus three matching fragments; verify node, sequence,29-bit IDs,DLC and500kbit/s|
|USB query has no result|Use native Type-C USB; UART0 requires1Mbps and T604 replies enabled|

ready reports the last initialization check, not permanent health. Combine enabled,state,wheel validity and motor_error. Recovery requires a new explicit motion command; old motion never resumes automatically. No power means no hold torque. Raw stepper debugging requires explicit output disable and does not share the chassis motion watchdog guarantee.

See [USB/UART](usb-cdc.md), [CAN](can.md) and the [API](api.md).
