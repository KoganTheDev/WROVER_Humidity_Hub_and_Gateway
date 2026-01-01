/**
 * @file ESP32.ino
 * @brief ESP32 I2C Master with WiFi and Web Server
 * 
 * This application implements an ESP32 that:
 * - Connects to WiFi in Station mode
 * - Communicates with Arduino Nano via I2C (Master role)
 * - Hosts an HTTP web server with user interface
 * - Manages humidity sensor data with min/max tracking
 * - Allows sending messages to Nano via web interface
 * 
 * @author Embedded Systems Course
 * @date 2026
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <EEPROM.h>

// ============================================================================
// CONFIGURATION PARAMETERS
// ============================================================================

/** @defgroup WiFi_Config WiFi Configuration
 *  @{
 */
#define SSID "YOUR_SSID"              ///< WiFi network SSID
#define PASSWORD "YOUR_PASSWORD"      ///< WiFi network password
/** @} */

/** @defgroup I2C_Config I2C Configuration
 *  @{
 */
#define I2C_SDA_PIN 21                ///< I2C SDA pin on ESP32
#define I2C_SCL_PIN 22                ///< I2C SCL pin on ESP32
#define NANO_I2C_ADDRESS 0x08         ///< I2C address of Arduino Nano
#define I2C_FREQUENCY 100000          ///< I2C clock frequency in Hz
/** @} */

/** @defgroup Server_Config Web Server Configuration
 *  @{
 */
#define SERVER_PORT 80                ///< HTTP server port
/** @} */

/** @defgroup EEPROM_Config EEPROM Configuration
 *  @{
 */
#define EEPROM_SIZE 512               ///< EEPROM size in bytes
#define EEPROM_ADDR_MIN_HUMIDITY 0    ///< EEPROM address for minimum humidity
#define EEPROM_ADDR_MAX_HUMIDITY 4    ///< EEPROM address for maximum humidity
/** @} */

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

WebServer server(SERVER_PORT);        ///< Web server instance

/** @struct SensorData
 *  @brief Structure to hold sensor readings
 */
struct SensorData {
  float humidity;                     ///< Current humidity reading
  float minHumidity;                  ///< Minimum humidity recorded
  float maxHumidity;                  ///< Maximum humidity recorded
} sensorData = {0.0, 100.0, 0.0};

bool wifiConnected = false;           ///< WiFi connection status flag

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

void setupWiFi(void);
void setupI2C(void);
void setupWebServer(void);
void requestDataFromNano(void);
void updateSensorMinMax(float humidity);
void saveSensorDataToEEPROM(void);
void loadSensorDataFromEEPROM(void);
void handleRoot(void);
void handleReset(void);
void handleSendMessage(void);
void handleGetStatus(void);

// ============================================================================
// SETUP AND LOOP
// ============================================================================

/**
 * @brief Arduino setup function - called once at startup
 * 
 * Initializes:
 * - Serial communication (for debugging)
 * - EEPROM storage
 * - WiFi connection
 * - I2C communication
 * - Web server with routes
 */
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== ESP32 I2C Master with Web Server ===");
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  loadSensorDataFromEEPROM();
  
  Serial.println("[SETUP] EEPROM initialized");
  Serial.printf("[SETUP] Loaded min humidity: %.2f, max humidity: %.2f\n", 
                sensorData.minHumidity, sensorData.maxHumidity);
  
  // Setup WiFi
  setupWiFi();
  
  // Setup I2C
  setupI2C();
  
  // Setup Web Server
  setupWebServer();
  
  server.begin();
  Serial.println("[SETUP] Web server started on port " + String(SERVER_PORT));
}

/**
 * @brief Arduino loop function - called repeatedly
 * 
 * Handles:
 * - Web server client requests
 * - Periodic I2C data requests from Nano
 * - WiFi reconnection if needed
 */
void loop() {
  server.handleClient();
  
  // Request data from Nano every 2 seconds
  static unsigned long lastRequest = 0;
  if (millis() - lastRequest > 2000) {
    requestDataFromNano();
    lastRequest = millis();
  }
  
  // Check WiFi connection
  if (!wifiConnected && WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Reconnecting to WiFi...");
    setupWiFi();
  }
  
  delay(10);
}

// ============================================================================
// WiFi SETUP
// ============================================================================

