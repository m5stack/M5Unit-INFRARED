/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  UnitTest for UnitSTHS34PF80
*/
#include <gtest/gtest.h>
#include <Wire.h>
#include <M5Unified.h>
#include <M5UnitUnified.hpp>
#include <googletest/test_template.hpp>
#include <googletest/test_helper.hpp>
#include <unit/unit_STHS34PF80.hpp>
#include <m5_unit_component/adapter_i2c.hpp>
#include <cmath>
#include <esp_random.h>

using namespace m5::unit::googletest;
using namespace m5::unit;
using namespace m5::unit::sths34pf80;
using namespace m5::unit::sths34pf80::command;
using m5::unit::types::elapsed_time_t;

constexpr uint32_t STORED_SIZE{4};
class TestSTHS34PF80 : public I2CComponentTestBase<UnitSTHS34PF80> {
protected:
    virtual UnitSTHS34PF80* get_instance() override
    {
        auto ptr = new m5::unit::UnitSTHS34PF80();
        if (ptr) {
            auto ccfg        = ptr->component_config();
            ccfg.stored_size = STORED_SIZE;
            ptr->component_config(ccfg);
        }
        return ptr;
    }
};

namespace {
constexpr AmbientTemperatureAverage avg_t_table[] = {
    AmbientTemperatureAverage::Samples8,
    AmbientTemperatureAverage::Samples4,
    AmbientTemperatureAverage::Samples2,
    AmbientTemperatureAverage::Samples1,
};

constexpr ObjectTemperatureAverage avg_tmos_table[] = {
    ObjectTemperatureAverage::Samples2,    ObjectTemperatureAverage::Samples8,    ObjectTemperatureAverage::Samples32,
    ObjectTemperatureAverage::Samples128,  ObjectTemperatureAverage::Samples256,  ObjectTemperatureAverage::Samples512,
    ObjectTemperatureAverage::Samples1024, ObjectTemperatureAverage::Samples2048,
};

void low_pass_filter_validation(UnitSTHS34PF80* unit)
{
    using LPF = LowPassFilter;

    constexpr LPF valid_table[][3 /*lpf_p_m, lpf_m, lpf_p */] = {
        {LPF::ODR9, LPF::ODR20, LPF::ODR20},     {LPF::ODR9, LPF::ODR800, LPF::ODR800},
        {LPF::ODR20, LPF::ODR50, LPF::ODR50},    {LPF::ODR20, LPF::ODR800, LPF::ODR800},
        {LPF::ODR50, LPF::ODR100, LPF::ODR100},  {LPF::ODR50, LPF::ODR800, LPF::ODR800},
        {LPF::ODR100, LPF::ODR200, LPF::ODR200}, {LPF::ODR100, LPF::ODR800, LPF::ODR800},
        {LPF::ODR200, LPF::ODR400, LPF::ODR400}, {LPF::ODR200, LPF::ODR800, LPF::ODR800},
        {LPF::ODR400, LPF::ODR800, LPF::ODR800},
    };
    constexpr LPF invalid_table[][3 /*lpf_p_m, lpf_m, lpf_p */] = {
        {LPF::ODR9, LPF::ODR9, LPF::ODR9},       {LPF::ODR20, LPF::ODR20, LPF::ODR50},
        {LPF::ODR20, LPF::ODR50, LPF::ODR20},    {LPF::ODR20, LPF::ODR20, LPF::ODR20},
        {LPF::ODR20, LPF::ODR9, LPF::ODR9},      {LPF::ODR50, LPF::ODR50, LPF::ODR100},
        {LPF::ODR50, LPF::ODR100, LPF::ODR50},   {LPF::ODR50, LPF::ODR50, LPF::ODR50},
        {LPF::ODR50, LPF::ODR9, LPF::ODR9},      {LPF::ODR100, LPF::ODR100, LPF::ODR200},
        {LPF::ODR100, LPF::ODR200, LPF::ODR100}, {LPF::ODR100, LPF::ODR100, LPF::ODR100},
        {LPF::ODR100, LPF::ODR9, LPF::ODR9},     {LPF::ODR200, LPF::ODR200, LPF::ODR400},
        {LPF::ODR200, LPF::ODR400, LPF::ODR200}, {LPF::ODR200, LPF::ODR200, LPF::ODR200},
        {LPF::ODR200, LPF::ODR9, LPF::ODR9},     {LPF::ODR400, LPF::ODR400, LPF::ODR800},
        {LPF::ODR400, LPF::ODR800, LPF::ODR400}, {LPF::ODR400, LPF::ODR400, LPF::ODR400},
        {LPF::ODR400, LPF::ODR9, LPF::ODR9},     {LPF::ODR800, LPF::ODR800, LPF::ODR800},
        {LPF::ODR800, LPF::ODR9, LPF::ODR9},
    };

    // Valid
    for (auto&& lpf : valid_table) {
        EXPECT_TRUE(unit->writeLowPassFilter(lpf[0], lpf[1], lpf[2], lpf[0]));
        LPF lpf_p_m{}, lpf_m{}, lpf_p{}, lpf_a_t{};
        EXPECT_TRUE(unit->readLowPassFilter(lpf_p_m, lpf_m, lpf_p, lpf_a_t));
        EXPECT_EQ(lpf_p_m, lpf[0]);
        EXPECT_EQ(lpf_m, lpf[1]);
        EXPECT_EQ(lpf_p, lpf[2]);
        EXPECT_EQ(lpf_a_t, lpf[0]);
    }

    // Invalid
    LPF prev_lpf_p_m{}, prev_lpf_m{}, prev_lpf_p{}, prev_lpf_a_t{};
    EXPECT_TRUE(unit->readLowPassFilter(prev_lpf_p_m, prev_lpf_m, prev_lpf_p, prev_lpf_a_t));

    for (auto&& lpf : invalid_table) {
        EXPECT_FALSE(unit->writeLowPassFilter(lpf[0], lpf[1], lpf[2], lpf[0]));
        LPF lpf_p_m{}, lpf_m{}, lpf_p{}, lpf_a_t{};
        EXPECT_TRUE(unit->readLowPassFilter(lpf_p_m, lpf_m, lpf_p, lpf_a_t));
        EXPECT_EQ(lpf_p_m, prev_lpf_p_m);
        EXPECT_EQ(lpf_m, prev_lpf_m);
        EXPECT_EQ(lpf_p, prev_lpf_p);
        EXPECT_EQ(lpf_a_t, prev_lpf_a_t);
    }
}

using AT                                       = ObjectTemperatureAverage;
constexpr std::pair<AT, ODR> odr_valid_table[] = {
    {AT::Samples2, ODR::Rate0_25},    {AT::Samples2, ODR::Rate30},      //
    {AT::Samples8, ODR::Rate0_25},    {AT::Samples8, ODR::Rate30},      //
    {AT::Samples32, ODR::Rate0_25},   {AT::Samples32, ODR::Rate30},     //
    {AT::Samples128, ODR::Rate0_25},  {AT::Samples128, ODR::Rate8},     //
    {AT::Samples256, ODR::Rate0_25},  {AT::Samples256, ODR::Rate4},     //
    {AT::Samples512, ODR::Rate0_25},  {AT::Samples512, ODR::Rate2},     //
    {AT::Samples1024, ODR::Rate0_25}, {AT::Samples1024, ODR::Rate1},    //
    {AT::Samples2048, ODR::Rate0_25}, {AT::Samples2048, ODR::Rate0_5},  //
};
// Subset for periodic timing measurement: one entry per unique ODR (max rate for each AVG_TMOS)
// Rate0_25 entries are excluded as odr_validation already covers start/stop for all combinations
constexpr std::pair<AT, ODR> periodic_test_table[] = {
    {AT::Samples2, ODR::Rate30},      //
    {AT::Samples128, ODR::Rate8},     //
    {AT::Samples256, ODR::Rate4},     //
    {AT::Samples512, ODR::Rate2},     //
    {AT::Samples1024, ODR::Rate1},    //
    {AT::Samples2048, ODR::Rate0_5},  //
};
constexpr std::pair<AT, ODR> odr_invalid_table[] = {
    {AT::Samples128, ODR::Rate15}, {AT::Samples128, ODR::Rate30},   //
    {AT::Samples256, ODR::Rate8},  {AT::Samples256, ODR::Rate30},   //
    {AT::Samples512, ODR::Rate4},  {AT::Samples512, ODR::Rate30},   //
    {AT::Samples1024, ODR::Rate2}, {AT::Samples1024, ODR::Rate30},  //
    {AT::Samples2048, ODR::Rate1}, {AT::Samples2048, ODR::Rate30},  //
};
void odr_validation(UnitSTHS34PF80* unit)
{
    EXPECT_FALSE(unit->inPeriodic());

    for (auto&& p : odr_valid_table) {
        EXPECT_TRUE(unit->writeAverageTrim(AmbientTemperatureAverage::Samples8, p.first));
        EXPECT_TRUE(unit->startPeriodicMeasurement(Gain::Default, p.second));
        EXPECT_TRUE(unit->inPeriodic());
        EXPECT_TRUE(unit->stopPeriodicMeasurement());
        EXPECT_FALSE(unit->inPeriodic());
    }

    for (auto&& p : odr_invalid_table) {
        EXPECT_TRUE(unit->writeAverageTrim(AmbientTemperatureAverage::Samples8, p.first));
        EXPECT_FALSE(unit->startPeriodicMeasurement(Gain::Default, p.second));
    }
}

}  // namespace

