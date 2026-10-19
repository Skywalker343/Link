/*
 * Arduino UNO + MAX30102 + MAX7219 8x8
 *
 * 心率检测对齐：MAX30102_Relay_Test（FIFO 读数）+ MAX30102_HeartRate_Relay（PBA 算法）
 * 手指放上 → 全亮 → 检测到心率后点阵实时显示图案 → 每 10 秒串口输出平均心率
 *
 * MAX30102: SDA→A4  SCL→A5
 * MAX7219:  DIN→D11  CS→D10  CLK→D12
 *
 * 库：LedControl、SoftwareWire
 */

#include <Wire.h>
#include <LedControl.h>
#include "MAX30102_SoftWire.h"
#include "heartRate.h"

#define PIN_MAX7219_DIN 11
#define PIN_MAX7219_CS 10
#define PIN_MAX7219_CLK 12

/** 与 pattern_test 一致：1=按列写入（已验证） */
#define MATRIX_DRAW_MODE 1

/** 与 Relay_Test / HeartRate_Relay 一致：IR 超过此值认为手指放上 */
#define IR_FINGER_THRESHOLD 50000L
#define FINGER_LEAVE_MS 1500UL
#define MEASURE_PERIOD_MS 10000UL
#define PERIOD_BPM_CAP 40
#define STATUS_PRINT_MS 200UL
#define TELEMETRY_PRINT_MS 1000UL

LedControl lc(PIN_MAX7219_DIN, PIN_MAX7219_CLK, PIN_MAX7219_CS, 1);
MAX30102_SoftWire particle;

static uint16_t periodBpms[PERIOD_BPM_CAP];
static uint8_t periodCount = 0;

static unsigned long lastBeatMs = 0;
static unsigned long periodStartMs = 0;
static unsigned long irLowSince = 0;
static unsigned long lastStatusMs = 0;
static unsigned long lastTelemetryMs = 0;
static int32_t lastIr = 0;
static bool sessionActive = false;
static int displayedBpm = 0;
static int lastLedBpm = -1;
static float beatsPerMinuteInstant = 0;

const byte BMP_CIRCLE[8] = {
    0b00111100, 0b01000010, 0b10000001, 0b10000001,
    0b10000001, 0b10000001, 0b01000010, 0b00111100};

const byte BMP_SQUARE[8] = {
    0b11111111, 0b10000001, 0b10000001, 0b10000001,
    0b10000001, 0b10000001, 0b10000001, 0b11111111};

const byte BMP_TRIANGLE[8] = {
    0b00000001, 0b00000011, 0b00000101, 0b00001001,
    0b00010001, 0b00100001, 0b01000001, 0b11111111};

const byte BMP_STAR[8] = {
    0b00011000, 0b10011001, 0b01011010, 0b00111100,
    0b11111111, 0b00111100, 0b01011010, 0b10011001};

static void matrixInit(void) {
  lc.shutdown(0, false);
  lc.setIntensity(0, 8);
  lc.setScanLimit(0, 7);
  lc.clearDisplay(0);
}

static void displayBitmapRow(const byte bmp[8]) {
  for (int row = 0; row < 8; row++) {
    lc.setRow(0, row, bmp[row]);
  }
}

static void displayBitmapColumn(const byte bmp[8]) {
  for (int col = 0; col < 8; col++) {
    byte colData = 0;
    for (int row = 0; row < 8; row++) {
      if (bmp[row] & (0x80 >> col)) {
        colData |= (1 << row);
      }
    }
    lc.setColumn(0, col, colData);
  }
}

static void displayBitmapLed(const byte bmp[8]) {
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      lc.setLed(0, row, col, (bmp[row] & (0x80 >> col)) != 0);
    }
  }
}

static void displayBitmap(const byte bmp[8]) {
  lc.clearDisplay(0);
#if MATRIX_DRAW_MODE == 1
  displayBitmapColumn(bmp);
#elif MATRIX_DRAW_MODE == 2
  displayBitmapLed(bmp);
#else
  displayBitmapRow(bmp);
#endif
}

static void displayFullBright(void) {
#if MATRIX_DRAW_MODE == 1
  for (int col = 0; col < 8; col++) {
    lc.setColumn(0, col, 0xFF);
  }
#elif MATRIX_DRAW_MODE == 2
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      lc.setLed(0, row, col, true);
    }
  }
#else
  for (int row = 0; row < 8; row++) {
    lc.setRow(0, row, 0xFF);
  }
#endif
}

static const char *shapeNameForBpm(int bpm) {
  if (bpm >= 50 && bpm <= 69) {
    return "圆";
  }
  if (bpm >= 70 && bpm <= 79) {
    return "方";
  }
  if (bpm >= 80 && bpm <= 99) {
    return "三角";
  }
  if (bpm >= 100) {
    return "星";
  }
  return "-";
}

