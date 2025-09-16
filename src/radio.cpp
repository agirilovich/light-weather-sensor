#include <OOKwiz.h>
#include "sensors.h"
#include "radio.h"
#include "lacrosse.h"


void RadoInit()
{
  //Set Data pins
  Settings::set("pin_sck", SCK_PIN);
  Settings::set("pin_miso", MISO_PIN);
  Settings::set("pin_mosi", MOSI_PIN);
  Settings::set("pin_reset", RESET_PIN);
  Settings::set("pin_irq", IRQ_PIN);
  Settings::set("pin_cs", NSS_PIN);
  Settings::set("pin_rx", RX_PIN);
  Settings::set("pin_tx", TX_PIN);
  Settings::set("radio", RADIO_NAME);
  Settings::set("errorlevel", ERROR_LEVEL);
  //Lower power usage mode than in receive
  Settings::set("start_in_standby", true);
  //Set high transmission power as default
  Settings::set("tx_high_power", true);

  Serial.println("Initialize Radio Transmitter");
  OOKwiz::setup();
  Serial.println("Done");
}



void LaCrosseTransmit()
{
  uint8_t frame[FRAME_LENGTH];
  char MessageBuf[64];
  for (int type = 0; type <= 1; ++type)
  {
    LaCrosse::EncodeFrame(frame, type);
    if(FRAME_INVERT)
    {
      LaCrosse::FrameInvert(frame);
    }

    sprintf(MessageBuf, "STM32Weather; %.2f C; %d %%; %.1f Pa; %.1f km/h; %d", ActualData.temperature, ActualData.humidity, ActualData.pressure, ActualData.wind, ActualData.battery);
    Serial.println(MessageBuf);

    Settings::set("tx_high_power", true);
    Meaning newPacket;
    //Preamble
    for (int preambl_count = 0; preambl_count <= 4; ++preambl_count)
    {
      newPacket.addPulse(833);
      newPacket.addGap(833);
    }

    //Data
    newPacket.addPWM(208, 417, 64, frame);

    //Repeat transmission few times
    newPacket.repeats = 6;
    newPacket.gap = 1700;    
    OOKwiz::transmit(newPacket);
    OOKwiz::standby();
  }
}