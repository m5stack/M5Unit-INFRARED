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
#include <wiring/m5_unit_unified_infrared_wiring.hpp>  // board-aware connection helpers (include last)

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

    // Board-aware I2C: NessoN1 -> SoftwareI2C (M5HAL), NanoC6/NanoH2 -> Ex_I2C, others -> Wire
    if (!m5::unit::wiring::addI2C(Units, unit) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }

    M5_LOGI("M5UnitUnified initialized");
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

#if !defined(ARDUINO)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>

#if CONFIG_FREERTOS_UNICORE
static inline void feedIdleTaskPeriodically(void)
{
    constexpr uint32_t FEED_INTERVAL_MS{2000};
    constexpr TickType_t FEED_SLEEP_TICKS{pdMS_TO_TICKS(5)};
    static uint32_t s_last_feed_ms{};
    const uint32_t now_ms{static_cast<uint32_t>(esp_timer_get_time() / 1000)};
    if (now_ms - s_last_feed_ms >= FEED_INTERVAL_MS) {
        s_last_feed_ms = now_ms;
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