TEST_F(TestSTHS34PF80, Settings)
{
    AmbientTemperatureAverage ata{};
    ObjectTemperatureAverage ota{};
    auto cfg = unit->config();

    SCOPED_TRACE(ustr);

    //
    {
        AmbientTemperatureAverage prev_t{};
        ObjectTemperatureAverage prev_tmos{};

        // Failed to write in periodic
        EXPECT_TRUE(unit->inPeriodic());
        EXPECT_TRUE(unit->readAverageTrim(prev_t, prev_tmos));
        for (auto&& avg_t : avg_t_table) {
            for (auto&& avg_tmos : avg_tmos_table) {
                EXPECT_FALSE(unit->writeAverageTrim(avg_t, avg_tmos));
                EXPECT_TRUE(unit->readAverageTrim(ata, ota));
                EXPECT_EQ(ata, prev_t);
                EXPECT_EQ(ota, prev_tmos);
            }
        }

        //
        EXPECT_TRUE(unit->stopPeriodicMeasurement());
        EXPECT_FALSE(unit->inPeriodic());
        for (auto&& avg_t : avg_t_table) {
            for (auto&& avg_tmos : avg_tmos_table) {
                EXPECT_TRUE(unit->writeAverageTrim(avg_t, avg_tmos));
                EXPECT_TRUE(unit->readAverageTrim(ata, ota));
                EXPECT_EQ(ata, avg_t);
                EXPECT_EQ(ota, avg_tmos);
            }
        }

        // For start periodic (ODR Maximum configurable value depends on AVG_TMOS)
        EXPECT_TRUE(unit->writeAverageTrim(AmbientTemperatureAverage::Samples8, ObjectTemperatureAverage::Samples2));
    }

    //
    {
        Gain g{};

        EXPECT_FALSE(unit->inPeriodic());
        EXPECT_TRUE(unit->writeGainMode(Gain::Default));
        EXPECT_TRUE(unit->readGainMode(g));
        EXPECT_EQ(g, Gain::Default);

        EXPECT_TRUE(unit->writeGainMode(Gain::Wide));
        EXPECT_TRUE(unit->readGainMode(g));
        EXPECT_EQ(g, Gain::Wide);

        // Failed to write in periodic
        EXPECT_TRUE(unit->startPeriodicMeasurement(cfg.mode, cfg.odr));
        EXPECT_TRUE(unit->inPeriodic());

        EXPECT_FALSE(unit->writeGainMode(Gain::Default));
        EXPECT_TRUE(unit->readGainMode(g));
        EXPECT_EQ(g, cfg.mode);
        EXPECT_FALSE(unit->writeGainMode(Gain::Wide));
        EXPECT_TRUE(unit->readGainMode(g));
        EXPECT_EQ(g, cfg.mode);
    }

    //
    {
        int8_t prev_raw{}, raw{};
        uint16_t prev_s{}, sens{};
        EXPECT_TRUE(unit->readSensitivityRaw(prev_raw));
        EXPECT_TRUE(unit->readSensitivity(prev_s));

        // Failed to write in periodic
        EXPECT_TRUE(unit->inPeriodic());
        EXPECT_FALSE(unit->writeSensitivityRaw(-128));
        EXPECT_TRUE(unit->readSensitivityRaw(raw));
        EXPECT_TRUE(unit->readSensitivity(sens));
        EXPECT_EQ(raw, prev_raw);
        EXPECT_EQ(sens, prev_s);
        EXPECT_FALSE(unit->writeSensitivityRaw(0));
        EXPECT_TRUE(unit->readSensitivityRaw(raw));
        EXPECT_TRUE(unit->readSensitivity(sens));
        EXPECT_EQ(raw, prev_raw);
        EXPECT_EQ(sens, prev_s);
        EXPECT_FALSE(unit->writeSensitivityRaw(127));
        EXPECT_TRUE(unit->readSensitivityRaw(raw));
        EXPECT_TRUE(unit->readSensitivity(sens));
        EXPECT_EQ(raw, prev_raw);
        EXPECT_EQ(sens, prev_s);

        EXPECT_FALSE(unit->writeSensitivity(4080));
        EXPECT_TRUE(unit->readSensitivityRaw(raw));
        EXPECT_TRUE(unit->readSensitivity(sens));
        EXPECT_EQ(raw, prev_raw);
        EXPECT_EQ(sens, prev_s);

        //
        EXPECT_TRUE(unit->stopPeriodicMeasurement());
        EXPECT_FALSE(unit->inPeriodic());

        EXPECT_TRUE(unit->writeSensitivityRaw(-128));
        EXPECT_TRUE(unit->readSensitivityRaw(raw));
        EXPECT_TRUE(unit->readSensitivity(sens));
        EXPECT_EQ(raw, -128);
        EXPECT_EQ(sens, 0);

        EXPECT_TRUE(unit->writeSensitivityRaw(0));
        EXPECT_TRUE(unit->readSensitivityRaw(raw));
        EXPECT_TRUE(unit->readSensitivity(sens));
        EXPECT_EQ(raw, 0);
        EXPECT_EQ(sens, 2048);

        EXPECT_TRUE(unit->writeSensitivityRaw(127));
        EXPECT_TRUE(unit->readSensitivityRaw(raw));
        EXPECT_TRUE(unit->readSensitivity(sens));
        EXPECT_EQ(raw, 127);
        EXPECT_EQ(sens, 4080);

        EXPECT_TRUE(unit->writeSensitivity(0));
        EXPECT_TRUE(unit->readSensitivityRaw(raw));
        EXPECT_TRUE(unit->readSensitivity(sens));
        EXPECT_EQ(raw, -128);
        EXPECT_EQ(sens, 0);

        EXPECT_TRUE(unit->writeSensitivity(4080));
        EXPECT_TRUE(unit->readSensitivityRaw(raw));
        EXPECT_TRUE(unit->readSensitivity(sens));
        EXPECT_EQ(raw, 127);
        EXPECT_EQ(sens, 4080);

        EXPECT_FALSE(unit->writeSensitivity(4081));  // Out of range
        EXPECT_TRUE(unit->readSensitivityRaw(raw));
        EXPECT_TRUE(unit->readSensitivity(sens));
        EXPECT_EQ(raw, 127);
        EXPECT_EQ(sens, 4080);
    }

    {
        ODR odr{};
        EXPECT_FALSE(unit->inPeriodic());
        EXPECT_TRUE(unit->readObjectDataRate(odr));
        EXPECT_EQ(odr, ODR::PowerDown);  // In power-down mode when stopped

        EXPECT_TRUE(unit->startPeriodicMeasurement(cfg.mode, ODR::Rate0_25));
        EXPECT_TRUE(unit->inPeriodic());
        EXPECT_TRUE(unit->readObjectDataRate(odr));
        EXPECT_EQ(odr, ODR::Rate0_25);
    }
}

