#include "sensors.h"

#ifndef DEVICE_BOARD_NAME
#  define DEVICE_BOARD_NAME "STM32OutdoorSensor"
#endif

struct SensorsData ActualData = {0, 0, 0, 0};

#include <Wire.h>
#include <SPI.h>

TwoWire Wire2(PB9, PB8);
TwoWire Wire3(PB11, PB10);

//#include <Adafruit_Sensor.h>
//#include <Adafruit_BME280.h>
//Adafruit_BME280 bme;
#include <forcedClimate.h>
ForcedClimate climateSensor = ForcedClimate(Wire2, 0x76);


#include <BH1750.h>
BH1750 lightMeter;

#include <RunningAverage.h>
#define ArrayLenght 20
RunningAverage LightArray(ArrayLenght);
RunningAverage TemperatureArray(ArrayLenght);
RunningAverage HumidityArray(ArrayLenght);
RunningAverage PressureArray(ArrayLenght);


void SensorsInit()
{
  // Initialize the I2C bus (BH1750 library doesn't do this automatically)
  Wire2.begin();
  Wire3.begin();

  Serial.print("Setting up Hardware Timer for Reading Weather Sensors values with period: ");
  TIM_TypeDef *WeatherSensorTimerInstance = TIM4;
  HardwareTimer *WeatherSensorThread = new HardwareTimer(WeatherSensorTimerInstance);
  WeatherSensorThread->pause();
  WeatherSensorThread->setPrescaleFactor(65536);
  WeatherSensorThread->setOverflow(8036);
  Serial.print(WeatherSensorThread->getOverflow() / (WeatherSensorThread->getTimerClkFreq() / WeatherSensorThread->getPrescaleFactor()));
  Serial.println(" sec");
  WeatherSensorThread->refresh();
  WeatherSensorThread->resume();
  WeatherSensorThread->attachInterrupt(WeatherSensorRead);

  //initialise BME280 sensor on I2C bus
  Serial.println(F("Initialise BME280"));
  climateSensor.begin();
  Serial.println("Done");


  //Initialise light sensor
  Serial.println(F("Initialise BH1750"));
  if (lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE, 0x23, &Wire3)) {
    Serial.println(F("Done"));
  } else {
    Serial.println(F("Error initialising BH1750"));
  }

  LightArray.clear();
  TemperatureArray.clear();
  HumidityArray.clear();
  PressureArray.clear();
}



void WeatherSensorRead()
{
  climateSensor.takeForcedMeasurement();

  TemperatureArray.addValue(climateSensor.getTemperatureCelcius());
  HumidityArray.addValue(climateSensor.getRelativeHumidity());
  PressureArray.addValue(climateSensor.getPressure());

  ActualData.temperature = TemperatureArray.getAverage();
  ActualData.humidity = HumidityArray.getAverage();
  ActualData.pressure = PressureArray.getAverage();


  Serial.print("Temperature: ");
  Serial.println(ActualData.temperature);
  Serial.print("Humidity: ");
  Serial.println(ActualData.humidity);
  Serial.print("Pressure: ");
  Serial.println(ActualData.pressure);

  lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
  while (!lightMeter.measurementReady(true)) {
    yield();
  }
  float light = lightMeter.readLightLevel();

  LightArray.addValue(light);
  ActualData.light =  LightArray.getAverage();
  Serial.print("Light: ");
  Serial.print(light);
  Serial.println(" lx");
}