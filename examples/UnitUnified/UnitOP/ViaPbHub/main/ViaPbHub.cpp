/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example of using UnitOP (ITR9606) via UnitPbHub
  Supports both UnitOP90 (U057) and UnitOP180 (U058)

  Core ---> PbHub ---> ch:4 UnitOP
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5HAL.hpp>
#include <M5UnitUnifiedINFRARED.h>
#include <M5UnitUnifiedHUB.h>  // UnitPbHub
#include <M5Utility.h>

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::UnitPbHub hub;
m5::unit::UnitOP unit;

uint32_t detect_count{};
unsigned long last_detect_ms{};
bool ever_detected{};

LGFX_Sprite sprite;

// Palette indices
constexpr uint8_t PAL_GREEN{0};
constexpr uint8_t PAL_BLUE{1};
constexpr uint8_t PAL_WHITE{2};

constexpr const char* UNIT_LABEL = "OP via PbHub";

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
    sprite.drawString(detected ? "BLOCKED" : "CLEAR", cx, cy - line_h);

    sprite.setTextSize(text_scale_small);
    char buf[32];
    snprintf(buf, sizeof(buf), "Count: %lu", (unsigned long)detect_count);
    sprite.drawString(buf, cx, cy);

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

    if (!hub.add(unit, 4)) {  // PbHub ch:4 -> UnitOP
        M5_LOGE("Failed to add children");
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

    auto board = M5.getBoard();

    // NessoN1: Arduino Wire (I2C_NUM_0) cannot be used for GROVE port.
    //   Wire is used by M5Unified In_I2C for internal devices.
    //   Reconfiguring Wire to GROVE pins breaks In_I2C.
    //   Solution: Use SoftwareI2C via M5HAL for the GROVE port.
    // NanoC6: Wire.begin() on GROVE pins conflicts with m5::I2C_Class
    //   registered by Ex_I2C.setPort() on the same I2C_NUM_0.
    //   Solution: Use M5.Ex_I2C directly instead of Arduino Wire.
    bool unit_ready{};
    if (board == m5::board_t::board_ArduinoNessoN1) {
        auto pin_num_sda = M5.getPin(m5::pin_name_t::port_a_sda);
        auto pin_num_scl = M5.getPin(m5::pin_name_t::port_a_scl);
        M5_LOGI("getPin(M5HAL): SDA:%u SCL:%u", pin_num_sda, pin_num_scl);
        m5::hal::bus::I2CBusConfig i2c_cfg;
        i2c_cfg.pin_sda = m5::hal::gpio::getPin(pin_num_sda);
        i2c_cfg.pin_scl = m5::hal::gpio::getPin(pin_num_scl);
        auto i2c_bus    = m5::hal::bus::i2c::getBus(i2c_cfg);
        M5_LOGI("Bus:%d", i2c_bus.has_value());
        unit_ready = Units.add(hub, i2c_bus ? i2c_bus.value() : nullptr) && Units.begin();
    } else if (board == m5::board_t::board_M5NanoC6) {
        M5_LOGI("Using M5.Ex_I2C");
        unit_ready = Units.add(hub, M5.Ex_I2C) && Units.begin();
    } else {
        auto pin_num_sda = M5.getPin(m5::pin_name_t::port_a_sda);
        auto pin_num_scl = M5.getPin(m5::pin_name_t::port_a_scl);
        M5_LOGI("getPin: SDA:%u SCL:%u", pin_num_sda, pin_num_scl);
        Wire.end();
        Wire.begin(pin_num_sda, pin_num_scl, 400000U);
        unit_ready = Units.add(hub, Wire) && Units.begin();
    }

    if (!unit_ready) {
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
