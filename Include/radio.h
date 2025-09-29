#define SCK_PIN PB13
#define MISO_PIN PB14
#define MOSI_PIN PB15
#define RESET_PIN PA9
#define NSS_PIN PB12
#define TX_PIN PA15
#define IRQ_PIN PA8

#define HIGH_POWER true
#define TX_POWER 13

#define FRAME_LENGTH 8
#define REPEATS 7

#include <SPI.h>
#include <RadioLib.h>

enum SIGNAL_T {
    NONE = -1,
    SIG_SYNC = 0,
    SIG_SYNC_GAP,
    SIG_ZERO,
    SIG_ONE,
    SIG_IM_GAP,
    SIG_PULSE
};

// Structure of the table of timing durations used to signal
typedef struct {
    SIGNAL_T sig_type;   // Index the type of signal
    String sig_name;     // Provide a brief descriptive name
    uint16_t up_time;    // duration for pulse with voltage up
    uint16_t delay_time; // delay with voltage down before next signal
} SIGNAL;

class Radio {

    public:
        static bool setup();
        static bool RF69_init();
        static bool standby();
        static bool sleep();
        static void Transmit();
        static SIGNAL_T cmdList[1200];
        static SIGNAL signals[6];

    private:
        static int pin_sck;
        static int pin_miso;
        static int pin_mosi;
        static int pin_reset;
        static int pin_irq;
        static int pin_cs;
        static int pin_tx;
        static int threshold_type;
        static int threshold_level;
        static int tx_power;
        static bool tx_high_power;
        static int tx_repeats;
        static float frequency;
        static float bandwidth;
        static float bitrate;

        static SPIClass* spi;
        //static Module* radioLibModule;
        static RF69 *RF69_Radio_Module;

        static uint16_t listEnd;
        static void insert(SIGNAL_T signal);
        static void make_wave(uint8_t *msg_1, uint8_t *msg_2, uint8_t msgLen);
        static bool tx();
};
