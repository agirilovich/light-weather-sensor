#include "sensors.h"
#include "radio.h"
#include "lacrosse.h"

TIM_TypeDef *transitionTimerInstance = TIM1;
HardwareTimer *transitionTimer = new HardwareTimer(transitionTimerInstance);

SIGNAL Radio::TX141TH_signals[6] = {
  {SIG_SYNC, "Sync", 712, 936},
  {SIG_SYNC_GAP, "Sync-gap", 0, 168},
  {SIG_ZERO, "Zero", 348, 348},
  {SIG_ONE, "One", 136, 544},
  {SIG_IM_GAP, "IM_gap", 0, 10004},
  {SIG_PULSE, "Pulse", 512, 512} // spare
};

bool Radio::setup() {
  Serial.println("Initialize Radio Transmitter");

  if (!RF69_init())
  {
    Serial.println("ERROR: Your radio doesn't set up correctly. Make sure you set the correct radio and pins, save settings and reboot.");
    return false;
  }

  pinMode(pin_tx, OUTPUT);

  // Timer for pulse_gap_len_new_packet
  transitionTimer->pause();
  transitionTimer->setPrescaleFactor(transitionTimer->getTimerClkFreq() / 1000000);
  //transitionTimer->setOverflow(pulse_gap_len_new_packet);
  transitionTimer->setOverflow(0xFFFF);
  transitionTimer->resume();

  Serial.println("Done");
  return true;
}

bool Radio::RF69_init()
{
  Serial.printf("Radio %s: SCK %i, MISO %i, MOSI %i, CS %i, RESET %i, TX %i\n", name().c_str(), pin_sck, pin_miso, pin_mosi, pin_cs, pin_reset, pin_tx);

  //spi = new SPIClass(pin_mosi, pin_miso, pin_sck, pin_cs);
  spi = new SPIClass(pin_mosi, pin_miso, pin_sck);
  spi->begin();

  RF69 radioLibModule = new Module(pin_cs, pin_irq, pin_reset, -1, *spi);
  Serial.printf("%s: Frequency: %.2f Mhz, bandwidth %.1f kHz, bitrate %.3f kbps\n", name().c_str(), frequency, bandwidth, bitrate);
  Serial.printf("%s: Transmit power set to %i dBm\n", tx_power);
  radioLibModule.begin(frequency, bitrate, 50.0F, bandwidth, 12, 16);
  radioLibModule.setOOK(true);
  radioLibModule.setDataShaping(RADIOLIB_SHAPING_NONE);
  radioLibModule.setEncoding(RADIOLIB_ENCODING_NRZ);
  radioLibModule.setOokThresholdType(threshold_type);
  radioLibModule.setOokFixedThreshold(threshold_level);
  radioLibModule.setOokPeakThresholdDecrement(RADIOLIB_RF69_OOK_PEAK_THRESH_DEC_1_8_CHIP);
  radioLibModule.setLnaTestBoost(true);
  return true;
}

bool Radio::standby() {
  radioLibModule.standby();
  return true;
}

bool Radio::sleep() {
    radioLibModule.sleep();
    return true;
}


void Radio::Transmit(uint8_t type)
{
  uint8_t frame[FRAME_LENGTH];
  char MessageBuf[64];
  LaCrosse::EncodeFrame(frame, type);
  LaCrosse::FrameInvert(frame);

  #ifdef LOWPOWER_DEBUG
    Serial.print("Frame:  ");
    for(int i = 0; i < FRAME_LENGTH; i++) {
      Serial.print(frame[i]);
      Serial.print(" ");
    }
    Serial.println("");
    sprintf(MessageBuf, "STM32Weather; %.2f C; %d %%; %.1f Pa; %.1f km/h; %d", ActualData.temperature, ActualData.humidity, ActualData.pressure, ActualData.wind, ActualData.battery);
    Serial.println(MessageBuf);
  #endif
    
  make_wave(frame, FRAME_LENGTH);
  tx();
}

bool Radio::tx() {
  Serial.printf("Transmitting raw: %s\n", raw.toString().c_str());
  delay(100);     // So INFO prints before we turn off interrupts
  radioLibModule.setOutputPower(tx_power, Settings::isSet("tx_high_power"));
  radioLibModule.transmitDirect();
  transitionTimer->refresh();
  transitionTimer->resume();
  int32_t tx_timer = transitionTimer->getCount();
  noInterrupts();

  {
    SIGNAL_T sig;
    for (int i = 0; i < listEnd; i++) {
      sig = cmdList[i];
      if (sig == NONE) { // Terminates list but should never be executed
        Serial.println(F(" \tERROR -- invalid signal: "));
        Serial.println((int)cmdList[i]);
        Serial.println((cmdList[i] == NONE) ? " (NONE)" : "");
        return;
      }
      if (signals[sig].up_time > 0) {
        digitalWrite(pin_tx, HIGH);
        delayMicroseconds(signals[sig].up_time);
      }
      if (signals[sig].delay_time > 0) {
        digitalWrite(pin_tx, LOW);
        delayMicroseconds(signals[sig].delay_time);
      }
    }
  }

  interrupts();
  tx_timer = transitionTimer->getCount() - tx_timer;
  Serial.printf("Transmission done, took %i µs.\n", tx_timer);
  delayMicroseconds(400);
  standby();
  return true;
}

void Radio::insert(SIGNAL_T signal)
{
  cmdList[listEnd++] = signal;
  return;
}

void Radio::make_wave(uint8_t *msg, uint8_t msgLen)
{
  listEnd = 0;
  bool first = true;

  // Repeat preamble+data REPEAT times
  for (uint8_t j = 0; j < tx_repeats; j++)
  {
    // Preamble
    insert(SIG_SYNC);
    insert(SIG_SYNC);
    insert(SIG_SYNC);
    insert(SIG_SYNC);

    // The data packet
    if (first)
    {
      Serial.println(F("The msg packet, length="));
      Serial.println((int)msgLen);
      Serial.println(F(", as a series of bits: "));
    }
    for (uint8_t i = 0; i < msgLen; i++)
    {
      insert(((uint8_t)((msg[i / 8] >> (7 - (i % 8))) & 0x01)) == 0 ? SIG_ZERO : SIG_ONE);
      if (first)
      {
        Serial.println(((msg[i / 8] >> (7 - (i % 8))) & 0x01));
      }
    }
    if (first)
    {
      Serial.println();
      first = false;
    }
  }

  // Postamble of two SIG_SYNCs and IM_GAP
  // and terminal marker for safety
  cmdList[listEnd++] = SIG_SYNC;
  cmdList[listEnd++] = SIG_SYNC;
  cmdList[listEnd++] = SIG_IM_GAP;
  cmdList[listEnd]   = NONE;
}
