#include "OOKwiz.h"
#include "serial_output.h"

volatile OOKwiz::Rx_State OOKwiz::rx_state = OOKwiz::RX_OFF;
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
int64_t OOKwiz::last_transition;
hw_timer_t* OOKwiz::transitionTimer = nullptr;
int64_t OOKwiz::repeat_time_start = 0;
long OOKwiz::repeat_timeout;
bool OOKwiz::rx_active_high;
bool OOKwiz::tx_active_high;
RawTimings OOKwiz::isr_in;
RawTimings OOKwiz::isr_out;
BufferPair OOKwiz::loop_in;
BufferPair OOKwiz::loop_compare;
BufferTriplet OOKwiz::loop_ready;
int64_t OOKwiz::last_periodic = 0;
void (*OOKwiz::callback)(RawTimings, Pulsetrain, Meaning) = nullptr;

/// @brief Starts OOKwiz. Loads settings, initializes the radio and starts receiving if it finds the appropriate settings.
/**
 * If you set the GPIO pin for a button on your ESP32 in 'pin_rescue' and press it during boot, OOKwiz will not
 * initialize SPI and the radio, possibly breaking an endless boot loop. Set 'rescue_active_high' if the button
 * connects to VCC instead of GND.
 * 
 * Normally, OOKwiz will start up in receive mode. If you set 'start_in_standby', it will start in standby mode instead.
*/
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
    rx_active_high = Settings::isSet("rx_active_high");
    tx_active_high = Settings::isSet("tx_active_high");

    // Timer for pulse_gap_len_new_packet
    transitionTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(transitionTimer, &ISR_transitionTimeout, false);
    timerAlarmWrite(transitionTimer, pulse_gap_len_new_packet, true);
    timerAlarmEnable(transitionTimer);
    timerStart(transitionTimer);

    // The ISR that actually reads the data
    attachInterrupt(Radio::pin_rx, ISR_transition, CHANGE);    

    if (Settings::isSet("start_in_standby")) {
        return standby();
    } else {
        return receive();
    }

}

void IRAM_ATTR OOKwiz::ISR_transition() {
    int64_t t = esp_timer_get_time() - last_transition;
    last_transition = esp_timer_get_time();
    if (rx_state == RX_WAIT_PREAMBLE) {
        // Set the state machine to put the transitions in isr_in
        if (t > first_pulse_min_len && digitalRead(Radio::pin_rx) != rx_active_high) {
            noise_score = 0;
            isr_in.zap();
            isr_in.intervals.reserve((max_nr_pulses * 2) + 1);
            rx_state = RX_RECEIVING_DATA;
        }
    }
    if (rx_state == RX_RECEIVING_DATA) {
        // t < pulse_gap_min_len means it's noise
        if (t < pulse_gap_min_len) {
            noise_score += noise_penalty;
            if (noise_score >= noise_threshold) {
                process_raw();
                return;
            }
        } else {
            noise_score -= noise_score > 0;
        }
        isr_in.intervals.push_back(t);
        // Longer would be too long: stop and process what we have
        if (isr_in.intervals.size() == (max_nr_pulses * 2) + 1) {
            process_raw();
        }

    }
    timerRestart(transitionTimer);
}

void IRAM_ATTR OOKwiz::ISR_transitionTimeout() {
    if (rx_state != RX_OFF) {
        process_raw();
    }
}

void IRAM_ATTR OOKwiz::process_raw() {
    if (!isr_out) {
        isr_out = isr_in;
    } else {
        lost_packets++;
    }
    isr_in.zap();
    rx_state = RX_WAIT_PREAMBLE;
}

/// @brief Use this to supply your own function that will be called every time a packet is received.
/**
 * The callback_function parameter has to be the function name of a function that takes the three 
 * packet representations as arguments and does not return anything. Here's an example sketch:
 * 
 * ```
 * setup() {
 *     Serial.begin(115200);
 *     OOKwiz::setup();
 *     OOKwiz::onReceive(myReceiveFunction);
 * }
 * 
 * loop() {
 *     OOKwiz::loop();
 * }
 * 
 * void myReceiveFunction(RawTimings raw, Pulsetrain train, Meaning meaning) {
 *     Serial.println("A packet was received and myReceiveFunction was called.");
 * }
 * ```
 * 
 * Make sure your own function is defined exactly as like this, even if you don't need all the
 * parameters. You may change the names of the function and the parameters, but nothing else. 
*/
/// @param callback_function The name of your own function, without parenthesis () after it. 
/// @return always returns `true`
bool OOKwiz::onReceive(void (*callback_function)(RawTimings, Pulsetrain, Meaning)) {
    callback = callback_function;
    return true;
}

