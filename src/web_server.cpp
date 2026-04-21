#include "web_server.hpp"
#include "base64.hpp"
#include <WiFi.h>

const char PrinterWebServer::HTML_PAGE[] PROGMEM = R"rawliteral(
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
  .field-file { color: #e0e0e0; font-size: 0.85rem; width: 100%%; }
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

  <div class="section">
    <div class="section-title">Image Print</div>
    <div class="input-row">
      <input type="file" id="imageFile" accept="image/*" class="field-file">
    </div>
    <div class="input-row">
      <select class="field-select" id="paperWidth">
        <option value="384">58mm (384 dots)</option>
        <option value="576" selected>80mm (576 dots)</option>
      </select>
      <select class="field-select" id="ditherMethod">
        <option value="floyd" selected>Floyd-Steinberg</option>
        <option value="threshold">Threshold</option>
      </select>
    </div>
    <canvas id="previewCanvas" style="display:none;border:1px solid #0f3460;border-radius:8px;margin-top:8px;"></canvas>
    <button class="btn btn-print" id="printImageBtn" onclick="printImage()" style="display:none;">PRINT IMAGE</button>
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

var _origImg = null;
var _imageBytes = null;
var _imageWidth = 0;
var _imageHeight = 0;

document.getElementById('imageFile').addEventListener('change', function(e) {
  var file = e.target.files[0];
  if (!file) return;
  var reader = new FileReader();
  reader.onload = function(ev) {
    var img = new Image();
    img.onload = function() {
      _origImg = img;
      processImage();
    };
    img.src = ev.target.result;
  };
  reader.readAsDataURL(file);
});

document.getElementById('paperWidth').addEventListener('change', function() { if (_origImg) processImage(); });
document.getElementById('ditherMethod').addEventListener('change', function() { if (_origImg) processImage(); });

function processImage() {
  var paperW = parseInt(document.getElementById('paperWidth').value);
  var scale = paperW / _origImg.width;
  var w = paperW;
  var h = Math.round(_origImg.height * scale);
  if (h > 800) h = 800;

  var offscreen = document.createElement('canvas');
  offscreen.width = w;
  offscreen.height = h;
  var octx = offscreen.getContext('2d');
  octx.drawImage(_origImg, 0, 0, w, h);
  var imgData = octx.getImageData(0, 0, w, h);

  var gray = new Float32Array(w * h);
  for (var i = 0; i < w * h; i++) {
    gray[i] = imgData.data[i*4] * 0.299 + imgData.data[i*4+1] * 0.587 + imgData.data[i*4+2] * 0.114;
  }

  var method = document.getElementById('ditherMethod').value;
  if (method === 'floyd') {
    for (var y = 0; y < h; y++) {
      for (var x = 0; x < w; x++) {
        var idx = y * w + x;
        var oldVal = gray[idx];
        var newVal = oldVal < 128 ? 0 : 255;
        gray[idx] = newVal;
        var err = oldVal - newVal;
        if (x + 1 < w) gray[idx + 1] += err * 7 / 16;
        if (y + 1 < h) {
          if (x - 1 >= 0) gray[(y+1)*w + x - 1] += err * 3 / 16;
          gray[(y+1)*w + x] += err * 5 / 16;
          if (x + 1 < w) gray[(y+1)*w + x + 1] += err * 1 / 16;
        }
      }
    }
  } else {
    for (var i = 0; i < gray.length; i++) {
      gray[i] = gray[i] < 128 ? 0 : 255;
    }
  }

  var widthBytes = Math.ceil(w / 8);
  var bytes = new Uint8Array(widthBytes * h);
  for (var y = 0; y < h; y++) {
    for (var x = 0; x < w; x++) {
      if (gray[y * w + x] < 128) {
        bytes[y * widthBytes + Math.floor(x / 8)] |= (0x80 >> (x & 7));
      }
    }
  }

  var preview = document.getElementById('previewCanvas');
  preview.width = w;
  preview.height = h;
  preview.style.display = 'block';
  var maxW = 436;
  var dScale = Math.min(maxW / w, 1);
  preview.style.width = Math.round(w * dScale) + 'px';
  preview.style.height = Math.round(h * dScale) + 'px';
  var pctx = preview.getContext('2d');
  var pid = pctx.createImageData(w, h);
  for (var y = 0; y < h; y++) {
    for (var x = 0; x < w; x++) {
      var val = gray[y * w + x] < 128 ? 0 : 255;
      var pidx = (y * w + x) * 4;
      pid.data[pidx] = val;
      pid.data[pidx+1] = val;
      pid.data[pidx+2] = val;
      pid.data[pidx+3] = 255;
    }
  }
  pctx.putImageData(pid, 0, 0);

  _imageBytes = bytes;
  _imageWidth = w;
  _imageHeight = h;
  document.getElementById('printImageBtn').style.display = 'block';
  showToast(w + 'x' + h + ' image ready');
}

function printImage() {
  if (!_imageBytes) { showToast('No image loaded'); return; }
  var binary = '';
  var chunk = 8192;
  for (var i = 0; i < _imageBytes.length; i += chunk) {
    var slice = _imageBytes.subarray(i, Math.min(i + chunk, _imageBytes.length));
    binary += String.fromCharCode.apply(null, slice);
  }
  var b64 = btoa(binary);
  setStatus('Printing image...');
  fetch('/printImage?width=' + _imageWidth + '&height=' + _imageHeight, {
    method: 'POST',
    headers: { 'Content-Type': 'text/plain' },
    body: b64
  })
    .then(function(r){ return r.json(); })
    .then(function(d){
      setStatus('Ready');
      showToast(d.ok ? 'Image printed' : 'Print failed: ' + (d.error || ''));
    })
    .catch(function(){ setStatus('Error'); showToast('Error communicating with ESP32'); });
}
</script>
</body>
</html>
)rawliteral";

