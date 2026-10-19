#pragma once

// Copy this file to secrets.h and fill in local values. secrets.h is gitignored.
#define WIFI_SSID "replace-with-2.4GHz-network"
#define WIFI_PASSWORD "replace-with-password"

// Use an HTTPS endpoint that accepts POST /api/v1/telemetry/batch.
// Do not use the plain-HTTP localhost demo server with real data.
#define API_URL "https://your-domain.example/api/v1/telemetry/batch"
#define API_BEARER_TOKEN "replace-with-device-token"

// Replace the empty string with adjacent C string literals, escaping each newline:
// static const char SERVER_ROOT_CA[] PROGMEM =
//   "-----BEGIN CERTIFICATE-----\n"
//   "base64-certificate-line\n"
//   "-----END CERTIFICATE-----\n";
// Leave empty only while compiling; uploads are blocked until a certificate is set.
#define SERVER_ROOT_CA_DEFINED
static const char SERVER_ROOT_CA[] PROGMEM = "";

#define DEVICE_ID "uno-demo-esp32-s3"
