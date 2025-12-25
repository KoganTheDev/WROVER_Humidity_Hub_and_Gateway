/** 
 * Example of a solution without a timer 

 #include <Arduino.h>

const int buttonPin = 2;
const int ledPin = 13;

const unsigned long SHORT_PRESS_TIME = 1500;
const unsigned long LONG_PRESS_TIME  = 4000;

const unsigned long interval = 500;   

int PressCounter = 0;
bool pressed = false;
unsigned long pressTime = 0;

bool Blink = false;                   
unsigned long blinkInterval = 0;      
unsigned long lastFlashStart = 0;
bool ledOn = false;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600);
}

void loop() {

  // Detect press start
  if (digitalRead(buttonPin) == LOW && !pressed) {
    pressed = true;
    pressTime = millis();
  }

  // Detect release and classify press
  if (digitalRead(buttonPin) == HIGH && pressed) {
    pressed = false;
    unsigned long duration = millis() - pressTime;

    if (duration <= SHORT_PRESS_TIME) {
      PressCounter++;
      Serial.print("Short press number: ");
      Serial.println(PressCounter);

      digitalWrite(ledPin, HIGH);    
      ledOn = true;
      lastFlashStart = millis();
    }
    else if (duration <= LONG_PRESS_TIME) {
      Serial.println("Medium press");

      if (PressCounter == 1) blinkInterval = 1000;
      else if (PressCounter == 2) blinkInterval = 2000;
      else blinkInterval = 3000;

      Blink = true;
      digitalWrite(ledPin, HIGH);
      ledOn = true;
      lastFlashStart = millis();
    }
    else {
      Serial.println("Long press - LED off");

      Blink = false;
      PressCounter = 0;
      digitalWrite(ledPin, LOW);
      ledOn = false;
    }
  }

  // Turn LED off after 500ms
  if (ledOn && (millis() - lastFlashStart >= interval)) {
    digitalWrite(ledPin, LOW);
    ledOn = false;
  }

  // Periodic blinking
  if (Blink && !ledOn && (millis() - lastFlashStart >= blinkInterval)) {
    digitalWrite(ledPin, HIGH);
    ledOn = true;
    lastFlashStart = millis();
  }
}
/**/

#include <Arduino.h>
#include <avr/interrupt.h>

const int buttonPin = 2;
const int ledPin = 13;

const unsigned long SHORT_PRESS_TIME = 1500;
const unsigned long LONG_PRESS_TIME  = 4000;

const unsigned long interval = 500;

int PressCounter = 0;
bool pressed = false;
unsigned long pressTime = 0;

bool Blink = false;
unsigned long blinkInterval = 0;
unsigned long lastFlashStart = 0;
bool ledOn = false;

volatile unsigned long systemMs = 0;

ISR(TIMER2_COMPA_vect) {
  systemMs++;
}

// Safe read of 32-bit variable updated in ISR (AVR is 8-bit)
unsigned long getSystemMs() {
  unsigned long t;
  noInterrupts();
  t = systemMs;
  interrupts();
  return t;
}

void setupTimer2_1ms() {
  TCCR2A = (1 << WGM21);
  TCCR2B = (1 << CS22);
  OCR2A = 249;
  TIMSK2 = (1 << OCIE2A);
}

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600);

  setupTimer2_1ms();
  sei(); // Enable global interrupts
}

void loop() {
  unsigned long now = getSystemMs();

  // Detect press start
  if (digitalRead(buttonPin) == LOW && !pressed) {
    pressed = true;
    pressTime = now;
  }

  // Detect release and classify press
  if (digitalRead(buttonPin) == HIGH && pressed) {
    pressed = false;
    unsigned long duration = now - pressTime;

    if (duration <= SHORT_PRESS_TIME) {
      PressCounter++;
      Serial.print("Short press number: ");
      Serial.println(PressCounter);

      digitalWrite(ledPin, HIGH);
      ledOn = true;
      lastFlashStart = now;
    }
    else if (duration <= LONG_PRESS_TIME) {
      Serial.println("Medium press");

      if (PressCounter == 1) blinkInterval = 1000;
      else if (PressCounter == 2) blinkInterval = 2000;
      else blinkInterval = 3000;

      Blink = true;
      digitalWrite(ledPin, HIGH);
      ledOn = true;
      lastFlashStart = now;
    }
    else {
      Serial.println("Long press - LED off");

      Blink = false;
      PressCounter = 0;
      digitalWrite(ledPin, LOW);
      ledOn = false;
    }
  }

  // Turn LED off after 500ms
  if (ledOn && (now - lastFlashStart >= interval)) {
    digitalWrite(ledPin, LOW);
    ledOn = false;
  }

  // Periodic blinking (flash every blinkInterval ms)
  if (Blink && !ledOn && (now - lastFlashStart >= blinkInterval)) {
    digitalWrite(ledPin, HIGH);
    ledOn = true;
    lastFlashStart = now;
  }
}
