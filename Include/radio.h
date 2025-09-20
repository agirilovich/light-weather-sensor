#define SCK_PIN PB13
#define MISO_PIN PB14
#define MOSI_PIN PB15
#define RESET_PIN PA9
#define NSS_PIN PB12
#define TX_PIN PA15
#define IRQ_PIN PA8

#define FRAME_LENGTH 8
#define REPEATS 6

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
        static void Transmit(uint8_t type);
        SIGNAL_T cmdList[300];
        SIGNAL *signals;
        SIGNAL TX141TH_signals[6];

    private:
        static int pin_sck=SCK_PIN;
        static int pin_miso=MISO_PIN;
        static int pin_mosi=MOSI_PIN;
        static int pin_reset=RESET_PIN;
        static int pin_irq=IRQ_PIN;
        static int pin_cs=NSS_PIN;
        static int pin_tx=TX_PIN;
        static int threshold_type=RADIOLIB_RF69_OOK_THRESH_FIXED;
        static int threshold_level=6;
        static int tx_power=13;
        static int tx_repeats=REPEATS;
        static float frequency=433.92;
        static float bandwidth=250;
        static float bitrate=9.6;
        static TIM_TypeDef *transitionTimerInstance;
        static HardwareTimer *transitionTimer;
        SPIClass* spi;
        // radiolib-specific
        Module* radioLibModule;

        uint16_t listEnd = 0;
        String Device_Name = "LaCrosse Device";
        void insert(SIGNAL_T signal);
        void make_wave(uint8_t *msg, uint8_t msgLen);
        static bool tx();
};
