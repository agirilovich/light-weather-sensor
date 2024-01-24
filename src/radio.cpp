#include "radio.h"
#include "sensors.h"

#include <RH_ASK.h>
//#include <SPI.h> //is needed for compile

#define RADIO_RX_PIN PA5
#define RADIO_TX_PIN PA6
#define POWERUP_PIN PA7

RH_ASK RadioDriver(2000, RADIO_RX_PIN, RADIO_TX_PIN, POWERUP_PIN, false);

void RadoInit()
{
    delay(1000);
    Serial.println("Initialize Radio Transmitter");
    RadioDriver.init();
    Serial.println("Done");
    
    Serial.print("Maximal message lenght: ");
    Serial.println(RadioDriver.maxMessageLength());
    delay(1000);
}



void RadioTransmit()
{
  char MessageBuf[64];
  sprintf(MessageBuf, "STM32Weather;%d;%d;%d;%d;", int(ActualData.temperature * 10), int(ActualData.humidity), int(ActualData.pressure), int(ActualData.light));
  
  Serial.println(MessageBuf);

  RadioDriver.send((uint8_t *)MessageBuf, strlen(MessageBuf));
  RadioDriver.waitPacketSent(1000);
}