# V2 update — ESP32-S3 high-performance node

V2 keeps the original sensor and symbolic matrix concept, then replaces the UNO's single-core, serial-only path with an ESP32-S3 Wi-Fi node. The repository includes a PlatformIO firmware project and a batch endpoint compatible with the existing Python service.

## Why ESP32-S3

The ESP32-S3 is a dual-core 240 MHz MCU with 2.4 GHz Wi-Fi and BLE support. The current firmware pins the time-sensitive sensor loop to core 1 and network work to core 0, so TLS/HTTP waits do not block FIFO polling. The DevKitC-1 exposes GPIO 8 and 9 for I²C and GPIO 10–12 for the MAX7219 wiring used by this build. See Espressif's [ESP32-S3 overview](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/about.html) and [DevKitC-1 pin table](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html).

Suggested board: **ESP32-S3-DevKitC-1**, preferably an **N8R8** module for future buffer/UI expansion. This firmware does not require external PSRAM today; if the exact board has PSRAM, select the matching QSPI/OPI setting for that module. Espressif warns that flash/PSRAM mode must match the actual hardware, so do not copy a memory-mode setting from a different module. The provided PlatformIO target defaults to the N8 (8 MB flash, no PSRAM) variant.

## V2 performance path

- Sensor samples: MAX30102 configured for 100 samples/s, one hardware sample per FIFO record, over 400 kHz I²C.
- Acquisition: dedicated high-priority FreeRTOS task on core 1; no Wi-Fi or HTTP calls in this task.
- Signal display: median of the latest five accepted beat-rate values drives the same symbolic matrix bands. These bands remain visual states, not medical thresholds.
- Telemetry: emit one summary record per second; raw 100 Hz waveforms are not uploaded.
- Network: core 0 uses Wi-Fi station mode, SNTP time sync for certificate validity checks, TLS server-certificate validation, bearer-token header, and a fixed-size JSON buffer. Up to 12 records are uploaded together every five seconds.
- Backpressure: a 60-record FreeRTOS queue and a 12-record retry batch absorb short outages. If the minute-sized RAM queue fills, the oldest queued entry is dropped so recent data can resume flowing. This is volatile buffering; reset or extended outage loses data.
- Power profile: Wi-Fi sleep is disabled for lower latency. This is a bench/demo performance setting, not a battery-optimized wearable configuration.

The design uses ESP32-S3's Wi-Fi/dual-core capacity without pretending the current optical sensor is a validated all-day garment sensor. The S3 can also support BLE provisioning, secure signed OTA, motion sensors, and a richer local UI in later versions. Those features are not implemented in this update.

## Wiring

| Signal | ESP32-S3 DevKitC-1 | Notes |
| --- | --- | --- |
| MAX30102 SDA | GPIO 8 | I²C SDA |
| MAX30102 SCL | GPIO 9 | I²C SCL |
| MAX7219 DIN | GPIO 11 | Add a 3.3 V→5 V level shifter if the matrix board is powered at 5 V and its input-high spec requires it |
| MAX7219 CS/LOAD | GPIO 10 | Same logic-level note as DIN |
| MAX7219 CLK | GPIO 12 | Same logic-level note as DIN |
| Ground | GND | Common ground |

Power each breakout according to its own regulator and logic-level specifications. Do not connect a 5 V signal directly to an ESP32-S3 GPIO. Use short I²C wires and pull-ups appropriate to the bus voltage.

## Build and configure

1. Install PlatformIO Core and open `firmware/esp32-s3` as the PlatformIO project.
2. Copy `include/secrets.example.h` to `include/secrets.h`.
3. Set the 2.4 GHz Wi-Fi credentials, a unique pseudonymous `DEVICE_ID` using letters, digits, `_` or `-`, an HTTPS `API_URL`, a device bearer token, and the trusted root CA PEM for the server certificate. Enter the PEM as adjacent C string literals with `\n` at each line end, as shown in the example header comments.
4. Build and upload:

   ```bash
   pio run
   pio run --target upload
   pio device monitor --baud 115200
   ```

The code refuses to send telemetry unless `API_URL` begins with `https://` and a CA certificate is configured. It does not call `setInsecure()`.

## API and deployment boundary

The ESP32 sends `POST /api/v1/telemetry/batch` with `{"records":[...]}`. The Python local demo server now accepts batches of 1–12 records and stores each with a server receive time. The included server binds to `127.0.0.1` and speaks plain HTTP for the local browser demo; an ESP32 cannot reach that loopback listener over Wi-Fi.

For V2 network use, place the service behind a TLS reverse proxy or a hosted gateway and configure the ESP32 to use that HTTPS URL. The gateway must validate the bearer token and protect dashboard/API reads as well. The sample Python service does not authenticate the token and must not be exposed directly to a LAN or public network. Use synthetic data only until the deployment has authentication, authorization, audit, retention, and operational review controls.

USB debug output still uses the V1 `@VITALS` line format, so the existing serial bridge can be used for a local test. Choose either USB forwarding or network batch upload to avoid writing duplicate readings.

## What V2 does not add

It does not add SpO₂, temperature, movement classification, a custom garment, persistent offline storage, signed OTA, device provisioning, or a medical/custodial decision rule. ESP32-S3 improves compute and connectivity; it does not by itself make the sensor readings more accurate or validate continuous wear.
