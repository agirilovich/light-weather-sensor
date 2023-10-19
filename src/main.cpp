#include <Arduino.h>

#include <IWatchdog.h>

#include "sensors.h"

#include "radio.h"

#ifndef DEVICE_BOARD_NAME
#  define DEVICE_BOARD_NAME "STM32OutdoorSensor"
#endif

#define LED_PIN PC13

#define POWERUP_PIN PA7

void SensorsCallback();

//char SensorCharMsg[256]; //message for transmit over RF

void setup() {
  // Debug console
  Serial.begin(115200);
 
  while (!Serial && millis() < 5000);
  
  if (IWatchdog.isReset()) {
    Serial.printf("Rebooted by Watchdog!\n");
    delay(120 * 1000);
    IWatchdog.clearReset(); 
  }

  //initialise hardware timer for periodic wake up
  Serial.print("Setting up Hardware Timer for backup to EEPROM with period: ");
  TIM_TypeDef *EEPROMTimerInstance = TIM2;
  HardwareTimer *PeriodicWakeUp = new HardwareTimer(EEPROMTimerInstance);
  PeriodicWakeUp->pause();
  PeriodicWakeUp->setPrescaleFactor(65536);
  //PeriodicWakeUp->setOverflow(4194304);
  Serial.print(PeriodicWakeUp->getOverflow() / (PeriodicWakeUp->getTimerClkFreq() / PeriodicWakeUp->getPrescaleFactor()) / 60);
  Serial.println(" min");
  PeriodicWakeUp->refresh();
  PeriodicWakeUp->resume();
  PeriodicWakeUp->attachInterrupt(SensorsCallback);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  //PowerUp modules during init procedure
  pinMode(POWERUP_PIN, OUTPUT);
  digitalWrite(POWERUP_PIN, HIGH);

  delay(500);
 
  RadoInit();
  SensorsInit();
  
  //Set witchdog timeout for 32 seconds
  IWatchdog.begin(24000000); // set to maximum value
  IWatchdog.reload();
  
  while (!IWatchdog.isEnabled()) {
    // LED blinks indefinitely
    digitalWrite(LED_PIN, LOW);
    delay(500);
    digitalWrite(LED_PIN, HIGH);
    delay(500);
  }

  Serial.println("Watchdog enabled");

  IWatchdog.reload();
}

void loop() {
  IWatchdog.reload();
}

void SensorsCallback()
{
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(POWERUP_PIN, HIGH);
  
  SensorsRead();
  RadioTransmit();

  digitalWrite(LED_PIN, LOW);
  digitalWrite(POWERUP_PIN, LOW);
}