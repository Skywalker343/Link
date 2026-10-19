/***************************************************
  SparkFun MAX3010x 驱动的 SoftwareWire 移植版（用于 MAX30102/MAX30105）。

  支持 SoftwareWire（任意脚软件 I2C）或硬件 TwoWire（Uno 的 Wire / A4 A5）。

  原始代码 BSD 许可，保留 SparkFun 原有版权声明。
***************************************************/

#pragma once

#if (ARDUINO >= 100)
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <Wire.h>
#include <SoftwareWire.h>

#define MAX30105_ADDRESS 0x57

#define I2C_SPEED_STANDARD 100000
#define I2C_SPEED_FAST 400000

#ifndef I2C_BUFFER_LENGTH
#define I2C_BUFFER_LENGTH 32
#endif

class MAX30102_SoftWire {
 public:
  MAX30102_SoftWire(void);

  boolean begin(SoftwareWire &wirePort, uint32_t i2cSpeed = I2C_SPEED_STANDARD,
                uint8_t i2caddr = MAX30105_ADDRESS);
  boolean begin(TwoWire &wirePort, uint32_t i2cSpeed = I2C_SPEED_STANDARD,
                uint8_t i2caddr = MAX30105_ADDRESS);

  uint32_t getRed(void);
  uint32_t getIR(void);
  uint32_t getGreen(void);
  bool safeCheck(uint8_t maxTimeToCheck);

  void softReset();
  void shutDown();
  void wakeUp();

  void setLEDMode(uint8_t mode);

  void setADCRange(uint8_t adcRange);
  void setSampleRate(uint8_t sampleRate);
  void setPulseWidth(uint8_t pulseWidth);

  void setPulseAmplitudeRed(uint8_t value);
  void setPulseAmplitudeIR(uint8_t value);
  void setPulseAmplitudeGreen(uint8_t value);
  void setPulseAmplitudeProximity(uint8_t value);

  void setProximityThreshold(uint8_t threshMSB);

  void enableSlot(uint8_t slotNumber, uint8_t device);
  void disableSlots(void);

  uint8_t getINT1(void);
  uint8_t getINT2(void);
  void enableAFULL(void);
  void disableAFULL(void);
  void enableDATARDY(void);
  void disableDATARDY(void);
  void enableALCOVF(void);
  void disableALCOVF(void);
  void enablePROXINT(void);
  void disablePROXINT(void);
  void enableDIETEMPRDY(void);
  void disableDIETEMPRDY(void);

  void setFIFOAverage(uint8_t samples);
  void enableFIFORollover();
  void disableFIFORollover();
  void setFIFOAlmostFull(uint8_t samples);

  uint16_t check(void);
  uint8_t available(void);
  void nextSample(void);
  uint32_t getFIFORed(void);
  uint32_t getFIFOIR(void);
  uint32_t getFIFOGreen(void);

  uint8_t getWritePointer(void);
  uint8_t getReadPointer(void);
  void clearFIFO(void);

  void setPROXINTTHRESH(uint8_t val);

  float readTemperature();
  float readTemperatureF();

  uint8_t getRevisionID();
  uint8_t readPartID();

  void setup(byte powerLevel = 0x1F, byte sampleAverage = 4, byte ledMode = 3, int sampleRate = 400,
             int pulseWidth = 411, int adcRange = 4096);

  uint8_t readRegister8(uint8_t address, uint8_t reg);
  void writeRegister8(uint8_t address, uint8_t reg, uint8_t value);

 private:
  SoftwareWire *_sw;
  TwoWire *_tw;
  uint8_t _i2caddr;

  byte activeLEDs;

  uint8_t revisionID;

  void readRevisionID();

  void bitMask(uint8_t reg, uint8_t mask, uint8_t thing);

  void busBegin(void);
  void busSetClock(uint32_t c);
  void busBeginTransmission(uint8_t address);
  size_t busWrite(uint8_t v);
  uint8_t busEndTransmission(uint8_t sendStop);
  uint8_t busRequestFrom(uint8_t addr, uint8_t qty);
  int busAvailable(void);
  int busRead(void);

#define STORAGE_SIZE 4
  typedef struct Record {
    uint32_t red[STORAGE_SIZE];
    uint32_t IR[STORAGE_SIZE];
    uint32_t green[STORAGE_SIZE];
    byte head;
    byte tail;
  } sense_struct;

  sense_struct sense;
};
