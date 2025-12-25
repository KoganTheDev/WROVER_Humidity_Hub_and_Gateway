#include <Arduino.h>

#define BUTTON_PIN 2
#define LED_PIN 10

volatile unsigned long pressStartTime = 0;
volatile uint8_t shortPressCount = 0;
volatile int blinkHalfPeriod = 0; // Time in ms for half a blink cycle

// Function Declarations
void startBlinking(uint8_t count);
void stopSystem();
void handleButtonISR();

// Main Functions

void setup() {
    // Setup button and LED to Arduino's GPIOs
    pinMode(BUTTON_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    
    // On button state change, activate handleButtonISR
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonISR, CHANGE);

    // Initialize timer

    // Don't allow interrupts while setting Timer0
    noInterrupts();

    // Clear TImer0 Control Registers
    TCCR1A = 0; 
    TCCR1B = 0; 
    TCNT1  = 0;

    // Reconfigure Timer0 Control Registers

    // Set CTC mode - reset timer when hitting the value in "OCR1A" register
    TCCR1B |= (1 << WGM12); 
    // Enable Timer Compare Interrupt
    TIMSK1 |= (1 << OCIE1A); 

    // Reallow interrupts 
    interrupts();
}

void loop() {}


// Helper Functions


// ISR for the Button
void handleButtonISR()
{
    if (digitalRead(BUTTON_PIN) == HIGH) // Button press
    {
        pressStartTime = millis(); // Create timestamp on button press
    }
    else // Button released
    {
        unsigned long duration = millis() - pressStartTime; // Calculate time diff between press and release
        
        if (duration < 1500) // Short press
        {
            shortPressCount++;
        }
        else if (duration >= 1500 && duration <= 4000) // Long press
        {
            startBlinking(shortPressCount);
        }
        else // Really long press, more than 4 Seconds
        {
            stopSystem();
        }
    }
}

// ISR for the Timer (This handles the actual blinking)
ISR(TIMER1_COMPA_vect)
{
    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Flip LED state, HIGH->LOW\LOW->HIGH
}

void startBlinking(uint8_t count)
{
    if (count == 0) return;
    
    // Calculate blinking time duration
    long intervalMs = count * 500;
    
    // Configure Timer1 Compare Match value based on intervalMs
    // formula: OCR1A = (16MHz / (Prescaler * TargetFrequency)) - 1
    // For 1024 prescaler: OCR1A = (15625 * intervalSeconds) - 1
    OCR1A = (15625 * intervalMs / 1000) - 1;
    
    // Start Timer with 1024 prescaler
    TCCR1B |= (1 << CS12) | (1 << CS10);
}

void stopSystem()
{
    TCCR1B &= ~((1 << CS12) | (1 << CS11) | (1 << CS10)); // Stop clock
    digitalWrite(LED_PIN, LOW);
    shortPressCount = 0;
}
