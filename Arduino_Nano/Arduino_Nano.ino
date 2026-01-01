/**
 * @file Arduino_Nano.ino
 * @brief Arduino Nano I2C Slave with Humidity Sensor and LCD Display
 * 
 * This application implements an Arduino Nano that:
 * - Communicates with ESP32 via I2C (Slave role)
 * - Reads humidity data from DHT22 sensor
 * - Displays messages on 16x2 LCD
 * - Tracks and reports sensor data to ESP32
 * 
 * @author Embedded Systems Course
 * @date 2026
 */

#include <Wire.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

// ============================================================================
// CONFIGURATION PARAMETERS
// ============================================================================

/** @defgroup I2C_Config I2C Configuration
 *  @{
 */
#define NANO_I2C_ADDRESS 0x08        ///< I2C Slave address for Nano
/** @} */

/** @defgroup DHT_Config DHT Sensor Configuration
 *  @{
 */
#define DHT_PIN 2                     ///< DHT sensor data pin
#define DHT_TYPE DHT22                ///< DHT sensor type (DHT22 or DHT11)
/** @} */

/** @defgroup LCD_Config LCD Display Configuration
 *  @{
 */
#define LCD_ADDR 0x27                 ///< I2C address of LCD display
#define LCD_COLS 16                   ///< LCD number of columns
#define LCD_ROWS 2                    ///< LCD number of rows
/** @} */

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

DHT dht(DHT_PIN, DHT_TYPE);           ///< DHT sensor object
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS); ///< LCD display object

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

float currentHumidity = 0.0;          ///< Current humidity reading
char lcdMessage[21];                  ///< Buffer for LCD message (20 chars max)
unsigned long lastSensorRead = 0;     ///< Timestamp of last sensor reading

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

void setupDHT(void);
void setupLCD(void);
void setupI2C(void);
void readHumiditySensor(void);
void sendHumidityToMaster(void);
void handleI2CReceive(int length);
void displayMessage(const char* message);

// ============================================================================
// SETUP AND LOOP
// ============================================================================

/**
 * @brief Arduino setup function - called once at startup
 * 
 * Initializes:
 * - Serial communication (for debugging)
 * - DHT humidity sensor
 * - LCD display
 * - I2C Slave mode
 */
void setup() {
  Serial.begin(9600);
  delay(1000);
  
  Serial.println("\n\n=== Arduino Nano I2C Slave with Sensor ===");
  
  // Initialize components
  setupDHT();
  setupLCD();
  setupI2C();
  
  // Display startup message
  displayMessage("System Ready!");
  
  Serial.println("[SETUP] All systems initialized");
}

/**
 * @brief Arduino loop function - called repeatedly
 * 
 * Handles:
 * - Periodic humidity sensor readings
 * - I2C communication (handled by interrupt)
 */
void loop() {
  // Read sensor every 2 seconds
  if (millis() - lastSensorRead > 2000) {
    readHumiditySensor();
    lastSensorRead = millis();
  }
  
  delay(100);
}

// ============================================================================
// SENSOR SETUP AND READING
// ============================================================================

/**
 * @brief Initializes DHT temperature and humidity sensor
 * 
 * Sets up the DHT sensor library and performs initial read.
 */
void setupDHT(void) {
  dht.begin();
  Serial.println("[DHT] Sensor initialized on pin " + String(DHT_PIN));
  delay(2000); // Wait for sensor to stabilize
}

/**
 * @brief Reads humidity from DHT sensor
 * 
 * Retrieves the humidity value from the DHT sensor.
 * Validates the reading and updates the global humidity variable.
 */
void readHumiditySensor(void) {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("[DHT] Error reading sensor");
    return;
  }
  
  currentHumidity = humidity;
  
  // Update LCD with current readings
  char line1[17], line2[17];
  snprintf(line1, sizeof(line1), "Temp: %.1f C", temperature);
  snprintf(line2, sizeof(line2), "Hum:  %.1f %%", humidity);
  
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
  
  Serial.printf("[DHT] Temperature: %.1f C, Humidity: %.1f %%\n", temperature, humidity);
}

// ============================================================================
// LCD SETUP AND DISPLAY
// ============================================================================

/**
 * @brief Initializes the 16x2 I2C LCD display
 * 
 * Configures the LCD for 16 columns and 2 rows,
 * initializes the display, and sets backlight on.
 */
void setupLCD(void) {
  lcd.init();
  lcd.backlight();
  lcd.print("Initializing...");
  Serial.println("[LCD] Display initialized at address 0x" + String(LCD_ADDR, HEX));
  delay(1000);
}

/**
 * @brief Displays a message on the LCD
 * 
 * @param message Pointer to message string (max 32 chars for 2 lines)
 * 
 * Clears the display and shows the provided message.
 * First 16 chars on line 1, next 16 chars on line 2.
 */
void displayMessage(const char* message) {
  lcd.clear();
  
  int len = strlen(message);
  
  // Line 1: first 16 characters
  lcd.setCursor(0, 0);
  lcd.print(message);
  
  // Line 2: next 16 characters (if available)
  if (len > 16) {
    lcd.setCursor(0, 1);
    lcd.print(message + 16);
  }
  
  Serial.printf("[LCD] Message displayed: %s\n", message);
}

// ============================================================================
// I2C SLAVE SETUP AND COMMUNICATION
// ============================================================================

/**
 * @brief Initializes I2C Slave mode
 * 
 * Configures the Nano as an I2C slave at address NANO_I2C_ADDRESS.
 * Registers callbacks for data receive and request events.
 */
void setupI2C(void) {
  Wire.begin(NANO_I2C_ADDRESS);
  Wire.onReceive(handleI2CReceive);
  Wire.onRequest(sendHumidityToMaster);
  
  Serial.println("[I2C] Configured as Slave at address 0x" + String(NANO_I2C_ADDRESS, HEX));
}

/**
 * @brief Sends humidity data to ESP32 Master via I2C
 * 
 * Called when master requests data via onRequest callback.
 * Transmits the current humidity reading as a 4-byte float.
 */
void sendHumidityToMaster(void) {
  byte buffer[4];
  float* ptr = (float*)buffer;
  *ptr = currentHumidity;
  
  Wire.write(buffer, 4);
  
  Serial.printf("[I2C] Sent humidity (%.2f %%) to master\n", currentHumidity);
}

/**
 * @brief Handles data received from ESP32 Master via I2C
 * 
 * @param length Number of bytes received
 * 
 * Processes commands from the master:
 * - 'M' (0x4D): Message for LCD display
 * - 'R' (0x52): Reset sensor readings
 * 
 * Message format: 'M' <length> <data>
 */
void handleI2CReceive(int length) {
  if (length == 0) {
    return;
  }
  
  byte command = Wire.read();
  
  // Message command
  if (command == 'M' && Wire.available() > 0) {
    byte msgLength = Wire.read();
    msgLength = min(msgLength, 20); // Limit to 20 characters
    
    char message[21];
    int i = 0;
    while (Wire.available() && i < msgLength) {
      message[i++] = (char)Wire.read();
    }
    message[i] = '\0';
    
    displayMessage(message);
    Serial.printf("[I2C] Received message command: %s\n", message);
  }
  // Reset command
  else if (command == 'R') {
    Serial.println("[I2C] Received reset command");
    // Reset logic would go here if needed
  }
}

