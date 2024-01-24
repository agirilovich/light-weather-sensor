#include <Arduino.h>

#include <IWatchdog.h>

#include "sensors.h"

#include "radio.h"


#ifndef DEVICE_BOARD_NAME
#  define DEVICE_BOARD_NAME "STM32OutdoorSensor"
#endif

#define LED_PIN PC13

void SensorTransmit() {
  digitalWrite(LED_PIN, LOW);
  
  Serial.println("Transmit data via radio");
  RadioTransmit();

  digitalWrite(LED_PIN, HIGH);
 
}

void setup() {
  // Debug console
  Serial.begin(115200);
 
  while (!Serial && millis() < 5000);

  //Set witchdog timeout for 32 seconds
  IWatchdog.begin(32000000); // set to maximum value
  IWatchdog.reload();
  
  delay(100);
  Serial.print("Setting up Hardware Timer for Transmitting Sensors  values with period: ");
  TIM_TypeDef *SensorsTimerInstance = TIM3;
  HardwareTimer *SensorsThread = new HardwareTimer(SensorsTimerInstance);
  SensorsThread->pause();
  SensorsThread->setPrescaleFactor(65536);
  //SensorsThread->setOverflow(224288);
  Serial.print(SensorsThread->getOverflow() / (SensorsThread->getTimerClkFreq() / SensorsThread->getPrescaleFactor()));
  Serial.println(" sec");
  SensorsThread->refresh();
  SensorsThread->resume();
  SensorsThread->attachInterrupt(SensorTransmit);
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  //PowerUp modules during init procedure

  RadoInit();
  SensorsInit();

}

void loop() {
  delay(200);
  if ((ActualData.temperature < 100) && (ActualData.pressure < 2000) && (ActualData.light >= 0))
  {
    IWatchdog.reload();
  } 
  //LowPower.sleep(10000);
}