/**
 * @brief Initializes WiFi connection in Station mode
 * 
 * Attempts to connect to the specified SSID with the given password.
 * Prints connection status to Serial.
 */
void setupWiFi(void) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  
  Serial.print("[WIFI] Connecting to ");
  Serial.println(SSID);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n[WIFI] Connected!");
    Serial.print("[WIFI] IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    wifiConnected = false;
    Serial.println("\n[WIFI] Connection failed");
  }
}

// ============================================================================
// I2C SETUP AND COMMUNICATION
// ============================================================================

/**
 * @brief Initializes I2C communication as Master
 * 
 * Configures pins and frequency for I2C Master mode.
 */
void setupI2C(void) {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQUENCY);
  Serial.println("[I2C] Initialized as Master");
  Serial.printf("[I2C] SDA: %d, SCL: %d, Frequency: %d Hz\n", 
                I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
}

/**
 * @brief Requests humidity data from Arduino Nano via I2C
 * 
 * Sends an I2C request to the Nano, receives humidity data,
 * updates min/max values, and saves to EEPROM if changed.
 */
void requestDataFromNano(void) {
  Wire.requestFrom(NANO_I2C_ADDRESS, 4); // Request 4 bytes (float)
  
  if (Wire.available() == 4) {
    byte buffer[4];
    for (int i = 0; i < 4; i++) {
      buffer[i] = Wire.read();
    }
    
    // Convert bytes to float
    float* humidityPtr = (float*)buffer;
    sensorData.humidity = *humidityPtr;
    
    updateSensorMinMax(sensorData.humidity);
    
    Serial.printf("[I2C] Received humidity: %.2f%%, Min: %.2f%%, Max: %.2f%%\n",
                  sensorData.humidity, sensorData.minHumidity, sensorData.maxHumidity);
  } else {
    Serial.println("[I2C] Failed to receive data from Nano");
  }
}

/**
 * @brief Updates minimum and maximum humidity values
 * 
 * @param humidity Current humidity reading from sensor
 * 
 * Updates the min/max values if the current reading exceeds
 * previous extremes. Saves updated values to EEPROM.
 */
void updateSensorMinMax(float humidity) {
  bool changed = false;
  
  if (humidity < sensorData.minHumidity) {
    sensorData.minHumidity = humidity;
    changed = true;
  }
  
  if (humidity > sensorData.maxHumidity) {
    sensorData.maxHumidity = humidity;
    changed = true;
  }
  
  if (changed) {
    saveSensorDataToEEPROM();
  }
}

// ============================================================================
// EEPROM OPERATIONS
// ============================================================================

/**
 * @brief Saves sensor min/max values to EEPROM
 * 
 * Stores minimum and maximum humidity values as floats (4 bytes each)
 * at predefined EEPROM addresses for persistent storage.
 */
void saveSensorDataToEEPROM(void) {
  EEPROM.writeFloat(EEPROM_ADDR_MIN_HUMIDITY, sensorData.minHumidity);
  EEPROM.writeFloat(EEPROM_ADDR_MAX_HUMIDITY, sensorData.maxHumidity);
  EEPROM.commit();
  Serial.println("[EEPROM] Sensor data saved");
}

/**
 * @brief Loads sensor min/max values from EEPROM
 * 
 * Reads minimum and maximum humidity values from EEPROM.
 * Initializes with defaults if EEPROM is empty or corrupted.
 */
void loadSensorDataFromEEPROM(void) {
  sensorData.minHumidity = EEPROM.readFloat(EEPROM_ADDR_MIN_HUMIDITY);
  sensorData.maxHumidity = EEPROM.readFloat(EEPROM_ADDR_MAX_HUMIDITY);
  
  // Validate values
  if (isnan(sensorData.minHumidity) || sensorData.minHumidity < 0) {
    sensorData.minHumidity = 100.0;
  }
  if (isnan(sensorData.maxHumidity) || sensorData.maxHumidity < 0) {
    sensorData.maxHumidity = 0.0;
  }
}

// ============================================================================
// WEB SERVER SETUP
// ============================================================================

/**
 * @brief Initializes web server routes and handlers
 * 
 * Registers HTTP endpoints:
 * - GET / : Serves HTML web interface
 * - GET /status : Returns JSON with sensor data
 * - POST /reset : Resets min/max values
 * - POST /message : Sends message to Nano
 */
