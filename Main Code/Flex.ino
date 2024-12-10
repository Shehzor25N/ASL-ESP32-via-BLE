#include "FlexLibrary.h"
 #include <SPI.h>
  #include <Wire.h>
#include <Adafruit_BusIO_Register.h>
#include <FS.h>
 #include "SPIFFS.h" // ESP32 only




#define VCC 5.0        // Supply voltage
#define R_DIV 10000.0  // Fixed resistor value in the voltage divider (10k ohms)
#define ADC_MAX 4095.0 // Maximum ADC value for a 12-bit ADC

// Initialize an array of Flex sensors on specified analog pins
Flex flex[5] = {Flex(36), Flex(39), Flex(32), Flex(33), Flex(26)};

void setup() {
  Serial.begin(9600);

  // Calibrate all flex sensors
  for (int i = 0; i < 5; i++) {
    flex[i].Calibrate();
    Serial.print("Calibrating Flex Sensor ");
    Serial.println(i + 1);
  }

  Serial.println("Calibration Complete");
}

void loop() {
  // Update and process each flex sensor
  for (int i = 0; i < 5; i++) {
    flex[i].updateVal();

    // Read raw analog value
    float rawValue = flex[i].getSensorValue();

    // Convert raw analog value to voltage
    float voltage = rawValue * (VCC / ADC_MAX);

    // Calculate flex sensor resistance
    float resistance = R_DIV * (VCC - voltage) / voltage;

    // Print resistance to Serial Monitor
    Serial.print("Resistance ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(resistance);
    Serial.println(" ohms");
  }

  delay(1000); // Delay for 1 second
}
