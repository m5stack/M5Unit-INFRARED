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
#include <cmath>
#include <random>

using namespace m5::unit::googletest;
using namespace m5::unit;
using namespace m5::unit::sths34pf80;
using namespace m5::unit::sths34pf80::command;
using m5::unit::types::elapsed_time_t;

const ::testing::Environment* global_fixture = ::testing::AddGlobalTestEnvironment(new GlobalFixture<400000U>());

constexpr uint32_t STORED_SIZE{4};
class TestSTHS34PF80 : public ComponentTestBase<UnitSTHS34PF80, bool> {
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
    virtual bool is_using_hal() const override
    {
        return GetParam();
    };
};

// INSTANTIATE_TEST_SUITE_P(ParamValues, TestSTHS34PF80, ::testing::Values(false, true));
// INSTANTIATE_TEST_SUITE_P(ParamValues, TestSTHS34PF80, ::testing::Values(true));
INSTANTIATE_TEST_SUITE_P(ParamValues, TestSTHS34PF80, ::testing::Values(false));

namespace {
auto rng = std::default_random_engine{};

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

template <class U>
elapsed_time_t test_periodic(U* unit, const uint32_t times, const uint32_t measure_duration = 0)
{
    auto tm         = unit->interval();
    auto timeout_at = m5::utility::millis() + 10 * 1000;

    do {
        unit->update();
        if (unit->updated()) {
            break;
        }
        std::this_thread::yield();
    } while (!unit->updated() && m5::utility::millis() <= timeout_at);
    // timeout
    if (!unit->updated()) {
        return 0;
    }

    //
    uint32_t measured{};
    auto start_at = m5::utility::millis();
    timeout_at    = start_at + (times * (tm + measure_duration) * 2);

    do {
        unit->update();
        measured += unit->updated() ? 1 : 0;
        if (measured >= times) {
            break;
        }
        std::this_thread::yield();

    } while (measured < times && m5::utility::millis() <= timeout_at);
    return (measured == times) ? m5::utility::millis() - start_at : 0;
    //   return (measured == times) ? unit->updatedMillis() - start_at : 0;
}

}  // namespace

TEST_P(TestSTHS34PF80, Settings)
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

        // Faild to write in periodic
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

        // Faild to write in periodic
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

TEST_P(TestSTHS34PF80, SettingsNeedResetAlgo)
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
    twv[0] = (100 + rng()) & 255;
    twv[1] = (100 + rng()) & 255;
    twv[2] = (100 + rng()) & 255;

    uint8_t hwv[3]{};
    hwv[0] = rng() % 50;
    hwv[1] = rng() % 50;
    hwv[2] = rng() % 50;

    //
    EXPECT_TRUE(unit->readLowPassFilter(prev_lpf[0], prev_lpf[1], prev_lpf[2], prev_lpf[3]));

    // Failed to read/write in periodic
    EXPECT_FALSE(unit->writeLowPassFilter(wlpf[0], wlpf[1], wlpf[2], wlpf[3]));

    EXPECT_FALSE(unit->readPresenceThreshold(prev_thres_p));
    EXPECT_FALSE(unit->readMotionThreshold(prev_thres_m));
    EXPECT_FALSE(unit->readAmbientShockThreshold(prev_thres_a));
    EXPECT_FALSE(unit->writePresenceThreshold(rng()));
    EXPECT_FALSE(unit->writeMotionThreshold(rng()));
    EXPECT_FALSE(unit->writeAmbientShockThreshold(rng()));

    EXPECT_FALSE(unit->readPresenceHysteresis(prev_hyst_p));
    EXPECT_FALSE(unit->readMotionHysteresis(prev_hyst_m));
    EXPECT_FALSE(unit->readAmbientShockHysteresis(prev_hyst_a));
    EXPECT_FALSE(unit->writePresenceHysteresis(rng()));
    EXPECT_FALSE(unit->writeMotionHysteresis(rng()));
    EXPECT_FALSE(unit->writeAmbientShockHysteresis(rng()));

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

TEST_P(TestSTHS34PF80, Reset)
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

    // Set default on reseet
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

TEST_P(TestSTHS34PF80, SingleSHot)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    EXPECT_TRUE(unit->writeLowPassFilter(LowPassFilter::ODR9, LowPassFilter::ODR200, LowPassFilter::ODR50,
                                         LowPassFilter::ODR50));

    for (auto&& avg_tmos : avg_tmos_table) {
        AmbientTemperatureAverage avg_t = static_cast<AmbientTemperatureAverage>(rng() & 0x03);
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

TEST_P(TestSTHS34PF80, Periodic)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    odr_validation(unit.get());

    for (auto&& p : odr_valid_table) {
        EXPECT_TRUE(unit->writeAverageTrim(AmbientTemperatureAverage::Samples8, p.first));

        EXPECT_TRUE(unit->startPeriodicMeasurement(Gain::Default, p.second));
        EXPECT_TRUE(unit->inPeriodic());
        auto elapsed = test_periodic(unit.get(), STORED_SIZE);
        EXPECT_TRUE(unit->stopPeriodicMeasurement());
        EXPECT_FALSE(unit->inPeriodic());

        EXPECT_GE(elapsed, unit->interval() * STORED_SIZE);

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
