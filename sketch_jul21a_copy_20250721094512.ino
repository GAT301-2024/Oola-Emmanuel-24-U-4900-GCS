#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

// Pin Definitions (Match your board)
#define LDR_PIN         36  // VP pin (GPIO36)
#define PIR_PIN         15  // D15 pin (GPIO15)
#define AUTO_LED_PIN    16  // D16 (PWM capable)
#define WIFI_LED2_PIN   17  // D17
#define WIFI_LED3_PIN   18  // D18

// WiFi Settings
const char* ssid = "MyHome WiFi";
const char* password = "password@Emmanuel";

// System Parameters
const int ldrThreshold = 1500;       // Calibrate this value
const unsigned long motionTimeout = 10000;  // 10 seconds
const int pwmFreq = 5000;            // PWM frequency
const int pwmChannel = 0;            // LEDC channel 0
const int pwmResolution = 8;         // 8-bit resolution (0-255)

// Global Variables
WebServer server(80);
bool motionActive = false;
unsigned long motionStart = 0;
int autoLedState = 0; // 0=off, 1=dim, 2=bright, 3=blink
int led2State = LOW;
int led3State = LOW;

void setup() {
  Serial.begin(115200);
  
  // Configure PWM for auto LED
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(AUTO_LED_PIN, pwmChannel);
  
  // Configure pins
  pinMode(LDR_PIN, ANALOG);
  pinMode(PIR_PIN, INPUT);
  pinMode(WIFI_LED2_PIN, OUTPUT);
  pinMode(WIFI_LED3_PIN, OUTPUT);
  
  // Initialize LEDs
  ledcWrite(pwmChannel, 0);
  digitalWrite(WIFI_LED2_PIN, LOW);
  digitalWrite(WIFI_LED3_PIN, LOW);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  // Configure Web Server
  server.on("/", handleDashboard);
  server.on("/control", HTTP_GET, handleControl);
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();
}

void loop() {
  server.handleClient();
  processAutoLight();
  updateDashboardData();
}

void processAutoLight() {
  int ldrValue = analogRead(LDR_PIN);
  bool motionDetected = digitalRead(PIR_PIN) == HIGH;

  // Debug output (optional)
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 2000) {
    Serial.print("LDR: ");
    Serial.print(ldrValue);
    Serial.print(" | Motion: ");
    Serial.println(motionDetected ? "DETECTED" : "NONE");
    lastPrint = millis();
  }

  // Darkness detection
  if (ldrValue < ldrThreshold) {
    if (motionDetected) {
      if (!motionActive) {
        motionActive = true;
        motionStart = millis();
        autoLedState = 2; // Bright mode
      } 
      else if (millis() - motionStart > motionTimeout) {
        autoLedState = 3; // Blink mode
      }
    } 
    else {
      motionActive = false;
      autoLedState = 1; // Dim mode
    }
  } 
  else {
    motionActive = false;
    autoLedState = 0; // Off
  }

  // Control auto LED
  switch(autoLedState) {
    case 0: // Off
      ledcWrite(pwmChannel, 0);
      break;
    case 1: // Dim (30%)
      ledcWrite(pwmChannel, 77); // 255 * 0.3 ≈ 77
      break;
    case 2: // Bright (100%)
      ledcWrite(pwmChannel, 255);
      break;
    case 3: // Blink
      ledcWrite(pwmChannel, (millis() % 1000 < 300) ? 255 : 0);
      break;
  }
}

