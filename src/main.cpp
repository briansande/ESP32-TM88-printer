#include <Arduino.h>
#include <WiFi.h>
#include "wifi_secrets.h"
#include "printer_driver.hpp"
#include "web_server.hpp"

static PrinterDriver printer(16, 17, 19200);
static PrinterWebServer webServer(printer);

void setup() {
  Serial.begin(115200);
  delay(500);
  printer.begin();

  Serial.printf("Connecting to WiFi SSID: %s ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.printf("Connected! IP address: %s\n", WiFi.localIP().toString().c_str());

  webServer.begin();
}

void loop() {
  webServer.loop();
}
