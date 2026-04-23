#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>

class PrinterDriver {
public:
  PrinterDriver(int rx, int tx, int baud, int txBufferSize = 4096);
  void begin();
  void reset();
  void print(const String& text);
  void printLine(const String& text);
  void cut();
  void feed(uint8_t n = 5);
  int getStatus();

  void setBold(bool on);
  void setUnderline(bool on);
  void setDoubleHeight(bool on);
  void setDoubleWidth(bool on);
  void setReverse(bool on);
  void resetStyle();

  void justifyLeft();
  void justifyCenter();
  void justifyRight();

  void setLineSpacing(uint8_t n);
  void defaultLineSpacing();
  void setCharacterSet(uint8_t n);
  void setPrintSpeed(uint8_t level);

// Fine-tune per-line paper advance to eliminate horizontal banding (TM-T88III).
// Sends ESC 3 n — sets line spacing to n dots (1/203 inch per dot).
// Default spacing (~34 dots) is restored by calling defaultLineSpacing().
// Tweak ±1–2 dots from baseline until banding disappears.
void setFeedAdjustment(uint8_t amount);

  void setBarcodeHeight(uint8_t n);
  void setBarcodeWidth(uint8_t n);
  void setBarcodeNumberPosition(uint8_t n);
  void printBarcode(uint8_t m, const String& data);

  void printImage(uint16_t widthDots, uint16_t heightDots,
                  const uint8_t* data, size_t dataLen);

private:
  HardwareSerial _serial;
  int _rx, _tx, _baud, _txBufferSize;
  uint8_t _style;
  void applyStyle();
  void writeBytes(const uint8_t* buf, size_t len);
};