#include "printer_driver.hpp"

// ---------------------------------------------------------------------------
// Construction & init
// ---------------------------------------------------------------------------

PrinterDriver::PrinterDriver(int rx, int tx, int baud, int txBufferSize)
  : _serial(2), _rx(rx), _tx(tx), _baud(baud),
    _txBufferSize(txBufferSize), _style(0x00) {}

void PrinterDriver::begin() {
  // Increase TX buffer BEFORE begin() so the OS allocates it correctly.
  // A large buffer lets printImage() queue the entire payload without stalling.
  _serial.setTxBufferSize(_txBufferSize);
  _serial.begin(_baud, SERIAL_8N1, _rx, _tx);
  delay(500);
  reset();
  setPrintSpeed(2);
  setFeedAdjustment(0x80);
  Serial.println("Printer initialized");
}

void PrinterDriver::reset() {
  _serial.write(0x1B);
  _serial.write(0x40);
  delay(100);
  _style = 0x00;  // keep software state in sync after reset
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void PrinterDriver::applyStyle() {
  _serial.write(0x1B);
  _serial.write(0x21);
  _serial.write(_style);
}

// Single write call keeps data transfer as atomic as possible.
void PrinterDriver::writeBytes(const uint8_t* buf, size_t len) {
  _serial.write(buf, len);
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------

void PrinterDriver::print(const String& text) {
  int start = 0;
  while (start < (int)text.length()) {
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
  // Three blank lines after block (original behaviour)
  _serial.write(0x0A);
  _serial.write(0x0A);
  _serial.write(0x0A);
}

void PrinterDriver::printLine(const String& text) {
  _serial.print(text);
  _serial.write(0x0A);
}

// ---------------------------------------------------------------------------
// Formatting
// ---------------------------------------------------------------------------

void PrinterDriver::setBold(bool on) {
  if (on) _style |= 0x08; else _style &= ~0x08;
  applyStyle();
}

void PrinterDriver::setUnderline(bool on) {
  // Bit 7 of ESC ! enables underline on TM-88 III
  if (on) _style |= 0x80; else _style &= ~0x80;
  applyStyle();
}

void PrinterDriver::setDoubleHeight(bool on) {
  if (on) _style |= 0x10; else _style &= ~0x10;
  applyStyle();
}

void PrinterDriver::setDoubleWidth(bool on) {
  if (on) _style |= 0x20; else _style &= ~0x20;
  applyStyle();
}

void PrinterDriver::setReverse(bool on) {
  _serial.write(0x1D);
  _serial.write(0x42);
  _serial.write(on ? 0x01 : 0x00);
}

void PrinterDriver::resetStyle() {
  _style = 0x00;
  applyStyle();
  setReverse(false);
}

// ---------------------------------------------------------------------------
// Alignment
// ---------------------------------------------------------------------------

void PrinterDriver::justifyLeft() {
  _serial.write(0x1B); _serial.write(0x61); _serial.write(0x00);
}

void PrinterDriver::justifyCenter() {
  _serial.write(0x1B); _serial.write(0x61); _serial.write(0x01);
}

void PrinterDriver::justifyRight() {
  _serial.write(0x1B); _serial.write(0x61); _serial.write(0x02);
}

// ---------------------------------------------------------------------------
// Spacing & feed
// ---------------------------------------------------------------------------

void PrinterDriver::setLineSpacing(uint8_t n) {
  _serial.write(0x1B); _serial.write(0x33); _serial.write(n);
}

void PrinterDriver::defaultLineSpacing() {
  _serial.write(0x1B); _serial.write(0x32);
}

void PrinterDriver::feed(uint8_t n) {
  _serial.write(0x1B); _serial.write(0x64); _serial.write(n);
}

// ---------------------------------------------------------------------------
// Character set
// ---------------------------------------------------------------------------

void PrinterDriver::setCharacterSet(uint8_t n) {
  _serial.write(0x1B); _serial.write(0x52); _serial.write(n);
}

void PrinterDriver::setPrintSpeed(uint8_t level) {
  if (level < 1) level = 1;
  if (level > 9) level = 9;
  uint8_t cmd[] = { 0x1D, 0x28, 0x4B, 0x02, 0x00, 0x32, level };
  _serial.write(cmd, sizeof(cmd));
}

// Fine-tune mechanical paper-feed amount (GS ( K <len> 0 50 amount).
// Valid range: 0x00 – 0xFF.  Neutral / factory default is 0x80.
// ESC 3 n — sets line spacing to n dots (1/203 inch per dot on TM-T88III).
// Default is ESC 2 which sets 1/6 inch (~34 dots at 203dpi).
// Typical body text: 24–30 dots. Tweak ±1 to fix banding.
// Call defaultLineSpacing() (ESC 2) to restore the default.
void PrinterDriver::setFeedAdjustment(uint8_t amount) {
  uint8_t cmd[] = { 0x1B, 0x33, amount };
  _serial.write(cmd, sizeof(cmd));
}

// ---------------------------------------------------------------------------
// Cut
// ---------------------------------------------------------------------------

void PrinterDriver::cut() {
  // Feed 3 lines then partial cut
  feed(3);
  _serial.write(0x1D);
  _serial.write(0x56);
  _serial.write(0x41);
  _serial.write(0x00);
}

// ---------------------------------------------------------------------------
// Barcode
// ---------------------------------------------------------------------------

void PrinterDriver::setBarcodeHeight(uint8_t n) {
  _serial.write(0x1D); _serial.write(0x68); _serial.write(n);
}

void PrinterDriver::setBarcodeWidth(uint8_t n) {
  _serial.write(0x1D); _serial.write(0x77); _serial.write(n);
}

void PrinterDriver::setBarcodeNumberPosition(uint8_t n) {
  _serial.write(0x1D); _serial.write(0x48); _serial.write(n);
}

void PrinterDriver::printBarcode(uint8_t m, const String& data) {
  setBarcodeHeight(162);
  setBarcodeWidth(3);
  setBarcodeNumberPosition(0x02);

  String barcodeData = data;
  if (m == 4 && !barcodeData.startsWith("*")) {
    barcodeData = "*" + barcodeData + "*";
  }

  _serial.write(0x1D);
  _serial.write(0x6B);
  _serial.write(m);
  for (unsigned int i = 0; i < barcodeData.length(); i++) {
    _serial.write(barcodeData[i]);
  }
  _serial.write(0x00);
  _serial.flush();
  _serial.write(0x0A);
}

// ---------------------------------------------------------------------------
// Image  ← THE KEY FIX
//
// Root cause of banding: the original code split data into 240-byte chunks,
// calling flush() + delay(10) between each one. The printer finished printing
// the current chunk, the paper stopped moving during the pause, then resumed —
// leaving a visible gap at each chunk boundary.
//
// Fix: send the header + ALL pixel data in a single continuous stream, then
// flush once at the end. This matches how barcode data is sent and gives the
// printer an uninterrupted feed to work from.
//
// The large TX buffer (default 4096 bytes) set in begin() means the ESP32's
// UART peripheral keeps the wire busy even for large images, so the printer
// never stalls waiting for data.
// ---------------------------------------------------------------------------

void PrinterDriver::printImage(uint16_t widthDots, uint16_t heightDots,
                                const uint8_t* data, size_t dataLen) {
  uint16_t widthBytes = (widthDots + 7) / 8;

  // Sanity check: dataLen should equal widthBytes * heightDots
  size_t expectedLen = (size_t)widthBytes * heightDots;
  if (dataLen != expectedLen) {
    Serial.printf("PrinterDriver::printImage: dataLen %u != expected %u — aborting\n",
                  (unsigned)dataLen, (unsigned)expectedLen);
    return;
  }

  // GS v 0 header (raster bit image)
  uint8_t header[8] = {
    0x1D, 0x76, 0x30, 0x00,
    (uint8_t)(widthBytes & 0xFF),  (uint8_t)((widthBytes >> 8) & 0xFF),
    (uint8_t)(heightDots & 0xFF),  (uint8_t)((heightDots >> 8) & 0xFF)
  };
  writeBytes(header, sizeof(header));

  // All pixel data in one shot — no chunking, no delays, no mid-stream flush
  writeBytes(data, dataLen);

  // Single flush after everything is queued
  _serial.flush();

  _serial.write(0x0A);
}

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

int PrinterDriver::getStatus() {
  _serial.flush();
  while (_serial.available()) _serial.read();  // drain any stale RX bytes

  // DLE EOT 1 — transmit paper/cover status
  _serial.write(0x10);
  _serial.write(0x04);
  _serial.write(0x01);

  unsigned long start = millis();
  while (millis() - start < 500) {
    if (_serial.available()) return _serial.read();
    delay(1);
  }
  return -1;  // timeout
}