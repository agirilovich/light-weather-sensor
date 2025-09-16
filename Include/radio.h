const int frequency = 433920;
const int dataRate = 17241;

#define SCK_PIN PB13
#define MISO_PIN PB14
#define MOSI_PIN PB15
#define RESET_PIN PA9
#define NSS_PIN PB12
#define RX_PIN PA15
#define TX_PIN PC13
#define IRQ_PIN PA8
#define RADIO_NAME "RF69"

#ifdef LOWPOWER_DEBUG
    #define ERROR_LEVEL "debug"
#else
    #define ERROR_LEVEL "none"
#endif


#define FRAME_LENGTH 8
#define FRAME_INVERT true

void LaCrosseTransmit();
void RadoInit();
