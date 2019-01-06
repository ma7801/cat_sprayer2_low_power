/* TODO 
 *  
 *  Code:
 *  - Test thoroughly to see what needs to be done, namely:
 *    - Test button during disabled state and inactivity state
 *    - Test motion detecting during both of those states
 *    - etc.
 *  - Code cleanup!  Lots of variables aren't used, remove comments   
 *  
 *  Hardware:
 *  - need to move PIR input to D2
 *  - need to move LED2 to D5
 *  - need to move LED1 to D4
 *  - need to move Button to D3
 *  
 *  
 *  
 *  Possible workarounds:
 *  - change functionality (at least of my current soldered version) so that the disable can only be on or off (for 10 minutes or off), i.e. get rid of an LED
 *  - could use D5 for a pin (perhaps LED2 or sprayer), but would have to be soldered directly to Arduino
 *  
 *  Notes:
 *  - D2 or D3 have to be used as interrupt inputs (for the button and the PIR)
 *  - A6 and A7 CAN'T be used as digital inputs or outputs (only as analog inputs, I believe)
 *  
 */

#include <LowPower.h>

#define DISABLE_INTERRUPTED 1
#define DISABLE_ELAPSED 2

//enum intr {PIR = 1, BUTTON = 2};

const int D = 1000; // DEBUG delay for timer so that serial communication can happen before power down

const int pirPin = 2;       
const int sprayerPin = A5; 
const int LED1Pin = 4;
const int LED2Pin = 5;
const int buttonPin = 3;     // MUST be pin 2 or 3 since used as an external interrupt pin
const int enabledLEDPin = 13;

const period_t sleepIterationLength = SLEEP_8S;
const int delayAfterEnable = 7;  // in seconds - pause value after going out of disabled state AND at power on (this is added on to power down delay above)
const int delayAfterSpray = 7;  // in seconds
const int sprayDuration = 250;   // in milliseconds
const int disabledIterationInterval = 2; // number of 8 second "chunks" 
const int powerDownDelay = 5;  // in seconds

const int buttonDelay = 350; //in milliseconds
//const int pirHIGH = 500;  // For analog input of PIR

unsigned long disableTimerStart;  // helps to indicate how long the sprayer is in a disabled state (after spray, or at start up)
unsigned long sprayTimerStart;  // helps to indicate how long the sprayer has been spraying
unsigned long waitingForMotionTimerStart;  // helps to indicate how long the sprayer has been waiting to motion detect; used for powering down to save power
//unsigned long lastButtonPress;  // Used to handle button bounce
bool spraying;
bool disabled;
bool ignoringPir;
bool firstLoop;
int pirInputValue;
volatile bool button_pressed;
volatile bool pir_triggered;
int LEDOnCount;
int remainingDisabledIterations;
bool secondInterval;
bool okToIdle;

void setup() {
  Serial.begin(9600);
  
  pinMode(sprayerPin, OUTPUT);
  pinMode(LED1Pin, OUTPUT);
  pinMode(LED2Pin, OUTPUT);
  //buttonPin and pirPin set to INPUT by default

  spraying = false;
  disabled = false;
  pir_triggered = false;
  LEDOnCount = 0;
  waitingForMotionTimerStart = millis();
  button_pressed = false;
  secondInterval = false;
  //lastButtonPress = millis();
  okToIdle = false;
  
  // This is to work out a bug that I can't quite figure out -- always sprays when initially enabled; we'll use this flag to ignore the "first spray trigger" and not actually spray
  firstLoop = true;

  // Start off disabled (initializes enabled LED to false) for 2 iterations ~= 8 seconds
  remainingDisabledIterations = 2;  

  // Initialize pin states
  digitalWrite(pirPin, LOW);   //per HC-SR505 datasheet
  digitalWrite(sprayerPin, LOW);
  digitalWrite(LED1Pin, LOW);
  digitalWrite(LED2Pin, LOW);
  digitalWrite(buttonPin, LOW);
  digitalWrite(enabledLEDPin, LOW);

  // Attach interrupt button interrupt (will attach PIR interrupt later since starting out with delay)
  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonISR, FALLING);
  
}

