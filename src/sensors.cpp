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


//Battery charger sensor
#define chargerpin PB1


#include <RunningAverage.h>
#define ArrayLenght 20

RunningAverage Temperature1Array(ArrayLenght);
RunningAverage Temperature2Array(ArrayLenght);
RunningAverage HumidityArray(ArrayLenght);
RunningAverage PressureArray(ArrayLenght);

//Wind measure section
float windscaler = WIND_FREQ_KNOT_SCALER;

TIM_TypeDef *WindTimerInstance = TIM3;
HardwareTimer *WindTimerThread = new HardwareTimer(WindTimerInstance);

extern "C" {
  #include "stm32f4xx_hal.h"

  TIM_HandleTypeDef htim2;
  
  static void MX_TIM2_Init(void)
  {
    TIM_SlaveConfigTypeDef sSlaveConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 0;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 0xffff;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.RepetitionCounter = 0;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    {
      Error_Handler();
    }

    sSlaveConfig.SlaveMode = TIM_SLAVEMODE_EXTERNAL1;
    sSlaveConfig.InputTrigger = TIM_TS_ETRF;
    sSlaveConfig.TriggerPrescaler = TIM_TRIGGERPRESCALER_DIV1;
    sSlaveConfig.TriggerPolarity = TIM_TRIGGERPOLARITY_NONINVERTED;
    sSlaveConfig.TriggerFilter = 15;
    if (HAL_TIM_SlaveConfigSynchro(&htim2, &sSlaveConfig) != HAL_OK)
    {
      Error_Handler();
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
    {
      Error_Handler();
    }

    if (HAL_TIM_OC_Init(&htim2) != HAL_OK)
    {
      Error_Handler();
    }
  }

  static void Base_MspInit(void)
  {
    GPIO_InitTypeDef   GPIO_InitStruct;
    __TIM2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }
}

void SensorsInit()
{
  // Initialize the I2C bus
  Wire1.begin();
  //Wire2.begin();

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


  Serial.print("Setting up Hardware Timers for wind sensor");
  WindTimerThread->pause();
  WindTimerThread->setPrescaleFactor(32768);
  WindTimerThread->refresh();

  MX_TIM2_Init();
  Base_MspInit();

  HAL_TIM_Base_Start(&htim2);
  WindTimerThread->resume();
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
  
  WindTimerThread->pause();
  HAL_TIM_Base_Stop(&htim2);

  uint32_t current_cnt = __HAL_TIM_GET_COUNTER(&htim2);
  ActualData.wind = current_cnt * windscaler;

  WindTimerThread->refresh();
  HAL_TIM_GenerateEvent(&htim2, TIM_EVENTSOURCE_UPDATE);

  WindTimerThread->resume();
  HAL_TIM_Base_Start(&htim2);

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