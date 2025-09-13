const int frequency = 433920;
const int dataRate = 17241;

#define RADIO_RX_PIN PA5
#define RADIO_TX_PIN PA6
#define POWERUP_PIN PA7

#define SCK_PIN PB13
#define MISO_PIN PB14
#define MOSI_PIN PB15
#define RESET_PIN PA9
#define NSS_PIN PB12
#define RX_PIN PA10
#define TX_PIN PA10
#define IRQ_PIN PA8
#define RADIO_NAME "RF69"


#define FRAME_LENGTH 8
#define FRAME_INVERT true

void LaCrosseTransmit();
void RadoInit();
