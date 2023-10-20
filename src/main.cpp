#include <Arduino.h>

//#include <IWatchdog.h>

#include "sensors.h"

#include "radio.h"

#include "STM32LowPower.h"

#ifndef DEVICE_BOARD_NAME
#  define DEVICE_BOARD_NAME "STM32OutdoorSensor"
#endif

#define LED_PIN PC13

#define POWERUP_PIN PA7

//char SensorCharMsg[256]; //message for transmit over RF

void setup() {
  // Debug console
  Serial.begin(115200);
 
  while (!Serial && millis() < 5000);
  
  /*
  if (IWatchdog.isReset()) {
    Serial.printf("Rebooted by Watchdog!\n");
    delay(120 * 1000);
    IWatchdog.clearReset(); 
  }
  */

  // Configure low power
  LowPower.begin();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  //PowerUp modules during init procedure
  pinMode(POWERUP_PIN, OUTPUT);
  digitalWrite(POWERUP_PIN, HIGH);

  delay(500);
 
  RadoInit();
  SensorsInit();

  
  /*
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
  */
}

void loop() {
  //IWatchdog.reload();
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(POWERUP_PIN, HIGH);
  
  SensorsRead();
  RadioTransmit();

  digitalWrite(LED_PIN, LOW);
  digitalWrite(POWERUP_PIN, LOW);

  LowPower.deepSleep(60000);
}
