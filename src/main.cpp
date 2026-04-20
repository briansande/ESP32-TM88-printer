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

static void printerPrint(const String& text) {
  printerReset();

  // Print each line
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

  // Extra line feeds for margin
  printer.write(0x0A);
  printer.write(0x0A);
  printer.write(0x0A);
}

static void printerCut() {
  // Feed some paper before cutting
  printer.write(0x1B);
  printer.write(0x64);
  printer.write(0x03);  // feed 3 lines

  // Partial cut
  printer.write(0x1D);
  printer.write(0x56);
  printer.write(0x41);
  printer.write(0x00);
}

static void printerFeed() {
  // Feed 5 lines
  printer.write(0x1B);
  printer.write(0x64);
  printer.write(0x05);
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
    max-width: 500px; width: 100%;
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

  <div class="ip">IP: %s</div>
</div>
<div class="toast" id="toast"></div>

<script>
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
</script>
</body>
</html>
)rawliteral";

// ─── Route handlers ──────────────────────────────────────────────────────────
static void handleRoot() {
  char buf[sizeof(HTML_PAGE) + 64];
  snprintf(buf, sizeof(buf), HTML_PAGE, WiFi.localIP().toString().c_str());
  server.send(200, "text/html", buf);
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

// ─── setup & loop ────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  // Initialize printer serial
  printer.begin(PRINTER_BAUD, SERIAL_8N1, PRINTER_RX, PRINTER_TX);
  delay(500);

  // Reset printer on boot
  printerReset();
  Serial.println("Printer initialized");

  // Connect to WiFi
  Serial.printf("Connecting to WiFi SSID: %s ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.printf("Connected! IP address: %s\n", WiFi.localIP().toString().c_str());

  // HTTP routes
  server.on("/",     HTTP_GET,  handleRoot);
  server.on("/print", HTTP_POST, handlePrint);
  server.on("/cut",   HTTP_POST, handleCut);
  server.on("/feed",  HTTP_POST, handleFeed);
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
}
