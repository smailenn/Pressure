/*
Project info:
ESP 32 Processor
ADS1115 16 bit = 2^16 = 65536 bits/counts
.256v / 32768 bits = 7.81 uV/bit (using 16x gain)
Pressure gauge TE Internal #: M3021-000005-05KPG
TE Internal Description: PRESS XDCR M3021-000005-05KPG
0 - 5000 psi, analog I2C via ADS1115, 0-0.1 V ouput
*/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_ADS1X15.h>
#include "config.h"
#include "SdFat.h"
#include "RTClib.h"

// SD and Adafruit IO On/Off
bool SDstatus = 1; // ON/OFF for SD CARD Record
bool IOstatus = 1; // ON/OFF for Adafruit IO

// OLED Screen
Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);
Adafruit_ADS1115 ads1115;  // Construct an ads1115

// RTC
RTC_PCF8523 rtc;

// Button Setup
#define BUTTON_A  15   //#9 for M0 Express
#define BUTTON_B  32   //#6 
#define BUTTON_C  14   //#5
#define VBATPIN A13 //For battery voltage/life

// Pressure Sensor
int calibration = 82; //sensor calibration value
int bit_results; //bit results from adc
float uV_results;
int pressure;
int Max;
int Min;
float hz = 10; //samplers per second
int timer = 2500; //1000 / hz; //For Datalogging timing
unsigned long previousMillis = 0;  //store last time something happened
unsigned long currentMillis; //for time logging
unsigned long debpreviousMillis = 0; //for debounce timing
unsigned long debcurrentMillis; //for debounce 
int debounce = 300; 
int DataState = 0; // For recording
int LastDataState = 0; //For recording
int DisplayScreen = 1; // Turn on/off screen 

// SD card
#define cardSelect 33 //GPIO for adalogger and ESP32
SdFat sd;
SdFile file;
#define FILE_BASE_NAME "PRESS" // Log file base name.  Must be six characters or less.

// AdafruitIO Feed/s
AdafruitIO_Feed *Pressure = io.feed("Pressure");

void setup() {
  Serial.begin(115200);
  while (! Serial); //wait for serial monitor to open
  Serial.println("128x64 OLED FeatherWing test");
  display.begin(0x3C, true); // Address 0x3C default
  Serial.println("OLED begun");

  // RTC setup
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    //Serial.flush();
    while (1) delay(10);
  }  

  //if (! rtc.initialized() || rtc.lostPower()) {
    //Serial.println("RTC is NOT initialized, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    //rtc.start(); 
    
  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();
  delay(1000);
  // Clear the buffer.
  display.clearDisplay();
  display.display();
  display.setRotation(1);
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  ads1115.begin();  // Initialize ads1115
  ads1115.setGain(GAIN_SIXTEEN); // 16x gain  +/- 0.256 v

  //Display data about gauge status and initilization
  display.setCursor(0, 0);
  display.println("PLogger  Version 1.0");
  //display.println("2/10/2022");
  display.println("Created by Mailman");
  display.print("Cal(bit) = ");
  display.println(calibration);
  DateTime now = rtc.now();
  //display.print("Time = ");
  display.print(now.month(), DEC);
  display.print('/');
  display.print(now.day(), DEC);
  display.print('/');
  display.print(now.year(), DEC);
  display.print(" ");
  display.print(now.hour(), DEC);
  display.print(':');
  display.print(now.minute(), DEC);
  display.print(':');
  display.println(now.second(), DEC);
  display.display();
  delay(2000);

  // SD Card Initialization:
  const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
  char fileName[13] = FILE_BASE_NAME "00.csv";
  
  if (SDstatus == 1) {
    // Initialize at the highest speed supported by the board that is
    // not over 50 MHz. Try a lower speed if SPI errors occur.
    if (!sd.begin(cardSelect, SD_SCK_MHZ(25))) {
      //sd.initErrorHalt(); 
      Serial.println("Card init. failed!");
      display.println("SD Card failed");
      display.display();
      delay(1000);
    }
    // Find an unused file name.
    if (BASE_NAME_SIZE > 6) {
      Serial.println("FILE_BASE_NAME too long");
    }
    while (sd.exists(fileName)) {
      if (fileName[BASE_NAME_SIZE + 1] != '9') {
        fileName[BASE_NAME_SIZE + 1]++;
      } else if (fileName[BASE_NAME_SIZE] != '9') {
        fileName[BASE_NAME_SIZE + 1] = '0';
        fileName[BASE_NAME_SIZE]++;
      } else {
        Serial.println("Can't create file name");
        display.print("Couldn't create ");
        display.println(fileName);
        display.display();
        delay(2000);
      }
    }
    if (!file.open(fileName, O_WRONLY | O_CREAT | O_EXCL)) {
      Serial.println("file.open");
    }
    // Read any Serial data.
    do {
      delay(10);
    } while (Serial.available() && Serial.read() >= 0);

    Serial.print(F("Logging to: "));
    Serial.println(fileName);
    display.print(fileName);
    display.println(" Created");
    display.display();
    delay(3000);

    //Header Info
    file.print("Time");
    file.print(','); 
    file.print("Pressure");
    file.print(','); 
    file.print("MIN");
    file.print(',');
    file.print("MAX");
    file.print(',');    
    file.println("Time mS");
  } 
   
  // connect to io.adafruit.com
  if (IOstatus == 1) {
    Serial.print("Connecting to Adafruit IO");
    io.connect();
    // wait for a connection
    while (io.status() < AIO_CONNECTED) {
      Serial.print(".");
      delay(500);
    }
    // we are connected
    Serial.println();
    Serial.println(io.statusText());
    display.println("Connected to Ada IO"); 
    display.display(); 
    delay(2000);
  }
}