TEST_F(TestSTHS34PF80, SettingsNeedResetAlgo)
{
    auto cfg = unit->config();

    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->inPeriodic());

    std::array<LowPassFilter, 4> prev_lpf{};
    uint16_t prev_thres_p{}, prev_thres_m{}, prev_thres_a{};
    uint8_t prev_hyst_p{}, prev_hyst_m{}, prev_hyst_a{};

    std::array<LowPassFilter, 4> wlpf = {LowPassFilter::ODR20, LowPassFilter::ODR50, LowPassFilter::ODR100,
                                         LowPassFilter::ODR9};
    uint16_t twv[3]{};
    twv[0] = 100 + (esp_random() % 156);  // 100-255, always > hwv max (49)
    twv[1] = 100 + (esp_random() % 156);
    twv[2] = 100 + (esp_random() % 156);

    uint8_t hwv[3]{};
    hwv[0] = esp_random() % 50;
    hwv[1] = esp_random() % 50;
    hwv[2] = esp_random() % 50;

    auto s = m5::utility::formatString("twv:%u,%u,%u hwv:%u,%u,%u", twv[0], twv[1], twv[2], hwv[0], hwv[1], hwv[2]);
    SCOPED_TRACE(s);

    //
    EXPECT_TRUE(unit->readLowPassFilter(prev_lpf[0], prev_lpf[1], prev_lpf[2], prev_lpf[3]));

    // Failed to read/write in periodic
    EXPECT_FALSE(unit->writeLowPassFilter(wlpf[0], wlpf[1], wlpf[2], wlpf[3]));

    EXPECT_FALSE(unit->readPresenceThreshold(prev_thres_p));
    EXPECT_FALSE(unit->readMotionThreshold(prev_thres_m));
    EXPECT_FALSE(unit->readAmbientShockThreshold(prev_thres_a));
    EXPECT_FALSE(unit->writePresenceThreshold(esp_random()));
    EXPECT_FALSE(unit->writeMotionThreshold(esp_random()));
    EXPECT_FALSE(unit->writeAmbientShockThreshold(esp_random()));

    EXPECT_FALSE(unit->readPresenceHysteresis(prev_hyst_p));
    EXPECT_FALSE(unit->readMotionHysteresis(prev_hyst_m));
    EXPECT_FALSE(unit->readAmbientShockHysteresis(prev_hyst_a));
    EXPECT_FALSE(unit->writePresenceHysteresis(esp_random()));
    EXPECT_FALSE(unit->writeMotionHysteresis(esp_random()));
    EXPECT_FALSE(unit->writeAmbientShockHysteresis(esp_random()));

    //
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    EXPECT_TRUE(unit->readPresenceThreshold(prev_thres_p));
    EXPECT_TRUE(unit->readMotionThreshold(prev_thres_m));
    EXPECT_TRUE(unit->readAmbientShockThreshold(prev_thres_a));
    EXPECT_TRUE(unit->readPresenceHysteresis(prev_hyst_p));
    EXPECT_TRUE(unit->readMotionHysteresis(prev_hyst_m));
    EXPECT_TRUE(unit->readAmbientShockHysteresis(prev_hyst_a));

    {
        low_pass_filter_validation(unit.get());
    }

    uint16_t thres_p{}, thres_m{}, thres_a{};
    uint8_t hyst_p{}, hyst_m{}, hyst_a{};
    {
        EXPECT_TRUE(unit->writePresenceThreshold(twv[0]));
        EXPECT_TRUE(unit->writeMotionThreshold(twv[1]));
        EXPECT_TRUE(unit->writeAmbientShockThreshold(twv[2]));

        EXPECT_TRUE(unit->readPresenceThreshold(thres_p));
        EXPECT_TRUE(unit->readMotionThreshold(thres_m));
        EXPECT_TRUE(unit->readAmbientShockThreshold(thres_a));
        EXPECT_EQ(thres_p, twv[0]);
        EXPECT_EQ(thres_m, twv[1]);
        EXPECT_EQ(thres_a, twv[2]);

        EXPECT_FALSE(unit->writePresenceThreshold(0x8000));
        EXPECT_FALSE(unit->writeMotionThreshold(0x8000));
        EXPECT_FALSE(unit->writeAmbientShockThreshold(0x8000));

        EXPECT_TRUE(unit->readPresenceThreshold(thres_p));
        EXPECT_TRUE(unit->readMotionThreshold(thres_m));
        EXPECT_TRUE(unit->readAmbientShockThreshold(thres_a));
        EXPECT_EQ(thres_p, twv[0]);
        EXPECT_EQ(thres_m, twv[1]);
        EXPECT_EQ(thres_a, twv[2]);
    }

    {
        // hyst must be smaller than thres
        EXPECT_FALSE(unit->writePresenceHysteresis(twv[0]));
        EXPECT_FALSE(unit->writeMotionHysteresis(twv[1]));
        EXPECT_FALSE(unit->writeAmbientShockHysteresis(twv[2]));

        EXPECT_TRUE(unit->writePresenceHysteresis(hwv[0]));
        EXPECT_TRUE(unit->writeMotionHysteresis(hwv[1]));
        EXPECT_TRUE(unit->writeAmbientShockHysteresis(hwv[2]));

        EXPECT_TRUE(unit->readPresenceHysteresis(hyst_p));
        EXPECT_TRUE(unit->readMotionHysteresis(hyst_m));
        EXPECT_TRUE(unit->readAmbientShockHysteresis(hyst_a));
        EXPECT_EQ(hyst_p, hwv[0]);
        EXPECT_EQ(hyst_m, hwv[1]);
        EXPECT_EQ(hyst_a, hwv[2]);
    }

    {
        uint8_t acfg{0xFF};
        EXPECT_FALSE(unit->inPeriodic());
        EXPECT_TRUE(unit->readAlgorithmConfig(acfg));
        EXPECT_NE(acfg, 0xFF);

        EXPECT_TRUE(unit->startPeriodicMeasurement(cfg.mode, cfg.odr));
        EXPECT_TRUE(unit->inPeriodic());
        acfg = 0xFF;
        EXPECT_FALSE(unit->readAlgorithmConfig(acfg));
        EXPECT_EQ(acfg, 0xFF);
    }

    {
        EXPECT_TRUE(unit->inPeriodic());
        EXPECT_FALSE(unit->resetAlgorithm());

        EXPECT_TRUE(unit->stopPeriodicMeasurement());
        EXPECT_FALSE(unit->inPeriodic());
        EXPECT_TRUE(unit->resetAlgorithm());
    }
}

