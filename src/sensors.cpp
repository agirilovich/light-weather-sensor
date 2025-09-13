#include "sensors.h"

#ifndef DEVICE_BOARD_NAME
#  define DEVICE_BOARD_NAME "STM32OutdoorSensor"
#endif

struct SensorsData ActualData = {0, 0, 0, 0, false};

#include <Wire.h>

TwoWire Wire1(PB9, PB8);
//TwoWire Wire2(PB3, PB10);

//BMP390 Pressure and temperature sensor
#include <Adafruit_Sensor.h>
#include "Adafruit_BMP3XX.h"

Adafruit_BMP3XX bmp;

//SHT41 Temperature and Humudity sensor
#include <SensirionI2cSht4x.h>
SensirionI2cSht4x sht;


//Wind speed sensor
#define windpin PB5

//Battery charger sensor
#define chargerpin PB1


uint32_t channel;
volatile uint32_t FrequencyMeasured, LastCapture = 0, CurrentCapture;
uint32_t input_freq = 0;
volatile uint32_t rolloverCompareCount = 0;
HardwareTimer *MyWindTim;

#include <RunningAverage.h>
#define ArrayLenght 20

RunningAverage Temperature1Array(ArrayLenght);
RunningAverage Temperature2Array(ArrayLenght);
RunningAverage HumidityArray(ArrayLenght);
RunningAverage PressureArray(ArrayLenght);

void InputCapture_Wind_callback(void)
{
  CurrentCapture = MyWindTim->getCaptureCompare(channel);
  /* frequency computation */
  if (CurrentCapture > LastCapture) {
    FrequencyMeasured = input_freq / (CurrentCapture - LastCapture);
  }
  else if (CurrentCapture <= LastCapture) {
    /* 0x1000 is max overflow value */
    FrequencyMeasured = input_freq / (0x10000 + CurrentCapture - LastCapture);
  }
  LastCapture = CurrentCapture;
  rolloverCompareCount = 0;
}

/* In case of timer rollover, frequency is to low to be measured set value to 0
   To reduce minimum frequency, it is possible to increase prescaler. But this is at a cost of precision. */
void Rollover_IT_callback(void)
{
  rolloverCompareCount++;

  if (rolloverCompareCount > 1)
  {
    FrequencyMeasured = 0;
  }
}


void SensorsInit()
{
  // Initialize the I2C bus
  Wire1.begin();
  //Wire2.begin();

  
  Serial.print("Setting up Hardware Timer for Reading Wind Speed Sensors: ");
  TIM_TypeDef *TimerWindInstance = TIM5;
  channel = STM_PIN_CHANNEL(pinmap_function(digitalPinToPinName(windpin), PinMap_PWM));
  HardwareTimer *TimerWindThread = new HardwareTimer(TimerWindInstance);
  TimerWindThread->setMode(channel, TIMER_INPUT_CAPTURE_RISING, windpin);
  TimerWindThread->pause();
  TimerWindThread->setPrescaleFactor(65536);
  TimerWindThread->setOverflow(0x10000);
  TimerWindThread->attachInterrupt(channel, InputCapture_Wind_callback);
  TimerWindThread->attachInterrupt(Rollover_IT_callback);
  Serial.print(TimerWindThread->getOverflow() / (TimerWindThread->getTimerClkFreq() / TimerWindThread->getPrescaleFactor()));
  Serial.println(" sec");
  TimerWindThread->refresh();
  TimerWindThread->resume();
  
  

  //Initialise BME390 sensor on I2C bus
  Serial.println(F("Initialise BME390... "));
  if (! bmp.begin_I2C(0x77, &Wire1)) {  
    Serial.println("Could not find a valid BMP390 sensor, check wiring!");
    while (1);
  }
  Serial.println("Done");

  Serial.print("Configuring BME390...  ");
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP3_ODR_25_HZ);
  Serial.println("Done");


  //Initialise SHT41 sensor
  Serial.print(F("Initialise SHT41....   "));
  sht.begin(Wire1, SHT41_I2C_ADDR_44);
  Serial.println(F("Done"));

  pinMode(chargerpin, INPUT_PULLUP);

  Temperature1Array.clear();
  Temperature2Array.clear();
  HumidityArray.clear();
  PressureArray.clear();
}



void WeatherSensorRead()
{
  Serial.println("Reading sensors...");
  if (! bmp.performReading()) {
    Serial.println("Failed to perform reading BMP390");
    return;
  }

  Temperature1Array.addValue(bmp.temperature);
  PressureArray.addValue(bmp.pressure / 100);

  float aTemperature = 0.0;
  float aHumidity = 0.0;
  sht.measureLowestPrecision(aTemperature, aHumidity);

  Temperature2Array.addValue(aTemperature);
  HumidityArray.addValue(aHumidity);

  ActualData.battery = digitalRead(chargerpin);

  ActualData.temperature = (Temperature1Array.getAverage() + Temperature2Array.getAverage()) / 2;
  ActualData.humidity = HumidityArray.getAverage();
  ActualData.pressure = PressureArray.getAverage();

  /*
  Serial.println("-------------------------------------------------------------------");
  Serial.print("Temperature: ");
  Serial.println(ActualData.temperature);
  Serial.print("Humidity: ");
  Serial.println(ActualData.humidity);
  Serial.print("Pressure: ");
  Serial.println(ActualData.pressure);
  Serial.print("Battery: ");
  Serial.println(ActualData.battery);
  Serial.println("-------------------------------------------------------------------");
  */
}