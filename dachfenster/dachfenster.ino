#include <Wire.h>                 // I2C-Kommunikation (für Display)
#include <LiquidCrystal_I2C.h>    // LCD mit I2C-Modul
#include <Stepper.h>              // Schrittmotor
#include <SoftwareSerial.h>       // serielle Kommunikation
#include <OneWire.h>              // Digitale Kommunikation
#include <DallasTemperature.h>    // Digitaler Temperatur-Sensor

// Schrittmotor (Modell 28BYJ-48)
#define STEPS_PER_REV 2048                        // Schritte pro Umdrehung (28BYJ-48)
Stepper stepper(STEPS_PER_REV, 3, 5, 4, 6);       // Initialisiert den Stepper mit Pins

// LCD-Bildschirm
LiquidCrystal_I2C lcd(0x27, 16, 2);               // LCD-Adresse (0x27), 16 Zeichen, 2 Zeilen
unsigned long lastLCDUpdate = 0;                  // Zeitpunkt der letzten LCD-Aktualisierung
const unsigned int LCD_UPDATE_INTERVAL = 500;     // LCD-Update Intervall in ms

// Temperatur-Sensor (TMP36)
//const int TEMP_PIN = A0;                 // Temperatur-Sensor am analogen Pin A0

// Temperatur-Sensor (DS18B20)
#define TEMP_PIN 2                      // Temperatur-Sensor am digiralen Pin 0
OneWire oneWire(TEMP_PIN);              // Erstellt OneWire-Verbindung auf Pin D2
DallasTemperature sensors(&oneWire);    // Übergibt OneWire an DallasTemperature Bibliothek
const int INVALID_TEMP = -127.0;           // Fehlerwert des Temperatursensors 

// Temperatur-Werte
#define TEMP_VALUES 5                       // Anzahl der letzten berücksichtigten Temperaturwerte
float tempReadings[TEMP_VALUES];            // Array für die letzten Temperaturwerte
int readingIndex = 0;                       // Aktuelle Position im Array
int readingCount = 0;                       // Anzahl der gespeicherten Werte
bool validTemp;                             // gültigen Temperaturwert gelesen
const unsigned char READ_TEMP_TRIES = 5;    // Anzahl der Versuche einen gültigen Wert zu lesen

// Potentiometer
#define POT_PIN A0                                                        // Potentiometer Pin
int potValue;                                                             // Analoger Signalwert des Potentiometers (0-1023)
const float POT_MIN_VALUE = 15.0;                                         // minimal einstellbarer Temperaturwert
const float POT_MAX_VALUE = 45.0;                                         // maximal einstellbarer Temperaturwert
const float POT_UNITS_PER_VALUE = 1023/(POT_MAX_VALUE - POT_MIN_VALUE);   // analoger Bereich pro Temperaturwert 

// Fenster
float openTemp;                       // Temperatur, ab der das Fenster geöffnet wird
float closeTemp;                      // Temperatur, ab der das Fenster geschlossen wird
const float THRESHOLD_DIFF = 1.0;     // Wert, um den die Schließschwelle kleiner ist als die Öffnungsschwelle
bool windowOpen = false;              // Status des Fensters (offen/geschlossen)
 
void setup() {
 
  // Konsole
  Serial.begin(9600);               // Startet serielle Kommunikation (Baud-Rate)
  delay(1000);
  Serial.println("Time(s) \tTempAbsolut(C) \tTempAverage(C) \tWindowState \tPotentiometer \tOpenThreshold \tCloseThreshold");   // Header für Logging

  // Schrittmotor
  stepper.setSpeed(5);              // Setzt Drehgeschwindigkeit des Motors
 
  // LCD-Bildschirm
  lcd.init();                       // Initialisiert das LCD
  lcd.backlight();                  // Schaltet die Hintergrundbeleuchtung ein
  lcd.setCursor(0,0);               // Setzt Cursor auf erste Zeile
  lcd.print("System Start...");     // Startmeldung anzeigen
  delay(2000);                      // Anzeigen abwarten

  // Temperatur-Sensor initialisieren (DS18B20)
  sensors.begin();
  delay(1000);

  // zuverlässige Start-Temperatur bilden
  Serial.println("Prefilling temp buffer...");
  float temperature = 0;
  float avgTemperature = 0;
  for (int i = 0; i < TEMP_VALUES; i++) {
    temperature = measureTemperature();
    float avgTemperature;
    if (validTemp) {
      avgTemperature = calculateAverageTemperature();
    } else {
      avgTemperature = INVALID_TEMP;
    }
    logTemperature(temperature, avgTemperature);
    delay(1000 / TEMP_VALUES);                      // insgesamt 1 Sekunde warten
  }
  Serial.println("Finished filling temp buffer.");
}
 
