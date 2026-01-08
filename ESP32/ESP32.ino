/**
 * @file WROVER_Web_Hub.ino
 * @brief ESP32 Network Gateway & Web Dashboard for Humidity Monitoring.
 * This file handles:
 * 1. WiFi connectivity and Web Server hosting.
 * 2. UART communication with an Arduino Nano (Hardware Serial 2).
 * 3. Parsing logic for the [DHT11] debug string format.
 */

#include <WiFi.h>
#include <WebServer.h>
#include "esp_log.h"

// --- HARDWARE & NETWORK CONSTANTS ---
const uint16_t MONITOR_BAUD = 9600;  // Speed for USB Serial Monitor
const uint16_t NANO_BAUD = 9600;     // Speed for Nano-ESP32 Interconnect
const uint8_t PIN_NANO_RX = 27;      // ESP32 RX Pin (Connect to Nano TX)
const uint8_t PIN_NANO_TX = 14;      // ESP32 TX Pin (Connect to Nano RX)
const uint8_t HTTP_SERVER_PORT = 80; // Defualt port for http

const char *WIFI_SSID = "SSID";
const char *WIFI_PASS = "PASSWORD";

// --- GLOBAL STATE ---
WebServer server(HTTP_SERVER_PORT);

float currentHum = 0.0;
float minHum = 100.0;
float maxHum = 0.0;

