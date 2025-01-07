#include <Wire.h>
#include <HT_SSD1306Wire.h> // OLED library
#include <TimeLib.h>
#include <HX711.h>          // HX711 library

// low power libraries
#include "Arduino.h"
//#include "LoRa_APP.h"

// low power constants
#define timetillsleep 10000
#define timetillwakeup 500
static TimerEvent_t sleep;
static TimerEvent_t wakeUp;
uint8_t lowpower=1;

// HX711 declarations
#define DOUT GPIO5
#define SCK GPIO6
HX711 scale;

float calibration_factor = 100.0;
float units;
float ounces;
float pounds;

// Button pins
int resetButton = GPIO2;
int menueButton1 = GPIO3;
int menueButton2 = GPIO4;
int menueButton3 = GPIO7;

// LED pins
int ledPin = GPIO8;
int flashLed = GPIO9;
int rgbLed1 = GPIO13;
int rgbLed2 = GPIO14;
int rgbLed3 = GPIO1;

// Constants and flags
int threshold = 20;
int maxValues = 5;
int flag = 0;

// Button state flags
volatile bool resetPressed = false;
volatile bool menuBackPressed = false;
volatile bool menuForwardPressed = false;
volatile bool menuConfirmPressed = false;

unsigned long debounceDelay = 50;
unsigned long lastDebounceReset = 0;
unsigned long lastDebounceMenuBack = 0;
unsigned long lastDebounceMenuForward = 0;
unsigned long lastDebounceMenuConfirm = 0;

// OLED setup
SSD1306Wire display(0x3c, 500000, SDA, SCL, GEOMETRY_128_64, GPIO10);
unsigned long lastOLEDUpdate = 0;
unsigned long oledUpdateInterval = 250; // 1/4-second interval for OLED update

// data logging struct
struct logEntry {
  unsigned long timestamp;
  float strain;
};

void displayBootScreen() {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_24);
    display.drawString(display.getWidth() / 2, 10, "JimCo");
    display.setFont(ArialMT_Plain_10);
    display.drawString(display.getWidth() / 2, 30, "WIDGETS & MORE!");
    display.display();
    delay(3000); // Display boot screen for 3 seconds
    display.clear();
}

void updateOLEDClock() {
    static unsigned long uptime = 0;
    uptime = millis() / 1000; // Calculate uptime in seconds
    int hours = uptime / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = uptime % 60;

    // Get strain gauge readings
    float strainUnits = scale.get_units(10); // Adjust number of samples as needed
    if (strainUnits < 0) strainUnits = 0.0;  // Ensure no negative values
    float strainOunces = strainUnits * 0.035274;


    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "Device Uptime:");
    display.drawString(0, 12, String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s");
    
    display.drawString(0, 24, "Strain Reading:");
    display.drawString(0, 36, String(strainUnits, 2) + " units");
    display.drawString(0, 48, String(strainOunces, 2) + " oz");

    for(int i = 0; i < flag && i < 5; i++) {
      display.drawString(80 + (i * 8), 0, "*");
    }

    display.display();
}

// low power functions
void onSleep()
{
  Serial.printf("Going into lowpower mode, %d ms later wake up.\r\n",timetillwakeup);
  lowpower=1;
  //timetillwakeup ms later wake up;
  TimerSetValue( &wakeUp, timetillwakeup );
  TimerStart( &wakeUp );
}
void onWakeUp()
{
  Serial.printf("Woke up, %d ms later into lowpower mode.\r\n",timetillsleep);
  lowpower=0;
  //timetillsleep ms later into lowpower mode;
  TimerSetValue( &sleep, timetillsleep );
  TimerStart( &sleep );
}

