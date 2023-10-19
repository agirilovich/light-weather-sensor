#include "radio.h"
#include "sensors.h"

#include <RH_ASK.h>
#include <SPI.h> //is needed for compile

#define RADIO_RX_PIN PA5
#define RADIO_TX_PIN PA6

RH_ASK RadioDriver(2000, RADIO_RX_PIN, RADIO_TX_PIN, 0);

void RadoInit()
{
    Serial.println("Initialize Radio Transmitter");
    if (!RadioDriver.init())
    {
      Serial.println("init failed");
    }
    else {
      Serial.println("Done");
    }
}



void RadioTransmit()
{
    byte tx_buf[sizeof(ActualData)] = {0};
    memcpy(tx_buf, &ActualData, sizeof(myData) );
    RadioDriver.send((uint8_t *)ActualData, strlen(ActualData));
    RadioDriver.waitPacketSent();
    delay(1000);
}