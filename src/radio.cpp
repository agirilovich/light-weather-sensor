#include <OOKwiz.h>
#include "sensors.h"
#include "radio.h"


void RadoInit()
{
  //Set Data pins
  Settings::set("pin_sck", SCK_PIN);
  Settings::set("pin_miso", MISO_PIN);
  Settings::set("pin_mosi", MOSI_PIN);
  Settings::set("pin_reset", RESET_PIN);
  Settings::set("pin_irq", IRQ_PIN);
  Settings::set("pin_cs", NSS_PIN);
  Settings::set("spi_port", SPIClass SPI_PORT);
  Settings::set("pin_rx", RX_PIN);
  Settings::set("pin_tx", TX_PIN);
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
  
  uint8_t frame[4];
  LaCrosse::EncodeFrame(frame, ActualData);
  uint8_t packet[FRAME_LENGTH];
  for(int i = 0; i < 4; i++) packet[i] = frame[i];
  packet[4] = LaCrosse::CalculateCRC(frame, 4);
  RadioTransmit(packet)
  
  char MessageBuf[64];
  sprintf(MessageBuf, "STM32Weather;%d;%d;%d;%d;", int(ActualData.temperature * 10), int(ActualData.humidity), int(ActualData.pressure), int(ActualData.light));
  
  Serial.println(MessageBuf);

  if (OOKwiz::setup()) {
    Settings::set("tx_high_power", true);
    Meaning newPacket; 
    newPacket.addPulse(5900);
    uint8_t data[3] = {0x17, 0x72, 0xA4};
    // 190 and 575 are space and mark timings, 24 is data length in bits
    newPacket.addPWM(190, 575, 24, data);
    newPacket.repeats = 6;
    newPacket.gap = 130;    
    OOKwiz::transmit(newPacket);
  }
}