static void periodReset(void) {
  periodCount = 0;
}

static void periodPush(uint16_t bpm) {
  if (periodCount < PERIOD_BPM_CAP) {
    periodBpms[periodCount++] = bpm;
  }
}

static int periodAverage(void) {
  if (periodCount == 0) {
    return 0;
  }
  unsigned long sum = 0;
  for (uint8_t i = 0; i < periodCount; i++) {
    sum += periodBpms[i];
  }
  return (int)(sum / periodCount);
}

/** 根据 BPM 驱动点阵图案（与 pattern_test 相同绘制方式） */
static void updateMatrixByBpm(int bpm) {
  const int zone =
      (bpm >= 100) ? 4 : (bpm >= 80) ? 3 : (bpm >= 70) ? 2 : (bpm >= 50) ? 1 : 0;

  if (zone == lastLedBpm) {
    return;
  }
  lastLedBpm = zone;

  if (bpm >= 50 && bpm <= 69) {
    displayBitmap(BMP_CIRCLE);
  } else if (bpm >= 70 && bpm <= 79) {
    displayBitmap(BMP_SQUARE);
  } else if (bpm >= 80 && bpm <= 99) {
    displayBitmap(BMP_TRIANGLE);
  } else if (bpm >= 100) {
    displayBitmap(BMP_STAR);
  }
}

/** 有心率时点阵显示图案，否则保持全亮 */
static void refreshLedDisplay(void) {
  if (!sessionActive) {
    return;
  }

  int bpmForLed = 0;

  if (periodCount > 0) {
    bpmForLed = periodAverage();
  } else if (displayedBpm > 0) {
    bpmForLed = displayedBpm;
  } else if (beatsPerMinuteInstant >= 50.0f) {
    bpmForLed = (int)(beatsPerMinuteInstant + 0.5f);
  }

  if (bpmForLed >= 50) {
    updateMatrixByBpm(bpmForLed);
  } else if (lastLedBpm != 0) {
    lastLedBpm = 0;
    displayFullBright();
  }
}

static void resetSession(void) {
  sessionActive = false;
  periodReset();
  lastBeatMs = 0;
  displayedBpm = 0;
  lastLedBpm = -1;
  periodStartMs = 0;
  irLowSince = 0;
  beatsPerMinuteInstant = 0;
  resetHeartRateAlgorithm();
  lc.clearDisplay(0);
}

static void startSession(void) {
  periodReset();
  lastBeatMs = 0;
  displayedBpm = 0;
  lastLedBpm = -1;
  beatsPerMinuteInstant = 0;
  periodStartMs = millis();
  irLowSince = 0;
  sessionActive = true;
  resetHeartRateAlgorithm();
  displayFullBright();
  Serial.println(F("\n>>> Finger placed: full dot matrix illuminated; pattern displayed after heart rate is detected."));
  Serial.println(F("    50-69: Circle 70-79: Square 80-99: Triangle >=100: Star"));
}

/** 与 HeartRate_Relay 一致：持续喂样，不卡 IR 门限 */
static void processIrSample(int32_t irValue) {
  if (checkForBeat(irValue)) {
    const unsigned long now = millis();
    const unsigned long delta = (lastBeatMs == 0) ? 0 : (now - lastBeatMs);
    lastBeatMs = now;

    if (delta >= 300 && delta <= 2500) {
      beatsPerMinuteInstant = 60.0f / (delta / 1000.0f);
      if (beatsPerMinuteInstant > 20.0f && beatsPerMinuteInstant < 255.0f && sessionActive) {
        periodPush((uint16_t)(beatsPerMinuteInstant + 0.5f));
        refreshLedDisplay();
      }
    }
  }
}

static unsigned long periodRemainMs(void) {
  const unsigned long e = millis() - periodStartMs;
  return (e >= MEASURE_PERIOD_MS) ? 0 : (MEASURE_PERIOD_MS - e);
}

static void finishPeriod(void) {
  const int avg = periodAverage();
  periodReset();
  periodStartMs = millis();

  if (avg > 0) {
    displayedBpm = avg;
    Serial.print(F("\n【10-Second Average Heart Rate】 "));
    Serial.print(avg);
    Serial.print(F(" BPM"));
    if (avg >= 50) {
      Serial.print(F("  Pattern:"));
      Serial.println(shapeNameForBpm(avg));
    } else {
      Serial.println(F("  (<50: Keep Fully Illuminated)"));
    }
    lastLedBpm = -1;
    refreshLedDisplay();
  } else {
    displayedBpm = 0;
    lastLedBpm = -1;
    Serial.println(F("\n【10-Second Average】No heartbeat detected, continue full illumination sampling."));
    Serial.println(F("    Tip: IR should be >50,000; gently secure your finger with a rubber band and avoid movement."));
    displayFullBright();
  }

  Serial.println(F("--- Next cycle: 10 seconds. ---"));
}