void loop() {
  if (IOstatus == 1) {
    io.run(); //for io.adafruit.com, required to run IO for all sketches
  }

  // Button A for data recording on/off
  if (!digitalRead(BUTTON_A)) {
    debcurrentMillis = millis();
    Serial.println("Button A pressed");
    if (DataState == 0) {
      LastDataState = 0;
      DataState = 1; // start recording data
      Serial.println("Start Recording");
    }
    else {
      LastDataState = 1;
      DataState = 0; //stop recording data
        if (SDstatus == 1) {
          file.close();  // Close file
          Serial.println("Stop Recording");
          Serial.println("File closed");
          display.clearDisplay();
          display.display();
          display.setCursor(0, 0);
          display.println("Stop Recording");
          display.println("SD file closed");
          display.display();
          delay(2000);
        }
    }
    //Serial.print("DataState =");
    //Serial.println(DataState);
    //Serial.print("LastDataState =");
    //Serial.println(LastDataState);
    while (millis() - debcurrentMillis <= debounce) {
      //just waiting
    }
  }

  // Button B for calibration
  if (!digitalRead(BUTTON_B) && (DataState != 1)) {
    Serial.println("Button B pressed");
    Calibrate();
    delay(debounce); 
  }

  // Button C for data recording on/off
  if (!digitalRead(BUTTON_C)) {
    debcurrentMillis = millis();
    Serial.println("Button C pressed");
    if (DisplayScreen == 1) {
      DisplayScreen = 0; // turn off screen
      display.clearDisplay();
      display.display();
      }
    else {
      DisplayScreen = 1;
      }
    while (millis() - debcurrentMillis <= debounce) {
      //just waiting
    }
  }

  // Measure
  Measure();
}

void Calibrate () {
  //Calibrate sensor
  calibration = - ads1115.readADC_Differential_0_1();
  Serial.print("Calibrate Value = ");
  Serial.println(calibration);
  display.clearDisplay();
  display.display();
  display.setCursor(0, 0);
  display.print("Cal(bit) = ");
  display.println(calibration);
  display.print("P Cal (PSI) = ");
  display.println(calibration * 7.81 / 20); 
  display.display();
  delay(1500);
}

void Measure () {
  // Timer
  currentMillis = millis();

  if (currentMillis - previousMillis >= timer) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;  
    // Get Pressure
    // Measure ADC pressure sensor
    bit_results = ads1115.readADC_Differential_0_1() + calibration;
    uV_results = bit_results * 7.81;
    pressure = uV_results / 20;
    if (Min > pressure){
      Min = pressure;
    }
    if (Max < pressure){
      Max = pressure; 
    }
    DateTime now = rtc.now();
  
    //Battery voltage Reading
    float measuredvbat = analogRead(VBATPIN);
    measuredvbat *= 2;    // we divided by 2, so multiply back
    //measuredvbat *= 5;  // Multiply by 3.3V, our reference voltage (on Feather M0 express)
    measuredvbat /= 1024; // convert to voltage
  
    //Display data and info
    //Serial.print("Differential: ");
    //Serial.print(bit_results);
    //Serial.print("(");
    //Serial.print(uV_results);
    //Serial.print("uV)");
    //Serial.print("(");
    Serial.print("Time= ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.print(" , ");
    Serial.print(pressure);
    Serial.print("psi");
    Serial.print(" , "); 
    Serial.print("VBat: " );
    Serial.println(measuredvbat);

    if (DisplayScreen == 1){
      display.clearDisplay();
      display.display();
      display.setCursor(0, 0);
      display.print("P1(PSI)= ");
      display.println(pressure);
      display.print("P1 MIN/MAX=");
      display.print(Min);
      display.print("/");
      display.println(Max);
      display.print("Time= ");
      display.print(now.hour(), DEC);
      display.print(':');
      display.print(now.minute(), DEC);
      display.print(':');
      display.println(now.second(), DEC);
      display.print("Battery(V)= ");
      display.println(measuredvbat);
      if (DataState == 0){
        display.println("Not recording"); }
      if (DataState == 1){
        display.println("Recording!"); }
      display.display(); // send to OLED
    }
  
    // Record SD Card and/or Adafruit IO
    if (DataState == 1) {
      Record();
    }
  }
}

void Record () {
  // SD Card Record
  if (SDstatus == 1) { 
    DateTime now = rtc.now();
    file.print(now.hour(), DEC);
    file.print(':');
    file.print(now.minute(), DEC);
    file.print(':');
    file.print(now.second(), DEC);
    file.print(',');
    file.print(pressure);
    file.print(',');
    file.print(Min);
    file.print(',');
    file.print(Max);
    file.print(',');
    file.println(currentMillis); 
    file.flush();     
  
      // Force data to SD and update the directory entry to avoid data loss.
    if (!file.sync() || file.getWriteError()) {
      Serial.println("write error");
      display.println("Write error, not recording");
      display.display(); 
    }
  }

  if (IOstatus == 1) {
    //Adafruit IO send data to Feed
    Pressure->save(pressure);
  }

}