PrinterWebServer::PrinterWebServer(PrinterDriver& printer)
  : _server(80), _printer(printer) {}

void PrinterWebServer::registerRoutes() {
  _server.on("/",                  HTTP_GET,  [this](){ handleRoot(); });
  _server.on("/print",             HTTP_POST, [this](){ handlePrint(); });
  _server.on("/cut",               HTTP_POST, [this](){ handleCut(); });
  _server.on("/feed",              HTTP_POST, [this](){ handleFeed(); });
  _server.on("/status",            HTTP_GET,  [this](){ handleStatus(); });
  _server.on("/bold",              HTTP_POST, [this](){ handleBold(); });
  _server.on("/underline",         HTTP_POST, [this](){ handleUnderline(); });
  _server.on("/doubleHeight",      HTTP_POST, [this](){ handleDoubleHeight(); });
  _server.on("/reverse",           HTTP_POST, [this](){ handleReverse(); });
  _server.on("/justify",           HTTP_POST, [this](){ handleJustify(); });
  _server.on("/lineSpacing",       HTTP_POST, [this](){ handleLineSpacing(); });
  _server.on("/defaultLineSpacing",HTTP_POST, [this](){ handleDefaultLineSpacing(); });
  _server.on("/characterSet",      HTTP_POST, [this](){ handleCharacterSet(); });
  _server.on("/barcode",           HTTP_POST, [this](){ handleBarcode(); });
  _server.on("/printImage",        HTTP_POST, [this](){ handlePrintImage(); });
}

void PrinterWebServer::begin() {
  registerRoutes();
  _server.begin();
  Serial.println("Web server started");
}

void PrinterWebServer::loop() {
  _server.handleClient();
}

void PrinterWebServer::handleRoot() {
  size_t needed = sizeof(HTML_PAGE) + 64;
  char* buf = (char*)malloc(needed);
  if (!buf) {
    _server.send(500, "text/plain", "Out of memory");
    return;
  }
  snprintf(buf, needed, HTML_PAGE, WiFi.localIP().toString().c_str());
  _server.send(200, "text/html", buf);
  free(buf);
}

void PrinterWebServer::handlePrint() {
  if (_server.hasArg("text")) {
    String text = _server.arg("text");
    Serial.printf("-> PRINT: \"%s\"\n", text.c_str());
    _printer.print(text);
    _server.send(200, "application/json", "{\"ok\":true}");
  } else {
    _server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing text\"}");
  }
}

void PrinterWebServer::handleCut() {
  Serial.println("-> CUT");
  _printer.cut();
  _server.send(200, "application/json", "{\"ok\":true}");
}

void PrinterWebServer::handleFeed() {
  Serial.println("-> FEED");
  _printer.feed();
  _server.send(200, "application/json", "{\"ok\":true}");
}

