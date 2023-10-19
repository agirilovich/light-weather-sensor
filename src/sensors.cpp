#include "sensors.h"

#ifndef DEVICE_BOARD_NAME
#  define DEVICE_BOARD_NAME "STM32OutdoorSensor"
#endif

struct SensorsData ActualData = {DEVICE_BOARD_NAME, 0, 0, 0, 0};

#include <Wire.h>
#include <SPI.h>
//#include <Adafruit_Sensor.h>
//#include <Adafruit_BME280.h>
//Adafruit_BME280 bme;
#include <forcedClimate.h>
ForcedClimate climateSensor = ForcedClimate(Wire, 0x76);


#include <BH1750.h>
BH1750 lightMeter(0x23);


void SensorsInit()
{
  // Initialize the I2C bus (BH1750 library doesn't do this automatically)
  Wire.begin();

  //Initialise light sensor
  Serial.println(F("Initialise BH1750"));
  if (lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE)) {
    Serial.println(F("Done"));
  } else {
    Serial.println(F("Error initialising BH1750"));
  }
  
  //initialise BME280 sensor on I2C bus
  Serial.println(F("Initialise BME280"));
  climateSensor.begin();
  Serial.println("Done");

}

void SensorsRead()
{
    climateSensor.takeForcedMeasurement();
    ActualData.temperature = climateSensor.getTemperatureCelcius();
    ActualData.humidity = climateSensor.getRelativeHumidity();
    ActualData.pressure = climateSensor.getPressure();
    Serial.print("Temperature: ");
    Serial.println(ActualData.temperature);
    Serial.print("Humidity: ");
    Serial.println(ActualData.humidity);
    Serial.print("Pressure: ");
    Serial.println(ActualData.pressure);

  while (!lightMeter.measurementReady(true)) {
    yield();
  }
  float light = lightMeter.readLightLevel();
  ActualData.light = light;
  Serial.print("Light: ");
  Serial.print(light);
  Serial.println(" lx");
  lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
}