// Enhanced Dashboard with Real-Time Updates
const char* dashboardHTML = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Smart Light System</title>
<style>
  :root {
    --primary: #3498db;
    --success: #2ecc71;
    --warning: #f39c12;
    --danger: #e74c3c;
    --dark: #2c3e50;
    --light: #ecf0f1;
  }
  
  * { box-sizing: border-box; font-family: 'Segoe UI', Tahoma, sans-serif; }
  body { margin: 0; padding: 20px; background: #f5f7fa; color: #333; }
  
  .container { max-width: 800px; margin: 0 auto; }
  .header { text-align: center; margin-bottom: 30px; }
  .header h1 { color: var(--dark); margin-bottom: 5px; }
  .header p { color: #7f8c8d; }
  
  .card-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
  .card { background: white; border-radius: 15px; box-shadow: 0 5px 15px rgba(0,0,0,0.1); overflow: hidden; }
  
  .card-header {
    background: var(--dark);
    color: white;
    padding: 15px 20px;
    font-weight: bold;
    display: flex;
    align-items: center;
    justify-content: space-between;
  }
  
  .card-body { padding: 20px; }
  
  .status-badge {
    display: inline-block;
    padding: 5px 12px;
    border-radius: 20px;
    font-size: 14px;
    font-weight: 500;
  }
  
  .status-on { background: var(--success); color: white; }
  .status-off { background: #e0e0e0; color: #777; }
  .status-auto { background: var(--primary); color: white; }
  .status-blink { background: var(--warning); color: white; }
  
  .control-row { 
    display: flex; 
    justify-content: space-between; 
    align-items: center; 
    margin: 15px 0;
  }
  
  .toggle-switch {
    position: relative;
    display: inline-block;
    width: 60px;
    height: 34px;
  }
  
  .toggle-switch input { 
    opacity: 0;
    width: 0;
    height: 0;
  }
  
  .slider {
    position: absolute;
    cursor: pointer;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background-color: #ccc;
    transition: .4s;
    border-radius: 34px;
  }
  
  .slider:before {
    position: absolute;
    content: "";
    height: 26px;
    width: 26px;
    left: 4px;
    bottom: 4px;
    background-color: white;
    transition: .4s;
    border-radius: 50%;
  }
  
  input:checked + .slider { background-color: var(--success); }
  input:checked + .slider:before { transform: translateX(26px); }
  
  .sensor-data {
    background: var(--light);
    border-radius: 10px;
    padding: 15px;
    margin-top: 15px;
  }
  
  .sensor-row {
    display: flex;
    justify-content: space-between;
    padding: 8px 0;
    border-bottom: 1px solid #ddd;
  }
  
  .sensor-row:last-child { border-bottom: none; }
  
  .btn {
    display: inline-block;
    padding: 10px 20px;
    background: var(--primary);
    color: white;
    text-decoration: none;
    border-radius: 8px;
    font-weight: 500;
    border: none;
    cursor: pointer;
    font-size: 16px;
    transition: background 0.3s;
  }
  
  .btn:hover { background: #2980b9; }
  
  .footer {
    text-align: center;
    margin-top: 30px;
    color: #7f8c8d;
    font-size: 14px;
  }
</style>
</head><body>
<div class="container">
  <div class="header">
    <h1>Smart Light Control System</h1>
    <p>ESP32 IP: %IP%</p>
  </div>
  
  <div class="card-grid">
    <!-- Auto Light Card -->
    <div class="card">
      <div class="card-header">
        <span>Automatic Light</span>
        <span class="status-badge" id="auto-status">%AUTO_STATUS%</span>
      </div>
      <div class="card-body">
        <div class="sensor-data">
          <div class="sensor-row">
            <span>Light Level:</span>
            <span id="ldr-value">%LDR%</span>
          </div>
          <div class="sensor-row">
            <span>Motion Detection:</span>
            <span id="pir-value">%PIR%</span>
          </div>
          <div class="sensor-row">
            <span>State Duration:</span>
            <span id="motion-time">%MOTION_TIME%</span>
          </div>
        </div>
      </div>
    </div>
    
    <!-- Manual Control Card -->
    <div class="card">
      <div class="card-header">
        <span>Manual Control</span>
      </div>
      <div class="card-body">
        <!-- LED 2 Control -->
        <div class="control-row">
          <span>WiFi Light 1 (D17)</span>
          <label class="toggle-switch">
            <input type="checkbox" id="led2-toggle" %LED2_CHECKED% onchange="toggleLight('led2')">
            <span class="slider"></span>
          </label>
        </div>
        
        <!-- LED 3 Control -->
        <div class="control-row">
          <span>WiFi Light 2 (D18)</span>
          <label class="toggle-switch">
            <input type="checkbox" id="led3-toggle" %LED3_CHECKED% onchange="toggleLight('led3')">
            <span class="slider"></span>
          </label>
        </div>
        
        <!-- System Actions -->
        <div style="margin-top: 20px; display: flex; gap: 10px;">
          <button class="btn" onclick="restartSystem()">Restart System</button>
          <button class="btn" onclick="calibrateLDR()">Calibrate LDR</button>
        </div>
      </div>
    </div>
  </div>
  
  <div class="footer">
    Smart Light System | ESP32-WROOM-32 | %UPTIME%
  </div>
</div>

<script>
// Update system status every 2 seconds
setInterval(updateStatus, 2000);

// Toggle light function
function toggleLight(led) {
  const toggle = document.getElementById(led + '-toggle');
  const state = toggle.checked ? 'on' : 'off';
  
  fetch('/control?' + led + '=' + state)
    .then(response => response.text())
    .then(data => {
      console.log(led + ' toggled:', state);
    });
}

// Update all status indicators
function updateStatus() {
  fetch('/status')
    .then(response => response.json())
    .then(data => {
      // Update auto light status
      document.getElementById('auto-status').textContent = data.auto_status;
      document.getElementById('auto-status').className = 
        'status-badge status-' + data.auto_status.toLowerCase();
      
      // Update sensor values
      document.getElementById('ldr-value').textContent = data.ldr_value;
      document.getElementById('pir-value').textContent = data.pir_status;
      document.getElementById('motion-time').textContent = data.motion_time + 's';
      
      // Update manual controls
      document.getElementById('led2-toggle').checked = data.led2_state === 'on';
      document.getElementById('led3-toggle').checked = data.led3_state === 'on';
      
      // Update footer
      document.querySelector('.footer').innerHTML = 
        `Smart Light System | ESP32-WROOM-32 | Uptime: ${data.uptime}`;
    });
}

// System actions
function restartSystem() {
  if(confirm("Restart ESP32? System will be offline for 10-15 seconds.")) {
    fetch('/control?restart=1');
  }
}

function calibrateLDR() {
  fetch('/control?calibrate=1');
  alert("LDR calibration started. Cover/uncover sensor for 10 seconds.");
}
</script>
</body></html>
)rawliteral";

// Global variables for status updates
String autoStatus = "OFF";
String ldrValue = "0";
String pirStatus = "NO MOTION";
String motionTime = "0";
String uptime = "0:00:00";

void handleDashboard() {
  String html = dashboardHTML;
  
  // Replace placeholders
  html.replace("%IP%", WiFi.localIP().toString());
  html.replace("%AUTO_STATUS%", autoStatus);
  html.replace("%LDR%", ldrValue);
  html.replace("%PIR%", pirStatus);
  html.replace("%MOTION_TIME%", motionTime);
  html.replace("%UPTIME%", uptime);
  html.replace("%LED2_CHECKED%", led2State ? "checked" : "");
  html.replace("%LED3_CHECKED%", led3State ? "checked" : "");
  
  server.send(200, "text/html", html);
}

void handleControl() {
  if (server.hasArg("led2")) {
    led2State = (server.arg("led2") == "on") ? HIGH : LOW;
    digitalWrite(WIFI_LED2_PIN, led2State);
  }
  
  if (server.hasArg("led3")) {
    led3State = (server.arg("led3") == "on") ? HIGH : LOW;
    digitalWrite(WIFI_LED3_PIN, led3State);
  }
  
  if (server.hasArg("restart")) {
    server.send(200, "text/plain", "Restarting...");
    delay(1000);
    ESP.restart();
  }
  
  if (server.hasArg("calibrate")) {
    // Placeholder for calibration routine
    server.send(200, "text/plain", "Calibration started");
  }
  
  handleDashboard();
}

void handleStatus() {
  String json = "{";
  json += "\"auto_status\":\"" + autoStatus + "\",";
  json += "\"ldr_value\":\"" + ldrValue + "\",";
  json += "\"pir_status\":\"" + pirStatus + "\",";
  json += "\"motion_time\":\"" + motionTime + "\",";
  json += "\"led2_state\":\"" + (led2State ? "on" : "off") + "\",";
  json += "\"led3_state\":\"" + (led3State ? "on" : "off") + "\",";
  json += "\"uptime\":\"" + uptime + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

void updateDashboardData() {
  // Update status variables
  switch(autoLedState) {
    case 0: autoStatus = "OFF"; break;
    case 1: autoStatus = "DIM"; break;
    case 2: autoStatus = "BRIGHT"; break;
    case 3: autoStatus = "BLINK"; break;
  }
  
  ldrValue = String(analogRead(LDR_PIN));
  pirStatus = digitalRead(PIR_PIN) ? "MOTION" : "NO MOTION";
  
  if (motionActive) {
    motionTime = String((millis() - motionStart) / 1000);
  } else {
    motionTime = "0";
  }
  
  // Format uptime HH:MM:SS
  long seconds = millis() / 1000;
  uptime = String(seconds / 3600) + ":" + 
           String((seconds % 3600) / 60).padStart(2, '0') + ":" + 
           String(seconds % 60).padStart(2, '0');
}
