# Changelog

## 2.1.0 — Blue bilingual dashboard

- Redesigned the dashboard with a blue monitoring theme and clearer responsive layouts.
- Added a Chinese/English language switch that remembers the selected language in the browser.
- Localized dashboard status, device rows, review notices, chart labels, and action feedback.

## 2.0.0 — ESP32-S3 update

- Added a PlatformIO firmware target for ESP32-S3.
- Moved sensor acquisition and network upload into separate FreeRTOS tasks pinned to separate cores.
- Added a 60-record volatile queue, latest-five beat median, and five-second HTTPS batch upload with exponential retry.
- Added TLS root-CA validation, SNTP time synchronization, bearer-token forwarding, and a secrets template excluded from Git.
- Added `POST /api/v1/telemetry/batch` to the local API.
- Kept the UNO firmware and USB serial path as V1.

## 1.0.0 — Arduino UNO prototype

- MAX30102 IR sampling and optical beat detection.
- MAX7219 symbolic output.
- USB serial bridge, local SQLite API, and responsive dashboard.
- Synthetic demo readings for interface preview.