TEST_F(TestSTHS34PF80, Reset)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    //
    EXPECT_TRUE(unit->writeLowPassFilter(LowPassFilter::ODR200, LowPassFilter::ODR400, LowPassFilter::ODR800,
                                         LowPassFilter::ODR200));
    EXPECT_TRUE(unit->writeAverageTrim(AmbientTemperatureAverage::Samples2, ObjectTemperatureAverage::Samples512));
    EXPECT_TRUE(unit->writeSensitivity(4080));
    EXPECT_TRUE(unit->writePresenceThreshold(1234));
    EXPECT_TRUE(unit->writeMotionThreshold(2345));
    EXPECT_TRUE(unit->writeAmbientShockThreshold(67));
    EXPECT_TRUE(unit->writePresenceHysteresis(100));
    EXPECT_TRUE(unit->writeMotionHysteresis(200));
    EXPECT_TRUE(unit->writeAmbientShockHysteresis(30));

    //
    EXPECT_TRUE(unit->startPeriodicMeasurement(Gain::Wide, ODR::Rate0_5));
    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_TRUE(unit->softReset());
    EXPECT_FALSE(unit->inPeriodic());

    std::array<LowPassFilter, 4> lpf{};
    uint16_t sens{};
    AmbientTemperatureAverage avg_t{};
    ObjectTemperatureAverage avg_tmos{};
    Gain mode{};
    uint16_t thres_p{}, thres_m{}, thres_a{};
    uint8_t hyst_p{}, hyst_m{}, hyst_a{};

    EXPECT_TRUE(unit->readLowPassFilter(lpf[0], lpf[1], lpf[2], lpf[3]));
    EXPECT_TRUE(unit->readSensitivity(sens));
    EXPECT_TRUE(unit->readAverageTrim(avg_t, avg_tmos));
    EXPECT_TRUE(unit->readGainMode(mode));
    EXPECT_TRUE(unit->readPresenceThreshold(thres_p));
    EXPECT_TRUE(unit->readMotionThreshold(thres_m));
    EXPECT_TRUE(unit->readAmbientShockThreshold(thres_a));
    EXPECT_TRUE(unit->readPresenceHysteresis(hyst_p));
    EXPECT_TRUE(unit->readMotionHysteresis(hyst_m));
    EXPECT_TRUE(unit->readAmbientShockHysteresis(hyst_a));

    // Set default on reset
    EXPECT_EQ(avg_t, AmbientTemperatureAverage::Samples8);
    EXPECT_EQ(avg_tmos, ObjectTemperatureAverage::Samples128);

    EXPECT_EQ(mode, Gain::Default);

    // Set from OTP memory on reset
    EXPECT_NE(sens, 4080);
    EXPECT_NE(sens, 0);

    // Keep
    EXPECT_EQ(lpf[0], LowPassFilter::ODR200);
    EXPECT_EQ(lpf[1], LowPassFilter::ODR400);
    EXPECT_EQ(lpf[2], LowPassFilter::ODR800);
    EXPECT_EQ(lpf[3], LowPassFilter::ODR200);

    EXPECT_EQ(thres_p, 1234);
    EXPECT_EQ(thres_m, 2345);
    EXPECT_EQ(thres_a, 67);

    EXPECT_EQ(hyst_p, 100);
    EXPECT_EQ(hyst_m, 200);
    EXPECT_EQ(hyst_a, 30);
}

