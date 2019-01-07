/* TODO 
 *  
 *  Code:
 *  - Add some anti-bounce code in button ISR
 *  - Seems to be working ok, but keep testing.  Ok to run test with sprayer attached.  (undefine DEBUG if doing so)
 *  
 *  (maybe):
 *  - minimize data sizes on variables (byte = 0 to 255, unsigned short int = 0 to 65535)
 *  
 *  Hardware:
 *  - make plug for sprayer; attach to prototype circuit and test
 *  
 *  - test LED wire scratch out on the "bad" arduino board I have -- plug it into to usb (making sure no pins are touching) to test power LED doesn't turn on (TX or RX will!)
 *  
 *  on actual circuit board:
 *  - need to move PIR input to D2
 *  - need to move LED2 to D5
 *  - need to move LED1 to D4
 *  - need to move Button to D3
 *  
 *  
 *  
 *  
 *  Notes:
 *  - D2 or D3 have to be used as interrupt inputs (for the button and the PIR)
 *  - A6 and A7 CAN'T be used as digital inputs or outputs (only as analog inputs, I believe)
 *  
 */

#include <LowPower.h>

//#define DEBUG 1
#define SERIAL_DELAY 200

const int pirPin = 2;        // MUST be pin 2 or 3 since used as an external interrupt pin
const int sprayerPin = A5; 
const int LED1Pin = 4;
const int LED2Pin = 5;
const int buttonPin = 3;     // MUST be pin 2 or 3 since used as an external interrupt pin
const int enabledLEDPin = 13;

const period_t sleepIterationLength = SLEEP_8S;
const int sprayDuration = 250;   // in milliseconds
const int disabledIterationInterval = 36; // number of 8 second "chunks" 
const int buttonDelay = 350; //in milliseconds

volatile bool button_pressed;
volatile bool pir_triggered;
int LEDOnCount;
int remainingDisabledIterations;
bool secondInterval;
bool okToIdle;

void setup() {
  #ifdef DEBUG
  Serial.begin(9600);
  #endif

  // Set up pins that aren't inputs
  pinMode(sprayerPin, OUTPUT);
  pinMode(LED1Pin, OUTPUT);
  pinMode(LED2Pin, OUTPUT);

  // Initialize flags, etc.
  pir_triggered = false;
  LEDOnCount = 0;
  button_pressed = false;
  secondInterval = false;
  okToIdle = false;

  // Start off disabled for 2 iterations ~= 16 seconds
  remainingDisabledIterations = 2;  

  // Initialize pin states
  digitalWrite(pirPin, LOW);   //per HC-SR505 datasheet
  digitalWrite(sprayerPin, LOW);
  digitalWrite(LED1Pin, LOW);
  digitalWrite(LED2Pin, LOW);
  digitalWrite(buttonPin, LOW);
  digitalWrite(enabledLEDPin, LOW);

  // Attach interrupts
  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(pirPin), pirOnISR, RISING);
  
}

void loop() {
  // If disabled
  if(remainingDisabledIterations > 0) {
    
    #ifdef DEBUG
    Serial.println("Sleeping for 8 seconds...");
    delay(SERIAL_DELAY);
    #endif
    
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);

    #ifdef DEBUG
    Serial.println("Waking up!");
    delay(SERIAL_DELAY);
    #endif
    
    remainingDisabledIterations--;

    // If last iteration elapsed:
    if(remainingDisabledIterations == 0) {
      // If there is a second disable interval (i.e. user hit button twice)
      if(secondInterval) {
        secondInterval = false;
    
        // Turn off LED2
        digitalWrite(LED2Pin, LOW);

        // Indicate that only one LED is on now
        LEDOnCount = 1;
    
        // Reset the iteration count to disabledIterationsInterval
        remainingDisabledIterations = disabledIterationInterval;
    
      }
      
      // Otherwise, disabled period totally elapsed
      else {
        // Turn off LED1
        digitalWrite(LED1Pin, LOW);

        // Indicate now LEDs on
        LEDOnCount = 0;

        // Set Ok to power down flag
        okToIdle = true;
      }
    } 
  }

  
  /* Button handler -- button_pressed set to true in buttonISR() */
  if (button_pressed) {

    button_pressed = false;

    // Prevent idle state
    okToIdle = false;
    
    // Case 0: No LEDs lit
    if (LEDOnCount == 0) {
      // Turn on LED1
      digitalWrite(LED1Pin, HIGH);
  
      LEDOnCount = 1;
      
      // Set "remainingDisabledIterations" to the disabledIterationsInteveral (acts as a flag and a counter)
      remainingDisabledIterations = disabledIterationInterval;

    }
  
    // Case 1: Only LED1 is lit; restart disabled mode with double the iteration length:
    else if (LEDOnCount == 1) {    
      // Turn on LED2
      digitalWrite(LED2Pin, HIGH);
      
      // Now 2 LEDs are on
      LEDOnCount = 2;
  
      secondInterval = true;

      // Reset the iterations amount
      remainingDisabledIterations = disabledIterationInterval;

    }
    else if (LEDOnCount == 2) {
    
      #ifdef DEBUG
      Serial.println("Turning off both LEDs - user cancelling disable");
      #endif

      //Turn off both LEDs
      digitalWrite(LED1Pin, LOW);
      digitalWrite(LED2Pin, LOW);

      // Reset flags / counters
      LEDOnCount = 0;
      remainingDisabledIterations = 1;  // The "1" is for a delay after coming out of disabled mode
      secondInterval = false;      
    }

    // Don't want to run rest of code (avoids possible spray immediately after disable cancellation).
    return;
  }

  // If motion detected
  if(pir_triggered) {

    // Reset flag
    pir_triggered = false;
    
    // Check if in disabled state; if so, don't spray, just repeat the loop()
    if (remainingDisabledIterations > 0) return;

    // Activate sprayer
    digitalWrite(sprayerPin, HIGH);
    
    #ifdef DEBUG
    Serial.println("Spray!");
    #endif
    
    digitalWrite(13, HIGH);
    delay(sprayDuration);

    #ifdef DEBUG
    digitalWrite(13, LOW);
    #endif
    
    digitalWrite(sprayerPin, LOW);
  }

  // Enter a low power idle until interrupt activated
  if(okToIdle) {
    #ifdef DEBUG
    Serial.println("Going to idle state...");
    delay(SERIAL_DELAY);
    #endif
    
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);

    #ifdef DEBUG
    Serial.println("Coming out of idle state...");
    delay(SERIAL_DELAY);
    #endif
  } 
}


void pirOnISR() {
  #ifdef DEBUG
  Serial.println("PIR trigger!");
  delay(SERIAL_DELAY);
  #endif

  pir_triggered = true;
}

void buttonISR() {
  #ifdef DEBUG 
  Serial.println("Button input!");
  delay(SERIAL_DELAY);
  #endif

  button_pressed = true;
}
