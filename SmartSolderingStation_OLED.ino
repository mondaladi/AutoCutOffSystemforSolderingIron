#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <Encoder.h>
#include <Bounce2.h>

// OLED settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define RELAY_PIN 5
#define DETECT_PIN 6

// Encoder
Encoder myEnc(2, 3);
Bounce debouncer_1 = Bounce();
int encoderButtonPin = 4;

// Ground detection debounce
Bounce debouncer_2 = Bounce();

// Time settings
int onTime = 60;
int offTime = 60;
int sleepTime = 5 * 60; // default 5 mins in seconds

int currentSelection = 0; // 0 = ON, 1 = OFF, 2 = SLEEP

int tempOnTime = 60;
int tempOffTime = 60;
int tempSleepTime = 300;

static bool relayOn = true;
static bool detected = true;
bool adjusting = false;
bool displayOff = false;
bool sleepTriggered = false;
long lastEncoderPosition = 0;

#define ON_TIME_ADDRESS     0
#define OFF_TIME_ADDRESS    4
#define SLEEP_TIME_ADDRESS  8

unsigned long onTimerStart = 0;
unsigned long offTimerStart = 0;
unsigned long groundPullTimeStart = 0;

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  pinMode(encoderButtonPin, INPUT_PULLUP);
  debouncer_1.attach(encoderButtonPin);
  debouncer_1.interval(50);

  pinMode(DETECT_PIN, INPUT_PULLUP);
  debouncer_2.attach(DETECT_PIN);
  debouncer_2.interval(150);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true);  // Hang if OLED not found
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 15);
  display.println("SolderSync");
  display.setCursor(50, 35);
  display.println("V2");
  display.display();
  delay(3000);

  EEPROM.get(ON_TIME_ADDRESS, onTime);
  EEPROM.get(OFF_TIME_ADDRESS, offTime);
  EEPROM.get(SLEEP_TIME_ADDRESS, sleepTime);

  // validate and reset if necessary
  if (onTime > 300 || offTime > 300) {
    onTime = 60;
    offTime = 60;
  }

  if (sleepTime < 60 || sleepTime > 1800) {
    sleepTime = 300; // reset to 5 minutes
  }

  tempOnTime = onTime;
  tempOffTime = offTime;
  tempSleepTime = sleepTime;

  saveTimerValues();
  updateDisplay();
}

void loop() {
  static long oldPosition = -999;
  long newPosition = myEnc.read() / 8;

  debouncer_1.update();
  debouncer_2.update();

  // Wake display on encoder movement
  if (displayOff && newPosition != oldPosition) {
    displayOff = false;
    display.ssd1306_command(SSD1306_DISPLAYON);
    updateDisplay();  // Refresh display content
  }

  if (debouncer_1.fell()) {
    if (!adjusting) {
      adjusting = true;
      lastEncoderPosition = newPosition;
    } else {
      adjusting = false;

      if (currentSelection == 0)
        onTime = tempOnTime;
      else if (currentSelection == 1)
        offTime = tempOffTime;
      else if (currentSelection == 2)
        sleepTime = tempSleepTime;

      saveTimerValues();
    }
    updateDisplay();
  }

  if (adjusting && newPosition != oldPosition) {
    adjustTimer(newPosition);
    oldPosition = newPosition;
    updateDisplay();
  } else if (!adjusting && newPosition != oldPosition) {
    currentSelection = newPosition % 3;
    lastEncoderPosition = myEnc.read() / 8;
    oldPosition = newPosition;
    updateDisplay();
  }

  controlRelay();
  checkDetectPin();
  updateDisplay();
}

void adjustTimer(long position) {
  long relativePosition = position - lastEncoderPosition;
  if (currentSelection == 0) {
    tempOnTime = constrain(tempOnTime + relativePosition, 0, 300);
  } else if (currentSelection == 1) {
    tempOffTime = constrain(tempOffTime + relativePosition, 0, 300);
  } else if (currentSelection == 2) {
    tempSleepTime = constrain(tempSleepTime + (relativePosition * 60), 60, 1800); // adjust in minutes
  }
  lastEncoderPosition = position;
}

void saveTimerValues() {
  int storedOnTime, storedOffTime, storedSleepTime;
  EEPROM.get(ON_TIME_ADDRESS, storedOnTime);
  EEPROM.get(OFF_TIME_ADDRESS, storedOffTime);
  EEPROM.get(SLEEP_TIME_ADDRESS, storedSleepTime);

  if (storedOnTime != onTime) EEPROM.put(ON_TIME_ADDRESS, onTime);
  if (storedOffTime != offTime) EEPROM.put(OFF_TIME_ADDRESS, offTime);
  if (storedSleepTime != sleepTime) EEPROM.put(SLEEP_TIME_ADDRESS, sleepTime);
}

void controlRelay() {
  if (onTime == 0 && offTime == 0) {
    digitalWrite(RELAY_PIN, HIGH);
    relayOn = false;
    return;
  }

  if (relayOn) {
    if (millis() - onTimerStart >= (unsigned long)onTime * 1000) {
      digitalWrite(RELAY_PIN, HIGH);
      relayOn = false;
      offTimerStart = millis();
    }
  } else {
    if (millis() - offTimerStart >= (unsigned long)offTime * 1000) {
      digitalWrite(RELAY_PIN, LOW);
      relayOn = true;
      onTimerStart = millis();
    }
  }
}

void checkDetectPin() {
  if (debouncer_2.read() == LOW) {
    detected = true;
    if (groundPullTimeStart == 0) {
      groundPullTimeStart = millis();
    }

    if (millis() - groundPullTimeStart >= (unsigned long)sleepTime * 1000) {
      digitalWrite(RELAY_PIN, HIGH);

      if (!displayOff) {
        displayOff = true;
        sleepTriggered = true;
        display.ssd1306_command(SSD1306_DISPLAYOFF);
      }
    }
  } else {
    detected = false;

    // Wake up if sleep was triggered
    if (displayOff && sleepTriggered) {
      displayOff = false;
      sleepTriggered = false;
      display.ssd1306_command(SSD1306_DISPLAYON);
      updateDisplay();
    }

    groundPullTimeStart = 0;
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);

  display.setCursor(10, 5);
  display.print((currentSelection == 0) ? ">" : " ");
  display.print(" ON: ");
  display.println(formatTime(adjusting && currentSelection == 0 ? tempOnTime : onTime));

  display.setCursor(10, 15);
  display.print((currentSelection == 1) ? ">" : " ");
  display.print(" OFF: ");
  display.println(formatTime(adjusting && currentSelection == 1 ? tempOffTime : offTime));

  display.setCursor(10, 25);
  display.print((currentSelection == 2) ? ">" : " ");
  display.print(" SLEEP: ");
  display.println(formatTime(adjusting && currentSelection == 2 ? tempSleepTime : sleepTime));

  display.setCursor(10, 40);
  display.print("Relay: ");
  display.println(relayOn == true ? "ON" : "OFF");

  display.setCursor(10, 50);
  display.print("Iron in Stand: ");
  display.println(detected == true ? "YES" : "NO");

  display.display();
}

String formatTime(int time) {
  int minutes = time / 60;
  int seconds = time % 60;
  String formattedTime = String(minutes) + ":";
  if (seconds < 10) formattedTime += "0";
  formattedTime += String(seconds);
  return formattedTime;
}
