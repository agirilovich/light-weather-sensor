#include <Arduino.h>

#include <IWatchdog.h>
#include <STM32LowPower.h>

#include "sensors.h"
#include "radio.h"

#ifndef DEVICE_BOARD_NAME
#  define DEVICE_BOARD_NAME "STM32OutdoorSensor"
#endif

#define LED_PIN PC13

void SensorTransmit() {
  digitalWrite(LED_PIN, LOW);
  
  Serial.println("Transmit data via radio");
  LaCrosseTransmit();

  digitalWrite(LED_PIN, HIGH);
 
}

void setup() {
  // Debug console
  Serial.begin(115200);
 
  while (!Serial && millis() < 5000);

 // Debug console
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  while (!Serial && millis() < 5000);

  if (IWatchdog.isReset()) {
    Serial.printf("Rebooted by Watchdog!\n");
    delay(60 * 1000);
    IWatchdog.clearReset(); 
  }

  //Set witchdog timeout for 32 seconds
  IWatchdog.begin(32000000); // set to maximum value
  IWatchdog.reload();

  while (!IWatchdog.isEnabled()) {
    // LED blinks indefinitely
    digitalWrite(LED_PIN, LOW);
    delay(500);
    digitalWrite(LED_PIN, HIGH);
    delay(500);
  }
  
  delay(100);

  Serial.print("Setting up Hardware Timer for Transmitting Sensors  values with period: ");
  TIM_TypeDef *SensorsTimerInstance = TIM3;
  HardwareTimer *SensorsThread = new HardwareTimer(SensorsTimerInstance);
  SensorsThread->pause();
  SensorsThread->setPrescaleFactor(65536);
  //SensorsThread->setOverflow(4294967295);
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

  LowPower.begin();

}

void loop() {
  IWatchdog.reload();
  if (!ActualData.battery)
  {
    LowPower.deepSleep(28 * 1000);
  } else {
    LowPower.sleep();
  }
}
