/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for UnitOP (ITR9606)
  Supports both UnitOP90 (U057) and UnitOP180 (U058)
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedINFRARED.h>
#include <M5Utility.h>

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::UnitOP unit;

uint32_t detect_count{};
unsigned long last_detect_ms{};
bool ever_detected{};

LGFX_Sprite sprite;

// Palette indices
constexpr uint8_t PAL_GREEN{0};
constexpr uint8_t PAL_BLUE{1};
constexpr uint8_t PAL_WHITE{2};

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

    // Line 0: Unit label
    sprite.setTextSize(text_scale_small);
    auto line_h = sprite.fontHeight() + 4;
    sprite.drawString("UnitOP (ITR9606)", cx, cy - line_h * 2);

    // Line 1: Detection status
    sprite.setTextSize(text_scale_large);
    sprite.drawString(detected ? "BLOCKED" : "CLEAR", cx, cy - line_h);

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
    M5.begin();

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

    // UnitOP: Use Port B (GPIO).  If not available, fallback to Port A pins.
    auto pin_in  = M5.getPin(m5::pin_name_t::port_b_in);
    auto pin_out = M5.getPin(m5::pin_name_t::port_b_out);
    if (pin_in < 0 || pin_out < 0) {
        M5_LOGW("PortB is not available, using PortA");
        pin_in  = M5.getPin(m5::pin_name_t::port_a_pin1);
        pin_out = M5.getPin(m5::pin_name_t::port_a_pin2);
    }
    M5_LOGI("UnitOP GPIO: IN:%d OUT:%d", pin_in, pin_out);

    if (!Units.add(unit, pin_in, pin_out) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
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
            M5.Log.printf(">Blocked:1\n>Count:%lu\n", (unsigned long)detect_count);
        }
        if (unit.wasReleased()) {
            M5.Log.printf(">Blocked:0\n");
        }
        update_display(unit.isDetected());
    }
}
