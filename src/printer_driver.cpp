#include "printer_driver.hpp"

PrinterDriver::PrinterDriver(int rx, int tx, int baud)
  : _serial(2), _rx(rx), _tx(tx), _baud(baud), _style(0x00) {}

void PrinterDriver::begin() {
  _serial.begin(_baud, SERIAL_8N1, _rx, _tx);
  delay(500);
  reset();
  Serial.println("Printer initialized");
}

void PrinterDriver::reset() {
  _serial.write(0x1B);
  _serial.write(0x40);
  delay(100);
}

void PrinterDriver::applyStyle() {
  _serial.write(0x1B);
  _serial.write(0x21);
  _serial.write(_style);
}

void PrinterDriver::print(const String& text) {
  int start = 0;
  while (start < text.length()) {
    int newline = text.indexOf('\n', start);
    if (newline == -1) {
      _serial.print(text.substring(start));
      _serial.write(0x0A);
      break;
    } else {
      _serial.print(text.substring(start, newline));
      _serial.write(0x0A);
      start = newline + 1;
    }
  }

  _serial.write(0x0A);
  _serial.write(0x0A);
  _serial.write(0x0A);
}

void PrinterDriver::cut() {
  _serial.write(0x1B);
  _serial.write(0x64);
  _serial.write(0x03);

  _serial.write(0x1D);
  _serial.write(0x56);
  _serial.write(0x41);
  _serial.write(0x00);
}

void PrinterDriver::feed(uint8_t n) {
  _serial.write(0x1B);
  _serial.write(0x64);
  _serial.write(n);
}

int PrinterDriver::getStatus() {
  _serial.flush();
  while (_serial.available()) _serial.read();
  _serial.write(0x10);
  _serial.write(0x04);
  _serial.write(0x01);
  unsigned long start = millis();
  while (millis() - start < 500) {
    if (_serial.available()) {
      return _serial.read();
    }
    delay(1);
  }
  return -1;
}

void PrinterDriver::setBold(bool on) {
  if (on) _style |= 0x08;
  else    _style &= ~0x08;
  applyStyle();
}

void PrinterDriver::setUnderline(bool on) {
  if (on) _style |= 0x80;
  else    _style &= ~0x80;
  applyStyle();
}

void PrinterDriver::setDoubleHeight(bool on) {
  if (on) _style |= 0x10;
  else    _style &= ~0x10;
  applyStyle();
}

void PrinterDriver::setReverse(bool on) {
  _serial.write(0x1D);
  _serial.write(0x42);
  _serial.write(on ? 0x01 : 0x00);
}

void PrinterDriver::justifyLeft() {
  _serial.write(0x1B);
  _serial.write(0x61);
  _serial.write(0x00);
}

void PrinterDriver::justifyCenter() {
  _serial.write(0x1B);
  _serial.write(0x61);
  _serial.write(0x01);
}

void PrinterDriver::justifyRight() {
  _serial.write(0x1B);
  _serial.write(0x61);
  _serial.write(0x02);
}

void PrinterDriver::setLineSpacing(uint8_t n) {
  _serial.write(0x1B);
  _serial.write(0x33);
  _serial.write(n);
}

void PrinterDriver::defaultLineSpacing() {
  _serial.write(0x1B);
  _serial.write(0x32);
}

void PrinterDriver::setCharacterSet(uint8_t n) {
  _serial.write(0x1B);
  _serial.write(0x52);
  _serial.write(n);
}

void PrinterDriver::setBarcodeHeight(uint8_t n) {
  _serial.write(0x1D);
  _serial.write(0x68);
  _serial.write(n);
}

void PrinterDriver::setBarcodeWidth(uint8_t n) {
  _serial.write(0x1D);
  _serial.write(0x77);
  _serial.write(n);
}

void PrinterDriver::setBarcodeNumberPosition(uint8_t n) {
  _serial.write(0x1D);
  _serial.write(0x48);
  _serial.write(n);
}

void PrinterDriver::printBarcode(uint8_t m, const String& data) {
  _serial.write(0x1D);
  _serial.write(0x6B);
  _serial.write(m);
  _serial.write((uint8_t)data.length());
  for (unsigned int i = 0; i < data.length(); i++) {
    _serial.write(data[i]);
  }
}

void PrinterDriver::printImage(uint16_t widthDots, uint16_t heightDots,
                                const uint8_t* data, size_t dataLen) {
  uint16_t widthBytes = (widthDots + 7) / 8;
  _serial.write(0x1D); _serial.write(0x76); _serial.write(0x30);
  _serial.write(0x00);
  _serial.write(widthBytes & 0xFF);
  _serial.write((widthBytes >> 8) & 0xFF);
  _serial.write(heightDots & 0xFF);
  _serial.write((heightDots >> 8) & 0xFF);
  _serial.write(data, dataLen);
  _serial.flush();
  _serial.write(0x0A);
}
