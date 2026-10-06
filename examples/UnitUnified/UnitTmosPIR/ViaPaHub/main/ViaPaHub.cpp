/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example of using UnitTmosPIR (STHS34PF80) via UnitPaHub

  Core ---> PaHub ---> ch:0 UnitTmosPIR
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedINFRARED.h>
#include <M5UnitUnifiedHUB.h>  // UnitPaHub
#include <M5Utility.h>
#include <wiring/m5_unit_unified_infrared_wiring.hpp>  // board-aware connection helpers (include last)

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::UnitPaHub hub;
m5::unit::UnitTmosPIR unit;

constexpr uint8_t TMOS_CHANNEL{0};  // PaHub channel the UnitTmosPIR is connected to

LGFX_Sprite sprite;

// Palette indices
constexpr uint8_t PAL_BG{0};
constexpr uint8_t PAL_WHITE{1};
constexpr uint8_t PAL_FRAME{2};
constexpr uint8_t PAL_IDLE{3};
constexpr uint8_t PAL_ACTIVE{4};

constexpr const char* UNIT_LABEL = "TmosPIR via PaHub";
constexpr uint32_t DRAW_INTERVAL_MS{100};  // Limit redraws; periodic data arrives faster than this
constexpr int32_t MIN_FULL_SCALE{1000};    // Lower bound of the auto-scaled bar range

int32_t full_scale{MIN_FULL_SCALE};  // Grows with the largest |value| seen so far
m5::utility::elapsed_time_t last_draw_ms{};

void draw_bar(const char* label, const int16_t value, const bool active, const int32_t y, const int32_t bar_h)
{
    const int32_t w     = sprite.width();
    const int32_t x0    = w / 4;
    const int32_t bar_w = w - x0 - 8;

    sprite.setTextDatum(middle_left);
    sprite.drawString(label, 8, y + bar_h / 2);

    const int32_t mag = std::min<int32_t>(std::abs(static_cast<int32_t>(value)), full_scale);
    const int32_t len = bar_w * mag / full_scale;
    sprite.fillRect(x0, y, len, bar_h, active ? PAL_ACTIVE : PAL_IDLE);
    sprite.drawRect(x0, y, bar_w, bar_h, PAL_FRAME);
}

void update_display(const m5::unit::sths34pf80::Data& d)
{
    const int32_t w = sprite.width();
    const int32_t h = sprite.height();

    full_scale = std::max<int32_t>(full_scale, std::abs(static_cast<int32_t>(d.presence())));
    full_scale = std::max<int32_t>(full_scale, std::abs(static_cast<int32_t>(d.motion())));

    sprite.fillSprite(PAL_BG);
    sprite.setTextColor(PAL_WHITE);
    sprite.setTextSize((w >= 480) ? 3.0f : (w >= 200) ? 2.0f : 1.0f);

    const int32_t line_h = sprite.fontHeight() + 4;
    const int32_t bar_h  = line_h;

    sprite.setTextDatum(top_center);
    sprite.drawString(UNIT_LABEL, w / 2, 4);

    draw_bar("Pres", d.presence(), d.isPresence(), h / 2 - bar_h - 4, bar_h);
    draw_bar("Mot", d.motion(), d.isMotion(), h / 2 + 4, bar_h);

    char buf[32];
    snprintf(buf, sizeof(buf), "Object: %.2f C", d.objectTemperature());
    sprite.setTextDatum(bottom_center);
    sprite.drawString(buf, w / 2, h - 4);

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
    sprite.setPaletteColor(PAL_BG, TFT_DARKGREEN);
    sprite.setPaletteColor(PAL_WHITE, TFT_WHITE);
    sprite.setPaletteColor(PAL_FRAME, TFT_LIGHTGREY);
    sprite.setPaletteColor(PAL_IDLE, TFT_CYAN);
    sprite.setPaletteColor(PAL_ACTIVE, TFT_ORANGE);

    if (!hub.add(unit, TMOS_CHANNEL)) {  // PaHub ch:0 -> UnitTmosPIR
        M5_LOGE("Failed to add children");
        m5::unit::wiring::failStop();
    }

    // Board-aware I2C for the PaHub: NessoN1 -> PortB GROVE (SoftwareI2C), NanoC6/NanoH2 -> Ex_I2C,
    // others -> Wire. The UnitTmosPIR is reached through the hub (added above), so only the hub is added here.
    if (!m5::unit::wiring::addI2C(Units, hub, 400000) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }

    M5_LOGI("M5UnitUnified initialized");
    M5_LOGI("%s", Units.debugInfo().c_str());
}

void loop()
{
    M5.update();
    Units.update();

    if (unit.updated()) {
        const auto d = unit.oldest();
        M5.Log.printf(">Object:%.2f\n>Ambient:%.2f\n>CompObj:%.2f\n", d.objectTemperature(), d.ambientTemperature(),
                      d.compensatedObjectTemperature());
        M5.Log.printf(">Presence:%d\n>Motion:%d\n>AmbientShock:%d\n", d.presence(), d.motion(), d.ambient_shock());
        M5.Log.printf(">isPresence:%u\n>isMotion:%u\n>isShock:%u\n", d.isPresence(), d.isMotion(), d.isAmbientShock());
        if (m5::utility::elapsedSince(last_draw_ms) >= DRAW_INTERVAL_MS) {
            last_draw_ms = m5::utility::millis();
            update_display(d);
        }
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
