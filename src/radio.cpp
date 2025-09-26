#include "sensors.h"
#include "radio.h"
#include "lacrosse.h"

#define TIMER TIM4

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
bool first = true;

SIGNAL_T Radio::cmdList[1200];
SIGNAL Radio::signals[6] = {
  {SIG_SYNC, "Sync", 712, 712},
  {SIG_SYNC_GAP, "Sync-gap", 1700, 712},
  {SIG_ZERO, "Zero", 476, 220},
  {SIG_ONE, "One", 220, 476},
  {SIG_IM_GAP, "IM_gap", 0, 1450},
  {SIG_PULSE, "Pulse", 833, 833}
};

SPIClass* Radio::spi;
RF69* Radio::RF69_Radio_Module;

uint16_t Radio::listEnd = 0;

extern "C" {
  #include "stm32f4xx_hal.h"

  TIM_HandleTypeDef HTIMx;
  uint32_t gu32_ticks;

  void TimerDelay_Init(void)
  {
    gu32_ticks = (HAL_RCC_GetHCLKFreq() / 1000000);
    
 
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
 
    HTIMx.Instance = TIMER;
    HTIMx.Init.Prescaler = gu32_ticks-1;
    HTIMx.Init.CounterMode = TIM_COUNTERMODE_UP;
    HTIMx.Init.Period = 65535;
    HTIMx.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HTIMx.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&HTIMx) != HAL_OK)
    {
      Error_Handler();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&HTIMx, &sClockSourceConfig) != HAL_OK)
    {
      Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&HTIMx, &sMasterConfig) != HAL_OK)
    {
      Error_Handler();
    }
 
    HAL_TIM_Base_Start(&HTIMx);
  }
 
  void delay_us(uint16_t au16_us)
  {
    HTIMx.Instance->CNT = 0;
    while (HTIMx.Instance->CNT < au16_us);
  }
}


bool Radio::setup() {
  Serial.println("Initialize Radio Transmitter");

  if (!RF69_init())
  {
    Serial.println("ERROR: Your radio doesn't set up correctly. Make sure you set the correct radio and pins, save settings and reboot.");
    return false;
  }

  pinMode(pin_tx, OUTPUT);

  TimerDelay_Init();

  Serial.println("Done");
  return true;
}

bool Radio::RF69_init()
{
  Serial.printf("Radio %s: SCK %i, MISO %i, MOSI %i, CS %i, RESET %i, TX %i\n", "RF69", pin_sck, pin_miso, pin_mosi, pin_cs, pin_reset, pin_tx);

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


void __attribute__((section(".RamFunc"))) Radio::Transmit()
{
  uint8_t frame_1[FRAME_LENGTH];
  uint8_t frame_2[FRAME_LENGTH];
  char MessageBuf[FRAME_LENGTH * 8];
  LaCrosse::EncodeFrame(frame_1, 1);
  LaCrosse::FrameInvert(frame_1);
  LaCrosse::EncodeFrame(frame_2, 2);
  LaCrosse::FrameInvert(frame_2);

  #ifdef LOWPOWER_DEBUG
    sprintf(MessageBuf, "STM32Weather; %.2f C; %d %%; %.1f Pa; %.1f km/h; %d", ActualData.temperature, ActualData.humidity, ActualData.pressure, ActualData.wind, ActualData.low_battery);
    Serial.println(MessageBuf);
    Serial.print("Encoded Frame 1:  ");
    for(int i = 0; i < FRAME_LENGTH; i++) {
      Serial.print(frame_1[i]);
      Serial.print(" ");
    }
    Serial.println("");
    Serial.print("Encoded Frame 2:  ");
    for(int i = 0; i < FRAME_LENGTH; i++) {
      Serial.print(frame_2[i]);
      Serial.print(" ");
    }
    Serial.println("");
    Serial.printf("Transmitting... Chanel: %d \n", LaCrosse::chanel);
  #endif

  make_wave(frame_1, frame_2, FRAME_LENGTH * 8);
  tx();
  Serial.println("Done.");
}

bool __attribute__((section(".RamFunc"))) Radio::tx() {
  RF69_Radio_Module->setOutputPower(tx_power, tx_high_power);
  RF69_Radio_Module->transmitDirect();

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
        digitalWriteFast(digitalPinToPinName(pin_tx), HIGH);
        delay_us(signals[sig].up_time);
      }
      if (signals[sig].delay_time > 0) {
        digitalWriteFast(digitalPinToPinName(pin_tx), LOW);
        delay_us(signals[sig].delay_time);
      }
    }
  }

  interrupts();
  delay_us(400);
  standby();
  return true;
}

void Radio::insert(SIGNAL_T signal)
{
  cmdList[listEnd++] = signal;
  return;
}

void Radio::make_wave(uint8_t *msg_1, uint8_t *msg_2, uint8_t msgLen)
{
  listEnd = 0;

  for (uint8_t j = 0; j < tx_repeats; j++)
  {
    // Preamble
    insert(SIG_SYNC);
    insert(SIG_SYNC);
    insert(SIG_SYNC);
    insert(SIG_SYNC);

    for (uint8_t i = 0; i < msgLen; i++)
    {
      insert(((uint8_t)((msg_1[i / 8] >> (7 - (i % 8))) & 0x01)) == 0 ? SIG_ZERO : SIG_ONE);
    }

    insert(SIG_ONE);
  }

  insert(SIG_IM_GAP);

  for (uint8_t j = 0; j < tx_repeats-2; j++)
  {
    // Preamble
    insert(SIG_SYNC);
    insert(SIG_SYNC);
    insert(SIG_SYNC);
    insert(SIG_SYNC);

    for (uint8_t i = 0; i < msgLen; i++)
    {
      insert(((uint8_t)((msg_2[i / 8] >> (7 - (i % 8))) & 0x01)) == 0 ? SIG_ZERO : SIG_ONE);
    }

    if (j == 2)
    {
      insert(SIG_ZERO);
    } else {
      insert(SIG_ONE);
    }
    
  }
  
  cmdList[listEnd++] = SIG_IM_GAP;
  cmdList[listEnd] = NONE;
}