void loop() {
  // If disabled
  if(remainingDisabledIterations > 0) {
    Serial.println("Sleeping for 8 seconds...");
    //delay(D);
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    
    Serial.println("Waking up!");
    //delay(D);
    remainingDisabledIterations--;

    // If last iteration elapsed:
    if(remainingDisabledIterations == 0) {
      // If there is a second disable interval (i.e. user hit button twice)
      if(secondInterval) {
        secondInterval = false;
    
        // Turn off LED2
        digitalWrite(LED2Pin, LOW);
    
        // Reset the iteration count to disabledIterationsInterval
        remainingDisabledIterations = disabledIterationInterval;
    
        // Don't need to run rest of code
        return;
      }
      // Otherwise, disabled period totally elapsed
      else {
        //Attach PIR interrupt
        attachInterrupt(digitalPinToInterrupt(pirPin), pirISR, RISING);
  
        // Turn off LED1
        digitalWrite(LED1Pin, LOW);

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

      // No need to run rest of code in loop()
      return;
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

      // No need to run rest of code
      return;
    }
    else if (LEDOnCount == 2) {
      //Turn off both LEDs
      Serial.println("Turning off both LEDs - user cancelling disable");
      digitalWrite(LED1Pin, LOW);
      digitalWrite(LED2Pin, LOW);

      // Reset flags / counters
      LEDOnCount = 0;
      remainingDisabledIterations = 1;  // The "1" is for a delay after coming out of disabled mode
      secondInterval = false;
      
    }
  }

  // If motion detected
  if(pir_triggered) {

    pir_triggered = false;
    // Activate sprayer
    digitalWrite(sprayerPin, HIGH);
    //DEBUG:
    Serial.println("Spray!");
    digitalWrite(13, HIGH);
    //ENDDEBUG
    
    delay(sprayDuration);

    //DEBUG
    digitalWrite(13, LOW);
    //ENDDEBUG
    
    digitalWrite(sprayerPin, LOW);
  }

  
  // Enter a low power idle until interrupt activated
  if(okToIdle) {
    Serial.println("Going to idle state...");
    delay(D);
    
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  }
  
  /*
  //See if we should save some power -- waiting too long 
  if(secondsSince(waitingForMotionTimerStart) > powerDownDelay) {
    // Set the arduino chip to a low power state (indefinitely - until interrupted by button or PIR input)
    Serial.println("Going to sleep due to inactivity...");
    delay(D);
    
    //Attach external interrupt to button pin
     // Power down
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
    // Should be execuated after wake up:
    //detachInterrupt(digitalPinToInterrupt(buttonPin));

    Serial.println("Waking up from inactivity.");
    delay(D);
    
    // Reset the waiting timer:
    waitingForMotionTimerStart = millis();
  }
  */
 
}

/*
int secondsSince(unsigned long timeMS) {
  return int((millis() - timeMS) / 1000);
}

/*
int disableSprayer(int iterations) {
  // Set disabled state to true
  disabled = true;

  // Ignore the PIR interrupt during the disabled state:
  detachInterrupt(digitalPinToInterrupt(pirPin));
  
  // Turn off enabled LED
  digitalWrite(enabledLEDPin, LOW);
  //disableTimerStart = millis();
  //disabledDuration = seconds;
  
  // Set the arduino chip to a low power state
  Serial.print("disableSprayer() called\n");

  //DEBUG
  delay(D);
  
  //Attach external interrupt to button pin
  //attachInterrupt(digitalPinToInterrupt(buttonPin), button_pressed, CHANGE);
  // Power down
  for (int i = 0; i < iterations; i++) {
    LowPower.powerDown(sleepIterationLength, ADC_OFF, BOD_OFF);
    
    // If interrupted by button
    if(interrupted_by_button) {
      interrupted_by_button = false;
      /*
      // Case 1: Only LED1 is lit; restart disabled mode with double the iteration length:
      if (LEDOnCount == 1) {
        iterations = disabledIterations * 2;
        
        // Turn on LED2
        digitalWrite(LED2Pin, HIGH);
        
        // Now 2 LEDs are on
        LEDOnCount = 2;

        // Delay by button delay amount (so that a single button input doesn't get counted again!)
        delay(buttonDelay);
      }
      else if (LEDOnCount == 2) {
        //Turn off both LEDs
        turnOffLEDs();
        
        LEDOnCount = 0;

        // Delay by button delay amount (so that a single button input doesn't get counted again!)
        delay(buttonDelay);
        
        
        // Stop disabling
        disabled = false;

        // Re-enable PIR interrupt
        attachInterrupt(digitalPinToInterrupt(pirPin), pirISR, FALLING);
        
        Serial.println("Exiting disabledSprayer() due to button interrupt.");
        delay(D);
        return DISABLE_INTERRUPTED;
      }
    }
  

  // Should be execuated after wake up:
  //detachInterrupt(digitalPinToInterrupt(buttonPin));

  //DEBUG
  Serial.println("Waking up from disable naturally...");
  delay(D);
  
  // Set pir PIN to low
  //digitalWrite(pirPin, LOW);

  // Restart the "waiting" timer
  waitingForMotionTimerStart = millis();

  
  // Stop disabling
  disabled = false;

  // Re-enable PIR interrupt
  attachInterrupt(digitalPinToInterrupt(pirPin), pirISR, FALLING);
        
  Serial.println("Exiting disabledSprayer() due to completion of disable time.");
  delay(D);

  return DISABLE_ELAPSED;
}

void turnOffLEDs() {
  digitalWrite(LED1Pin, LOW);
  digitalWrite(LED2Pin, LOW);  
  LEDOnCount = 0;
}
*/

void pirISR() {

  Serial.println("PIR trigger!");
  delay(D);

  pir_triggered = true;

  //interrupted_by_pir = true;

}

void buttonISR() {
  Serial.println("Button input!");
  //delay(D);
  
  button_pressed = true;
}