void setupWebServer(void) {
  server.on("/", handleRoot);
  server.on("/status", handleGetStatus);
  server.on("/reset", handleReset);
  server.on("/message", handleSendMessage);
  
  Serial.println("[SERVER] Routes registered");
}

// ============================================================================
// HTTP REQUEST HANDLERS
// ============================================================================

/**
 * @brief Handles HTTP GET / request - serves HTML web interface
 * 
 * Returns a complete HTML page with:
 * - Real-time humidity display
 * - Min/max humidity values
 * - Text input for LCD messages
 * - Reset and Send buttons
 * - Auto-refresh functionality
 */
void handleRoot(void) {
  String html = R"=====(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 Humidity Monitor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body { 
      font-family: Arial, sans-serif; 
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
    }
    .container { 
      max-width: 600px; 
      margin: 0 auto; 
      background: white;
      border-radius: 15px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.3);
      padding: 30px;
    }
    h1 { 
      color: #333; 
      margin-bottom: 30px;
      text-align: center;
      border-bottom: 3px solid #667eea;
      padding-bottom: 15px;
    }
    .sensor-data { 
      display: grid; 
      grid-template-columns: 1fr 1fr; 
      gap: 20px;
      margin-bottom: 30px;
    }
    .data-card { 
      background: #f8f9fa;
      padding: 20px;
      border-radius: 10px;
      border-left: 4px solid #667eea;
    }
    .data-label { 
      color: #666; 
      font-size: 14px;
      margin-bottom: 8px;
    }
    .data-value { 
      font-size: 32px; 
      font-weight: bold;
      color: #667eea;
    }
    .unit {
      font-size: 16px;
      color: #999;
    }
    .controls {
      display: grid;
      gap: 15px;
      margin-bottom: 30px;
    }
    input[type="text"] {
      width: 100%;
      padding: 12px;
      border: 2px solid #ddd;
      border-radius: 8px;
      font-size: 16px;
      transition: border-color 0.3s;
    }
    input[type="text"]:focus {
      outline: none;
      border-color: #667eea;
    }
    .button-group {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
    }
    button {
      padding: 12px 24px;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: bold;
      cursor: pointer;
      transition: all 0.3s;
    }
    .btn-send {
      background: #667eea;
      color: white;
    }
    .btn-send:hover {
      background: #5568d3;
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);
    }
    .btn-reset {
      background: #ff6b6b;
      color: white;
    }
    .btn-reset:hover {
      background: #ee5a52;
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(255, 107, 107, 0.4);
    }
    .status {
      padding: 15px;
      border-radius: 8px;
      text-align: center;
      font-weight: bold;
      margin-top: 20px;
    }
    .status.connected {
      background: #d4edda;
      color: #155724;
    }
    .status.disconnected {
      background: #f8d7da;
      color: #721c24;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🌡️ Humidity Monitor</h1>
    
    <div class="sensor-data">
      <div class="data-card">
        <div class="data-label">Current Humidity</div>
        <div class="data-value" id="humidity">-<span class="unit">%</span></div>
      </div>
      <div class="data-card">
        <div class="data-label">Max Humidity</div>
        <div class="data-value" id="maxHumidity">-<span class="unit">%</span></div>
      </div>
      <div class="data-card">
        <div class="data-label">Min Humidity</div>
        <div class="data-value" id="minHumidity">-<span class="unit">%</span></div>
      </div>
      <div class="data-card">
        <div class="data-label">Last Update</div>
        <div id="lastUpdate" style="font-size: 14px; color: #666; margin-top: 5px;">Connecting...</div>
      </div>
    </div>

    <div class="controls">
      <label for="message" style="font-weight: bold; color: #333;">Send Message to LCD:</label>
      <input type="text" id="message" placeholder="Enter message (max 20 chars)" maxlength="20">
      <div class="button-group">
        <button class="btn-send" onclick="sendMessage()">Send Message</button>
        <button class="btn-reset" onclick="resetMinMax()">Reset Min/Max</button>
      </div>
    </div>

    <div class="status" id="wifiStatus">Checking connection...</div>
  </div>

  <script>
    /**
     * Fetches sensor data from ESP32 via /status endpoint
     * Updates UI with current, min, and max humidity values
     */
    function updateStatus() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          document.getElementById('humidity').textContent = data.humidity.toFixed(1) + '%';
          document.getElementById('maxHumidity').textContent = data.maxHumidity.toFixed(1) + '%';
          document.getElementById('minHumidity').textContent = data.minHumidity.toFixed(1) + '%';
          
          const now = new Date();
          document.getElementById('lastUpdate').textContent = now.toLocaleTimeString();
          
          const wifiStatus = document.getElementById('wifiStatus');
          if (data.wifiConnected) {
            wifiStatus.textContent = '✓ WiFi Connected';
            wifiStatus.className = 'status connected';
          } else {
            wifiStatus.textContent = '✗ WiFi Disconnected';
            wifiStatus.className = 'status disconnected';
          }
        })
        .catch(error => console.log('Error fetching status:', error));
    }

    /**
     * Sends a message to the Arduino Nano via I2C
     * Message will be displayed on the LCD
     */
    function sendMessage() {
      const message = document.getElementById('message').value;
      if (message.trim() === '') {
        alert('Please enter a message');
        return;
      }

      fetch('/message', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'msg=' + encodeURIComponent(message)
      })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          alert('Message sent to LCD!');
          document.getElementById('message').value = '';
        } else {
          alert('Failed to send message');
        }
      })
      .catch(error => console.log('Error sending message:', error));
    }

    /**
     * Resets the minimum and maximum humidity values
     */
    function resetMinMax() {
      if (confirm('Are you sure you want to reset min/max values?')) {
        fetch('/reset', { method: 'POST' })
          .then(response => response.json())
          .then(data => {
            if (data.success) {
              alert('Min/Max values reset!');
              updateStatus();
            }
          })
          .catch(error => console.log('Error resetting values:', error));
      }
    }

    // Update status every 2 seconds
    updateStatus();
    setInterval(updateStatus, 2000);
  </script>