void PrinterWebServer::handleStatus() {
  int raw = _printer.getStatus();
  String json = "{\"raw\":" + String(raw);
  if (raw < 0) {
    json += "}";
  } else {
    json += ",\"online\":" + String((raw & 0x08) ? "true" : "false");
    json += ",\"paper\":" + String((raw & 0x40) ? "false" : "true");
    json += "}";
  }
  _server.send(200, "application/json", json);
}

void PrinterWebServer::handleBold() {
  bool on = _server.hasArg("on") && _server.arg("on") == "1";
  _printer.setBold(on);
  _server.send(200, "application/json", "{\"ok\":true}");
}

void PrinterWebServer::handleUnderline() {
  bool on = _server.hasArg("on") && _server.arg("on") == "1";
  _printer.setUnderline(on);
  _server.send(200, "application/json", "{\"ok\":true}");
}

void PrinterWebServer::handleDoubleHeight() {
  bool on = _server.hasArg("on") && _server.arg("on") == "1";
  _printer.setDoubleHeight(on);
  _server.send(200, "application/json", "{\"ok\":true}");
}

void PrinterWebServer::handleReverse() {
  bool on = _server.hasArg("on") && _server.arg("on") == "1";
  _printer.setReverse(on);
  _server.send(200, "application/json", "{\"ok\":true}");
}

void PrinterWebServer::handleJustify() {
  if (_server.hasArg("pos")) {
    String pos = _server.arg("pos");
    if (pos == "left") _printer.justifyLeft();
    else if (pos == "center") _printer.justifyCenter();
    else if (pos == "right") _printer.justifyRight();
  }
  _server.send(200, "application/json", "{\"ok\":true}");
}

void PrinterWebServer::handleLineSpacing() {
  if (_server.hasArg("n")) {
    int n = _server.arg("n").toInt();
    if (n >= 0 && n <= 255) {
      _printer.setLineSpacing((uint8_t)n);
    }
  }
  _server.send(200, "application/json", "{\"ok\":true}");
}

void PrinterWebServer::handleDefaultLineSpacing() {
  _printer.defaultLineSpacing();
  _server.send(200, "application/json", "{\"ok\":true}");
}

void PrinterWebServer::handleCharacterSet() {
  if (_server.hasArg("n")) {
    int n = _server.arg("n").toInt();
    if (n >= 0 && n <= 10) {
      _printer.setCharacterSet((uint8_t)n);
    }
  }
  _server.send(200, "application/json", "{\"ok\":true}");
}

void PrinterWebServer::handleBarcode() {
  if (_server.hasArg("m") && _server.hasArg("data")) {
    int m = _server.arg("m").toInt();
    String data = _server.arg("data");
    if (m >= 65 && m <= 73 && data.length() > 0) {
      _printer.printBarcode((uint8_t)m, data);
      _server.send(200, "application/json", "{\"ok\":true}");
      return;
    }
  }
  _server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid params\"}");
}

void PrinterWebServer::handlePrintImage() {
  if (!_server.hasArg("width") || !_server.hasArg("height")) {
    _server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing params\"}");
    return;
  }
  int width = _server.arg("width").toInt();
  int height = _server.arg("height").toInt();
  if (width <= 0 || width > 576 || height <= 0 || height > 800) {
    _server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid dimensions\"}");
    return;
  }
  String b64 = _server.arg("plain");
  if (b64.length() == 0) {
    _server.send(400, "application/json", "{\"ok\":false,\"error\":\"empty body\"}");
    return;
  }
  size_t maxLen = (b64.length() * 3) / 4 + 3;
  uint8_t* buf = (uint8_t*)malloc(maxLen);
  if (!buf) {
    _server.send(500, "application/json", "{\"ok\":false,\"error\":\"out of memory\"}");
    return;
  }
  size_t actualLen = base64::decode(b64.c_str(), b64.length(), buf);
  b64 = String();
  uint16_t widthBytes = (width + 7) / 8;
  size_t expectedLen = (size_t)widthBytes * height;
  Serial.printf("-> IMAGE: %dx%d, %u bytes (expected %u)\n", width, height, actualLen, expectedLen);
  if (actualLen != expectedLen) {
    Serial.println("   ERROR: data size mismatch!");
    free(buf);
    _server.send(400, "application/json", "{\"ok\":false,\"error\":\"data size mismatch\"}");
    return;
  }
  _printer.printImage((uint16_t)width, (uint16_t)height, buf, actualLen);
  free(buf);
  _server.send(200, "application/json", "{\"ok\":true}");
}
