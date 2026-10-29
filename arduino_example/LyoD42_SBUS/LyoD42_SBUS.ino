// Lyo-D42 optional S.BUS teaching sketch. Replaces the complete firmware.
// Board: ESP32S3 Dev Module, Arduino ESP32 3.3.0, USB CDC On Boot disabled.
// Dependency: install this repository lib/lygion_devs into Arduino libraries.
// UART0 monitor: 115200. Node ID30 and the documented transmitter are required.
#include <Arduino.h>
#include <lygion_devs.h>
#include <math.h>
#ifdef Serial
#undef Serial
#endif
#define Serial Serial0

TTLSDClass ttlsd;
NodeClass node;


constexpr int BUS_RX_PIN = 5;
constexpr int BUS_TX_PIN = 4;
constexpr u8 NODE_ID = 30;
constexpr u8 LEFT_MOTOR_ID = 1;
constexpr u8 RIGHT_MOTOR_ID = 2;
constexpr int8_t LEFT_MOTOR_SIGN = 1;
constexpr int8_t RIGHT_MOTOR_SIGN = -1;
constexpr float WHEEL_TRACK_M = 0.159f;


constexpr u8 CHANNEL_FORWARD = 2;
constexpr u8 CHANNEL_YAW = 1;
constexpr int8_t FORWARD_SIGN = -1;
constexpr int8_t YAW_SIGN = -1;
constexpr u8 CHANNEL_ENABLE = 5;
constexpr u16 ENABLE_THRESHOLD = 1200;


constexpr u8 CHANNEL_LED = 7;
constexpr u16 LED_SWITCH_THRESHOLD = 1200;
constexpr u8 LED_PWM_CHANNEL = 1;
constexpr u16 LED_PWM_ON = 1000;
constexpr u16 LED_PWM_OFF = 0;

constexpr float SBUS_MIN = 282.0f;
constexpr float SBUS_MID = 1002.0f;
constexpr float SBUS_MAX = 1722.0f;
constexpr float SBUS_DEADBAND = 0.05f;
constexpr u16 SBUS_PLAUSIBLE_MIN = 100;
constexpr u16 SBUS_PLAUSIBLE_MAX = 2000;

constexpr u8 SBUS_FRAME_LOST_MASK = 1U << 2;
constexpr u8 SBUS_FAILSAFE_MASK = 1U << 3;


// 489 * 50 steps/s / 76495.31 steps/m stays below the 0.32 m/s cap.
constexpr float FORWARD_SPEED_GAIN = 489.0f;
constexpr float MAX_DRIVE_SPEED = 489.0f;


constexpr float TURN_SPEED_GAIN_PER_M = 1800.0f;
constexpr u16 MOTOR_CURRENT = 200;
constexpr u8 DRIVE_ACCELERATION = 15;
constexpr u8 DRIVE_BRAKE_ACCELERATION = 50;

constexpr uint32_t BUS_IO_TIMEOUT_MS = 30;
constexpr uint32_t BUS_TURNAROUND_DELAY_US = 2000;
constexpr uint32_t CONTROL_PERIOD_MS = 20;
constexpr uint32_t RADIO_TIMEOUT_MS = 200;
constexpr uint32_t DEBUG_PERIOD_MS = 250;
constexpr bool PRINT_REMOTE_DEBUG = true;

static_assert(LEFT_MOTOR_ID != RIGHT_MOTOR_ID && LEFT_MOTOR_ID != NODE_ID &&
                  RIGHT_MOTOR_ID != NODE_ID, "Device IDs must be unique");
static_assert(WHEEL_TRACK_M > 0.0f && MAX_DRIVE_SPEED > 0.0f &&
                  MAX_DRIVE_SPEED <= 32767.0f, "Invalid drive limits");
static_assert(CHANNEL_FORWARD >= 1 && CHANNEL_FORWARD <= 16 &&
                  CHANNEL_YAW >= 1 && CHANNEL_YAW <= 16 &&
                  CHANNEL_ENABLE >= 1 && CHANNEL_ENABLE <= 16 &&
                  CHANNEL_LED >= 1 && CHANNEL_LED <= 16,
              "S.BUS channels must be in 1..16");

bool setupSucceeded = false;
bool receivedValidRadioFrame = false;
bool motionArmed = false;
bool enableLowSeen = false;
uint32_t lastValidRadioFrameMs = 0;
uint32_t lastControlMs = 0;
uint32_t lastDebugMs = 0;
int lastLedPwm = -1;
s16 lastLeftCommand = 0;
s16 lastRightCommand = 0;

