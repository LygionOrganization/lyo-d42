# Quick start and web control

1. Raise the drive wheels and check battery polarity and motor wiring. USB power alone does not supply the motors.
2. Connect to Wi-Fi `Robot`, password `12345678`, unless saved network settings differ.
3. Open `http://192.168.4.1`. Use the top-right button to switch English and Chinese. Only one language is shown at a time.
4. Check ID1/ID2 feedback, temperatures, current validity and tilt. Start with a low speed and an explicit acceleration in m/s².
5. Hold the joystick, W/A/S/D or arrow keys to drive. Motion automatically enables the drivers. Release to stop; Space requests a hard stop. The old STOP button has been removed.
6. Verify directions, stopping and lost-connection behavior before a floor test.

Wheel speed cannot exceed0.32m/s. With Soft stop enabled, releasing the controls ramps down using the selected acceleration. With it off, the target stops directly. Faults, T800 and watchdog expiry always disable directly.

Auto hill hold allows normal driving. After stopping it holds wheel position at current register60. With auto hold off, stopping releases torque; firmware checks the disable register and retries failed releases. A pending release blocks new motion. Register60 does not mean60A.

Distance is meters, angle is degrees, positive rotation is counterclockwise. Start position tasks at rest. Long tasks split into segments of at most30000 motor steps and stop between segments; they are not ground-truth odometry.

Default heartbeat timeout is3s. The Settings page accepts100–60000ms while disabled. Position moves, soft stopping and hill hold also need updates. A frozen or disconnected page stops renewing the ESP32-S3 watchdog. Use one control source. Losing page focus stops driving.

Light brightness0 means off; nonzero brightness maps from the default40% PWM minimum to100%. The OLED uses cached battery voltage and updates only when disabled and traffic has been quiet for1s, no more than once per minute after the first idle refresh. Busy periods postpone updates. Chassis tilt has Roll/Pitch only; the IMU debug panel retains relative Yaw.

Raw stepper debugging is separate from chassis control; explicitly disable debug outputs before leaving it. See [host control](host.md) for applications.
