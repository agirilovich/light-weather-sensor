#ifndef _LACROSSE_h
#define _LACROSSE_h

#define SENSOR_ID 160170
#define SENSOR_CHANEL 0
#define SENSOR_TEST_MODE false

#include <Arduino.h>

class LaCrosse {
    public:
        uint8_t frame[8];
    
        enum MessageType : uint8_t
        {
            test,
            temperature,
            wind
        };
        static void EncodeFrame(uint8_t *frame, int message_type);
        static uint8_t crc8(uint8_t const *message, unsigned nBytes, uint8_t polynomial, uint8_t init);
        static void FrameInvert(uint8_t *frame);
        static int chanel;

    private:
        
};

#endif