TEST_F(TestSTHS34PF80, SingleShot)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    EXPECT_TRUE(unit->writeLowPassFilter(LowPassFilter::ODR9, LowPassFilter::ODR200, LowPassFilter::ODR50,
                                         LowPassFilter::ODR50));

    for (auto&& avg_tmos : avg_tmos_table) {
        AmbientTemperatureAverage avg_t = static_cast<AmbientTemperatureAverage>(esp_random() & 0x03);
        Data d{};
        uint32_t count{4};
        while (count--) {
            EXPECT_TRUE(unit->measureSingleshot(d, avg_t, avg_tmos));

            EXPECT_TRUE(std::isfinite(d.objectTemperature()));
            EXPECT_TRUE(std::isfinite(d.ambientTemperature()));
            EXPECT_TRUE(std::isfinite(d.compensatedObjectTemperature()));
            EXPECT_EQ(d.presence(), 0);
            EXPECT_EQ(d.motion(), 0);
            EXPECT_EQ(d.ambient_shock(), 0);
            EXPECT_FALSE(d.isPresence());
            EXPECT_FALSE(d.isMotion());
            EXPECT_FALSE(d.isAmbientShock());
            // M5_LOGW("%u/%u: %d %d %d", avg_t, avg_tmos, d.object(), d.ambient(), d.compensated_object());
        }
    }
}

