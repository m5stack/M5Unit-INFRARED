/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for UnitTmosPIR
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedINFRARED.h>
#include <M5Utility.h>
#include <M5HAL.hpp>  // For NessoN1

using namespace m5::unit::sths34pf80;

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::UnitTmosPIR unit;
};  // namespace

void setup()
{
    M5.begin();
    M5.setTouchButtonHeightByRatio(100);

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

    auto board       = M5.getBoard();
    auto pin_num_sda = M5.getPin(m5::pin_name_t::port_a_sda);
    auto pin_num_scl = M5.getPin(m5::pin_name_t::port_a_scl);

    // For NessoN1 GROVE
    if (board == m5::board_t::board_ArduinoNessoN1) {
        // Port A of the NessoN1 is QWIIC, then use portB (GROVE)
        pin_num_sda = M5.getPin(m5::pin_name_t::port_b_out);
        pin_num_scl = M5.getPin(m5::pin_name_t::port_b_in);
        M5_LOGI("getPin(NessoN1): SDA:%u SCL:%u", pin_num_sda, pin_num_scl);
        // Wire is used internally, so SoftwareI2C handles the unit
        m5::hal::bus::I2CBusConfig i2c_cfg;
        i2c_cfg.pin_sda = m5::hal::gpio::getPin(pin_num_sda);
        i2c_cfg.pin_scl = m5::hal::gpio::getPin(pin_num_scl);
        auto i2c_bus    = m5::hal::bus::i2c::getBus(i2c_cfg);
        M5_LOGI("Bus:%d", i2c_bus.has_value());
        if (!Units.add(unit, i2c_bus ? i2c_bus.value() : nullptr) || !Units.begin()) {
            M5_LOGE("Failed to begin");
            lcd.fillScreen(TFT_RED);
            while (true) {
                m5::utility::delay(10000);
            }
        }
    } else {
        // Using TwoWire
        M5_LOGI("getPin: SDA:%u SCL:%u", pin_num_sda, pin_num_scl);
        Wire.end();
        Wire.begin(pin_num_sda, pin_num_scl, 400 * 1000U);
        if (!Units.add(unit, Wire) || !Units.begin()) {
            M5_LOGE("Failed to begin");
            lcd.fillScreen(TFT_RED);
            while (true) {
                m5::utility::delay(10000);
            }
        }
    }

    M5_LOGI("M5UnitUnified has been begun");
    M5_LOGI("%s", Units.debugInfo().c_str());
    lcd.fillScreen(TFT_DARKGREEN);
}

void loop()
{
    M5.update();
    Units.update();

    // Periodic
    if (unit.updated()) {
        auto d = unit.oldest();
        M5.Log.printf(">Object:%.2f\n>Ambient:%.2f\n>CompObj:%.2f\n", d.objectTemperature(), d.ambientTemperature(),
                      d.compensatedObjectTemperature());
        M5.Log.printf(">Presence:%d\n>Motion:%d\n>AmbientShock:%d\n", d.presence(), d.motion(), d.ambient_shock());
        M5.Log.printf(">isPresence:%u\n>isMotion:%u\n>isShock:%u\n", d.isPresence(), d.isMotion(), d.isAmbientShock());
    }

    // Toggle single <-> periodic
    if (M5.BtnA.wasClicked()) {
        static bool single{};
        single = !single;

        if (single) {
            unit.stopPeriodicMeasurement();
            Data d{};
            if (unit.measureSingleshot(d, AmbientTemperatureAverage::Samples8, ObjectTemperatureAverage::Samples128)) {
                M5.Speaker.tone(3000, 30);
                M5.Log.printf("Single:\n");
                M5.Log.printf("Object:%.2f/%d Ambient:%.2f/%d CompObj:%.2f/%d\n", d.objectTemperature(), d.object(),
                              d.ambientTemperature(), d.ambient(), d.compensatedObjectTemperature(),
                              d.compensated_object());
            }
        } else {
            M5.Speaker.tone(4000, 20);
            auto cfg = unit.config();
            unit.writeAverageTrim(cfg.avg_t, cfg.avg_tmos);
            unit.startPeriodicMeasurement(cfg.mode, cfg.odr, cfg.comp_type, cfg.abs);
        }
    }

    // Reset
    if (M5.BtnA.wasHold()) {
        M5.Speaker.tone(2000, 30);
        unit.stopPeriodicMeasurement();
        unit.softReset();
        M5.Speaker.tone(2000, 30);
        auto cfg = unit.config();
        unit.writeAverageTrim(cfg.avg_t, cfg.avg_tmos);
        unit.startPeriodicMeasurement(cfg.mode, cfg.odr, cfg.comp_type, cfg.abs);
    }
}
