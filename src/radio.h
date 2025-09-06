const int frequency = 433920;
const int dataRate = 17241;

#define RADIO_RX_PIN PA5
#define RADIO_TX_PIN PA6
#define POWERUP_PIN PA7

#define SCK_PIN PB13
#define MISO_PIN PB14
#define MOSI_PIN PB15
#define RESET_PIN PA9
#define NSS_PIN PB15
#define RX_PIN PB15
#define TX_PIN PA9
#define IRQ_PIN PA8
#define SPI_PORT SPI2


#define FRAME_LENGTH 5

void LaCrosseTransmit();
