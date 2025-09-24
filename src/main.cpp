#include <Arduino.h>

#include <IWatchdog.h>
#include <STM32LowPower.h>

#include "sensors.h"
#include "radio.h"

#ifndef DEVICE_BOARD_NAME
#  define DEVICE_BOARD_NAME "STM32OutdoorSensor"
#endif

#define LED_PIN PC13

int flip_type = 0;

void setup() {
  // Debug console
  Serial.begin(115200);
 
  while (!Serial && millis() < 5000);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  if (IWatchdog.isReset()) {
    Serial.printf("Rebooted by Watchdog!\n");
    delay(15 * 1000);
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

  //PowerUp modules during init procedure

  Radio::setup();
  SensorsInit();

  LowPower.begin();

  delay(1000);
  Serial.flush();
}

void loop() {
  digitalWrite(LED_PIN, LOW);
  WeatherSensorRead();

  Radio::Transmit(flip_type);

  if (flip_type == 1)
  {
    flip_type = 2;
  } else {
    flip_type = 1;
  }

  digitalWrite(LED_PIN, HIGH);
  IWatchdog.reload();
  
  if (ActualData.low_battery = 1)
  {
      //Radio::sleep();
      //LowPower.deepSleep(24000);
      delay(5000);
  } else {
      Radio::standby();
      //LowPower.sleep(28000);
      delay(8000);
  }
}
