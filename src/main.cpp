#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WebServer.h>
#include "wifi_secrets.h"

// ─── Configuration ───────────────────────────────────────────────────────────
static const int PRINTER_RX = 16;
static const int PRINTER_TX = 17;
static const int PRINTER_BAUD = 9600;
// ──────────────────────────────────────────────────────────────────────────────

static HardwareSerial printer(2);
static WebServer      server(80);

// ─── Printer helpers ─────────────────────────────────────────────────────────
static void printerReset() {
  printer.write(0x1B);
  printer.write(0x40);
  delay(100);
}

static uint8_t currentStyle = 0x00;

static void applyStyle() {
  printer.write(0x1B);
  printer.write(0x21);
  printer.write(currentStyle);
}

static void printerPrint(const String& text) {
  int start = 0;
  while (start < text.length()) {
    int newline = text.indexOf('\n', start);
    if (newline == -1) {
      printer.print(text.substring(start));
      printer.write(0x0A);
      break;
    } else {
      printer.print(text.substring(start, newline));
      printer.write(0x0A);
      start = newline + 1;
    }
  }

  printer.write(0x0A);
  printer.write(0x0A);
  printer.write(0x0A);
}

static void printerCut() {
  printer.write(0x1B);
  printer.write(0x64);
  printer.write(0x03);

  printer.write(0x1D);
  printer.write(0x56);
  printer.write(0x41);
  printer.write(0x00);
}

static void printerFeed(uint8_t n) {
  printer.write(0x1B);
  printer.write(0x64);
  printer.write(n);
}

static void printerFeed() {
  printerFeed(5);
}

static int printerGetStatus() {
  printer.flush();
  while (printer.available()) printer.read();
  printer.write(0x10);
  printer.write(0x04);
  printer.write(0x01);
  unsigned long start = millis();
  while (millis() - start < 500) {
    if (printer.available()) {
      return printer.read();
    }
    delay(1);
  }
  return -1;
}

static void printerLineSpacing(uint8_t n) {
  printer.write(0x1B);
  printer.write(0x33);
  printer.write(n);
}

static void printerDefaultLineSpacing() {
  printer.write(0x1B);
  printer.write(0x32);
}

static void printerCharacterSet(uint8_t n) {
  printer.write(0x1B);
  printer.write(0x52);
  printer.write(n);
}

static void printerReverseOn() {
  printer.write(0x1D);
  printer.write(0x42);
  printer.write(0x01);
}

static void printerReverseOff() {
  printer.write(0x1D);
  printer.write(0x42);
  printer.write(0x00);
}

static void printerJustifyLeft() {
  printer.write(0x1B);
  printer.write(0x61);
  printer.write(0x00);
}

static void printerJustifyCenter() {
  printer.write(0x1B);
  printer.write(0x61);
  printer.write(0x01);
}

static void printerJustifyRight() {
  printer.write(0x1B);
  printer.write(0x61);
  printer.write(0x02);
}

static void printerBarcodeHeight(uint8_t n) {
  printer.write(0x1D);
  printer.write(0x68);
  printer.write(n);
}

static void printerBarcodeWidth(uint8_t n) {
  printer.write(0x1D);
  printer.write(0x77);
  printer.write(n);
}

static void printerBarcodeNumberPosition(uint8_t n) {
  printer.write(0x1D);
  printer.write(0x48);
  printer.write(n);
}

static void printerPrintBarcode(uint8_t m, const String& data) {
  printer.write(0x1D);
  printer.write(0x6B);
  printer.write(m);
  printer.write((uint8_t)data.length());
  for (unsigned int i = 0; i < data.length(); i++) {
    printer.write(data[i]);
  }
}