void setup() {
    Serial.begin(9600);
    Serial.println("HX711 Test");
    scale.begin(DOUT, SCK, 128);
    scale.set_scale(calibration_factor);
    scale.tare();
    delay(500);

    if (scale.is_ready()) {
        Serial.println("HX711 is ready.");
    } else {
        Serial.println("HX711 not found. Check wiring or pin configuration.");
        while (1);
    }

    // Initialize buttons with interrupts
    pinMode(resetButton, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(resetButton), handleResetPress, FALLING);
    pinMode(menueButton1, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(menueButton1), handleMenuBackPress, FALLING);
    pinMode(menueButton2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(menueButton2), handleMenuForwardPress, FALLING);
    pinMode(menueButton3, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(menueButton3), handleMenuConfirmPress, FALLING);

    // Initialize LEDs
    pinMode(ledPin, OUTPUT);
    pinMode(flashLed, OUTPUT);
    pinMode(rgbLed1, OUTPUT);
    pinMode(rgbLed2, OUTPUT);
    pinMode(rgbLed3, OUTPUT);

    // Initialize OLED
    display.init();
    displayBootScreen();

  // low power setup
  //Radio.Sleep( );
  TimerInit( &sleep, onSleep );
  TimerInit( &wakeUp, onWakeUp );
  onSleep();
}

void loop() {

  if(lowpower){
    lowPowerHandler();
  }

  // Non-blocking OLED updates
  if (millis() - lastOLEDUpdate >= oledUpdateInterval) {
    lastOLEDUpdate = millis();
    updateOLEDClock();
  }

  // Handle button presses
  if (resetPressed) {
    resetPressed = false;
    digitalWrite(ledPin, HIGH);
    Serial.println("Reset button pressed");
    scale.tare();
    flag = 0;
    delay(500);
    digitalWrite(ledPin, LOW);
  }

  if (menuBackPressed) {
    menuBackPressed = false;
    Serial.println("Menu Back");
    digitalWrite(rgbLed1, LOW);
    digitalWrite(rgbLed2, LOW);
    digitalWrite(rgbLed3, HIGH);
    delay(500);
    digitalWrite(rgbLed3, LOW);
  }

  if (menuForwardPressed) {
    menuForwardPressed = false;
    Serial.println("Menu Forward");
    digitalWrite(rgbLed1, HIGH);
    digitalWrite(rgbLed2, LOW);
    digitalWrite(rgbLed3, LOW);
    delay(500);
    digitalWrite(rgbLed1, LOW);
  }

  if (menuConfirmPressed) {
    menuConfirmPressed = false;
    Serial.println("Menu Confirm");
    digitalWrite(rgbLed1, LOW);
    digitalWrite(rgbLed2, HIGH);
    digitalWrite(rgbLed3, LOW);
    delay(500);
    digitalWrite(rgbLed2, LOW);
  }

  // Read HX711 scale values
  units = scale.get_units(10);
  ounces = units * 0.035274;

  if (ounces > threshold) {
    flag++;
    Serial.print("Value exceeded threshold!: ");
    Serial.print(ounces);
    Serial.println();
    if (flag > maxValues) {
      ledFlash();
      flag = 0;
    }
  }

  Serial.print("Ounces: ");
  Serial.println(ounces);

  if (Serial.available()) {
    char temp = Serial.read();
    if (temp == '+' || temp == 'a') {
      calibration_factor += 1;
    } else if (temp == '-' || temp == 'z') {
      calibration_factor -= 1;
    }
  }
}

void ledFlash() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(flashLed, HIGH);
    delay(500);
    digitalWrite(flashLed, LOW);
    delay(500);
  }
}

void handleResetPress() {
  if ((millis() - lastDebounceReset) > debounceDelay) {
    resetPressed = true;
    lastDebounceReset = millis();
  }
}

void handleMenuBackPress() {
  if ((millis() - lastDebounceMenuBack) > debounceDelay) {
    menuBackPressed = true;
    lastDebounceMenuBack = millis();
  }
}

void handleMenuForwardPress() {
  if ((millis() - lastDebounceMenuForward) > debounceDelay) {
    menuForwardPressed = true;
    lastDebounceMenuForward = millis();
  }
}

void handleMenuConfirmPress() {
  if ((millis() - lastDebounceMenuConfirm) > debounceDelay) {
    menuConfirmPressed = true;
    lastDebounceMenuConfirm = millis();
  }
}
