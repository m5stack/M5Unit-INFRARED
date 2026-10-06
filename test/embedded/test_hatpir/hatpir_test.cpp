/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  UnitTest for HatPIR (AS312) connected via the Hat header.

  HatPIR uses the same AS312 sensor as UnitPIR, but it is wired to the board's Hat DOUT pin
  instead of the Grove port. The connection therefore goes through the library-local wiring
  helper m5::unit::infrared::wiring::addHatPIR() rather than the default Grove PortB path.
*/
#include <gtest/gtest.h>
#include <Wire.h>
#include <M5Unified.h>
#include <M5UnitUnified.hpp>
#include <googletest/test_template.hpp>
#include <googletest/test_helper.hpp>
#include <M5UnitUnifiedINFRARED.hpp>
#include <wiring/m5_unit_unified_infrared_wiring.hpp>  // include LAST (board-aware wiring)

using namespace m5::unit::googletest;
using namespace m5::unit;

class TestHatPIR : public GPIOComponentTestBase<HatPIR> {
protected:
    virtual HatPIR* get_instance() override
    {
        return new m5::unit::HatPIR();
    }
    // Connect on the board's Hat header DOUT pin (differs from UnitPIR's Grove PortB)
    virtual bool begin() override
    {
        return m5::unit::infrared::wiring::addHatPIR(Units, *unit) && Units.begin();
    }
};

// Basic properties
TEST_F(TestHatPIR, BasicProperties)
{
    EXPECT_STREQ(unit->deviceName(), "UnitAS312");
    EXPECT_TRUE(unit->canAccessGPIO());
    EXPECT_FALSE(unit->canAccessI2C());
}

// Config
TEST_F(TestHatPIR, Config)
{
    auto cfg     = unit->config();
    cfg.interval = 200;
    unit->config(cfg);
    auto cfg2 = unit->config();
    EXPECT_EQ(cfg2.interval, 200U);
}

// Detection reading
TEST_F(TestHatPIR, ReadDetection)
{
    bool detected{};
    EXPECT_TRUE(unit->readDetection(detected));
    // Value depends on actual sensor state, just verify the call succeeds
}

// Update and state
TEST_F(TestHatPIR, UpdateState)
{
    // Initial state
    EXPECT_FALSE(unit->isDetected());
    EXPECT_FALSE(unit->wasDetected());
    EXPECT_FALSE(unit->wasReleased());

    // Force update
    unit->update(true);
    EXPECT_NE(unit->updatedMillis(), 0U);                // the pin was read
    EXPECT_EQ(unit->updated(), unit->isDetected());      // changed only if it is now detected
    EXPECT_EQ(unit->wasDetected(), unit->isDetected());  // rising edge from the initial false
    EXPECT_FALSE(unit->wasReleased());
}
