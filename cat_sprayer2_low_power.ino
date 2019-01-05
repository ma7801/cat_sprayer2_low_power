/* TODO 
 *  
 *  Code:
 *  - leave Arduino powered up when manually disabled (otherwise it will stay disabled forever!)
 *    - OR do some math with the sleep delay (i.e. SLEEP_8S) and run this in some kind of for loop to total to the amount of the desired disable duration
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

enum intr {PIR = 1, BUTTON = 2};

const int pirPin = 2;       
const int sprayerPin = A5; 
const int LED1Pin = 4;
const int LED2Pin = 5;
const int buttonPin = 3;     // MUST be pin 2 or 3 since used as an external interrupt pin
const int enabledLEDPin = 13;

const period_t tempPowerDownDuration = SLEEP_8S;
const int delayAfterEnable = 7;  // in seconds - pause value after going out of disabled state AND at power on (this is added on to power down delay above)
const int delayAfterSpray = 7;  // in seconds
const int sprayDuration = 250;   // in milliseconds
const int disabledIncrement = 300; // in seconds  
const int powerDownDelay = 5;  // in seconds

const int buttonDelay = 350; //in milliseconds
//const int pirHIGH = 500;  // For analog input of PIR

unsigned long disableTimerStart;  // helps to indicate how long the sprayer is in a disabled state (after spray, or at start up)
unsigned long sprayTimerStart;  // helps to indicate how long the sprayer has been spraying
unsigned long waitingForMotionTimerStart;  // helps to indicate how long the sprayer has been waiting to motion detect; used for powering down to save power
bool spraying;
bool disabled;
bool ignoringPir;
bool firstSpray;
int pirInputValue;
int buttonState;
int LEDOnCount;
int disabledDuration;

void setup() {
  Serial.begin(9600);
  
  pinMode(sprayerPin, OUTPUT);
  pinMode(LED1Pin, OUTPUT);
  pinMode(LED2Pin, OUTPUT);
  //buttonPin and pirPin set to INPUT by default

  spraying = false;
  pirInputValue = LOW;
  buttonState = LOW; 
  LEDOnCount = 0;
  disabledDuration = 0;
  waitingForMotionTimerStart = millis();
  
  // This is to work out a bug that I can't quite figure out -- always sprays when initially enabled; we'll use this flag to ignore the "first spray trigger" and not actually spray
  firstSpray = true;

  // Start off disabled (initializes enabled LED to false)
  disableSprayer(delayAfterEnable); 

  // Initialize pin states
  digitalWrite(pirPin, LOW);   //per HC-SR505 datasheet
  digitalWrite(sprayerPin, LOW);
  digitalWrite(LED1Pin, LOW);
  digitalWrite(LED2Pin, LOW);
  digitalWrite(buttonPin, LOW);
  digitalWrite(enabledLEDPin, LOW);
}

void loop() {

  //See if we should save some power -- waiting too long 
  if(secondsSince(waitingForMotionTimerStart) > powerDownDelay) {
    // Set the arduino chip to a low power state (indefinitely - until interrupted by button or PIR input)
    Serial.print("Going to sleep...\n");
    //Attach external interrupt to button pin
    attachInterrupt(digitalPinToInterrupt(buttonPin), wakeup_button, CHANGE);
    attachInterrupt(digitalPinToInterrupt(pirPin), wakeup_pir, CHANGE);
    // Power down
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
    // Should be execuated after wake up:
    detachInterrupt(digitalPinToInterrupt(buttonPin));

    // Reset the waiting timer:
    waitingForMotionTimerStart = millis();
  }

  /* Disable button code */
  // Get state of button pin
  buttonState = digitalRead(buttonPin);

  // If button pressed 
  if (buttonState == HIGH) {
    
    // Reset the button state
    buttonState = LOW;

    // Case 0: No LEDs currently on (sprayer enabled);  put in a disabled state and start disable timer
    if(LEDOnCount == 0) {
      
      // Set LED counter to 1 (i.e. one LED on)
      LEDOnCount = 1;
      
      // Turn on LED1
      digitalWrite(LED1Pin, HIGH);

      // Disable sprayer for disabledIncrement seconds
      disableSprayer(disabledIncrement);
    }
    
    // Case 1: One LED on; set disabled state to double the disable increment (and restart timer)
    else if (LEDOnCount == 1) {
      // Set LED counter to 2 (i.e. two LEDs on)
      LEDOnCount = 2;

      // Turn on LED2
      digitalWrite(LED2Pin, HIGH);
      
      // Disable sprayer for disabledIncrement*2 seconds
      disableSprayer(disabledIncrement * 2);

    }

    // Case 2: Both LEDs on; take out of disabled state (with delay)
    else if (LEDOnCount == 2) {
      // Set counter to 0; no LEDs on
      LEDOnCount = 0;

      // Turn off both LEDs
      turnOffLEDs();
      
      // Set disable duration to delayAfterEnable -- the "if(disabled)" section will re-enable after this delay
      disableSprayer(delayAfterEnable);

    }

    // Delay so that a button press is not counted more than once in buttonDelay milliseconds
    delay(buttonDelay);

    // No need to run rest of code
    return;
  }
  
  /* END Disable button code */
 
  /* Sprayer code */
  
  // If currently spraying:
  if(spraying) {
    // If spraying timer has elapsed, stop spraying!
    if(millis() - sprayTimerStart >= sprayDuration) {
      // Stop the sprayer
      digitalWrite(sprayerPin, LOW);

      // Set spraying state to false
      spraying = false;
      //DEBUG:
      //Serial.print("stopped sprayer\n");

      // Disable the sprayer for the delayAfterSpray seconds
      disableSprayer(delayAfterSpray);
    }

    // Make sure LEDs stay off -- somehow they turn on when sprayer motor sucks a lot of current
    turnOffLEDs();
    
    // Shouldn't need to run rest of code if spraying (or just finished spraying)
    return;
  }
  
  
  // If currently in a disabled state:
  if (disabled) {

    // If both LEDS on and half the duration has elapsed, turn off the 2nd LED
    if (LEDOnCount == 2 && secondsSince(disableTimerStart) >= disabledDuration - disabledIncrement) {
      digitalWrite(LED2Pin, LOW);
      LEDOnCount = 1;
    }
    
    // If disable timer has elapsed, turn off LED1 (may already be off if not disabled from button push), re-enable the sprayer 
    else if (secondsSince(disableTimerStart) >= disabledDuration) {
      // Turn off LED1
      digitalWrite(LED1Pin, LOW);
      LEDOnCount = 0;

      // Re-enable sprayer
      disabled = false;
      digitalWrite(enabledLEDPin, HIGH);

    }
    
    // Otherwise don't run rest of code
    else return;
  }
  
  // Read state of pin connected to motion sensor (PIR)
  pirInputValue = digitalRead(pirPin);
  // TEMPORARY FOR TESTING:
  //Serial.print("value of pirPin: ");
  //Serial.print(pirInputValue);
  //Serial.print("\n");
  //return;
  // END TEMPORARY CODE

  
  // If motion detector activated
  if(pirInputValue == HIGH) {
    
    //DEBUG:
    Serial.print("motion detected\n");
    
    // Reset pir state
    pirInputValue = LOW;
    
    // Start a timer for the sprayer, set spraying state to true, switch on sprayer
    sprayTimerStart = millis();
    spraying = true;

    // If first spray, don't actually spray!  This is a workaround for a bug I can't quite figure out...
    if(firstSpray) {
      firstSpray = false;
      return;
    }
    digitalWrite(sprayerPin, HIGH);
  }
}

int secondsSince(unsigned long timeMS) {
  return int((millis() - timeMS) / 1000);
}


void disableSprayer(int seconds) {
  // Set disabled state to true
  disabled = true;


  // Turn off enabled LED
  digitalWrite(enabledLEDPin, LOW);
  disableTimerStart = millis();
  disabledDuration = seconds;

  // Set the arduino chip to a low power state (temporarily - 8 seconds)
  Serial.print("Going to sleep...\n");
  //Attach external interrupt to button pin
  attachInterrupt(digitalPinToInterrupt(buttonPin), wakeup_button, CHANGE);
  // Power down
  LowPower.powerDown(tempPowerDownDuration, ADC_OFF, BOD_OFF);
  // Should be execuated after wake up:
  detachInterrupt(digitalPinToInterrupt(buttonPin));

  // Set pir PIN to low
  //digitalWrite(pirPin, LOW);

  // Restart the "waiting" timer
  waitingForMotionTimerStart = millis();
}

void turnOffLEDs() {
  digitalWrite(LED1Pin, LOW);
  digitalWrite(LED2Pin, LOW);  
}

void wakeup_pir() {

  Serial.println("Waking up...PIR input!");

}

void wakeup_button() {
  Serial.println("Waking up...button input!");
}