// ─── HTML page (embedded raw string) ─────────────────────────────────────────
static const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Epson Printer Dashboard</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    background: #1a1a2e; color: #e0e0e0;
    display: flex; justify-content: center; align-items: center;
    min-height: 100vh; padding: 20px;
  }
  .card {
    background: #16213e; border-radius: 16px; padding: 32px;
    max-width: 500px; width: 100%%;
    box-shadow: 0 8px 32px rgba(0,0,0,0.4);
  }
  h1 { text-align: center; font-size: 1.4rem; margin-bottom: 20px; color: #e94560; }
  .status {
    text-align: center; font-size: 1.0rem; margin-bottom: 20px;
    padding: 10px; border-radius: 8px; background: #0f3460;
  }
  .status .pos { font-weight: bold; color: #e94560; }
  .field { margin-bottom: 14px; }
  .field label {
    display: block; font-size: 0.85rem; color: #aaa;
    margin-bottom: 4px;
  }
  .field textarea {
    width: 100%%; padding: 12px; border: 1px solid #0f3460;
    border-radius: 8px; background: #1a1a2e; color: #e0e0e0;
    font-size: 1rem; outline: none; resize: vertical;
    min-height: 120px; font-family: monospace;
  }
  .field textarea:focus { border-color: #e94560; }
  .btn-row { display: flex; gap: 12px; margin-top: 16px; }
  .btn {
    flex: 1; padding: 14px; border: none; border-radius: 10px;
    font-size: 1rem; font-weight: bold; cursor: pointer;
    transition: opacity 0.2s;
  }
  .btn:hover { opacity: 0.85; }
  .btn:active { transform: scale(0.97); }
  .btn-print { background: #e94560; color: #fff; width: 100%%; margin-top: 12px; }
  .btn-cut   { background: #0f3460; color: #fff; }
  .btn-feed  { background: #533483; color: #fff; }
  .ip { text-align: center; margin-top: 16px; font-size: 0.8rem; color: #555; }
  .toast {
    display: none; position: fixed; bottom: 30px; left: 50%%;
    transform: translateX(-50%%); background: #533483; color: #fff;
    padding: 12px 24px; border-radius: 8px; font-size: 0.9rem;
    box-shadow: 0 4px 16px rgba(0,0,0,0.4);
  }
  .toast.show { display: block; animation: fade 2s forwards; }
  @keyframes fade { 0%%{opacity:1} 70%%{opacity:1} 100%%{opacity:0} }
  .section { margin-top: 20px; padding-top: 16px; border-top: 1px solid #0f3460; }
  .section-title { font-size: 0.75rem; color: #666; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 10px; }
  .toggle-row { display: flex; gap: 8px; flex-wrap: wrap; margin-bottom: 4px; }
  .toggle-btn {
    padding: 10px 16px; border: 2px solid #0f3460; border-radius: 8px;
    background: transparent; color: #e0e0e0; font-size: 0.85rem; cursor: pointer;
    transition: all 0.2s; font-weight: bold;
  }
  .toggle-btn:hover { border-color: #e94560; }
  .toggle-btn.active { background: #e94560; border-color: #e94560; }
  .input-row { display: flex; gap: 8px; align-items: center; margin-bottom: 8px; }
  .field-num {
    width: 80px; padding: 10px; border: 1px solid #0f3460; border-radius: 8px;
    background: #1a1a2e; color: #e0e0e0; font-size: 0.9rem; outline: none;
  }
  .field-num:focus { border-color: #e94560; }
  .field-select {
    padding: 10px; border: 1px solid #0f3460; border-radius: 8px;
    background: #1a1a2e; color: #e0e0e0; font-size: 0.9rem; outline: none;
    width: 100%%;
  }
  .field-select:focus { border-color: #e94560; }
  .field-text {
    flex: 1; padding: 10px; border: 1px solid #0f3460; border-radius: 8px;
    background: #1a1a2e; color: #e0e0e0; font-size: 0.9rem; outline: none;
  }
  .field-text:focus { border-color: #e94560; }
  .btn-sm {
    padding: 10px 16px; border: none; border-radius: 8px;
    font-size: 0.85rem; font-weight: bold; cursor: pointer;
    background: #0f3460; color: #fff; transition: opacity 0.2s;
  }
  .btn-sm:hover { opacity: 0.85; }
  .btn-sm:active { transform: scale(0.97); }
  .btn-status { background: #0f3460; color: #fff; width: 100%%; margin-top: 16px; }
</style>
</head>
<body>
<div class="card">
  <h1>&#128424; Epson Printer</h1>
  <div class="status" id="status">Status: <span class="pos" id="statusText">Ready</span></div>

  <div class="field">
    <label for="printText">Message to print</label>
    <textarea id="printText" placeholder="Type your message here..."></textarea>
  </div>

  <button class="btn btn-print" onclick="printMessage()">PRINT</button>

  <div class="btn-row">
    <button class="btn btn-cut" onclick="cutPaper()">CUT PAPER</button>
    <button class="btn btn-feed" onclick="feedPaper()">FEED PAPER</button>
  </div>

  <div class="section">
    <div class="section-title">Style</div>
    <div class="toggle-row">
      <button class="toggle-btn" id="btnBold" onclick="toggleStyle('bold')">Bold</button>
      <button class="toggle-btn" id="btnUnderline" onclick="toggleStyle('underline')">Underline</button>
      <button class="toggle-btn" id="btnDoubleHeight" onclick="toggleStyle('doubleHeight')">Double-Height</button>
      <button class="toggle-btn" id="btnReverse" onclick="toggleStyle('reverse')">Reverse</button>
    </div>
  </div>

  <div class="section">
    <div class="section-title">Justify</div>
    <div class="toggle-row">
      <button class="toggle-btn justify-btn active" id="btnLeft" onclick="setJustify('left')">Left</button>
      <button class="toggle-btn justify-btn" id="btnCenter" onclick="setJustify('center')">Center</button>
      <button class="toggle-btn justify-btn" id="btnRight" onclick="setJustify('right')">Right</button>
    </div>
  </div>

  <div class="section">
    <div class="section-title">Line Spacing</div>
    <div class="input-row">
      <input type="number" class="field-num" id="lineSpacing" min="0" max="255" value="30">
      <button class="btn-sm" onclick="setLineSpacing()">Set</button>
      <button class="btn-sm" onclick="defaultLineSpacing()">Default</button>
    </div>
  </div>

  <div class="section">
    <div class="section-title">Character Set</div>
    <div class="input-row">
      <select class="field-select" id="charSet" onchange="setCharSet()">
        <option value="0">USA</option>
        <option value="1">France</option>
        <option value="2">Germany</option>
        <option value="3">UK</option>
        <option value="4">Denmark I</option>
        <option value="5">Sweden</option>
        <option value="6">Italy</option>
        <option value="7">Spain I</option>
        <option value="8">Japan</option>
        <option value="9">Norway</option>
        <option value="10">Denmark II</option>
      </select>
    </div>
  </div>

  <div class="section">
    <div class="section-title">Barcode</div>
    <div class="input-row">
      <select class="field-select" id="barcodeType">
        <option value="65">UPC-A</option>
        <option value="66">UPC-E</option>
        <option value="67">EAN-13</option>
        <option value="68">EAN-8</option>
        <option value="69" selected>CODE39</option>
        <option value="70">ITF</option>
        <option value="71">CODABAR</option>
        <option value="72">CODE93</option>
        <option value="73">CODE128</option>
      </select>
    </div>
    <div class="input-row">
      <input type="text" class="field-text" id="barcodeData" placeholder="Barcode data...">
      <button class="btn-sm" onclick="printBarcode()">Print</button>
    </div>
  </div>

  <button class="btn btn-status" onclick="refreshStatus()">REFRESH STATUS</button>

  <div class="ip">IP: %s</div>
</div>
<div class="toast" id="toast"></div>

<script>
var styleState = { bold: false, underline: false, doubleHeight: false, reverse: false };

function showToast(msg) {
  var t = document.getElementById('toast');
  t.textContent = msg; t.className = 'toast show';
  setTimeout(function(){ t.className = 'toast'; }, 2000);
}

function setStatus(txt) {
  document.getElementById('statusText').textContent = txt;
}

function printMessage() {
  var text = document.getElementById('printText').value;
  if (!text.trim()) { showToast('Enter a message first'); return; }
  setStatus('Printing...');
  fetch('/print', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'text=' + encodeURIComponent(text)
  })
    .then(function(r){ return r.json(); })
    .then(function(d){
      setStatus('Ready');
      showToast(d.ok ? 'Printed successfully' : 'Print failed');
    })
    .catch(function(){ setStatus('Error'); showToast('Error communicating with ESP32'); });
}

function cutPaper() {
  setStatus('Cutting...');
  fetch('/cut', {method:'POST'})
    .then(function(r){ return r.json(); })
    .then(function(d){
      setStatus('Ready');
      showToast(d.ok ? 'Paper cut' : 'Cut failed');
    })
    .catch(function(){ setStatus('Error'); showToast('Error communicating with ESP32'); });
}

function feedPaper() {
  setStatus('Feeding...');
  fetch('/feed', {method:'POST'})
    .then(function(r){ return r.json(); })
    .then(function(d){
      setStatus('Ready');
      showToast(d.ok ? 'Paper fed' : 'Feed failed');
    })
    .catch(function(){ setStatus('Error'); showToast('Error communicating with ESP32'); });
}

function toggleStyle(name) {
  styleState[name] = !styleState[name];
  var on = styleState[name] ? 1 : 0;
  var btnId = 'btn' + name.charAt(0).toUpperCase() + name.slice(1);
  document.getElementById(btnId).className = 'toggle-btn' + (styleState[name] ? ' active' : '');
  fetch('/' + name, {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'on=' + on
  })
    .then(function(r){ return r.json(); })
    .then(function(d){ showToast(d.ok ? (name + ' ' + (on ? 'on' : 'off')) : 'Failed'); })
    .catch(function(){ showToast('Error communicating with ESP32'); });
}

function setJustify(pos) {
  var btns = document.querySelectorAll('.justify-btn');
  for (var i = 0; i < btns.length; i++) btns[i].className = 'toggle-btn justify-btn';
  var id = 'btn' + pos.charAt(0).toUpperCase() + pos.slice(1);
  document.getElementById(id).className = 'toggle-btn justify-btn active';
  fetch('/justify', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'pos=' + pos
  })
    .then(function(r){ return r.json(); })
    .then(function(d){ showToast(d.ok ? ('Justified ' + pos) : 'Failed'); })
    .catch(function(){ showToast('Error communicating with ESP32'); });
}

function setLineSpacing() {
  var n = document.getElementById('lineSpacing').value;
  fetch('/lineSpacing', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'n=' + n
  })
    .then(function(r){ return r.json(); })
    .then(function(d){ showToast(d.ok ? 'Line spacing set' : 'Failed'); })
    .catch(function(){ showToast('Error communicating with ESP32'); });
}

function defaultLineSpacing() {
  fetch('/defaultLineSpacing', {method:'POST'})
    .then(function(r){ return r.json(); })
    .then(function(d){ showToast(d.ok ? 'Default spacing set' : 'Failed'); })
    .catch(function(){ showToast('Error communicating with ESP32'); });
}

function setCharSet() {
  var n = document.getElementById('charSet').value;
  fetch('/characterSet', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'n=' + n
  })
    .then(function(r){ return r.json(); })
    .then(function(d){ showToast(d.ok ? 'Character set updated' : 'Failed'); })
    .catch(function(){ showToast('Error communicating with ESP32'); });
}

function printBarcode() {
  var m = document.getElementById('barcodeType').value;
  var data = document.getElementById('barcodeData').value;
  if (!data.trim()) { showToast('Enter barcode data'); return; }
  fetch('/barcode', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'm=' + m + '&data=' + encodeURIComponent(data)
  })
    .then(function(r){ return r.json(); })
    .then(function(d){ showToast(d.ok ? 'Barcode printed' : 'Failed'); })
    .catch(function(){ showToast('Error communicating with ESP32'); });
}

function refreshStatus() {
  setStatus('Querying...');
  fetch('/status')
    .then(function(r){ return r.json(); })
    .then(function(d){
      var txt = 'Raw: ' + d.raw;
      if (d.online !== undefined) txt += ' | ' + (d.online ? 'Online' : 'Offline');
      if (d.paper !== undefined) txt += ' | Paper: ' + (d.paper ? 'OK' : 'Empty');
      if (d.raw < 0) txt = 'Timeout';
      document.getElementById('statusText').textContent = txt;
      showToast('Status refreshed');
    })
    .catch(function(){ setStatus('Error'); showToast('Error fetching status'); });
}
</script>
</body>
</html>
)rawliteral";

// ─── Route handlers ──────────────────────────────────────────────────────────
static void handleRoot() {
  size_t needed = sizeof(HTML_PAGE) + 64;
  char* buf = (char*)malloc(needed);
  if (!buf) {
    server.send(500, "text/plain", "Out of memory");
    return;
  }
  snprintf(buf, needed, HTML_PAGE, WiFi.localIP().toString().c_str());
  server.send(200, "text/html", buf);
  free(buf);
}

static void handlePrint() {
  if (server.hasArg("text")) {
    String text = server.arg("text");
    Serial.printf("-> PRINT: \"%s\"\n", text.c_str());
    printerPrint(text);
    server.send(200, "application/json", "{\"ok\":true}");
  } else {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing text\"}");
  }
}

static void handleCut() {
  Serial.println("-> CUT");
  printerCut();
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleFeed() {
  Serial.println("-> FEED");
  printerFeed();
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleStatus() {
  int raw = printerGetStatus();
  String json = "{\"raw\":" + String(raw);
  if (raw < 0) {
    json += "}";
  } else {
    json += ",\"online\":" + String((raw & 0x08) ? "true" : "false");
    json += ",\"paper\":" + String((raw & 0x40) ? "false" : "true");
    json += "}";
  }
  server.send(200, "application/json", json);
}

static void handleBold() {
  if (server.hasArg("on") && server.arg("on") == "1") {
    currentStyle |= 0x08;
  } else {
    currentStyle &= ~0x08;
  }
  applyStyle();
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleUnderline() {
  if (server.hasArg("on") && server.arg("on") == "1") {
    currentStyle |= 0x80;
  } else {
    currentStyle &= ~0x80;
  }
  applyStyle();
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleDoubleHeight() {
  if (server.hasArg("on") && server.arg("on") == "1") {
    currentStyle |= 0x10;
  } else {
    currentStyle &= ~0x10;
  }
  applyStyle();
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleReverse() {
  if (server.hasArg("on") && server.arg("on") == "1") {
    printerReverseOn();
  } else {
    printerReverseOff();
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleJustify() {
  if (server.hasArg("pos")) {
    String pos = server.arg("pos");
    if (pos == "left") printerJustifyLeft();
    else if (pos == "center") printerJustifyCenter();
    else if (pos == "right") printerJustifyRight();
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleLineSpacing() {
  if (server.hasArg("n")) {
    int n = server.arg("n").toInt();
    if (n >= 0 && n <= 255) {
      printerLineSpacing((uint8_t)n);
    }
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleDefaultLineSpacing() {
  printerDefaultLineSpacing();
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleCharacterSet() {
  if (server.hasArg("n")) {
    int n = server.arg("n").toInt();
    if (n >= 0 && n <= 10) {
      printerCharacterSet((uint8_t)n);
    }
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleBarcode() {
  if (server.hasArg("m") && server.hasArg("data")) {
    int m = server.arg("m").toInt();
    String data = server.arg("data");
    if (m >= 65 && m <= 73 && data.length() > 0) {
      printerPrintBarcode((uint8_t)m, data);
      server.send(200, "application/json", "{\"ok\":true}");
      return;
    }
  }
  server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid params\"}");
}

// ─── setup & loop ────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  printer.begin(PRINTER_BAUD, SERIAL_8N1, PRINTER_RX, PRINTER_TX);
  delay(500);

  printerReset();
  Serial.println("Printer initialized");

  Serial.printf("Connecting to WiFi SSID: %s ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.printf("Connected! IP address: %s\n", WiFi.localIP().toString().c_str());

  server.on("/",                  HTTP_GET,  handleRoot);
  server.on("/print",             HTTP_POST, handlePrint);
  server.on("/cut",               HTTP_POST, handleCut);
  server.on("/feed",              HTTP_POST, handleFeed);
  server.on("/status",            HTTP_GET,  handleStatus);
  server.on("/bold",              HTTP_POST, handleBold);
  server.on("/underline",         HTTP_POST, handleUnderline);
  server.on("/doubleHeight",      HTTP_POST, handleDoubleHeight);
  server.on("/reverse",           HTTP_POST, handleReverse);
  server.on("/justify",           HTTP_POST, handleJustify);
  server.on("/lineSpacing",       HTTP_POST, handleLineSpacing);
  server.on("/defaultLineSpacing",HTTP_POST, handleDefaultLineSpacing);
  server.on("/characterSet",      HTTP_POST, handleCharacterSet);
  server.on("/barcode",           HTTP_POST, handleBarcode);
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
}
