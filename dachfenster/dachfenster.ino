#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Stepper.h>
#include <SoftwareSerial.h>
 
#define STEPS_PER_REV 2048
 
Stepper stepper(STEPS_PER_REV, 3, 5, 4, 6);
 
LiquidCrystal_I2C lcd(0x27, 16, 2);
 
const int tempPin = A0;
const int buttonPin = 2;
 
unsigned long lastLogTime = 0;
const unsigned long LOG_INTERVAL = 600000;
 
float openTemp = 30.0;
float closeTemp = 28.0;
 
bool windowOpen = false;
 
float tempReadings[10];
int readingIndex = 0;
int readingCount = 0;
unsigned long lastLCDUpdate = 0;
const unsigned long LCD_UPDATE_INTERVAL = 2000;
 
void setup() {
 
  Serial.begin(9600);
 
  pinMode(buttonPin, INPUT_PULLUP);
 
  lcd.init();
  lcd.backlight();
 
  stepper.setSpeed(5);
 
  lcd.setCursor(0,0);
  lcd.print("System Start...");
  delay(2000);
 
  Serial.println("Time(ms),Temperature(C),WindowState,OpenThreshold,CloseThreshold");
}
 
void loop() {
 
  float voltage = analogRead(tempPin) * (5.0 / 1023.0);
  float temperature = (voltage - 0.5) * 100.0;
 
  tempReadings[readingIndex] = temperature;
  readingIndex = (readingIndex + 1) % 10;
  if (readingCount < 10) {
    readingCount++;
  }
 
  unsigned long currentTime = millis();
 
  if (currentTime - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
    float sum = 0;
    for (int i = 0; i < readingCount; i++) {
      sum += tempReadings[i];
    }
    float avgTemperature = sum / readingCount;
 
    lcd.setCursor(0,0);
    lcd.print("T:");
    lcd.print(avgTemperature, 1);
    lcd.print("C O:");
    lcd.print(openTemp, 0);
    lcd.print("C  ");
 
    lcd.setCursor(0,1);
    if (windowOpen) {
      lcd.print("Window: OPEN    ");
    } else {
      lcd.print("Window: CLOSED  ");
    }
 
    lastLCDUpdate = currentTime;
  }
 
  if (temperature >= openTemp && !windowOpen) {
    openWindow();
    windowOpen = true;
  }
 
  if (temperature <= closeTemp && windowOpen) {
    closeWindow();
    windowOpen = false;
  }
 
  logTemperature(temperature);
 
  if (digitalRead(buttonPin) == LOW) {
    delay(50);
    if (digitalRead(buttonPin) == LOW) {
      while (digitalRead(buttonPin) == LOW);
      Serial.println("---MANUAL RESET---");
      lcd.setCursor(0,1);
      lcd.print("Manual Reset!   ");
      delay(1000);
    }
  }
 
  delay(1000);
}
 
void openWindow() {
  lcd.setCursor(0,1);
  lcd.print("Opening...     ");
  stepper.step(1024);
}
 
void closeWindow() {
  lcd.setCursor(0,1);
  lcd.print("Closing...     ");
  stepper.step(-1024);
}
 
void logTemperature(float temp) {
  unsigned long currentTime = millis();
  if (currentTime - lastLogTime >= LOG_INTERVAL) {
    Serial.print(currentTime);
    Serial.print(",");
    Serial.print(temp, 1);
    Serial.print(",");
    Serial.print(windowOpen ? "OPEN" : "CLOSED");
    Serial.print(",");
    Serial.print(openTemp);
    Serial.print(",");
    Serial.println(closeTemp);
    lastLogTime = currentTime;
  }
}
