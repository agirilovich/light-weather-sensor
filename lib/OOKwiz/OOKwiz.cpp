#include "OOKwiz.h"
#include "serial_output.h"

bool OOKwiz::serial_cli_disable = false;
int OOKwiz::first_pulse_min_len;
int OOKwiz::pulse_gap_min_len;
int OOKwiz::min_nr_pulses;
int OOKwiz::max_nr_pulses;
int OOKwiz::pulse_gap_len_new_packet;
int OOKwiz::noise_penalty;
int OOKwiz::noise_threshold;
int OOKwiz::noise_score;
bool OOKwiz::no_noise_fix = false;
int OOKwiz::lost_packets = 0;
int32_t OOKwiz::last_transition;
int32_t OOKwiz::repeat_time_start = 0;
long OOKwiz::repeat_timeout;
bool OOKwiz::tx_active_high;
RawTimings OOKwiz::isr_in;
RawTimings OOKwiz::isr_out;
BufferPair OOKwiz::loop_in;
BufferPair OOKwiz::loop_compare;
BufferTriplet OOKwiz::loop_ready;
int32_t OOKwiz::last_periodic = 0;
void (*OOKwiz::callback)(RawTimings, Pulsetrain, Meaning) = nullptr;

TIM_TypeDef *transitionTimerInstance = TIM1;
HardwareTimer *transitionTimer = new HardwareTimer(transitionTimerInstance);


/// @return true if setup succeeded, false if it could not complete, e.g. because the radio is not configured yet.
bool OOKwiz::setup() {

    // Tada !
    INFO("\n\nOOKwiz version %s (built %s %s) initializing.\n", OOKWIZ_VERSION, __DATE__, __TIME__);

    if (!Radio::setup()) {
        ERROR("ERROR: Your radio doesn't set up correctly. Make sure you set the correct\n       radio and pins, save settings and reboot.\n");
        return false;
    }

    Device::setup();

    // These settings determine what a valid transmission is
    SETTING_OR_ERROR(pulse_gap_len_new_packet);
    SETTING_OR_ERROR(repeat_timeout);
    SETTING_OR_ERROR(first_pulse_min_len);
    SETTING_OR_ERROR(pulse_gap_min_len);
    SETTING_OR_ERROR(min_nr_pulses);
    SETTING_OR_ERROR(max_nr_pulses);
    SETTING_OR_ERROR(noise_penalty);
    SETTING_OR_ERROR(noise_threshold);
    no_noise_fix = Settings::isSet("no_noise_fix");
    tx_active_high = Settings::isSet("tx_active_high");

    // Timer for pulse_gap_len_new_packet
    transitionTimer->pause();
    transitionTimer->setPrescaleFactor(transitionTimer->getTimerClkFreq() / 1000000);
    //transitionTimer->setOverflow(pulse_gap_len_new_packet);
    transitionTimer->setOverflow(0xFFFF);
    transitionTimer->resume();

    return standby();
}

/// @brief Transmits this string representation of a `RawTimings`, `Pulsetrain` or `Meaning` instance.
/// @param str The string representation of what needs to be simulated
/// @return `true` if it worked, `false` if not. Will show error message telling you why it didn't work in latter case.
bool OOKwiz::transmit(String &str) {
    if (RawTimings::maybe(str)) {
        RawTimings raw;
        if (raw.fromString(str)) {
            return transmit(raw);
        }
    } else if (Pulsetrain::maybe(str)) {
        Pulsetrain train;
        if (train.fromString(str)) {
            return transmit(train);
        }
    } else if (Meaning::maybe(str)) {
        Meaning meaning;
        if (meaning.fromString(str)) {
            return transmit(meaning);
        }
    } else if (str.indexOf(":") != -1) {
        String plugin_name;
        String tx_str;
        tools::split(str, ":", plugin_name, tx_str);
        return Device::transmit(plugin_name, tx_str);
    } else {
        ERROR("ERROR: string does not look like RawTimings, Pulsetrain or Meaning.\n");
    }
    return false;
}

/// @brief Transmits this `RawTimings` instance.
/// @param raw the instance to be transmitted
/// @return `true` if it worked, `false` if not. Will show error message telling you why it didn't work in latter case.
bool OOKwiz::transmit(RawTimings &raw) {
    if (!Radio::radio_tx()) {
        ERROR("ERROR: Transceiver could not be set to transmit.\n");
        return false;
    }
    INFO("Transmitting raw: %s\n", raw.toString().c_str());
    INFO("              %s\n", raw.visualizer().c_str());    
    delay(100);     // So INFO prints before we turn off interrupts
    transitionTimer->refresh();
    transitionTimer->resume();
    int32_t tx_timer = transitionTimer->getCount();
    noInterrupts();
    {
        bool bit = tx_active_high;
        PIN_WRITE(Radio::pin_tx, bit);
        for (uint16_t interval: raw.intervals) {
            delayMicroseconds(interval);
            bit = !bit;
            PIN_WRITE(Radio::pin_tx, bit);
        }
        PIN_WRITE(Radio::pin_tx, !tx_active_high);    // Just to make sure we end with TX off
    }
    interrupts();
    tx_timer = transitionTimer->getCount() - tx_timer;
    INFO("Transmission done, took %i µs.\n", tx_timer);
    delayMicroseconds(400);
    standby();
    return true;
}

/// @brief Transmits this `Pulsetrain` instance.
/// @param train the instance to be transmitted
/// @return `true` if it worked, `false` if not. Will show error message telling you why it didn't work in latter case.
bool OOKwiz::transmit(Pulsetrain &train) {
    if (!Radio::radio_tx()) {
        ERROR("ERROR: Transceiver could not be set to transmit.\n");
        return false;
    }
    INFO("Transmitting %s\n", train.toString().c_str());
    INFO("             %s\n", train.visualizer().c_str()); 
    delay(100);     // So INFO prints before we turn off interrupts
    transitionTimer->refresh();
    transitionTimer->resume();
    int32_t tx_timer = transitionTimer->getCount();
    for (int repeat = 0; repeat < train.repeats; repeat++) {
        noInterrupts();
        {
            bool bit = tx_active_high;
            PIN_WRITE(Radio::pin_tx, bit);
            for (int transition : train.transitions) {
                uint16_t t = train.bins[transition].average;
                delayMicroseconds(t);
                bit = !bit;
                PIN_WRITE(Radio::pin_tx, bit);
            }
            PIN_WRITE(Radio::pin_tx, !tx_active_high);    // Just to make sure we end with TX off
        }
        interrupts();
        delayMicroseconds(train.gap);
    }
    tx_timer = transitionTimer->getCount() - tx_timer;
    INFO("Transmission done, took %i µs.\n", tx_timer);
    delayMicroseconds(400);
    transitionTimer->pause();
    standby();
    return true;
}

/// @brief Transmits this `Meaning` instance.
/// @param meaning the instance to be transmitted
/// @return `true` if it worked, `false` if not. Will show error message telling you why it didn't work in latter case.
bool OOKwiz::transmit(Meaning &meaning) {
    Pulsetrain train;
    if (train.fromMeaning(meaning)) {
        return transmit(train);
    }
    return false;
}

/// @brief Sets radio standby mode, turning off reception
/// @return The counterpart to `receive()`, turns off reception.
bool OOKwiz::standby() {
    Radio::radio_standby();
    return true;
}

bool OOKwiz::sleep() {
    Radio::radio_standby();
    return true;
}