// --- HTML INTERFACE ---
const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>Humidity Hub</title>
    <style>
        :root { --bg: #f0f2f5; --card: #ffffff; --text: #333; --cyan: #00d2d3; --teal: #0097a7; --blue: #2e86de; }
        body { font-family: 'Segoe UI', sans-serif; background: var(--bg); color: var(--text); text-align: center; padding: 20px; }
        .container { max-width: 400px; margin: auto; }
        .card { background: var(--card); padding: 25px; border-radius: 20px; box-shadow: 0 10px 25px rgba(0,0,0,0.05); margin-bottom: 20px; }
        .progress-container { background: #eee; border-radius: 15px; height: 20px; width: 100%; margin: 20px 0; overflow: hidden; }
        #progress-bar { height: 100%; width: 0%; transition: width 0.5s ease, background-color 0.5s ease; border-radius: 15px; }
        .val-big { font-size: 3.5rem; font-weight: bold; margin: 10px 0; color: #444; }
        .stats { display: flex; justify-content: space-around; border-top: 1px solid #eee; padding-top: 15px; }
        input[type=text] { width: 100%; box-sizing: border-box; padding: 12px; border: 1px solid #ddd; border-radius: 12px; margin-bottom: 12px; font-size: 1rem; }
        button { width: 100%; padding: 14px; border: none; border-radius: 12px; font-weight: bold; cursor: pointer; transition: 0.2s; font-size: 1rem; }
        .btn-send { background: #007bff; color: white; margin-bottom: 10px; }
        .btn-reset { background: #6c757d; color: white; }
        #toast { visibility: hidden; background: #333; color: #fff; padding: 16px; position: fixed; left: 50%; bottom: 30px; transform: translateX(-50%); border-radius: 50px; }
        #toast.show { visibility: visible; animation: fade 0.5s; }
    </style>
</head>
<body>
    <div class="container">
        <h1 style="color: #555;">Humidity Hub</h1>
        <div class="card">
            <div id="hum-val" class="val-big">--%</div>
            <div class="progress-container"><div id="progress-bar"></div></div>
            <div class="stats">
                <div>Min: <b id="min-val">--</b>%</div>
                <div>Max: <b id="max-val">--</b>%</div>
            </div>
        </div>
        <div class="card">
            <input type="text" id="msgInput" placeholder="LCD Message...">
            <button class="btn-send" onclick="sendMsg()">Send Message</button>
            <button class="btn-reset" onclick="resetValues()">Reset History</button>
        </div>
    </div>
    <div id="toast">Sent!</div>
    <script>
        setInterval(fetchData, 3000);
        function fetchData() {
            fetch('/api/data').then(res => res.json()).then(data => {
                document.getElementById('hum-val').innerText = data.curr + '%';
                document.getElementById('min-val').innerText = data.min;
                document.getElementById('max-val').innerText = data.max;
                let bar = document.getElementById('progress-bar');
                let val = data.curr;
                bar.style.width = val + '%';
                if(val < 35) bar.style.backgroundColor = 'var(--cyan)';
                else if(val <= 65) bar.style.backgroundColor = 'var(--teal)';
                else bar.style.backgroundColor = 'var(--blue)';
            });
        }
        function showToast(m) {
            var x = document.getElementById("toast"); x.innerText = m; x.className = "show";
            setTimeout(function(){ x.className = ""; }, 3000);
        }
        function sendMsg() {
            let v = document.getElementById('msgInput').value;
            if(!v) return;
            fetch('/api/msg?val=' + encodeURIComponent(v)).then(() => {
                showToast("Sent to LCD!"); document.getElementById('msgInput').value = "";
            });
        }
        function resetValues() { fetch('/api/reset').then(() => showToast("History Reset!")); }
    </script>
</body>
</html>
)=====";

// --- HANDLERS ---

/** Serves the main HTML page */
void handleRoot() { server.send(200, "text/html", INDEX_HTML); }

/** Provides current humidity stats in JSON format for the web dashboard */
void handleGetData()
{
    String json = "{\"curr\":" + String(currentHum, 1) + ",\"min\":" + String(minHum, 1) + ",\"max\":" + String(maxHum, 1) + "}";
    server.send(200, "application/json", json);
}

/** Receives a message string from the web and forwards it to the Nano via UART */
void handleMsg()
{
    String message = server.arg("val");
    
    // Log to Serial Monitor (USB)
    Serial.print("[WEB] New Message for LCD: ");
    Serial.println(message);
    
    // Send to Nano (UART)
    Serial2.println("M:" + message);
    
    server.send(200, "text/plain", "OK");
}

/** Sends a reset command to the Nano */
void handleReset()
{
    // Log to Serial Monitor (USB)
    Serial.println("[WEB] Reset Command Received -> Sending to Nano...");
    
    // Send to Nano (UART)
    Serial2.println("R:1");
    
    server.send(200, "text/plain", "OK");
}

void setup() {
    Serial.begin(MONITOR_BAUD);
    Serial2.begin(NANO_BAUD, SERIAL_8N1, PIN_NANO_RX, PIN_NANO_TX);
    delay(2000); 

    // --- 1. SILENCE SYSTEM LOGS ---
    esp_log_level_set("wifi", ESP_LOG_NONE); 
    
    Serial.println("\n[SYSTEM] Initializing WiFi...");

    // --- 2. THE "CLEAN" CONNECTION SEQUENCE ---
    WiFi.persistent(false);
    WiFi.disconnect(true);  
    WiFi.mode(WIFI_STA);
    delay(500);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    int attemptCounter = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        attemptCounter++;

        if (attemptCounter >= 20) {
            Serial.println("\n[WiFi] Connection taking too long, retrying...");
            WiFi.begin(WIFI_SSID, WIFI_PASS);
            attemptCounter = 0;
        }
    }

    Serial.println("\n[WiFi] Connected successfully!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());

    // Register API endpoints & Start Server
    server.on("/", handleRoot);
    server.on("/api/data", handleGetData);
    server.on("/api/msg", handleMsg);
    server.on("/api/reset", handleReset);
    server.begin();
}

void loop()
{
    server.handleClient(); // Handle incoming web requests

    // Check for incoming data from the Arduino Nano
    if (Serial2.available())
    {
        String incoming = Serial2.readStringUntil('\n');
        incoming.trim();

        // PARSING LOGIC:
        // Expected format: "[DHT11] Current = 45.0, Min = 30.0, Max = 60.0,"
        if (incoming.startsWith("[DHT11]"))
        {
            int curIdx = incoming.indexOf("Current = ");
            int minIdx = incoming.indexOf("Min = ");
            int maxIdx = incoming.indexOf("Max = ");

            if (curIdx != -1 && minIdx != -1 && maxIdx != -1)
            {
                currentHum = incoming.substring(curIdx + 10, incoming.indexOf(",", curIdx)).toFloat();
                minHum = incoming.substring(minIdx + 6, incoming.indexOf(",", minIdx)).toFloat();
                maxHum = incoming.substring(maxIdx + 6, incoming.indexOf(",", maxIdx)).toFloat();

                Serial.printf("[DHT11->UART] Current = %.1f, Min:%.1f, Max = %.1f,\n", currentHum, minHum, maxHum);
            }
        }
    }
}
