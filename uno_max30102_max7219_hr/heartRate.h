/*
 Optical Heart Rate Detection (PBA Algorithm)
 By: Nathan Seidle @ SparkFun Electronics
 Date: October 2nd, 2016
 SPDX: BSD / Maxim copyright — 见 heartRate.cpp
*/

#if (ARDUINO >= 100)
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

bool checkForBeat(int32_t sample);
void resetHeartRateAlgorithm(void);
int16_t averageDCEstimator(int32_t *p, int32_t x);
int16_t lowPassFIRFilter(int16_t din);
int32_t mul16(int16_t x, int16_t y);
