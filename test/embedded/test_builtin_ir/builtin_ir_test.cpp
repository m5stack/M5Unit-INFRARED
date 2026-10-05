/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  UnitTest for the board's built-in IR (UnitIR driver on the on-board IR LED / receiver).

  Built-in IR is wired to board-specific GPIO instead of the Grove port, so the connection goes
  through the library-local wiring helpers m5::unit::infrared::wiring::addBuiltinIrTx()/Rx().
  Most boards expose a built-in IR LED (TX only); StickS3 additionally has a receiver (RX).
*/
#include <gtest/gtest.h>
#include <Wire.h>
#include <M5Unified.h>
#include <M5UnitUnified.hpp>
#include <googletest/test_template.hpp>
#include <googletest/test_helper.hpp>
#include <unit/unit_IR.hpp>
#include <unit/ir/nec_codec.hpp>
#include <wiring/m5_unit_unified_infrared_wiring.hpp>  // include LAST (board-aware wiring)

using namespace m5::unit::googletest;
using namespace m5::unit;
using namespace m5::unit::ir;

#if M5_UNIT_UNIFIED_HAS_RMT
class TestBuiltinIR : public GPIOComponentTestBase<UnitIR> {
protected:
    virtual UnitIR* get_instance() override
    {
        return new m5::unit::UnitIR();
    }
    // Connect on the board's built-in IR instead of the Grove port.
    // StickS3 has a receiver (RX); the other supported boards are TX-only (IR LED).
    virtual bool begin() override
    {
        namespace iw = m5::unit::infrared::wiring;
        if (iw::builtinIrRxPin() >= 0) {
            return iw::addBuiltinIrRx(Units, *unit) && Units.begin();
        }
        return iw::addBuiltinIrTx(Units, *unit) && Units.begin();
    }
};

// Basic properties
TEST_F(TestBuiltinIR, BasicProperties)
{
    EXPECT_STREQ(unit->deviceName(), "UnitIR");
    EXPECT_TRUE(unit->canAccessGPIO());
    EXPECT_FALSE(unit->canAccessI2C());
}

// Connection: built-in IR is RX on StickS3, TX-only on the other boards
TEST_F(TestBuiltinIR, Connection)
{
    namespace iw = m5::unit::infrared::wiring;
    if (iw::builtinIrRxPin() >= 0) {
        EXPECT_TRUE(unit->hasRX());
    } else {
        EXPECT_TRUE(unit->hasTX());
    }
}

// Codec management (no signal needed)
TEST_F(TestBuiltinIR, CodecManagement)
{
    // Default is AutoDetectCodec (type = Unknown)
    EXPECT_EQ(unit->codec().type(), CodecType::Unknown);

    NecCodec nec;
    unit->setCodec(nec);
    EXPECT_EQ(unit->codec().type(), CodecType::NEC);

    unit->resetCodec();
    EXPECT_EQ(unit->codec().type(), CodecType::Unknown);
}
#endif
