/** 
 * Example of a solution without a timer 

 #include <Arduino.h>

const int buttonPin = 2;
const int ledPin = 10;

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
const int ledPin = 10; 

// Times (ms)
const unsigned long SHORT_PRESS_TIME = 1500;
const unsigned long LONG_PRESS_TIME  = 4000;
const unsigned long DEBOUNCE_TIME = 50;

// Globals
volatile int PressCounter = 0;
volatile unsigned long pressTime = 0;
volatile unsigned long lastISRTime = 0; 

// Flags
volatile bool actionShort = false;
volatile bool actionStart = false; 
volatile bool actionStop = false;

// System State
bool Blink = false; 
unsigned long lastFlashStart = 0;
bool feedbackLedOn = false;

// ==========================================
// INTERRUPTS
// ==========================================

void buttonISR() {
  unsigned long now = millis();
  if (now - lastISRTime < DEBOUNCE_TIME) return;
  lastISRTime = now;

  if (digitalRead(buttonPin) == HIGH) { 
    pressTime = now;
  } 
  else { 
    unsigned long duration = now - pressTime;

    if (duration < SHORT_PRESS_TIME) {
      if (!Blink) { 
        PressCounter++;
        actionShort = true; 
      }
    } 
    else if (duration < LONG_PRESS_TIME) {
      actionStart = true; 
    } 
    else {
      actionStop = true;  
    }
  }
}

ISR(TIMER1_COMPA_vect) {
  digitalWrite(ledPin, !digitalRead(ledPin)); 
}

// ==========================================
// HELPERS
// ==========================================

void startTimer(int counts) {
  if (counts == 0) counts = 1;
  noInterrupts();
  TCCR1A = 0; TCCR1B = 0; TCNT1 = 0;
  OCR1A = (7812 * counts) - 1; 
  TCCR1B |= (1 << WGM12) | (1 << CS12) | (1 << CS10); 
  TIMSK1 |= (1 << OCIE1A); 
  interrupts();
}

void stopTimer() {
  TCCR1B = 0; TIMSK1 = 0; 
  digitalWrite(ledPin, LOW);
  PressCounter = 0;
  Blink = false;
}


void setup() {
  pinMode(buttonPin, INPUT); 
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600);

  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonISR, CHANGE);
  Serial.println("System Ready.");
}

void loop() {
  if (actionStop) {
    actionStop = false;
    
    Serial.println("!!! SYSTEM SHUTDOWN !!!");
    Serial.println("The system is turning OFF now...");
    
    delay(100); 
    stopTimer();
  }

  if (actionShort) {
    actionShort = false;
    Serial.print("Short press number: ");
    Serial.println(PressCounter);
    
    digitalWrite(ledPin, HIGH);
    feedbackLedOn = true;
    lastFlashStart = millis();
  }

  if (feedbackLedOn && (millis() - lastFlashStart >= 200)) {
    digitalWrite(ledPin, LOW);
    feedbackLedOn = false;
  }

  // 3. Handle Start 
  if (actionStart) {
    actionStart = false;
    if (!Blink && PressCounter > 0) {
      Serial.print("Medium press. Starting with: ");
      Serial.println(PressCounter);
      
      digitalWrite(ledPin, LOW); 
      startTimer(PressCounter);
      Blink = true;
    }
  }
}