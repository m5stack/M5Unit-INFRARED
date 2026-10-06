/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example of using UnitPIR (AS312) via UnitPbHub

  Core ---> PbHub ---> ch:3 UnitPIR
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedINFRARED.h>
#include <M5UnitUnifiedHUB.h>  // UnitPbHub
#include <M5Utility.h>
#include <wiring/m5_unit_unified_infrared_wiring.hpp>  // board-aware connection helpers (include last)

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::UnitPbHub hub;
m5::unit::UnitPIR pir;

uint32_t detect_count{};
m5::utility::elapsed_time_t last_detect_ms{};
bool ever_detected{};

LGFX_Sprite sprite;

// Palette indices
constexpr uint8_t PAL_GREEN{0};
constexpr uint8_t PAL_BLUE{1};
constexpr uint8_t PAL_WHITE{2};

constexpr const char* UNIT_LABEL = "PIR via PbHub";

void update_display(const bool detected)
{
    auto w = sprite.width();
    auto h = sprite.height();

    sprite.fillSprite(detected ? PAL_BLUE : PAL_GREEN);
    sprite.setTextColor(PAL_WHITE);
    sprite.setTextDatum(middle_center);

    float text_scale_large = (w >= 480) ? 4.0f : (w >= 200) ? 3.0f : 1.5f;
    float text_scale_small = (w >= 480) ? 3.0f : (w >= 200) ? 2.0f : 1.0f;

    auto cx = w / 2;
    auto cy = h / 2;

    sprite.setTextSize(text_scale_small);
    auto line_h = sprite.fontHeight() + 4;
    sprite.drawString(UNIT_LABEL, cx, cy - line_h * 2);

    sprite.setTextSize(text_scale_large);
    sprite.drawString(detected ? "PIR: DETECTED" : "PIR: CLEAR", cx, cy - line_h);

    sprite.setTextSize(text_scale_small);
    char buf[32];
    snprintf(buf, sizeof(buf), "Count: %lu", (unsigned long)detect_count);
    sprite.drawString(buf, cx, cy);

    if (ever_detected) {
        auto elapsed_ms = m5::utility::elapsedSince(last_detect_ms);
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
    M5.begin();
    M5.setTouchButtonHeightByRatio(100);

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

    if (!hub.add(pir, 3)) {  // PbHub ch:3 -> UnitPIR
        M5_LOGE("Failed to add children");
        m5::unit::wiring::failStop();
    }

    // Board-aware I2C for the PbHub: NessoN1 -> PortB GROVE (SoftwareI2C), NanoC6/NanoH2 -> Ex_I2C,
    // others -> Wire. The UnitPIR is reached through the hub (added above), so only the hub is added here.
    if (!m5::unit::wiring::addI2C(Units, hub, 400000) || !Units.begin()) {
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

    if (pir.updated()) {
        if (pir.wasDetected()) {
            ++detect_count;
            last_detect_ms = m5::utility::millis();
            ever_detected  = true;
            M5.Log.printf(">Detected:1\n>Count:%lu\n", (unsigned long)detect_count);
        }
        if (pir.wasReleased()) {
            M5.Log.printf(">Detected:0\n");
        }
        update_display(pir.isDetected());
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
