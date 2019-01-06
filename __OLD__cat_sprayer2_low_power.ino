/* TODO 
 *  
 *  Code:
 *  - Test thoroughly to see what needs to be done, namely:
 *    - Test button during disabled state and inactivity state
 *    - Test motion detecting during both of those states
 *    - etc.
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
const int disabledIterations = 2; // number of 8 second "chunks" 
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

// "Semaphore" flags (I think that's what they're called!):
volatile bool interrupted_by_button;

//void button_pressed(void);
//void pir_triggered(void);

void setup() {
  Serial.begin(9600);
  
  pinMode(sprayerPin, OUTPUT);
  pinMode(LED1Pin, OUTPUT);
  pinMode(LED2Pin, OUTPUT);
  //buttonPin and pirPin set to INPUT by default

  spraying = false;
  disabled = false;
  pirInputValue = LOW;
  buttonState = LOW; 
  LEDOnCount = 0;
  disabledDuration = 0;
  waitingForMotionTimerStart = millis();
  interrupted_by_button = false;
  
  // This is to work out a bug that I can't quite figure out -- always sprays when initially enabled; we'll use this flag to ignore the "first spray trigger" and not actually spray
  firstSpray = true;

  // Start off disabled (initializes enabled LED to false) for 2 iterations ~= 8 seconds
  //disableSprayer(1);   

  // Initialize pin states
  digitalWrite(pirPin, LOW);   //per HC-SR505 datasheet
  digitalWrite(sprayerPin, LOW);
  digitalWrite(LED1Pin, LOW);
  digitalWrite(LED2Pin, LOW);
  digitalWrite(buttonPin, LOW);
  digitalWrite(enabledLEDPin, LOW);

  // Attach interrupts (button and PIR)
  attachInterrupt(digitalPinToInterrupt(buttonPin), button_pressed, FALLING);
  attachInterrupt(digitalPinToInterrupt(pirPin), pir_triggered, RISING);
  
}

void loop() {

  // See if currently disabled; if so, do nothing but wait (disableSprayer is handling power down)
  if(disabled) {
    Serial.print("...still disabled");
    delay(D);
    return;
  }

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

  /* Disable button code 
  // Get state of button pin
  buttonState = digitalRead(buttonPin);

  // If button pressed 
  if (buttonState == HIGH) {
    
    // Reset the button state
    buttonState = LOW;

    // Delay so that a button press is not counted more than once in buttonDelay milliseconds
    delay(buttonDelay);

    // Case 0: No LEDs currently on (sprayer enabled);  put in a disabled state and start disable timer
    if(LEDOnCount == 0) {
      
      // Set LED counter to 1 (i.e. one LED on)
      LEDOnCount = 1;
      
      // Turn on LED1
      digitalWrite(LED1Pin, HIGH);

      // Disable sprayer for disabledIterations
      disableSprayer(disabledIterations);
    }
    /*
    // Case 1: One LED on; set disabled state to double the disable increment (and restart timer)
    else if (LEDOnCount == 1) {
      // Set LED counter to 2 (i.e. two LEDs on)
      LEDOnCount = 2;

      // Turn on LED2
      digitalWrite(LED2Pin, HIGH);
      
      // Disable sprayer for disabledIncrement*2 seconds
      disableSprayer(disabledIterations * 2);

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
    
   

    // No need to run rest of code
    return;
  }
  
  /* END Disable button code */
 
  /* Sprayer code */

  /*
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
  */
  
  /*
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
  */

  /*
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
    Serial.println("motion detected");
    
    // Reset pir state
    pirInputValue = LOW;
    
    // Start a timer for the sprayer, set spraying state to true, switch on sprayer
    //sprayTimerStart = millis();
    //spraying = true;

    // If first spray, don't actually spray!  This is a workaround for a bug I can't quite figure out...
    if(firstSpray) {
      Serial.println("Ignoring first trigger to spray...");
      firstSpray = false;
      disableSprayer(1);
      return;
    }
    digitalWrite(sprayerPin, HIGH);
    Serial.print("Pssssss...");
    delay(sprayDuration);
    digitalWrite(sprayerPin, LOW);
    Serial.print("...tttt\n");
    

    //Disable sprayer for 2 iterations:
    disableSprayer(2);
  }
  */
}

int secondsSince(unsigned long timeMS) {
  return int((millis() - timeMS) / 1000);
}


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
        */
        
        // Stop disabling
        disabled = false;

        // Re-enable PIR interrupt
        attachInterrupt(digitalPinToInterrupt(pirPin), pir_triggered, FALLING);
        
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
  attachInterrupt(digitalPinToInterrupt(pirPin), pir_triggered, FALLING);
        
  Serial.println("Exiting disabledSprayer() due to completion of disable time.");
  delay(D);

  return DISABLE_ELAPSED;
}

void turnOffLEDs() {
  digitalWrite(LED1Pin, LOW);
  digitalWrite(LED2Pin, LOW);  
  LEDOnCount = 0;
}

void pir_triggered() {

  Serial.println("PIR trigger!");
  delay(D);
  //interrupted_by_pir = true;

}

void button_pressed() {
  int exit_code = 0;
  
  Serial.println("Button input!");

  // This semaphore flag should stop any previously running disable loop:  ***IT'S NOT!!!***
  interrupted_by_button = true;
  //TEMP:
  delay(2000);

  // Delay by button delay amount (so that a single button input doesn't get counted again!)
  delay(buttonDelay);

  /* Button / LED code */

  // Case 0: No LEDs lit
  if (LEDOnCount == 0) {
    // Turn on LED1
    digitalWrite(LED1Pin, HIGH);

    LEDOnCount = 1;
    
    // Reset disable semaphore flag
    interrupted_by_button = false;

    // Enter disabled state for disabledIterations
    exit_code = disableSprayer(disabledIterations);

    // Turn off LED1 (if disable elapsed)
    if(exit_code == DISABLE_ELAPSED) {
      Serial.println("One LED is on, turning it off.");
      delay(D);
      turnOffLEDs();
    }

    // Kill this thread
    return;
  }

  // Case 1: Only LED1 is lit; restart disabled mode with double the iteration length:
  else if (LEDOnCount == 1) {    
    // Turn on LED2
    digitalWrite(LED2Pin, HIGH);
    
    // Now 2 LEDs are on
    LEDOnCount = 2;

    // Reset disable semaphore flag
    interrupted_by_button = false;
  
    // Enter disabled state for disabledIterations (twice, turning off one LED in between)
    exit_code = disableSprayer(disabledIterations);

    // Turn off LED2 (if disable elapsed)
    if(exit_code == DISABLE_ELAPSED) {
      Serial.println("Two LEDs are on, turning LED2 off.");
      delay(D);
      digitalWrite(LED2Pin, LOW);
      LEDOnCount = 1;
    }

    // Continue the disable state for disabledIterations
    exit_code = disableSprayer(disabledIterations);
    
    // Turn off LED1 (if disable elapsed)
    if(exit_code == DISABLE_ELAPSED) {
      Serial.println("One LED is still on from a longer disable, turning LED1 off.");
      delay(D);
      turnOffLEDs();
    }

    // Kill this thread
    return;
  }
  else if (LEDOnCount == 2) {
    //Turn off both LEDs
    Serial.println("Turning off both LEDs - user cancelling disable");
    turnOffLEDs();
    
    LEDOnCount = 0;

    // For a delay, disable for one last iteration:
    //disableSprayer(1);

  }
}
