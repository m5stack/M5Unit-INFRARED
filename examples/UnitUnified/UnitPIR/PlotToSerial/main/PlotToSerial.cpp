/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for UnitPIR / HatPIR (AS312)
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedINFRARED.h>
#include <M5Utility.h>
#include <wiring/m5_unit_unified_infrared_wiring.hpp>  // board-aware connection helpers (include last)

// *************************************************************
// Choose one define symbol to match the unit you are using
// *************************************************************
#if !defined(USING_UNIT_PIR) && !defined(USING_HAT_PIR)
// For UnitPIR (U004)
// #define USING_UNIT_PIR
// For HatPIR (U054)
// #define USING_HAT_PIR
#endif

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
#if defined(USING_UNIT_PIR)
#pragma message "Using UnitPIR (U004)"
m5::unit::UnitPIR unit;
#elif defined(USING_HAT_PIR)
#pragma message "Using HatPIR (U054)"
m5::unit::HatPIR unit;
#else
#error Please choose unit!
#endif

uint32_t detect_count{};
unsigned long last_detect_ms{};
bool ever_detected{};
bool prev_display_detected{};
bool display_initialized{};

LGFX_Sprite sprite;

// Palette indices
constexpr uint8_t PAL_GREEN{0};
constexpr uint8_t PAL_BLUE{1};
constexpr uint8_t PAL_WHITE{2};

#if defined(USING_UNIT_PIR)
constexpr const char* UNIT_LABEL = "UnitPIR (U004)";
#elif defined(USING_HAT_PIR)
constexpr const char* UNIT_LABEL = "HatPIR (U054)";
#endif

void update_display(const bool detected)
{
    auto w = sprite.width();
    auto h = sprite.height();

    // Background color
    sprite.fillSprite(detected ? PAL_BLUE : PAL_GREEN);
    sprite.setTextColor(PAL_WHITE);
    sprite.setTextDatum(middle_center);

    // Adaptive text size based on screen width
    float text_scale_large = (w >= 480) ? 4.0f : (w >= 200) ? 3.0f : 1.5f;
    float text_scale_small = (w >= 480) ? 3.0f : (w >= 200) ? 2.0f : 1.0f;

    auto cx = w / 2;
    auto cy = h / 2;

    // Line 0: Unit/Hat label
    sprite.setTextSize(text_scale_small);
    auto line_h = sprite.fontHeight() + 4;
    sprite.drawString(UNIT_LABEL, cx, cy - line_h * 2);

    // Line 1: PIR status
    sprite.setTextSize(text_scale_large);
    sprite.drawString(detected ? "PIR: DETECTED" : "PIR: CLEAR", cx, cy - line_h);

    // Line 2: Count
    sprite.setTextSize(text_scale_small);
    char buf[32];
    snprintf(buf, sizeof(buf), "Count: %lu", (unsigned long)detect_count);
    sprite.drawString(buf, cx, cy);

    // Line 3: Last detection elapsed
    if (ever_detected) {
        auto elapsed_ms = m5::utility::millis() - last_detect_ms;
        auto elapsed_s  = elapsed_ms / 1000;
        snprintf(buf, sizeof(buf), "Last: %lu.%lus ago", (unsigned long)(elapsed_s),
                 (unsigned long)((elapsed_ms / 100) % 10));
    } else {
        snprintf(buf, sizeof(buf), "Last: --");
    }
    sprite.drawString(buf, cx, cy + line_h);

    lcd.startWrite();
    sprite.pushSprite(&lcd, 0, 0);
    lcd.endWrite();
}

}  // namespace

void setup()
{
    auto m5cfg = M5.config();
#if defined(USING_HAT_PIR)
    m5cfg.pmic_button  = false;  // Disable BtnPWR
    m5cfg.internal_imu = false;  // Disable internal IMU
    m5cfg.internal_rtc = false;  // Disable internal RTC
#endif

    M5.begin(m5cfg);

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

    // Create sprite for flicker-free rendering (minimal memory: 4-bit palette)
    sprite.setColorDepth(4);
    sprite.setPsram(false);
    if (!sprite.createSprite(lcd.width(), lcd.height())) {
        // Fallback to PSRAM for large screens (e.g. Tab5)
        sprite.setPsram(true);
        sprite.createSprite(lcd.width(), lcd.height());
    }
    sprite.createPalette();
    sprite.setPaletteColor(0, TFT_DARKGREEN);
    sprite.setPaletteColor(1, TFT_BLUE);
    sprite.setPaletteColor(2, TFT_WHITE);

#if defined(USING_HAT_PIR)
    // HatPIR (U054): single DOUT input on the board's Hat header
    bool unit_ready = m5::unit::infrared::wiring::addHatPIR(Units, unit) && Units.begin();
#else
    // UnitPIR (U004): input-only PIR, PortB preferred, fallback to PortA
    bool unit_ready = m5::unit::wiring::addGPIO(Units, unit, m5::unit::wiring::GpioRole::InOnly) && Units.begin();
#endif

    if (!unit_ready) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }

    M5_LOGI("M5UnitUnified initialized");
    M5_LOGI("%s", Units.debugInfo().c_str());
    update_display(false);
}

void loop()
{
    M5.update();
    Units.update();

    if (unit.updated()) {
        if (unit.wasDetected()) {
            ++detect_count;
            last_detect_ms = m5::utility::millis();
            ever_detected  = true;
            M5.Log.printf(">Detected:1\n>Count:%lu\n", (unsigned long)detect_count);
        }
        if (unit.wasReleased()) {
            M5.Log.printf(">Detected:0\n");
        }
        update_display(unit.isDetected());
    }
}

#if !defined(ARDUINO)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>

#if CONFIG_FREERTOS_UNICORE
static inline void feedIdleTaskPeriodically(void)
{
    constexpr uint32_t FEED_INTERVAL_MS   = 2000;
    constexpr TickType_t FEED_SLEEP_TICKS = pdMS_TO_TICKS(5);
    static uint32_t s_next_feed_ms        = 0;
    const uint32_t now_ms                 = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    if (now_ms >= s_next_feed_ms) {
        s_next_feed_ms = now_ms + FEED_INTERVAL_MS;
        vTaskDelay(FEED_SLEEP_TICKS);
    }
}
#endif

extern "C" void app_main(void)
{
    setup();
    for (;;) {
#if CONFIG_FREERTOS_UNICORE
        feedIdleTaskPeriodically();
#endif
        loop();
    }
}
#endif