bool driveStopCommandSent = false;


void prepareBus() {
  Serial1.flush();
  delayMicroseconds(BUS_TURNAROUND_DELAY_US);
}

bool commandSucceeded(int result) {
  return result != 0 && ttlsd.getLastError() == LYS::ERR_NORMAL;
}

void sendDriveSpeeds(s16 left, s16 right, u8 acceleration) {
  u8 ids[] = {LEFT_MOTOR_ID, RIGHT_MOTOR_ID};
  s16 speeds[] = {left, right};
  u8 accelerations[] = {acceleration, acceleration};
  u16 currents[] = {MOTOR_CURRENT, MOTOR_CURRENT};
  lastLeftCommand = left;
  lastRightCommand = right;
  prepareBus();

  ttlsd.SyncWriteSpe(ids, 2, speeds, accelerations, currents);
  driveStopCommandSent = left == 0 && right == 0;
}

void stopDriveWheels(bool force = false) {
  if (driveStopCommandSent && !force) {
    return;
  }


  sendDriveSpeeds(0, 0, DRIVE_BRAKE_ACCELERATION);
}

void disarmMotion() {
  motionArmed = false;
  enableLowSeen = false;
  stopDriveWheels();
}

void setLedFromSwitch(bool enabled) {
  const u16 pwmValue = enabled ? LED_PWM_ON : LED_PWM_OFF;
  if (lastLedPwm == pwmValue) {
    return;
  }
  prepareBus();
  if (node.pwmCtl(NODE_ID, LED_PWM_CHANNEL, pwmValue) != 0 &&
      node.getLastError() == LYS::ERR_NORMAL) {
    lastLedPwm = pwmValue;
  }
}

bool initializeDriveMotors() {
  bool allSucceeded = true;
  const u8 ids[] = {LEFT_MOTOR_ID, RIGHT_MOTOR_ID};
  for (u8 id : ids) {
    prepareBus();
    if (!commandSucceeded(ttlsd.SetMode(id, 1))) {
      Serial.print("[error] set speed mode failed: ID ");
      Serial.print(id);
      Serial.print(", comm error=");
      Serial.println(ttlsd.getLastError());
      allSucceeded = false;
    }
    stopDriveWheels(true);
  }
  return allSucceeded;
}

bool isPlausibleSbusValue(u16 value) {
  return value >= SBUS_PLAUSIBLE_MIN && value <= SBUS_PLAUSIBLE_MAX;
}

float sbusAxis(u16 rawValue, int8_t sign) {
  const float halfRange = rawValue >= SBUS_MID ? SBUS_MAX - SBUS_MID
                                               : SBUS_MID - SBUS_MIN;
  float value = (static_cast<float>(rawValue) - SBUS_MID) / halfRange;
  value = constrain(value, -1.0f, 1.0f);
  const float magnitude = fabsf(value);
  if (magnitude <= SBUS_DEADBAND) {
    return 0.0f;
  }
  return copysignf((magnitude - SBUS_DEADBAND) / (1.0f - SBUS_DEADBAND),
                   value) * static_cast<float>(sign);
}

void setChassisMotion(float forward, float yaw) {
  const float centerSpeed = constrain(forward, -1.0f, 1.0f) * FORWARD_SPEED_GAIN;
  const float differential = constrain(yaw, -1.0f, 1.0f) *
                             TURN_SPEED_GAIN_PER_M * WHEEL_TRACK_M / 2.0f;


  float left = centerSpeed - differential;
  float right = centerSpeed + differential;
  const float largest = fmaxf(fabsf(left), fabsf(right));
  if (largest > MAX_DRIVE_SPEED) {
    const float scale = MAX_DRIVE_SPEED / largest;
    left *= scale;
    right *= scale;
  }
  const s16 leftCommand = static_cast<s16>(lroundf(left * LEFT_MOTOR_SIGN));
  const s16 rightCommand = static_cast<s16>(lroundf(right * RIGHT_MOTOR_SIGN));
  if (leftCommand == 0 && rightCommand == 0) {
    stopDriveWheels();
  } else {
    sendDriveSpeeds(leftCommand, rightCommand, DRIVE_ACCELERATION);
  }
}