TEST_F(TestSTHS34PF80, Periodic)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    odr_validation(unit.get());

    for (auto&& p : periodic_test_table) {
        EXPECT_TRUE(unit->writeAverageTrim(AmbientTemperatureAverage::Samples8, p.first));

        EXPECT_TRUE(unit->startPeriodicMeasurement(Gain::Default, p.second));
        EXPECT_TRUE(unit->inPeriodic());

        auto ad          = unit->asAdapter<m5::unit::AdapterI2C>(m5::unit::Adapter::Type::I2C);
        bool is_bus      = ad && ad->implType() == m5::unit::AdapterI2C::ImplType::Bus;
        uint32_t timeout = is_bus ? std::max<uint32_t>(unit->interval(), 500) * (STORED_SIZE + 1) * 4
                                  : unit->interval() * (STORED_SIZE + 1) * 2;
        auto r           = collect_periodic_measurements(unit.get(), STORED_SIZE, timeout);
        EXPECT_FALSE(r.timed_out);
        EXPECT_EQ(r.update_count, STORED_SIZE);
        // Sensor actual interval is ~1.5% longer than nominal
        uint32_t tol =
            is_bus ? std::max<uint32_t>(unit->interval() / 20, 5) : std::max<uint32_t>(unit->interval() / 50, 2);
        EXPECT_LE(r.median(), r.expected_interval + tol);

        EXPECT_TRUE(unit->stopPeriodicMeasurement());
        EXPECT_FALSE(unit->inPeriodic());

        EXPECT_EQ(unit->available(), STORED_SIZE);
        EXPECT_FALSE(unit->empty());
        EXPECT_TRUE(unit->full());

        EXPECT_TRUE(std::isfinite(unit->objectTemperature()));
        EXPECT_TRUE(std::isfinite(unit->ambientTemperature()));
        EXPECT_TRUE(std::isfinite(unit->compensatedObjectTemperature()));
        EXPECT_NE(unit->presence(), std::numeric_limits<int16_t>::lowest());
        EXPECT_NE(unit->presence(), std::numeric_limits<int16_t>::max());
        EXPECT_NE(unit->motion(), std::numeric_limits<int16_t>::lowest());
        EXPECT_NE(unit->motion(), std::numeric_limits<int16_t>::max());
        EXPECT_NE(unit->ambient_shock(), std::numeric_limits<int16_t>::lowest());
        EXPECT_NE(unit->ambient_shock(), std::numeric_limits<int16_t>::max());

        uint32_t cnt{STORED_SIZE / 2};
        while (cnt-- && unit->available()) {
            EXPECT_TRUE(std::isfinite(unit->objectTemperature()));
            EXPECT_TRUE(std::isfinite(unit->ambientTemperature()));
            EXPECT_TRUE(std::isfinite(unit->compensatedObjectTemperature()));
            EXPECT_NE(unit->presence(), std::numeric_limits<int16_t>::lowest());
            EXPECT_NE(unit->presence(), std::numeric_limits<int16_t>::max());
            EXPECT_NE(unit->motion(), std::numeric_limits<int16_t>::lowest());
            EXPECT_NE(unit->motion(), std::numeric_limits<int16_t>::max());
            EXPECT_NE(unit->ambient_shock(), std::numeric_limits<int16_t>::lowest());
            EXPECT_NE(unit->ambient_shock(), std::numeric_limits<int16_t>::max());

            EXPECT_FLOAT_EQ(unit->objectTemperature(), unit->oldest().objectTemperature());
            EXPECT_FLOAT_EQ(unit->ambientTemperature(), unit->oldest().ambientTemperature());
            EXPECT_FLOAT_EQ(unit->compensatedObjectTemperature(), unit->oldest().compensatedObjectTemperature());
            EXPECT_EQ(unit->presence(), unit->oldest().presence());
            EXPECT_EQ(unit->motion(), unit->oldest().motion());
            EXPECT_EQ(unit->ambient_shock(), unit->oldest().ambient_shock());
            EXPECT_EQ(unit->isPresence(), unit->oldest().isPresence());
            EXPECT_EQ(unit->isMotion(), unit->oldest().isMotion());
            EXPECT_EQ(unit->isAmbientShock(), unit->oldest().isAmbientShock());

            EXPECT_FALSE(unit->empty());
            unit->discard();
        }

        //
        EXPECT_EQ(unit->available(), STORED_SIZE / 2);
        EXPECT_FALSE(unit->empty());
        EXPECT_FALSE(unit->full());

        EXPECT_TRUE(std::isfinite(unit->objectTemperature()));
        EXPECT_TRUE(std::isfinite(unit->ambientTemperature()));
        EXPECT_TRUE(std::isfinite(unit->compensatedObjectTemperature()));
        EXPECT_NE(unit->presence(), std::numeric_limits<int16_t>::lowest());
        EXPECT_NE(unit->presence(), std::numeric_limits<int16_t>::max());
        EXPECT_NE(unit->motion(), std::numeric_limits<int16_t>::lowest());
        EXPECT_NE(unit->motion(), std::numeric_limits<int16_t>::max());
        EXPECT_NE(unit->ambient_shock(), std::numeric_limits<int16_t>::lowest());
        EXPECT_NE(unit->ambient_shock(), std::numeric_limits<int16_t>::max());

        //
        unit->flush();
        EXPECT_EQ(unit->available(), 0);
        EXPECT_TRUE(unit->empty());
        EXPECT_FALSE(unit->full());

        EXPECT_FALSE(std::isfinite(unit->objectTemperature()));
        EXPECT_FALSE(std::isfinite(unit->ambientTemperature()));
        EXPECT_FALSE(std::isfinite(unit->compensatedObjectTemperature()));
        EXPECT_EQ(unit->presence(), 0);
        EXPECT_EQ(unit->motion(), 0);
        EXPECT_EQ(unit->ambient_shock(), 0);
        EXPECT_FALSE(unit->isPresence());
        EXPECT_FALSE(unit->isMotion());
        EXPECT_FALSE(unit->isAmbientShock());
    }
}

// Test 1: measureSingleshot should fail during periodic measurement
TEST_F(TestSTHS34PF80, MeasureSingleshotInPeriodic)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->inPeriodic());
    Data d{};
    EXPECT_FALSE(unit->measureSingleshot(d, AmbientTemperatureAverage::Samples8, ObjectTemperatureAverage::Samples32));

    // Verify data is untouched (zeroed)
    EXPECT_EQ(d.object(), 0);
    EXPECT_EQ(d.ambient(), 0);
    EXPECT_EQ(d.compensated_object(), 0);
    EXPECT_EQ(d.presence(), 0);
    EXPECT_EQ(d.motion(), 0);
    EXPECT_EQ(d.ambient_shock(), 0);
}

// Test 2: startPeriodicMeasurement with PowerDown ODR should fail
TEST_F(TestSTHS34PF80, StartPeriodicWithPowerDown)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    EXPECT_FALSE(unit->startPeriodicMeasurement(Gain::Default, ODR::PowerDown));
    EXPECT_FALSE(unit->inPeriodic());

    EXPECT_FALSE(unit->startPeriodicMeasurement(Gain::Wide, ODR::PowerDown));
    EXPECT_FALSE(unit->inPeriodic());
}

