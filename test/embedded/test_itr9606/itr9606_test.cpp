/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  UnitTest for UnitITR9606
*/
#include <gtest/gtest.h>
#include <Wire.h>
#include <M5Unified.h>
#include <M5UnitUnified.hpp>
#include <googletest/test_template.hpp>
#include <googletest/test_helper.hpp>
#include <unit/unit_ITR9606.hpp>

using namespace m5::unit::googletest;
using namespace m5::unit;

class TestITR9606 : public GPIOComponentTestBase<UnitITR9606> {
protected:
    virtual UnitITR9606* get_instance() override
    {
        auto ptr = new m5::unit::UnitITR9606();
        return ptr;
    }
};

// Basic properties
TEST_F(TestITR9606, BasicProperties)
{
    EXPECT_STREQ(unit->deviceName(), "UnitITR9606");
    EXPECT_TRUE(unit->canAccessGPIO());
    EXPECT_FALSE(unit->canAccessI2C());
}

// Config
TEST_F(TestITR9606, Config)
{
    auto cfg     = unit->config();
    cfg.interval = 100;
    unit->config(cfg);
    auto cfg2 = unit->config();
    EXPECT_EQ(cfg2.interval, 100U);
}

// Detection reading
TEST_F(TestITR9606, ReadDetection)
{
    bool detected{};
    EXPECT_TRUE(unit->readDetection(detected));
    // Value depends on actual sensor state, just verify the call succeeds
}

// Update and state
TEST_F(TestITR9606, UpdateState)
{
    // Initial state
    EXPECT_FALSE(unit->isDetected());
    EXPECT_FALSE(unit->wasDetected());
    EXPECT_FALSE(unit->wasReleased());

    // Force update
    unit->update(true);
}
