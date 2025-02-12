#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <Encoder.h>
#include <Bounce2.h>  //Library for debouncing the encoder button and stand detection logic for the soldering iron

// Pin definitions
#define RELAY_PIN 5          // Controls the soldering iron's power
#define DETECT_PIN 6         // Pin to detect if the iron is placed on the stand (pulled to ground)

// LCD initialization (16x2 display with I2C address 0x27)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Rotary Encoder setup
Encoder myEnc(2, 3);                // Encoder pins for rotation
Bounce debouncer_1 = Bounce();      // Debouncer instance for encoder button
int encoderButtonPin = 4;           // Encoder button pin

// Stand detection debouncer
Bounce debouncer_2 = Bounce();      // Debouncer for the ground detection pin

// Timer variables (0 to 300 seconds = max 5 minutes)
int onTime = 60;        // Default heating time (1 minute)
int offTime = 60;       // Default cool-down time (1 minute)
int currentSelection = 0;  // 0 = adjusting On-time, 1 = adjusting Off-time

// Temporary variables for real-time adjustments
int tempOnTime = 60;    
int tempOffTime = 60;   

// Flags and encoder tracking
bool adjusting = false;              // Indicates if the timer is being adjusted
long lastEncoderPosition = 0;        // Tracks last encoder position to prevent jumps

// EEPROM memory addresses for saving timer settings
#define ON_TIME_ADDRESS 0
#define OFF_TIME_ADDRESS 4           // 4 bytes for each integer value in EEPROM

// Timing variables for relay control
unsigned long onTimerStart = 0;      
unsigned long offTimerStart = 0;     
unsigned long groundPullTimeStart = 0;  // Tracks how long the iron has been on the stand
const unsigned long TIME_LIMIT = 300000; // 5-minute timeout to auto power off

void setup() {
  // Relay initialization (set LOW initially to avoid glitches)
  digitalWrite(RELAY_PIN, LOW);
  pinMode(RELAY_PIN, OUTPUT);

  // Encoder button setup with pull-up resistor and debouncing
  pinMode(encoderButtonPin, INPUT_PULLUP);
  debouncer_1.attach(encoderButtonPin);
  debouncer_1.interval(50);  // 50 ms debounce time

  // Stand detection pin setup with pull-up resistor and debouncing
  pinMode(DETECT_PIN, INPUT_PULLUP);
  debouncer_2.attach(DETECT_PIN);
  debouncer_2.interval(150); // 150 ms debounce time to avoid false triggers

  // LCD initialization
  lcd.begin(16, 2);
  lcd.backlight();

  // Startup message
  lcd.setCursor(0, 0);
  lcd.print("Heating Up......");
  lcd.setCursor(0, 1);
  lcd.print("Set Your Timers!");
  delay(2000);  // Display message for 2 seconds

  // Retrieve saved timer settings from EEPROM
  EEPROM.get(ON_TIME_ADDRESS, onTime);
  EEPROM.get(OFF_TIME_ADDRESS, offTime);

  // Load retrieved settings into temporary adjustment variables
  tempOnTime = onTime;
  tempOffTime = offTime;

  // Reset corrupted values (if they exceed max limit)
  if (onTime > 300 || offTime > 300) {
    onTime = 60;
    offTime = 60;
    saveTimerValues();  // Save default values
  }

  // Display initial status
  updateDisplay();
}

void loop() {
  static long oldPosition = -999;
  long newPosition = myEnc.read() / 8;  // Reduces encoder sensitivity by using larger steps

  // Update debouncers for button and stand detection
  debouncer_1.update();
  debouncer_2.update();

  // Handle encoder button press (for entering/exiting adjustment mode)
  if (debouncer_1.fell()) {  
    if (!adjusting) {
      // Enter adjustment mode
      adjusting = true;
      lastEncoderPosition = newPosition;
    } else {
      // Exit adjustment mode and save changes
      adjusting = false;
      if (currentSelection == 0) {
        onTime = tempOnTime;  // Apply new On-time
      } else if (currentSelection == 1) {
        offTime = tempOffTime;  // Apply new Off-time
      }
      saveTimerValues();
    }
    updateDisplay();
  }

  // Adjust timer when in adjustment mode
  if (adjusting && newPosition != oldPosition) {
    adjustTimer(newPosition);
    oldPosition = newPosition;
    updateDisplay();
  } 
  // Switch between On-time and Off-time settings when not adjusting
  else if (!adjusting && newPosition != oldPosition) {
    currentSelection = newPosition % 2;  // Toggle between 0 and 1
    lastEncoderPosition = myEnc.read() / 8;
    oldPosition = newPosition;
    updateDisplay();
  }

  // Manage relay based on timers
  controlRelay();

  // Check for stand detection and auto power-off logic
  checkDetectPin();
}