static void printStatus(void) {
  const int bpmInst = (int)(beatsPerMinuteInstant + 0.5f);
  const int bpmShow = (periodCount > 0) ? periodAverage()
                    : (displayedBpm > 0) ? displayedBpm : bpmInst;
  Serial.print(F("IR="));
  Serial.print(lastIr);
  Serial.print(lastIr >= (int32_t)IR_FINGER_THRESHOLD ? F(" Finger OK ") : F(" No finger detected "));
  Serial.print(F("Remaining"));
  Serial.print((periodRemainMs() + 999) / 1000);
  Serial.print(F("s  Heartbeats="));
  Serial.print(periodCount);
  Serial.print(F("  BPM="));
  Serial.print(bpmShow);
  Serial.print(F("  Dot matrix:"));
  Serial.println(shapeNameForBpm(bpmShow));
}

/** Machine-readable serial contract consumed by backend/serial_bridge.py. */
static void printTelemetry(void) {
  const bool bpmFresh = lastBeatMs > 0 && (millis() - lastBeatMs) <= 2500UL &&
                        beatsPerMinuteInstant > 0.0f;

  Serial.print(F("@VITALS {\"device_id\":\"uno-demo-01\",\"ts_ms\":"));
  Serial.print(millis());
  Serial.print(F(",\"heart_rate_bpm\":"));
  if (bpmFresh) {
    Serial.print((int)(beatsPerMinuteInstant + 0.5f));
  } else {
    Serial.print(F("null"));
  }
  Serial.print(F(",\"ir_raw\":"));
  Serial.print(lastIr);
  Serial.print(F(",\"finger_detected\":"));
  Serial.print(lastIr >= (int32_t)IR_FINGER_THRESHOLD ? F("true") : F("false"));
  Serial.print(F(",\"session_active\":"));
  Serial.print(sessionActive ? F("true") : F("false"));
  Serial.println(F(",\"source\":\"max30102\"}"));
}

static void updateFingerSession(void) {
  const bool fingerOk = (lastIr >= (int32_t)IR_FINGER_THRESHOLD);

  if (fingerOk) {
    irLowSince = 0;
    if (!sessionActive) {
      startSession();
    }
    return;
  }

  if (!sessionActive) {
    return;
  }

  if (irLowSince == 0) {
    irLowSince = millis();
  }
  if (millis() - irLowSince < FINGER_LEAVE_MS) {
    return;
  }

  Serial.println(F("\nFinger removed → End session"));
  resetSession();
}

void setup() {
  Serial.begin(115200);
#if defined(USBCON)
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0 < 3000)) {
    delay(10);
  }
#endif
  delay(200);

  Serial.println(F("\n=== MAX30102 + MAX7219 ==="));
  Serial.println(F("MAX7219: DIN=11 CS=10 CLK=12 | Serial port 115200"));
  Serial.println(F("50~69Circle 70~79Square 80~99Triangle >=100Star\n"));

  matrixInit();

  Wire.begin();
  if (!particle.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println(F("[Error] MAX30102 not found at 0x57. Please check A4/A5 wiring and power supply"));
    while (true) {
      delay(500);
    }
  }

  // 与 Relay_Test 相同的传感器配置
  const uint8_t ledBrightness = 0x1F;
  const uint8_t sampleAverage = 4;
  const uint8_t ledMode = 2;
  const uint8_t sampleRate = 100;
  const int pulseWidth = 411;
  const int adcRange = 4096;

  particle.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  particle.setPulseAmplitudeRed(0x1F);
  particle.setPulseAmplitudeIR(0x1F);

  resetSession();
  Serial.println(F("Ready. Place your finger on the sensor. IR value should increase (>50,000).\n"));
}

void loop() {
  particle.check();

  bool hadFifoSample = false;

  // 与 HeartRate_Relay / Relay_Test 相同：FIFO 逐样本处理
  while (particle.available()) {
    lastIr = (int32_t)particle.getFIFOIR();
    particle.nextSample();
    processIrSample(lastIr);
    hadFifoSample = true;
  }

  if (!hadFifoSample) {
    lastIr = (int32_t)particle.getIR();
    processIrSample(lastIr);
  }

  updateFingerSession();

  if (millis() - lastTelemetryMs >= TELEMETRY_PRINT_MS) {
    lastTelemetryMs = millis();
    printTelemetry();
  }

  if (!sessionActive) {
    return;
  }

  if (millis() - lastStatusMs >= STATUS_PRINT_MS) {
    lastStatusMs = millis();
    printStatus();
  }

  refreshLedDisplay();

  if (millis() - periodStartMs >= MEASURE_PERIOD_MS) {
    finishPeriod();
  }
}