// Test 3: config() getter/setter
TEST_F(TestSTHS34PF80, Config)
{
    SCOPED_TRACE(ustr);

    // Get default config
    auto cfg = unit->config();
    EXPECT_TRUE(cfg.start_periodic);
    EXPECT_EQ(cfg.mode, Gain::Default);
    EXPECT_EQ(cfg.odr, ODR::Rate30);
    EXPECT_TRUE(cfg.comp_type);
    EXPECT_FALSE(cfg.abs);
    EXPECT_EQ(cfg.avg_t, AmbientTemperatureAverage::Samples8);
    EXPECT_EQ(cfg.avg_tmos, ObjectTemperatureAverage::Samples32);

    // Set and get config
    UnitSTHS34PF80::config_t new_cfg{};
    new_cfg.start_periodic = false;
    new_cfg.mode           = Gain::Wide;
    new_cfg.odr            = ODR::Rate4;
    new_cfg.comp_type      = false;
    new_cfg.abs            = true;
    new_cfg.avg_t          = AmbientTemperatureAverage::Samples2;
    new_cfg.avg_tmos       = ObjectTemperatureAverage::Samples256;
    unit->config(new_cfg);

    auto readback = unit->config();
    EXPECT_FALSE(readback.start_periodic);
    EXPECT_EQ(readback.mode, Gain::Wide);
    EXPECT_EQ(readback.odr, ODR::Rate4);
    EXPECT_FALSE(readback.comp_type);
    EXPECT_TRUE(readback.abs);
    EXPECT_EQ(readback.avg_t, AmbientTemperatureAverage::Samples2);
    EXPECT_EQ(readback.avg_tmos, ObjectTemperatureAverage::Samples256);

    // Restore original config
    unit->config(cfg);
    auto restored = unit->config();
    EXPECT_TRUE(restored.start_periodic);
    EXPECT_EQ(restored.mode, Gain::Default);
    EXPECT_EQ(restored.odr, ODR::Rate30);
}

// Test 4: begin() with start_periodic=false should not start periodic measurement
TEST_F(TestSTHS34PF80, BeginWithoutStartPeriodic)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    // Set config to not start periodic on begin
    auto original_cfg  = unit->config();
    auto cfg           = original_cfg;
    cfg.start_periodic = false;
    unit->config(cfg);

    // Call begin() directly - should succeed without starting periodic
    EXPECT_TRUE(unit->begin());
    EXPECT_FALSE(unit->inPeriodic());

    // Verify sensitivity was read correctly
    EXPECT_NE(unit->sensitivity(), 0);

    // Restore config and restart periodic for subsequent tests
    unit->config(original_cfg);
    EXPECT_TRUE(
        unit->startPeriodicMeasurement(original_cfg.mode, original_cfg.odr, original_cfg.comp_type, original_cfg.abs));
    EXPECT_TRUE(unit->inPeriodic());
}

// Test 4b: begin() with start_periodic=true should apply config to hardware
TEST_F(TestSTHS34PF80, BeginAppliesConfig)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    // Save original config for restoration
    auto original_cfg = unit->config();

    // Set non-default config
    auto cfg           = original_cfg;
    cfg.start_periodic = true;
    cfg.mode           = Gain::Default;
    cfg.odr            = ODR::Rate1;
    cfg.comp_type      = false;
    cfg.abs            = true;
    cfg.avg_t          = AmbientTemperatureAverage::Samples2;
    cfg.avg_tmos       = ObjectTemperatureAverage::Samples8;
    unit->config(cfg);

    // begin() should apply all config fields to hardware
    EXPECT_TRUE(unit->begin());
    EXPECT_TRUE(unit->inPeriodic());

    // Verify gain mode (readable during periodic)
    Gain gain{};
    EXPECT_TRUE(unit->readGainMode(gain));
    EXPECT_EQ(gain, cfg.mode);

    // Verify ODR (readable during periodic)
    ODR odr{};
    EXPECT_TRUE(unit->readObjectDataRate(odr));
    EXPECT_EQ(odr, cfg.odr);

    // Verify average trim (readable during periodic)
    AmbientTemperatureAverage avg_t{};
    ObjectTemperatureAverage avg_tmos{};
    EXPECT_TRUE(unit->readAverageTrim(avg_t, avg_tmos));
    EXPECT_EQ(avg_t, cfg.avg_t);
    EXPECT_EQ(avg_tmos, cfg.avg_tmos);

    // Verify algorithm config (requires stop)
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    uint8_t acfg{};
    EXPECT_TRUE(unit->readAlgorithmConfig(acfg));
    // ALGO_CONFIG (28h): bit2=COMP_TYPE, bit1=INT_PULSED, bit0=SEL_ABS
    // comp_type=false -> bit2=0, abs=true -> bit0=1 (SEL_ABS)
    EXPECT_EQ(acfg & 0x04, 0x00);  // COMP_TYPE off
    EXPECT_EQ(acfg & 0x02, 0x00);  // INT_PULSED must not be set
    EXPECT_EQ(acfg & 0x01, 0x01);  // SEL_ABS on

    // Restore original config and restart
    unit->config(original_cfg);
    EXPECT_TRUE(unit->begin());
    EXPECT_TRUE(unit->inPeriodic());
}

// Verify ALGO_CONFIG bit layout: bit2=COMP_TYPE, bit1=INT_PULSED, bit0=SEL_ABS (datasheet 28h)
TEST_F(TestSTHS34PF80, AlgorithmConfigBits)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    uint8_t acfg{};

    // comp_type=true, abs=true -> COMP_TYPE|SEL_ABS (0x05)
    EXPECT_TRUE(unit->startPeriodicMeasurement(Gain::Default, ODR::Rate8, true, true));
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_TRUE(unit->readAlgorithmConfig(acfg));
    EXPECT_EQ(acfg & 0x04, 0x04);  // COMP_TYPE
    EXPECT_EQ(acfg & 0x02, 0x00);  // INT_PULSED must not be set
    EXPECT_EQ(acfg & 0x01, 0x01);  // SEL_ABS

    // comp_type=true, abs=false -> COMP_TYPE only (0x04)
    EXPECT_TRUE(unit->startPeriodicMeasurement(Gain::Default, ODR::Rate8, true, false));
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_TRUE(unit->readAlgorithmConfig(acfg));
    EXPECT_EQ(acfg & 0x04, 0x04);
    EXPECT_EQ(acfg & 0x02, 0x00);
    EXPECT_EQ(acfg & 0x01, 0x00);

    // comp_type=false, abs=true -> SEL_ABS only (0x01)
    EXPECT_TRUE(unit->startPeriodicMeasurement(Gain::Default, ODR::Rate8, false, true));
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_TRUE(unit->readAlgorithmConfig(acfg));
    EXPECT_EQ(acfg & 0x04, 0x00);
    EXPECT_EQ(acfg & 0x02, 0x00);
    EXPECT_EQ(acfg & 0x01, 0x01);

    // comp_type=false, abs=false -> all clear (0x00)
    EXPECT_TRUE(unit->startPeriodicMeasurement(Gain::Default, ODR::Rate8, false, false));
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_TRUE(unit->readAlgorithmConfig(acfg));
    EXPECT_EQ(acfg & 0x07, 0x00);
}