/// @brief Tell OOKwiz to start receiving and processing packets.
/**
 * OOKwiz starts in receive mode normally, so you would only need to call this if your
 * code has turned off reception (with `standby()`) or if you configured OOKwiz to not
 * start in receive mode by setting `start_in_standby`.
*/
/// @return `true` if receive mode could be activated, `false` if not.
bool OOKwiz::receive() {
    if (!Radio::radio_rx()) {
        return false;
    }
    // Turns on the state machine
    rx_state = RX_WAIT_PREAMBLE;
    return true;
}

bool OOKwiz::tryToBeNice(int ms) {
    // Try and wait for max ms for current reception to end
    // return false if it doesn't end, true if it does
    long start = millis();
    while (millis() - start < ms) {
        if (rx_state == RX_WAIT_PREAMBLE) {
            return true;
        }
    }
    return false;
}

/// @brief Pretends this string representation of a `RawTimings`, `Pulsetrain` or `Meaning` instance was just received by the radio.
/// @param str The string representation of what needs to be simulated
/// @return `true` if it worked, `false` if not. Will show error message telling you why it didn't work in latter case.
bool OOKwiz::simulate(String &str) {
    if (RawTimings::maybe(str)) {
        RawTimings raw;
        if (raw.fromString(str)) {
            return simulate(raw);
        }
    } else if (Pulsetrain::maybe(str)) {
        Pulsetrain train;
        if (train.fromString(str)) {
            return simulate(train);
        }
    } else if (Meaning::maybe(str)) {
        Meaning meaning;
        if (meaning.fromString(str)) {
            return simulate(meaning);
        }
    } else {
        ERROR("ERROR: string does not look like RawTimings, Pulsetrain or Meaning.\n");
    }
    return false;
}

/// @brief Pretends this `RawTimings` instance was just received by the radio.
/// @param raw  the instance to be simulated
/// @return `true` if it worked, `false` if not. Will show error message telling you why it didn't work in latter case.
bool OOKwiz::simulate(RawTimings &raw) {
    tryToBeNice(50);
    isr_out = raw;
    return true;
}

/// @brief Pretends this `Pulsetrain` instance was just received by the radio.
/// @param train  the instance to be simulated
/// @return `true` if it worked, `false` if not. Will show error message telling you why it didn't work in latter case.
bool OOKwiz::simulate(Pulsetrain &train) {
    tryToBeNice(50);
    loop_ready.train = train;
    return true;
}

/// @brief Pretends this `Meaning` instance was just received by the radio.
/// @param meaning the instance to be simulated
/// @return `true` if it worked, `false` if not. Will show error message telling you why it didn't work in latter case.
bool OOKwiz::simulate(Meaning &meaning) {
    Pulsetrain train;
    if (train.fromMeaning(meaning)) {
        return simulate(train);
    }
    return false;
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
    bool rx_was_on = (rx_state != RX_OFF);
    // Set receiver state machine off, remove any incomplete packet in buffer
    if (rx_was_on) {
        tryToBeNice(500);
        rx_state = RX_OFF;
        isr_in.zap();
    }
    if (!Radio::radio_tx()) {
        ERROR("ERROR: Transceiver could not be set to transmit.\n");
        return false;
    }
    INFO("Transmitting: %s\n", raw.toString().c_str());
    INFO("              %s\n", raw.visualizer().c_str());    
    delay(100);     // So INFO prints before we turn off interrupts
    int64_t tx_timer = esp_timer_get_time();
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
    tx_timer = esp_timer_get_time() - tx_timer;
    INFO("Transmission done, took %i µs.\n", tx_timer);
    delayMicroseconds(400);
    // return to state it was in before transmit
    if (rx_was_on) {
        receive();
    } else {
        standby();
    }
    return true;
}

/// @brief Transmits this `Pulsetrain` instance.
/// @param train the instance to be transmitted
/// @return `true` if it worked, `false` if not. Will show error message telling you why it didn't work in latter case.
bool OOKwiz::transmit(Pulsetrain &train) {
    bool rx_was_on = (rx_state != RX_OFF);
    // Set receiver state machine off, remove any incomplete packet in buffer
    if (rx_was_on) {
        tryToBeNice(500);
        rx_state = RX_OFF;
        isr_in.zap();
    }
    if (!Radio::radio_tx()) {
        ERROR("ERROR: Transceiver could not be set to transmit.\n");
        return false;
    }
    INFO("Transmitting %s\n", train.toString().c_str());
    INFO("             %s\n", train.visualizer().c_str()); 
    delay(100);     // So INFO prints before we turn off interrupts
    int64_t tx_timer = esp_timer_get_time();
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
    tx_timer = esp_timer_get_time() - tx_timer;
    INFO("Transmission done, took %i µs.\n", tx_timer);
    delayMicroseconds(400);
    // return to state it was in before transmit
    if (rx_was_on) {
        receive();
    } else {
        standby();
    }
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
    if (rx_state != RX_OFF) {
        tryToBeNice(500);
        isr_in.zap();
        rx_state = RX_OFF;
        Radio::radio_standby();
    }
    return true;
}

