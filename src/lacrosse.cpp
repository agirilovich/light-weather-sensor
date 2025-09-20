#include "lacrosse.h"
#include "sensors.h"

/*
* Message Format:
*
* .- [0] -. .- [1] -. .- [2] -. .- [3] -. .- [4] -.
* SSSS.DDDD DDN_.TTTT TTTT.TTTT WHHH.HHHH CCCC.CCCC
* |  | |     ||  |  | |  | |  | ||      | |       |
* |  | |     ||  |  | |  | |  | ||      | `--------- CRC
* |  | |     ||  |  | |  | |  | |`-------- humidity%
* |  | |     ||  |  | |  | |  | `---- weak battery
* |  | |     ||  `--------------- Temperature BCD, T = X/10-40
* |  | |     | `--- new battery
* |  | `-------- sensor ID
* `---- start byte
*
* more details:
* https://github.com/merbanan/rtl_433/blob/master/src/devices/lacrosse_tx35.c
*/

LaCrosse(){};

// Inserts a signal into the commmand list
void ISM_Device::insert(SIGNAL_T signal)
{
  cmdList[listEnd++] = signal;
  return;
}

void LaCrosse::EncodeFrame(uint8_t *frame, int message_type) {
  //convert transmitter ID
  int id = SENSOR_ID;
  int test_mode = SENSOR_TEST_MODE;
  
  int temperatureValue = ActualData.temperature * 10 + 500;
  int windValue = ActualData.wind * 10;
  int pressureValue = ActualData.pressure;

  frame[0] = (SENSOR_ID >> 16);
  frame[1] = (SENSOR_ID & 0xFFFF) >> 8;
  frame[2] = (SENSOR_ID & 0xFF);
  frame[3] = (ActualData.battery << 7) | (test_mode << 6) | (chanel << 4) | (message_type);
  
  switch(message_type)
  {
    case MessageType::temperature:
      frame[4] = temperatureValue >> 4;
      frame[5] = ((temperatureValue & 0x0F) << 4) | (ActualData.humidity >> 8);
      frame[6] = (ActualData.humidity & 0xFF);
      break;

    case MessageType::wind:
      frame[4] = windValue >> 4;
      frame[5] = ((windValue & 0x0F) << 4) | (pressureValue >> 8);
      frame[6] = (pressureValue & 0xFF);
      break;
  
    default:
      //unknown subtype
      break;
  }

  //int8_t b[8] = frame[];
  //frame[7] = crc8(b, 8, 0x31, 0x00);
  frame[7] = crc8(frame, 8, 0x31, 0x00);
}


//Calculate CRC function
uint8_t LaCrosse::crc8(uint8_t const *message, unsigned nBytes, uint8_t polynomial, uint8_t init)
{
    uint8_t remainder = init;
    unsigned byte, bit;

    for (byte = 0; byte < nBytes; ++byte) {
        remainder ^= message[byte];
        for (bit = 0; bit < 8; ++bit) {
            if (remainder & 0x80) {
                remainder = (remainder << 1) ^ polynomial;
            } else {
                remainder = (remainder << 1);
            }
        }
    }
    return remainder;
}

//Byte invert function
void LaCrosse::FrameInvert(uint8_t *frame)
{
  uint8_t *b = frame;

  const unsigned last_col  = 8;
  const unsigned last_bits = 0;
  for (unsigned col = 0; col <= last_col; ++col) {
    b[col] = ~b[col]; // Invert
  }
  b[last_col] ^= 0xFF >> last_bits; // Re-invert unused bits in last byte
}