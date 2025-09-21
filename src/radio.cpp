#include "sensors.h"
#include "radio.h"
#include "lacrosse.h"

//TIM_TypeDef Radio::transitionTimerInstance = TIM1;

int Radio::pin_sck=SCK_PIN;
int Radio::pin_miso=MISO_PIN;
int Radio::pin_mosi=MOSI_PIN;
int Radio::pin_reset=RESET_PIN;
int Radio::pin_irq=IRQ_PIN;
int Radio::pin_cs=NSS_PIN;
int Radio::pin_tx=TX_PIN;
int Radio::threshold_type=RADIOLIB_RF69_OOK_THRESH_FIXED;
int Radio::threshold_level=6;
int Radio::tx_power=TX_POWER;
bool Radio::tx_high_power=HIGH_POWER;
int Radio::tx_repeats=REPEATS;
float Radio::frequency=433.92;
float Radio::bandwidth=250;
float Radio::bitrate=9.6;

SIGNAL_T Radio::cmdList[300];
SIGNAL* Radio::signals;

TIM_TypeDef* Radio::transitionTimerInstance = TIM1;
HardwareTimer* Radio::transitionTimer;
SPIClass* Radio::spi;
RF69* Radio::RF69_Radio_Module;

SIGNAL Radio::TX141TH_signals[6] = {
  {SIG_SYNC, "Sync", 712, 936},
  {SIG_SYNC_GAP, "Sync-gap", 0, 168},
  {SIG_ZERO, "Zero", 348, 348},
  {SIG_ONE, "One", 136, 544},
  {SIG_IM_GAP, "IM_gap", 0, 10004},
  {SIG_PULSE, "Pulse", 512, 512} // spare
};

uint16_t Radio::listEnd = 0;

bool Radio::setup() {
  Serial.println("Initialize Radio Transmitter");

  if (!RF69_init())
  {
    Serial.println("ERROR: Your radio doesn't set up correctly. Make sure you set the correct radio and pins, save settings and reboot.");
    return false;
  }

  pinMode(pin_tx, OUTPUT);

  // Timer for pulse_gap_len_new_packet
  transitionTimer = new HardwareTimer(transitionTimerInstance);
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
  Serial.printf("Radio %s: SCK %i, MISO %i, MOSI %i, CS %i, RESET %i, TX %i\n", "RF69", pin_sck, pin_miso, pin_mosi, pin_cs, pin_reset, pin_tx);

  //spi = new SPIClass(pin_mosi, pin_miso, pin_sck, pin_cs);
  spi = new SPIClass(pin_mosi, pin_miso, pin_sck);
  spi->begin();

  Module* radioLibModule = new Module(pin_cs, pin_irq, pin_reset, -1, *spi);
  RF69_Radio_Module = new RF69(radioLibModule);
  Serial.printf("%s: Frequency: %.2f Mhz, bandwidth %.1f kHz, bitrate %.3f kbps\n", "RF69", frequency, bandwidth, bitrate);
  Serial.printf("%s: Transmit power set to %i dBm\n", tx_power);
  RF69_Radio_Module->begin(frequency, bitrate, 50.0F, bandwidth, 12, 16);
  RF69_Radio_Module->setOOK(true);
  RF69_Radio_Module->setDataShaping(RADIOLIB_SHAPING_NONE);
  RF69_Radio_Module->setEncoding(RADIOLIB_ENCODING_NRZ);
  RF69_Radio_Module->setOokThresholdType(threshold_type);
  RF69_Radio_Module->setOokFixedThreshold(threshold_level);
  RF69_Radio_Module->setOokPeakThresholdDecrement(RADIOLIB_RF69_OOK_PEAK_THRESH_DEC_1_8_CHIP);
  RF69_Radio_Module->setLnaTestBoost(true);
  return true;
}

bool __attribute__((section(".RamFunc"))) Radio::standby() {
  RF69_Radio_Module->standby();
  return true;
}

bool __attribute__((section(".RamFunc"))) Radio::sleep() {
  RF69_Radio_Module->sleep();
  return true;
}


void __attribute__((section(".RamFunc"))) Radio::Transmit(uint8_t type)
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
    sprintf(MessageBuf, "STM32Weather; %.2f C; %d %%; %.1f Pa; %.1f km/h; %d", ActualData.temperature, ActualData.humidity, ActualData.pressure, ActualData.wind, ActualData.low_battery);
    Serial.println(MessageBuf);
  #endif
    
  make_wave(frame, FRAME_LENGTH);
  tx();
}

bool __attribute__((section(".RamFunc"))) Radio::tx() {
  delay(100);     // So INFO prints before we turn off interrupts
  RF69_Radio_Module->setOutputPower(tx_power, tx_high_power);
  RF69_Radio_Module->transmitDirect();
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
        return false;
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
  cmdList[listEnd] = NONE;
}