void loop() {

  // Schwellwerte entsprechend des Potentiometers einstellen 
  setThresholds();
  
  // Temperatur messen und auf der Konsole ausgeben
  float temperature = measureTemperature();
  float avgTemperature;
  if (validTemp) {
    avgTemperature = calculateAverageTemperature();
  } else {
    avgTemperature = INVALID_TEMP;
  }
  logTemperature(temperature, avgTemperature);
 
  // Bildschirm aktualisieren
  unsigned long currentTime = millis();                         // Aktuelle Laufzeit seit Start
  if (currentTime - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {     // Prüft, ob LCD aktualisiert werden soll
    updateScreen(avgTemperature);
    lastLCDUpdate = currentTime;                                // Aktualisierungszeit merken
  }
  
  if (validTemp) { 
    
    // Fenster schließen/öffnen
    if (avgTemperature >= openTemp && !windowOpen) {            // Wenn zu warm und Fenster noch zu
      openWindow();                                             // Fenster öffnen
      windowOpen = true;                                        // Status setzen
    } else if (avgTemperature <= closeTemp && windowOpen) {     // Wenn wieder kühl genug und Fenster offen
      closeWindow();                                            // Fenster schließen
      windowOpen = false;                                       // Status setzen
    }
  }
 
  delay(500); // Pause (Loop-Zyklus)
}

void setThresholds() {
  potValue = analogRead(POT_PIN);                                         // Aktuellen Signalwert des Potentiometers auslesen
  
  float openTempRaw = POT_MIN_VALUE +(potValue / POT_UNITS_PER_VALUE);    // analogen Wert in Temperaturwert umwandeln
  openTemp = round(openTempRaw * 2.0) / 2.0;                              // runden, um Schwankungen abzudämpfen
  closeTemp = openTemp - THRESHOLD_DIFF;                                  // Schließschwelle ist immer um einen Toleranzwert niedriger als Öffnungsschwelle
}

float measureTemperature() {

  // Temperatur messen (analog)
  //float voltage = analogRead(TEMP_PIN) * (5.0 / 1023.0);     // Wandelt ADC-Wert in Spannung um (ADC = Analog-Digital-Converter)
  //float voltage = analogRead(TEMP_PIN) * (3.3 / 1023.0);     //falls an 3.3V angeschlossen
  //float temperature = (voltage - 0.5) * 100.0;              // Umrechnung Spannung → Temperatur

  float temperature = INVALID_TEMP;
  validTemp = false;
  for (int i = 0; i < READ_TEMP_TRIES; i++) {                 // versuche mehrfach eine gültige Temperatur zu messen

    // Temperatur messen (digital)
    sensors.requestTemperatures();                            // Startet eine neue Temperaturmessung
    temperature = sensors.getTempCByIndex(0);                 // Liest Temperatur vom ersten Sensor (Index 0)

    if (temperature != INVALID_TEMP) {                        // breche ab, falls Temperatur gültig
      validTemp = true;
      break;
    }
  }                           

  if (validTemp) {                                            // nur gültige Werte einbeziehen

    // Temperatur speichern im Ringspeicher
    tempReadings[readingIndex] = temperature;                 // Speichert aktuellen Wert im Array
    readingIndex = (readingIndex + 1) % TEMP_VALUES;          // Nächste Position (Ringpuffer)
    if (readingCount < TEMP_VALUES) {
      readingCount++;                                         // Erhöht Anzahl der gültigen Werte (max. TEMP_VALUES)
    }
  }

  return temperature;
}

// Temperatur-Durchschnitt bilden auf Basis der letzten Werte
float calculateAverageTemperature() {
  float sum = 0;
  for (int i = 0; i < readingCount; i++) {
    sum += tempReadings[i];                       // Summiert alle gespeicherten Werte
  }
  float avgTemperature = sum / readingCount;      // Berechnet Durchschnitt

  return avgTemperature;
}
 
void logTemperature(float temperature, float avgTemperature) {
  
  // Temperatur auf Konsole ausgeben
  unsigned long currentTime = millis();               // aktuelle Zeit
  Serial.print(currentTime / 1000);                   // Zeit seit Start
  Serial.print("\t\t");
  Serial.print(temperature, 1);                       // Temperatur
  Serial.print("\t\t");
  Serial.print(avgTemperature, 1);                    // Temperatur
  Serial.print("\t\t");
  Serial.print(windowOpen ? "OPEN" : "CLOSED");       // Fensterstatus
  Serial.print("\t\t");
  Serial.print(potValue);                             // analoger Potentiometer-Wert
  Serial.print("\t\t");
  Serial.print(openTemp);                             // Öffnungsschwelle
  Serial.print("\t\t");
  Serial.print(closeTemp);                            // Schließschwelle
  Serial.println();
}

// den Bildschirm aktualisieren
void updateScreen(float avgTemperature) {
  lcd.setCursor(0,0);
  lcd.print("T:");
  if (validTemp) {
    lcd.print(avgTemperature, 1);               // Zeigt Durchschnittstemperatur mit 1 Nachkommastelle
    lcd.print("C O:");
    lcd.print(openTemp, 1);                     // Zeigt Öffnungs-Temperatur
    lcd.print("C  ");
  } else {
    lcd.print("ERROR");                         // Messung eines ungültigen Werts bei elektronischem Fehler (Wackelkontakt)
  }

  lcd.setCursor(0,1);
  if (windowOpen) {
    lcd.print("Window: OPEN    ");              // Anzeige wenn offen
  } else {
    lcd.print("Window: CLOSED  ");              // Anzeige wenn geschlossen
  }
}
 
void openWindow() {
  lcd.setCursor(0,1);
  lcd.print("Opening...     ");             // Anzeige beim Öffnen
  Serial.println("Opening...");
  stepper.step(512);                        // Motor dreht vorwärts (halbe Umdrehung)
}
 
void closeWindow() {
  lcd.setCursor(0,1);
  lcd.print("Closing...     ");             // Anzeige beim Schließen
  Serial.println("Closing...");
  stepper.step(-512);                       // Motor dreht rückwärts
}

void shutdownRoutine() {
  closeWindow();                            // Motor ansteuern
  lcd.clear();                              // Display aus
  Serial.println("System safe shutdown");
}