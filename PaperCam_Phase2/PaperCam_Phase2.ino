/*
 * PaperCam — Phase 2: Shutter button
 *
 * A momentary switch to GND, INPUT_PULLUP so the resting state is HIGH and a
 * press reads LOW. Set USE_BOOT_BUTTON below to pick which switch.
 *
 * Reports two distinct events over serial:
 *
 *   TAP   — a clean press and release under 1.5s.  Will trigger capture.
 *   HOLD  — the button held past 1.5s.             Reserved for Phase 7.
 *
 * No panel, no TFT_eSPI, no driver.h. If this sketch misbehaves it is the
 * button or the wiring, and nothing else.
 */

// TEMPORARY: no momentary switch on hand yet, so borrow the XIAO's onboard
// BOOT button. Set this to 0 once the real shutter is wired to D5 — that is
// the only change needed.
#define USE_BOOT_BUTTON 1

#if USE_BOOT_BUTTON
// GPIO0, hard-wired to the BOOT button on the XIAO and pulled up on-board.
// It is not exposed as a D-number: D0 is GPIO1, so this has to be the raw
// GPIO. Perfectly usable as an ordinary input once the chip is running —
// see the strapping-pin caveat in setup().
static constexpr uint8_t  PIN_SHUTTER = 0;
static constexpr char     PIN_LABEL[] = "BOOT button (GPIO0, temporary)";
#else
// D5 is a variant-provided constant that maps to GPIO6 on the XIAO ESP32S3.
// Using the silkscreen name keeps this readable against the board; setup()
// prints the numeric GPIO so we can confirm the mapping rather than assume it.
static constexpr uint8_t  PIN_SHUTTER = D5;
static constexpr char     PIN_LABEL[] = "D5 (GPIO6)";
#endif

// 25ms comfortably outrides the contact bounce on a cheap tactile switch
// (typically under 5ms) while staying far below the shortest deliberate press
// a human makes, so no real input is ever swallowed.
static constexpr uint32_t DEBOUNCE_MS = 25;

static constexpr uint32_t HOLD_MS     = 1500;

// ---------------------------------------------------------------------------
// Debounce and edge state.
//
// All of this is file-scope `static`: single instance, zero-initialised, not
// visible to other translation units. The closest MicroPython equivalent is a
// module-level variable, except `static` here also means "private to this
// file" — the linker will not let anything else reach it.
// ---------------------------------------------------------------------------

static bool     raw_pressed      = false;  // what the pin says right now
static bool     stable_pressed   = false;  // what we believe after debouncing
static uint32_t last_raw_change  = 0;      // when raw_pressed last flipped
static uint32_t press_started    = 0;      // when the debounced press began
static bool     hold_reported    = false;  // HOLD already fired this press
static uint32_t event_count      = 0;

// ---------------------------------------------------------------------------

void setup(void)
{
    Serial.begin(115200);
    delay(2000);  // USB CDC enumeration; not while(!Serial), see Phase 1

    Serial.println("\n=== PaperCam Phase 2 — shutter button ===");
    Serial.printf("Build: %s %s\n", __DATE__, __TIME__);
    Serial.printf("Shutter: %s\n", PIN_LABEL);

#if USE_BOOT_BUTTON
    // GPIO0 is a strapping pin: held LOW at reset or power-on, the ROM enters
    // download mode and this sketch never runs. Only matters while the button
    // is held *across* a reset — pressing it during normal operation is an
    // ordinary input read. Do not hold BOOT while plugging in or resetting.
    Serial.println("NOTE: BOOT is a strapping pin — do not hold it while resetting.");
#endif

    pinMode(PIN_SHUTTER, INPUT_PULLUP);

    // Let the pullup charge the pin and any cable capacitance before trusting
    // the first read.
    delay(10);

    // Resting state should be HIGH. LOW here means the button is stuck closed,
    // wired to something other than GND, or on the wrong pin — all of which
    // present later as "every event fires at once", which is a confusing way
    // to discover a wiring fault.
    const bool resting_high = (digitalRead(PIN_SHUTTER) == HIGH);
    Serial.printf("Resting state: %s %s\n",
                  resting_high ? "HIGH" : "LOW",
                  resting_high ? "(correct — pullup holding, button open)"
                               : "(WRONG — expected HIGH; button held, stuck, or miswired)");

    // Seed the debouncer with reality so a press already held at boot does not
    // register as a fresh edge on the first loop.
    raw_pressed     = !resting_high;
    stable_pressed  = raw_pressed;
    last_raw_change = millis();

    Serial.println("\nReady. Tap for TAP, hold 1.5s for HOLD.");
}

void loop(void)
{
    const uint32_t now = millis();
    const bool     now_pressed = (digitalRead(PIN_SHUTTER) == LOW);

    // --- Debounce ---------------------------------------------------------
    //
    // Every time the raw reading flips, restart the settling clock. The state
    // is only promoted to `stable_pressed` once the pin has held the same
    // value for DEBOUNCE_MS. Bounce keeps resetting the clock and therefore
    // never promotes.
    //
    // `now - last_raw_change` on unsigned types is safe across the millis()
    // rollover at ~49.7 days: the subtraction wraps modulo 2^32 and yields the
    // correct elapsed time. This is the C detail worth internalising, because
    // MicroPython's utime.ticks_diff() exists precisely because you cannot do
    // this with Python ints. Here the wrap is the feature.

    if (now_pressed != raw_pressed) {
        raw_pressed     = now_pressed;
        last_raw_change = now;
    }

    if ((now - last_raw_change) >= DEBOUNCE_MS && raw_pressed != stable_pressed) {
        stable_pressed = raw_pressed;

        if (stable_pressed) {
            press_started = now;
            hold_reported = false;
        } else if (!hold_reported) {
            // Released before the hold threshold, so this was a tap. If HOLD
            // already fired we stay quiet — one physical press produces
            // exactly one logical event, never both.
            event_count++;
            Serial.printf("TAP   %4lu ms   #%lu\n",
                          (unsigned long)(now - press_started),
                          (unsigned long)event_count);
        }
    }

    // --- Hold detection ---------------------------------------------------
    //
    // Fires the instant the threshold passes rather than waiting for release.
    //
    // The alternative — classify on release — is simpler, but it means holding
    // the button produces no feedback until you let go, so you cannot tell
    // whether the device registered the hold without releasing and finding
    // out. Firing at the threshold is what lets Phase 7 light the LED at the
    // moment the mode changes. The cost is the `hold_reported` flag to
    // suppress the tap that would otherwise follow on release.

    if (stable_pressed && !hold_reported && (now - press_started) >= HOLD_MS) {
        hold_reported = true;
        event_count++;
        Serial.printf("HOLD  %4lu ms   #%lu\n",
                      (unsigned long)HOLD_MS,
                      (unsigned long)event_count);
    }

    // No delay(). Polling freely keeps latency at microseconds and costs
    // nothing we need yet; from Phase 5 the loop has real work to do and a
    // blocking delay here would sit in front of it.
}
