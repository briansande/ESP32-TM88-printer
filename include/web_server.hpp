#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "printer_driver.hpp"

class PrinterWebServer {
public:
  PrinterWebServer(PrinterDriver& printer);
  void begin();
  void loop();

private:
  WebServer _server;
  PrinterDriver& _printer;
  static const char HTML_PAGE[] PROGMEM;

  void registerRoutes();
  void handleRoot();
  void handlePrint();
  void handleCut();
  void handleFeed();
  void handleStatus();
  void handleBold();
  void handleUnderline();
  void handleDoubleHeight();
  void handleReverse();
  void handleJustify();
  void handleLineSpacing();
  void handleDefaultLineSpacing();
  void handleCharacterSet();
  void handlePrintSpeed();
  void handleFeedAdjustment();
  void handleBarcode();
  void handlePrintImage();
};
