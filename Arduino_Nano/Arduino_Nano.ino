/**
 * @file Nano_Humidity_Controller.ino
 * @brief Humidity Controller Node. Handles sensor reading, local display, 
 * and responds to UART commands from the ESP32 Web Gateway.
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// --- CONSTANTS ---
const uint8_t PIN_DHT          = 4;      
const uint16_t BAUD_RATE       = 9600;   
const uint32_t SENSOR_INTERVAL = 2000;   

// LCD Configuration
const uint8_t LCD_I2C_ADDR     = 0x27;   
const uint8_t LCD_COLUMNS      = 16;
const uint8_t LCD_ROWS         = 2;

// --- GLOBAL OBJECTS ---
DHT dht(PIN_DHT, DHT11);
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLUMNS, LCD_ROWS);

float currentHum = 0.0;
float minHum     = 100.0;
float maxHum     = 0.0;
unsigned long lastSensorReadTime = 0;

/**
 * @brief Initialization: Sets up peripherals and displays boot splash.
 */
void setup() {
  Serial.begin(BAUD_RATE);
  dht.begin();
  
  lcd.init();
  lcd.backlight();
  
  // Initial Boot Screen
  lcd.setCursor(0, 0);
  lcd.print("SYSTEM STARTING");
  lcd.setCursor(0, 1);
  lcd.print("WAITING FOR DATA");
  delay(1500);
  lcd.clear();
}

void loop() {
  unsigned long currentTime = millis();

  // --- TASK 1: SENSOR ACQUISITION & OUTBOUND TELEMETRY ---
  // Uses non-blocking millis() to ensure the Serial port stays responsive
  if (currentTime - lastSensorReadTime >= SENSOR_INTERVAL)
  {
    float h = dht.readHumidity();

    // Only process if the reading is valid
    if (!isnan(h))
    {
      currentHum = h;
      
      // Track lifetime highs and lows
      if (currentHum < minHum) minHum = currentHum;
      if (currentHum > maxHum) maxHum = currentHum;

      // Update Local LCD Display (Row 0: Current, Row 1: Stats)
      lcd.setCursor(0, 0);
      lcd.print("Humidity: "); 
      lcd.print(currentHum, 1);
      lcd.print("% "); 
      
      lcd.setCursor(0, 1);
      lcd.print("L:"); lcd.print(minHum, 0); 
      lcd.print("%  H:"); lcd.print(maxHum, 0); lcd.print("%  ");

      // TRANSMIT: Sent to ESP32 for parsing. 
      // Prefix [DHT11] is the trigger for the ESP32's parsing logic.
      Serial.print("[DHT11] ");
      Serial.print("Current = ");
      Serial.print(currentHum, 1);
      Serial.print(", Min = ");
      Serial.print(minHum, 1);
      Serial.print(", Max = ");
      Serial.print(maxHum, 1);
      Serial.println(",");
    }
    lastSensorReadTime = currentTime;
  }

  // --- TASK 2: COMMAND INBOUND PROCESSING ---
  // Listens for commands coming from the ESP32 Web Interface
  if (Serial.available() > 0) 
  {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    // PROTOCOL: R:1 -> Reset min/max history
    if (cmd == "R:1")
    {
      Serial.println("[LOG] Reset command received. Clearing history...");
      
      minHum = currentHum;
      maxHum = currentHum;
      
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(">> RESETTING <<");
      lcd.setCursor(0,1);
      lcd.print(" MIN/MAX CLEARED");
      delay(2000); // Temporary block to show status on LCD
      lcd.clear();
    } 
    // PROTOCOL: M:<text> -> Remote message display
    else if (cmd.startsWith("M:"))
    {
      String msg = cmd.substring(2);
      
      Serial.print("[LOG] Web Message received: ");
      Serial.println(msg);
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WEB MESSAGE:");
      lcd.setCursor(0, 1);
      
      // Truncate to 16 characters to fit standard LCD width
      lcd.print(msg.substring(0, 16)); 
      delay(4000); // Display message for 4 seconds before returning to sensor data
      lcd.clear();
    }
  }
}