void printRemoteDebug(bool readOk) {
  const uint32_t now = millis();
  if (!PRINT_REMOTE_DEBUG || now - lastDebugMs < DEBUG_PERIOD_MS) {
    return;
  }
  lastDebugMs = now;
  if (!readOk) {
    Serial.println("[S.BUS] read failed (Node ID30)");
    return;
  }
  Serial.print("[S.BUS] status=");
  Serial.print(node.sbusStatus());
  for (u8 ch = 1; ch <= node.subsGetNum(); ++ch) {
    Serial.print(" CH");
    Serial.print(ch);
    Serial.print('=');
    Serial.print(node.sbusGetch(ch));
  }
  Serial.print(" setup=");
  Serial.print(setupSucceeded ? "ready" : "failed");
  Serial.print(" armed=");
  Serial.print(motionArmed ? "yes" : "no");
  Serial.print(" ID1=");
  Serial.print(lastLeftCommand);
  Serial.print(" ID2=");
  Serial.println(lastRightCommand);
}

void updateRemoteControl(bool allowMotion) {
  const uint32_t now = millis();
  if (now - lastControlMs < CONTROL_PERIOD_MS) {
    return;
  }
  lastControlMs = now;
  prepareBus();
  const int bytesRead = node.sbusFlush(NODE_ID);

  if (bytesRead != 33 || node.getLastError() != LYS::ERR_NORMAL) {
    if (!allowMotion || !receivedValidRadioFrame ||
        millis() - lastValidRadioFrameMs >= RADIO_TIMEOUT_MS) {
      disarmMotion();
      setLedFromSwitch(false);
    }
    printRemoteDebug(false);
    return;
  }

  const u8 status = node.sbusStatus();
  const u16 forwardRaw = node.sbusGetch(CHANNEL_FORWARD);
  const u16 yawRaw = node.sbusGetch(CHANNEL_YAW);
  const u16 enableRaw = node.sbusGetch(CHANNEL_ENABLE);
  if ((status & (SBUS_FRAME_LOST_MASK | SBUS_FAILSAFE_MASK)) != 0 ||
      !isPlausibleSbusValue(forwardRaw) ||
      !isPlausibleSbusValue(yawRaw) ||
      !isPlausibleSbusValue(enableRaw)) {
    disarmMotion();
    setLedFromSwitch(false);
    printRemoteDebug(true);
    return;
  }


  if (receivedValidRadioFrame &&
      millis() - lastValidRadioFrameMs >= RADIO_TIMEOUT_MS) {
    motionArmed = false;
    enableLowSeen = false;
  }
  receivedValidRadioFrame = true;
  lastValidRadioFrameMs = millis();
  const bool enabled = enableRaw >= ENABLE_THRESHOLD;
  const float forward = sbusAxis(forwardRaw, FORWARD_SIGN);
  const float yaw = sbusAxis(yawRaw, YAW_SIGN);
  const bool neutral = forward == 0.0f && yaw == 0.0f;

  if (!allowMotion) {
    disarmMotion();
  } else if (!enabled) {
    motionArmed = false;
    enableLowSeen = true;
    stopDriveWheels();
  } else {

    if (!motionArmed && enableLowSeen && neutral) {
      motionArmed = true;
    }
    if (motionArmed) {
      setChassisMotion(forward, yaw);
    } else {
      stopDriveWheels();
    }
  }


  const u16 ledRaw = node.sbusGetch(CHANNEL_LED);
  setLedFromSwitch(isPlausibleSbusValue(ledRaw) && ledRaw >= LED_SWITCH_THRESHOLD);
  printRemoteDebug(true);
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(1000000, SERIAL_8N1, BUS_RX_PIN, BUS_TX_PIN);
  ttlsd.pSerial = &Serial1;
  node.pSerial = &Serial1;
  ttlsd.IOTimeOut = BUS_IO_TIMEOUT_MS;
  node.IOTimeOut = BUS_IO_TIMEOUT_MS;
  stopDriveWheels();
  delay(1000);
  Serial.println("Two-wheel differential chassis starting (track 159 mm)...");
  setupSucceeded = initializeDriveMotors();
  stopDriveWheels();
  setLedFromSwitch(false);
  if (setupSucceeded) {
    Serial.println("Ready: CH5 low, center CH1/CH2, then CH5 high to enable.");
  } else {
    Serial.println("Motor initialization failed; motion disabled. Check IDs 1/2 and reboot.");
  }
}

void loop() {
  updateRemoteControl(setupSucceeded);
}