void adjustTimer(long position) {
  long relativePosition = position - lastEncoderPosition;

  // Adjust On-time or Off-time based on current selection
  if (currentSelection == 0) {
    tempOnTime = constrain(tempOnTime + relativePosition, 0, 300);
  } else if (currentSelection == 1) {
    tempOffTime = constrain(tempOffTime + relativePosition, 0, 300);
  }

  lastEncoderPosition = position;
}

void saveTimerValues() {
  int storedOnTime, storedOffTime;
  EEPROM.get(ON_TIME_ADDRESS, storedOnTime);
  EEPROM.get(OFF_TIME_ADDRESS, storedOffTime);

  // Write to EEPROM only if the value has changed
  if (storedOnTime != onTime) EEPROM.put(ON_TIME_ADDRESS, onTime);
  if (storedOffTime != offTime) EEPROM.put(OFF_TIME_ADDRESS, offTime);
}

void controlRelay() {
  static bool relayOn = true;

  // If both timers are zero, keep the relay off permanently
  if (onTime == 0 && offTime == 0) {
    digitalWrite(RELAY_PIN, HIGH);  // Relay OFF
    relayOn = false;
    lcd.setCursor(15, 0);
    lcd.print("F");  // Display 'F' indicating relay OFF
    return;
  }

  // Manage relay ON/OFF based on On-time and Off-time
  if (relayOn) {
    if (millis() - onTimerStart >= (unsigned long)onTime * 1000) {
      digitalWrite(RELAY_PIN, HIGH);  // Turn OFF relay
      relayOn = false;
      offTimerStart = millis();       // Start off-timer
    }
  } else {
    if (millis() - offTimerStart >= (unsigned long)offTime * 1000) {
      digitalWrite(RELAY_PIN, LOW);   // Turn ON relay
      relayOn = true;
      onTimerStart = millis();        // Start on-timer
    }
  }

  // Display relay status (T = ON, F = OFF)
  lcd.setCursor(15, 1);
  lcd.print(relayOn ? "T" : "F");
}

void checkDetectPin() {
  // Check if the soldering iron is placed on the stand (pin pulled LOW)
  if (debouncer_2.read() == LOW) {
    lcd.setCursor(14, 0);
    lcd.print(" D");  // Display 'D' for detection

    // Start the timer when stand is detected
    if (groundPullTimeStart == 0) {
      groundPullTimeStart = millis();
    }

    // Auto turn-off relay after 3 minutes on the stand
    if (millis() - groundPullTimeStart >= TIME_LIMIT) {
      digitalWrite(RELAY_PIN, HIGH);  // Turn OFF relay
    }
  } else {
    // Reset timer when iron is not on the stand
    groundPullTimeStart = 0;
    lcd.setCursor(14, 0);
    lcd.print("ND");  // Display 'ND' (Not Detected)
  }
}

void updateDisplay() {
  lcd.clear();
  
  // Display On-time with current value or temporary adjustment
  lcd.setCursor(1, 0);
  lcd.print("On-time:");
  lcd.print(formatTime(adjusting && currentSelection == 0 ? tempOnTime : onTime));
  lcd.setCursor(13, 0);
  lcd.print(",");

  // Display Off-time with current value or temporary adjustment
  lcd.setCursor(1, 1);
  lcd.print("Off-time:");
  lcd.print(formatTime(adjusting && currentSelection == 1 ? tempOffTime : offTime));
  lcd.setCursor(14, 1);
  lcd.print(",");

  // Show selection indicator (">" points to the current setting)
  lcd.setCursor(0, currentSelection);
  lcd.print(">");
}

String formatTime(int time) {
  int minutes = time / 60;
  int seconds = time % 60;

  // Format time as MM:SS (e.g., 1:05)
  String formattedTime = String(minutes) + ":";
  if (seconds < 10) formattedTime += "0";
  formattedTime += String(seconds);

  return formattedTime;
}