</body>
</html>
  )=====" ;
  
  server.sendHeader("Content-Type", "text/html; charset=UTF-8");
  server.send(200, "text/html", html);
  Serial.println("[SERVER] Root page served");
}

/**
 * @brief Handles HTTP GET /status request - returns sensor data as JSON
 * 
 * Response JSON:
 * @code
 * {
 *   "humidity": <float>,
 *   "minHumidity": <float>,
 *   "maxHumidity": <float>,
 *   "wifiConnected": <bool>
 * }
 * @endcode
 */
void handleGetStatus(void) {
  String json = "{";
  json += "\"humidity\":" + String(sensorData.humidity, 2) + ",";
  json += "\"minHumidity\":" + String(sensorData.minHumidity, 2) + ",";
  json += "\"maxHumidity\":" + String(sensorData.maxHumidity, 2) + ",";
  json += "\"wifiConnected\":" + String(wifiConnected ? "true" : "false");
  json += "}";
  
  server.sendHeader("Content-Type", "application/json");
  server.send(200, "application/json", json);
}

/**
 * @brief Handles HTTP POST /reset request - resets min/max humidity values
 * 
 * Resets minHumidity to 100% and maxHumidity to 0%,
 * saves changes to EEPROM, and sends confirmation to client.
 */
void handleReset(void) {
  sensorData.minHumidity = 100.0;
  sensorData.maxHumidity = 0.0;
  saveSensorDataToEEPROM();
  
  Serial.println("[SERVER] Min/Max values reset");
  
  String json = "{\"success\":true}";
  server.sendHeader("Content-Type", "application/json");
  server.send(200, "application/json", json);
}

/**
 * @brief Handles HTTP POST /message request - sends message to Nano via I2C
 * 
 * Receives a message from the web interface and transmits it to the
 * Arduino Nano via I2C for display on the LCD.
 * 
 * Expected POST parameter: msg (max 20 characters)
 */
void handleSendMessage(void) {
  if (!server.hasArg("msg")) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }
  
  String message = server.arg("msg");
  message = message.substring(0, 20); // Limit to 20 chars
  
  // Send message to Nano via I2C
  Wire.beginTransmission(NANO_I2C_ADDRESS);
  Wire.write('M'); // Command: Message
  Wire.write(message.length());
  for (int i = 0; i < message.length(); i++) {
    Wire.write(message[i]);
  }
  Wire.endTransmission();
  
  Serial.printf("[I2C] Message sent to Nano: %s\n", message.c_str());
  
  String json = "{\"success\":true}";
  server.sendHeader("Content-Type", "application/json");
  server.send(200, "application/json", json);
}
