# Third-party notices

## SparkFun MAX3010x sensor library and PBA heart-rate algorithm

The provided `MAX30102_SoftWire.*` and `heartRate.*` files retain attribution to Peter Jansen, Nathan Seidle, SparkFun Electronics, and Maxim Integrated in their source headers. They are adapted from the sensor-driver and optical beat-detection work in:

- [SparkFun MAX3010x Sensor Library](https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library)
- [SparkFun `heartRate.cpp`](https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library/blob/master/src/heartRate.cpp)
- [SparkFun license information](https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library/blob/master/LICENSE.md)

The upstream repository's current license file identifies the code as MIT licensed. The source files supplied with this project also contain the original BSD attribution wording; those notices have been retained. The local driver was modified to support both `SoftwareWire` and Arduino `TwoWire`.

## SoftwareWire dependency

The Arduino build includes the `SoftwareWire` library header. It is installed separately through Arduino Library Manager and is not redistributed here. See [Testato/SoftwareWire](https://github.com/Testato/SoftwareWire) for its GPL-3.0 license and installation details.

## LedControl dependency

The sketch uses the separately installed [LedControl Arduino library](https://github.com/wayoda/LedControl). It is not redistributed here. The upstream project includes an MIT-style license; retain its notice if you redistribute that library.

## Implementation references

These repositories informed the architecture and integration patterns; their source code is not copied into this repository:

- [rub3sh/iot-vitals](https://github.com/rub3sh/iot-vitals) — local-first sensor-to-MQTT-to-storage-to-dashboard separation, clear simulator boundary, and withholding unsupported readings.
- [rahul-s12/Smart_Ambulance](https://github.com/rahul-s12/Smart_Ambulance) — sensor node, message transport, backend ingest, and live dashboard structure.

The dashboard, local API, serial bridge, and project documentation in this repository were written for this prototype.
