#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_ADS1X15.h>

Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);
Adafruit_ADS1115 ads1115;  // Construct an ads1115 

#define BUTTON_A  9
#define BUTTON_B  6
#define BUTTON_C  5

// Pressure Sensor 
int SensorPmax = 5000; //5000 psi
int calibration; //sensor calibration value 
int16_t bit_results; //bit results from adc 
float uV_results;
float pressure; 

#define VBATPIN A7 //For battery voltage/life

void setup() {
  Serial.begin(115200);
  Serial.println("128x64 OLED FeatherWing test");
  display.begin(0x3C, true); // Address 0x3C default
  Serial.println("OLED begun");
  
  ads1115.begin();  // Initialize ads1115
  ads1115.setGain(GAIN_SIXTEEN); // 16x gain  +/- 0.256 v
  // ads1115 = 16 bit = 2^16 = 65536 bits/counts
  // .256v / 32768 bits = 7.81 uV/bit 

  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();
  delay(1000);

  // Clear the buffer.
  display.clearDisplay();
  display.display();

  display.setRotation(1);
  //Serial.println("Button test");

  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);
/*
  // text display tests
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println("Pressure=");
  display.println("Time");
  display.display(); // actually display all of the above
*/
}

void loop() {
  int time = millis() / 1000; 
  //Serial.println(time);

  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  

  bit_results = ads1115.readADC_Differential_0_1() + calibration;
  uV_results = bit_results * 7.81;
  pressure = uV_results / 20; 

  Serial.print("Differential: "); 
  Serial.print(bit_results); 
  Serial.print("("); 
  Serial.print(uV_results); 
  Serial.print("uV)");
  Serial.print("(");
  Serial.print(pressure);
  Serial.println("psi)");
  Serial.print("VBat: " ); Serial.println(measuredvbat);
  
  display.clearDisplay();
  display.display();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.print("Diff = ");
  display.print(bit_results);
  display.println("  bits");
  display.print("Voltage = ");
  display.print(uV_results);
  display.println(" uV"); 
  display.print("Pressure = ");
  display.print(pressure);
  display.println(" PSI"); 
  display.print("Time = ");
  display.print(time);
  display.println(" Secs");
  display.print("Battery = ");
  display.print(measuredvbat);
  display.print(" V"); 
  display.display(); // actually display all of the above
  
  delay(1000); 
  //if(!digitalRead(BUTTON_A)) display.print("A");
  //if(!digitalRead(BUTTON_B)) display.print("B");
  //if(!digitalRead(BUTTON_C)) display.print("C");
  //delay(10);
  //yield();
  //display.display();
}
