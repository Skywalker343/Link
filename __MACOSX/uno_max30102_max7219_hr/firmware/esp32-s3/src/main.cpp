#include <Arduino.h>
#include <HTTPClient.h>
#include <LedControl.h>
#include <MAX30105.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <heartRate.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "secrets.h"

#ifndef WIFI_SSID
#error "Copy include/secrets.example.h to include/secrets.h and configure Wi-Fi."
#endif
#ifndef WIFI_PASSWORD
#error "Define WIFI_PASSWORD in include/secrets.h."
#endif

#ifndef DEVICE_ID
#define DEVICE_ID "esp32-s3-demo"
#endif
#ifndef API_URL
#define API_URL ""
#endif
#ifndef API_BEARER_TOKEN
#define API_BEARER_TOKEN ""
#endif
#ifndef SERVER_ROOT_CA_DEFINED
static const char SERVER_ROOT_CA[] PROGMEM = "";
#endif

namespace {
constexpr uint8_t PIN_I2C_SDA = 8;
constexpr uint8_t PIN_I2C_SCL = 9;
constexpr uint8_t PIN_MATRIX_DIN = 11;
constexpr uint8_t PIN_MATRIX_CS = 10;
constexpr uint8_t PIN_MATRIX_CLK = 12;
constexpr uint32_t IR_FINGER_THRESHOLD = 50000;
constexpr uint32_t SAMPLE_STALE_MS = 500;
constexpr uint32_t BEAT_STALE_MS = 2500;
constexpr uint32_t FINGER_LEAVE_MS = 1500;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 1000;
constexpr uint32_t BATCH_FLUSH_MS = 5000;
constexpr uint8_t TELEMETRY_QUEUE_LENGTH = 60;
constexpr uint8_t HTTP_BATCH_SIZE = 12;
constexpr size_t HTTP_BODY_CAPACITY = 4096;

struct TelemetryRecord {
  uint32_t deviceTsMs;
  uint32_t irRaw;
  uint16_t bpm;
  bool bpmValid;
  bool fingerDetected;
  bool sessionActive;
};

MAX30105 particleSensor;
LedControl matrix(PIN_MATRIX_DIN, PIN_MATRIX_CLK, PIN_MATRIX_CS, 1);
QueueHandle_t telemetryQueue = nullptr;
SemaphoreHandle_t serialMutex = nullptr;

uint32_t latestIr = 0;
uint32_t lastSampleMs = 0;
uint32_t lastBeatMs = 0;
uint32_t fingerLowSince = 0;
uint32_t lastTelemetryMs = 0;
float instantBpm = 0.0f;
uint16_t recentBpms[5] = {};
uint8_t bpmSampleCount = 0;
uint8_t bpmSampleCursor = 0;
bool sessionActive = false;
bool clockReady = false;
int8_t lastMatrixZone = -1;

const uint8_t PATTERN_CIRCLE[8] = {
    0b00111100, 0b01000010, 0b10000001, 0b10000001,
    0b10000001, 0b10000001, 0b01000010, 0b00111100};
const uint8_t PATTERN_SQUARE[8] = {
    0b11111111, 0b10000001, 0b10000001, 0b10000001,
    0b10000001, 0b10000001, 0b10000001, 0b11111111};
const uint8_t PATTERN_TRIANGLE[8] = {
    0b00000001, 0b00000011, 0b00000101, 0b00001001,
    0b00010001, 0b00100001, 0b01000001, 0b11111111};
const uint8_t PATTERN_STAR[8] = {
    0b00011000, 0b10011001, 0b01011010, 0b00111100,
    0b11111111, 0b00111100, 0b01011010, 0b10011001};

void logLine(const char *format, ...) {
  char message[192];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  if (serialMutex) xSemaphoreTake(serialMutex, portMAX_DELAY);
  Serial.println(message);
  if (serialMutex) xSemaphoreGive(serialMutex);
}

void printTelemetryLine(const TelemetryRecord &record) {
  char line[240];
  char bpmValue[8];
  if (record.bpmValid) {
    snprintf(bpmValue, sizeof(bpmValue), "%u", record.bpm);
  } else {
    snprintf(bpmValue, sizeof(bpmValue), "null");
  }
  snprintf(line, sizeof(line),
           "@VITALS {\"device_id\":\"%s\",\"ts_ms\":%lu,\"heart_rate_bpm\":%s,\"ir_raw\":%lu,\"finger_detected\":%s,\"session_active\":%s,\"source\":\"max30102-esp32-s3\"}",
           DEVICE_ID, static_cast<unsigned long>(record.deviceTsMs), bpmValue,
           static_cast<unsigned long>(record.irRaw),
           record.fingerDetected ? "true" : "false",
           record.sessionActive ? "true" : "false");
  if (serialMutex) xSemaphoreTake(serialMutex, portMAX_DELAY);
  Serial.println(line);
  if (serialMutex) xSemaphoreGive(serialMutex);
}

void matrixInit() {
  matrix.shutdown(0, false);
  matrix.setIntensity(0, 8);
  matrix.setScanLimit(0, 7);
  matrix.clearDisplay(0);
}

void showFullMatrix() {
  for (uint8_t column = 0; column < 8; ++column) matrix.setColumn(0, column, 0xFF);
}

void showBitmap(const uint8_t bitmap[8]) {
  matrix.clearDisplay(0);
  for (uint8_t column = 0; column < 8; ++column) {
    uint8_t columnData = 0;
    for (uint8_t row = 0; row < 8; ++row) {
      if (bitmap[row] & (0x80 >> column)) columnData |= (1U << row);
    }
    matrix.setColumn(0, column, columnData);
  }
}

uint16_t medianRecentBpm() {
  if (bpmSampleCount == 0) return 0;
  uint16_t sorted[5];
  for (uint8_t index = 0; index < bpmSampleCount; ++index) sorted[index] = recentBpms[index];
  for (uint8_t i = 1; i < bpmSampleCount; ++i) {
    const uint16_t value = sorted[i];
    int8_t j = static_cast<int8_t>(i) - 1;
    while (j >= 0 && sorted[j] > value) {
      sorted[j + 1] = sorted[j];
      --j;
    }
    sorted[j + 1] = value;
  }
  return sorted[bpmSampleCount / 2];
}

void updateMatrix(bool bpmFresh) {
  const uint16_t bpm = medianRecentBpm();
  if (!sessionActive || !bpmFresh || bpm < 50) {
    if (lastMatrixZone != 0) {
      showFullMatrix();
      lastMatrixZone = 0;
    }
    return;
  }

  const int8_t zone = bpm >= 100 ? 4 : bpm >= 80 ? 3 : bpm >= 70 ? 2 : 1;
  if (zone == lastMatrixZone) return;
  lastMatrixZone = zone;
  if (zone == 1) showBitmap(PATTERN_CIRCLE);
  else if (zone == 2) showBitmap(PATTERN_SQUARE);
  else if (zone == 3) showBitmap(PATTERN_TRIANGLE);
  else showBitmap(PATTERN_STAR);
}

void pushTelemetry(const TelemetryRecord &record) {
  if (xQueueSend(telemetryQueue, &record, 0) == pdTRUE) return;
  TelemetryRecord discarded;
  xQueueReceive(telemetryQueue, &discarded, 0);  // Keep the newest minute of data.
  xQueueSend(telemetryQueue, &record, 0);
}

void addBpmSample(float bpm) {
  if (bpm < 20.0f || bpm > 250.0f) return;
  recentBpms[bpmSampleCursor] = static_cast<uint16_t>(bpm + 0.5f);
  bpmSampleCursor = (bpmSampleCursor + 1) % 5;
  if (bpmSampleCount < 5) ++bpmSampleCount;
}

void sensorTask(void *) {
  for (;;) {
    particleSensor.check();
    while (particleSensor.available()) {
      latestIr = particleSensor.getFIFOIR();
      lastSampleMs = millis();
      particleSensor.nextSample();

      if (checkForBeat(static_cast<int32_t>(latestIr))) {
        const uint32_t now = millis();
        const uint32_t interval = lastBeatMs == 0 ? 0 : now - lastBeatMs;
        lastBeatMs = now;
        if (interval >= 300 && interval <= 2500) {
          instantBpm = 60000.0f / static_cast<float>(interval);
          addBpmSample(instantBpm);
        }
      }
    }

    const bool sampleFresh = lastSampleMs > 0 && (millis() - lastSampleMs) <= SAMPLE_STALE_MS;
    const bool fingerDetected = sampleFresh && latestIr >= IR_FINGER_THRESHOLD;
    if (fingerDetected) {
      fingerLowSince = 0;
      sessionActive = true;
    } else if (sessionActive) {
      if (fingerLowSince == 0) fingerLowSince = millis();
      if (millis() - fingerLowSince >= FINGER_LEAVE_MS) {
        sessionActive = false;
        lastBeatMs = 0;
        instantBpm = 0.0f;
        bpmSampleCount = 0;
        bpmSampleCursor = 0;
        lastMatrixZone = -1;
      }
    }

    const bool bpmFresh = sampleFresh && sessionActive && bpmSampleCount > 0 && lastBeatMs > 0 &&
                          (millis() - lastBeatMs) <= BEAT_STALE_MS;
    updateMatrix(bpmFresh);
    if (millis() - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
      lastTelemetryMs = millis();
      TelemetryRecord record{
          millis(), latestIr, bpmFresh ? medianRecentBpm() : 0, bpmFresh,
          fingerDetected, sessionActive};
      pushTelemetry(record);
      printTelemetryLine(record);
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

bool connectWiFi(uint32_t timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < timeoutMs) {
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFi.setSleep(false);  // Throughput/latency profile; not a battery-life setting.
  if (!clockReady) {
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
    const uint32_t clockWaitStarted = millis();
    while (time(nullptr) < 1704067200 && millis() - clockWaitStarted < 15000) {
      vTaskDelay(pdMS_TO_TICKS(250));
    }
    clockReady = time(nullptr) >= 1704067200;
    if (!clockReady) logLine("System time not synchronized; certificate checks will remain blocked");
  }
  logLine("Wi-Fi connected; RSSI=%d dBm", WiFi.RSSI());
  return true;
}

bool buildBatchJson(const TelemetryRecord *records, uint8_t count, char *body, size_t capacity) {
  size_t used = static_cast<size_t>(snprintf(body, capacity, "{\"records\":["));
  if (used >= capacity) return false;
  for (uint8_t index = 0; index < count; ++index) {
    const TelemetryRecord &record = records[index];
    char bpmValue[8];
    if (record.bpmValid) snprintf(bpmValue, sizeof(bpmValue), "%u", record.bpm);
    else snprintf(bpmValue, sizeof(bpmValue), "null");
    const int written = snprintf(
        body + used, capacity - used,
        "%s{\"device_id\":\"%s\",\"ts_ms\":%lu,\"heart_rate_bpm\":%s,\"ir_raw\":%lu,\"finger_detected\":%s,\"session_active\":%s,\"source\":\"max30102-esp32-s3\"}",
        index == 0 ? "" : ",", DEVICE_ID,
        static_cast<unsigned long>(record.deviceTsMs), bpmValue,
        static_cast<unsigned long>(record.irRaw),
        record.fingerDetected ? "true" : "false",
        record.sessionActive ? "true" : "false");
    if (written < 0 || static_cast<size_t>(written) >= capacity - used) return false;
    used += static_cast<size_t>(written);
  }
  if (used + 2 >= capacity) return false;
  body[used++] = ']';
  body[used++] = '}';
  body[used] = '\0';
  return true;
}

bool uploadBatch(const TelemetryRecord *records, uint8_t count) {
  if (!clockReady) {
    logLine("Upload paused: synchronize system time before validating TLS certificates");
    return false;
  }
  if (strlen(SERVER_ROOT_CA) < 40 || strncmp(API_URL, "https://", 8) != 0) {
    logLine("Upload paused: configure an HTTPS URL and trusted root CA in secrets.h");
    return false;
  }
  char body[HTTP_BODY_CAPACITY];
  if (!buildBatchJson(records, count, body, sizeof(body))) {
    logLine("Upload skipped: telemetry batch exceeded the fixed buffer");
    return false;
  }

  WiFiClientSecure tlsClient;
  tlsClient.setCACert(SERVER_ROOT_CA);
  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(4000);
  if (!http.begin(tlsClient, API_URL)) return false;
  http.addHeader("Content-Type", "application/json");
  char authorization[160];
  snprintf(authorization, sizeof(authorization), "Bearer %s", API_BEARER_TOKEN);
  http.addHeader("Authorization", authorization);
  const int status = http.POST(reinterpret_cast<uint8_t *>(body), strlen(body));
  http.end();
  if (status < 200 || status >= 300) {
    logLine("Batch upload failed: HTTP %d", status);
    return false;
  }
  logLine("Uploaded %u records; free heap=%u bytes", count,
          static_cast<unsigned>(ESP.getFreeHeap()));
  return true;
}

void networkTask(void *) {
  TelemetryRecord pending[HTTP_BATCH_SIZE];
  uint8_t pendingCount = 0;
  uint32_t batchStartedMs = 0;
  uint32_t retryAtMs = 0;
  uint32_t retryDelayMs = 1000;
  uint32_t reconnectDelayMs = 1000;

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      if (!connectWiFi(15000)) {
        logLine("Wi-Fi reconnect failed; retry in %lu ms",
                static_cast<unsigned long>(reconnectDelayMs));
        vTaskDelay(pdMS_TO_TICKS(reconnectDelayMs));
        reconnectDelayMs = reconnectDelayMs < 15000 ? reconnectDelayMs * 2 : 30000;
        continue;
      }
      reconnectDelayMs = 1000;
    }
    if (!clockReady && time(nullptr) >= 1704067200) {
      clockReady = true;
      logLine("System time synchronized for TLS validation");
    }

    if (pendingCount < HTTP_BATCH_SIZE) {
      TelemetryRecord next;
      const TickType_t waitTicks = pendingCount == 0 ? pdMS_TO_TICKS(500) : 0;
      if (xQueueReceive(telemetryQueue, &next, waitTicks) == pdTRUE) {
        if (pendingCount == 0) batchStartedMs = millis();
        pending[pendingCount++] = next;
      }
      while (pendingCount < HTTP_BATCH_SIZE &&
             xQueueReceive(telemetryQueue, &pending[pendingCount], 0) == pdTRUE) {
        ++pendingCount;
      }
    }

    if (pendingCount == 0) continue;
    const bool flushDue = pendingCount >= HTTP_BATCH_SIZE ||
                          millis() - batchStartedMs >= BATCH_FLUSH_MS;
    if (!flushDue || static_cast<int32_t>(millis() - retryAtMs) < 0) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    if (uploadBatch(pending, pendingCount)) {
      pendingCount = 0;
      retryDelayMs = 1000;
      retryAtMs = millis();
    } else {
      retryAtMs = millis() + retryDelayMs;
      retryDelayMs = retryDelayMs < 15000 ? retryDelayMs * 2 : 30000;
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  serialMutex = xSemaphoreCreateMutex();
  setCpuFrequencyMhz(240);
  matrixInit();
  showFullMatrix();

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    logLine("MAX30102 not found. Check I2C wiring, voltage, and address 0x57.");
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  particleSensor.setup(0x1F, 1, 2, 100, 411, 4096);
  particleSensor.setPulseAmplitudeRed(0x00);
  particleSensor.setPulseAmplitudeGreen(0x00);
  particleSensor.setPulseAmplitudeIR(0x1F);

  telemetryQueue = xQueueCreate(TELEMETRY_QUEUE_LENGTH, sizeof(TelemetryRecord));
  if (!telemetryQueue) {
    logLine("Could not create telemetry queue; rebooting in 3 seconds.");
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP.restart();
  }

  logLine("CARE SIGNAL V2 / ESP32-S3 / 100 Hz sensor / 1 Hz telemetry");
  logLine("Device ID: %s", DEVICE_ID);
  const BaseType_t sensorTaskStatus =
      xTaskCreatePinnedToCore(sensorTask, "sensor", 4096, nullptr, 4, nullptr, 1);
  const BaseType_t networkTaskStatus =
      xTaskCreatePinnedToCore(networkTask, "network", 8192, nullptr, 2, nullptr, 0);
  if (sensorTaskStatus != pdPASS || networkTaskStatus != pdPASS) {
    logLine("Could not start both FreeRTOS tasks; rebooting in 3 seconds.");
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP.restart();
  }
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