// Test 5: maximum_odr() static function
TEST_F(TestSTHS34PF80, MaximumODR)
{
    SCOPED_TRACE(ustr);

    EXPECT_EQ(UnitSTHS34PF80::maximum_odr(ObjectTemperatureAverage::Samples2), ODR::Rate30);
    EXPECT_EQ(UnitSTHS34PF80::maximum_odr(ObjectTemperatureAverage::Samples8), ODR::Rate30);
    EXPECT_EQ(UnitSTHS34PF80::maximum_odr(ObjectTemperatureAverage::Samples32), ODR::Rate30);
    EXPECT_EQ(UnitSTHS34PF80::maximum_odr(ObjectTemperatureAverage::Samples128), ODR::Rate8);
    EXPECT_EQ(UnitSTHS34PF80::maximum_odr(ObjectTemperatureAverage::Samples256), ODR::Rate4);
    EXPECT_EQ(UnitSTHS34PF80::maximum_odr(ObjectTemperatureAverage::Samples512), ODR::Rate2);
    EXPECT_EQ(UnitSTHS34PF80::maximum_odr(ObjectTemperatureAverage::Samples1024), ODR::Rate1);
    EXPECT_EQ(UnitSTHS34PF80::maximum_odr(ObjectTemperatureAverage::Samples2048), ODR::Rate0_5);
}

// Test 7: Data struct raw byte conversion (pure logic, no device needed)
TEST(DataTest, RawConversion)
{
    Data d{};
    d.sensitivity = 2048;

    // TOBJECT (raw[0:1]) - positive
    d.raw[0] = 0x00;
    d.raw[1] = 0x04;  // 0x0400 = 1024
    EXPECT_EQ(d.object(), 1024);
    EXPECT_FLOAT_EQ(d.objectTemperature(), 1024.0f / 2048.0f);

    // TOBJECT - negative
    d.raw[0] = 0x00;
    d.raw[1] = 0xFC;  // 0xFC00 = -1024 (signed)
    EXPECT_EQ(d.object(), -1024);
    EXPECT_FLOAT_EQ(d.objectTemperature(), -1024.0f / 2048.0f);

    // TAMBIENT (raw[2:3])
    d.raw[2] = 0xE8;
    d.raw[3] = 0x03;  // 0x03E8 = 1000
    EXPECT_EQ(d.ambient(), 1000);
    EXPECT_FLOAT_EQ(d.ambientTemperature(), 1000.0f / 100.0f);  // Fixed divisor 100

    // TOBJ_COMP (raw[4:5])
    d.raw[4] = 0x00;
    d.raw[5] = 0x02;  // 0x0200 = 512
    EXPECT_EQ(d.compensated_object(), 512);
    EXPECT_FLOAT_EQ(d.compensatedObjectTemperature(), 512.0f / 2048.0f);

    // TPRESENCE (raw[6:7])
    d.raw[6] = 0xD2;
    d.raw[7] = 0x04;  // 0x04D2 = 1234
    EXPECT_EQ(d.presence(), 1234);

    // TPRESENCE - negative
    d.raw[6] = 0x2E;
    d.raw[7] = 0xFB;  // 0xFB2E = -1234 (signed)
    EXPECT_EQ(d.presence(), -1234);

    // TMOTION (raw[8:9])
    d.raw[8] = 0x39;
    d.raw[9] = 0x05;  // 0x0539 = 1337
    EXPECT_EQ(d.motion(), 1337);

    // TAMB_SHOCK (raw[10:11])
    d.raw[10] = 0x2A;
    d.raw[11] = 0x00;  // 0x002A = 42
    EXPECT_EQ(d.ambient_shock(), 42);

    // Detection flags (raw[12])
    d.raw[12] = 0x00;
    EXPECT_FALSE(d.isPresence());
    EXPECT_FALSE(d.isMotion());
    EXPECT_FALSE(d.isAmbientShock());

    d.raw[12] = Data::PRES_FLAG;
    EXPECT_TRUE(d.isPresence());
    EXPECT_FALSE(d.isMotion());
    EXPECT_FALSE(d.isAmbientShock());

    d.raw[12] = Data::MOT_FLAG;
    EXPECT_FALSE(d.isPresence());
    EXPECT_TRUE(d.isMotion());
    EXPECT_FALSE(d.isAmbientShock());

    d.raw[12] = Data::TAMB_SHOCK_FLAG;
    EXPECT_FALSE(d.isPresence());
    EXPECT_FALSE(d.isMotion());
    EXPECT_TRUE(d.isAmbientShock());

    d.raw[12] = Data::PRES_FLAG | Data::MOT_FLAG | Data::TAMB_SHOCK_FLAG;
    EXPECT_TRUE(d.isPresence());
    EXPECT_TRUE(d.isMotion());
    EXPECT_TRUE(d.isAmbientShock());
}

// Test 7b: Data struct with zero sensitivity should produce NaN
TEST(DataTest, ZeroSensitivity)
{
    Data d{};
    d.sensitivity = 0;

    d.raw[0] = 0x00;
    d.raw[1] = 0x04;
    d.raw[2] = 0xE8;
    d.raw[3] = 0x03;
    d.raw[4] = 0x00;
    d.raw[5] = 0x02;

    EXPECT_FALSE(std::isfinite(d.objectTemperature()));
    EXPECT_FALSE(std::isfinite(d.ambientTemperature()));
    EXPECT_FALSE(std::isfinite(d.compensatedObjectTemperature()));

    // Raw values should still be readable
    EXPECT_EQ(d.object(), 1024);
    EXPECT_EQ(d.ambient(), 1000);
    EXPECT_EQ(d.compensated_object(), 512);
}
