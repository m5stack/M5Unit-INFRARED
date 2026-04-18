/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  UnitTest for UnitIR and IR protocol codecs
*/
#include <gtest/gtest.h>
#include <Wire.h>
#include <M5Unified.h>
#include <M5UnitUnified.hpp>
#include <googletest/test_template.hpp>
#include <googletest/test_helper.hpp>
#include <unit/unit_IR.hpp>
#include <unit/ir/nec_codec.hpp>
#include <unit/ir/sirc_codec.hpp>
#include <unit/ir/rc5_codec.hpp>
#include <unit/ir/rc6_codec.hpp>
#include <unit/ir/panasonic_codec.hpp>
#include <unit/ir/mitsubishi_codec.hpp>
#include <unit/ir/auto_detect_codec.hpp>
#include <unit/ir/ir_rmt_items.hpp>

using namespace m5::unit::googletest;
using namespace m5::unit;
using namespace m5::unit::ir;

// ============================================================
// NecCodec encode/decode round-trip tests (no hardware needed)
// ============================================================
class TestNecCodec : public ::testing::Test {
protected:
    NecCodec codec;
};

// Standard NEC: 8-bit address
TEST_F(TestNecCodec, EncodeDecodeStandard)
{
    uint16_t address = 0x04;
    uint16_t command = 0x08;

    auto items = codec.encode(address, command, false);
    // Leader(1) + 32 bits + stop(1) = 34 items
    EXPECT_EQ(items.size(), 34U);

    // Decode
    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::NEC);
    EXPECT_EQ(result.address, address);
    EXPECT_EQ(result.command, command);
    EXPECT_EQ(result.bits, 32U);
    EXPECT_FALSE(result.repeat);
}

// Extended NEC: 16-bit address
TEST_F(TestNecCodec, EncodeDecodeExtended)
{
    uint16_t address = 0x1234;
    uint16_t command = 0x56;

    auto items = codec.encode(address, command, false);
    EXPECT_EQ(items.size(), 34U);

    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::NEC);
    EXPECT_EQ(result.address, address);
    EXPECT_EQ(result.command, command);
    EXPECT_EQ(result.bits, 32U);
    EXPECT_FALSE(result.repeat);
}

// NEC repeat code
TEST_F(TestNecCodec, EncodeDecodeRepeat)
{
    auto items = codec.encode(0x00, 0x00, true);
    // Repeat: leader(1) + stop(1) = 2 items
    EXPECT_EQ(items.size(), 2U);

    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::NEC);
    EXPECT_TRUE(result.repeat);
}

// encodeRaw round-trip
TEST_F(TestNecCodec, EncodeRawRoundTrip)
{
    // Standard NEC (LSB first): [addr=0x00][~addr=0xFF][cmd=0x1F][~cmd=0xE0]
    uint32_t raw = 0xE01FFF00;
    auto items   = codec.encodeRaw(raw);
    EXPECT_EQ(items.size(), 34U);

    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.raw, raw);
    EXPECT_EQ(result.address, 0x00U);
    EXPECT_EQ(result.command, 0x1FU);
}

// Various address/command combinations
TEST_F(TestNecCodec, MultipleValues)
{
    struct TestCase {
        uint16_t address;
        uint16_t command;
    };
    TestCase cases[] = {
        {0x00, 0x00}, {0xFF, 0xFF}, {0x01, 0x01}, {0x80, 0x80}, {0xABCD, 0x42},
    };

    for (const auto& tc : cases) {
        auto items = codec.encode(tc.address, tc.command, false);
        DecodeResult result{};
        EXPECT_TRUE(codec.decode(items.data(), items.size(), result))
            << "addr=0x" << std::hex << tc.address << " cmd=0x" << tc.command;
        EXPECT_EQ(result.address, tc.address);
        EXPECT_EQ(result.command, tc.command);
    }
}

// Codec properties
TEST_F(TestNecCodec, Properties)
{
    EXPECT_EQ(codec.type(), CodecType::NEC);
    EXPECT_EQ(codec.carrierFrequencyHz(), 38000U);
    EXPECT_GT(codec.carrierDuty(), 0.0f);
    EXPECT_LT(codec.carrierDuty(), 1.0f);
}

// Decode should reject garbage data
TEST_F(TestNecCodec, DecodeRejectsGarbage)
{
    gpio::m5_rmt_item_t garbage[34]{};
    for (auto& item : garbage) {
        item.duration0 = 100;
        item.level0    = 1;
        item.duration1 = 100;
        item.level1    = 0;
    }
    DecodeResult result{};
    EXPECT_FALSE(codec.decode(garbage, 34, result));
}

// Decode should reject too-short data
TEST_F(TestNecCodec, DecodeRejectsTooShort)
{
    DecodeResult result{};
    gpio::m5_rmt_item_t items[1]{};
    EXPECT_FALSE(codec.decode(items, 1, result));
    EXPECT_FALSE(codec.decode(nullptr, 0, result));
}

// ============================================================
// SircCodec encode/decode round-trip tests
// ============================================================
class TestSircCodec : public ::testing::Test {
protected:
    SircCodec codec12{SircCodec::Variant::SIRC12};
    SircCodec codec15{SircCodec::Variant::SIRC15};
    SircCodec codec20{SircCodec::Variant::SIRC20};
};

TEST_F(TestSircCodec, Properties)
{
    EXPECT_EQ(codec12.type(), CodecType::SIRC);
    EXPECT_EQ(codec12.carrierFrequencyHz(), 40000U);
}

TEST_F(TestSircCodec, EncodeDecodeSIRC12)
{
    uint16_t addr = 0x01;  // TV
    uint16_t cmd  = 0x15;
    auto items    = codec12.encode(addr, cmd, false);
    EXPECT_EQ(items.size(), 13U);  // start + 12 bits

    DecodeResult result{};
    EXPECT_TRUE(codec12.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::SIRC);
    EXPECT_EQ(result.address, addr);
    EXPECT_EQ(result.command, cmd);
    EXPECT_EQ(result.bits, 12U);
}

TEST_F(TestSircCodec, EncodeDecodeSIRC15)
{
    uint16_t addr = 0xAB;
    uint16_t cmd  = 0x3F;
    auto items    = codec15.encode(addr, cmd, false);
    EXPECT_EQ(items.size(), 16U);  // start + 15 bits

    DecodeResult result{};
    EXPECT_TRUE(codec15.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::SIRC);
    EXPECT_EQ(result.address, addr);
    EXPECT_EQ(result.command, cmd);
    EXPECT_EQ(result.bits, 15U);
}

TEST_F(TestSircCodec, EncodeDecodeSIRC20)
{
    // 20-bit: 5-bit addr + 8-bit extended in upper byte
    uint16_t addr = 0x1F | (0xAB << 5);  // addr=0x1F, ext=0xAB
    uint16_t cmd  = 0x42;
    auto items    = codec20.encode(addr, cmd, false);
    EXPECT_EQ(items.size(), 21U);  // start + 20 bits

    DecodeResult result{};
    EXPECT_TRUE(codec20.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::SIRC);
    EXPECT_EQ(result.address, addr);
    EXPECT_EQ(result.command, cmd);
    EXPECT_EQ(result.bits, 20U);
}

TEST_F(TestSircCodec, MultipleValues)
{
    struct TestCase {
        SircCodec::Variant variant;
        uint16_t address;
        uint16_t command;
    };
    const TestCase cases[] = {
        {SircCodec::Variant::SIRC12, 0x00, 0x00},
        {SircCodec::Variant::SIRC12, 0x1F, 0x7F},
        {SircCodec::Variant::SIRC12, 0x01, 0x15},
        {SircCodec::Variant::SIRC15, 0x00, 0x00},
        {SircCodec::Variant::SIRC15, 0xFF, 0x7F},
        {SircCodec::Variant::SIRC15, 0xAB, 0x3F},
        {SircCodec::Variant::SIRC20, 0x00, 0x00},
        // SIRC20 address = 5-bit low + 8-bit ext (upper byte of the 13-bit field)
        {SircCodec::Variant::SIRC20, static_cast<uint16_t>(0x1F | (0xAB << 5)), 0x42},
        {SircCodec::Variant::SIRC20, static_cast<uint16_t>(0x1F | (0xFF << 5)), 0x7F},
    };
    for (const auto& tc : cases) {
        SircCodec c{tc.variant};
        auto items = c.encode(tc.address, tc.command, false);
        DecodeResult result{};
        EXPECT_TRUE(c.decode(items.data(), items.size(), result))
            << "variant=" << static_cast<int>(tc.variant) << " addr=0x" << std::hex << tc.address << " cmd=0x"
            << tc.command;
        EXPECT_EQ(result.address, tc.address);
        EXPECT_EQ(result.command, tc.command);
        EXPECT_EQ(result.bits, static_cast<uint8_t>(tc.variant));
    }
}

TEST_F(TestSircCodec, DecodeRejectsTooShort)
{
    DecodeResult result{};
    gpio::m5_rmt_item_t items[1]{};
    EXPECT_FALSE(codec12.decode(items, 1, result));
    EXPECT_FALSE(codec12.decode(nullptr, 0, result));
}

TEST_F(TestSircCodec, DecodeRejectsGarbage)
{
    gpio::m5_rmt_item_t garbage[20]{};
    for (auto& item : garbage) {
        item.duration0 = 100;
        item.level0    = 1;
        item.duration1 = 100;
        item.level1    = 0;
    }
    DecodeResult result{};
    EXPECT_FALSE(codec12.decode(garbage, 20, result));
}

// decode() synchronises the internal `_variant` to the detected one so that
// a subsequent encode() on the same codec instance emits the matching width.
// This matters when AutoDetectCodec's internal SircCodec receives e.g. SIRC20
// and the user then wants to echo the command back.
TEST_F(TestSircCodec, DecodeSyncsVariant)
{
    SircCodec codec{SircCodec::Variant::SIRC12};
    EXPECT_EQ(codec.variant(), SircCodec::Variant::SIRC12);

    // Encode a SIRC20 signal with a separate encoder and decode it through `codec`.
    SircCodec encoder{SircCodec::Variant::SIRC20};
    uint16_t addr = static_cast<uint16_t>(0x1F | (0xAB << 5));  // 5-bit addr + 8-bit ext
    uint16_t cmd  = 0x42;
    auto items    = encoder.encode(addr, cmd, false);

    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.bits, 20U);
    EXPECT_EQ(codec.variant(), SircCodec::Variant::SIRC20);

    // Subsequent encode() must now emit a SIRC20-shaped frame (start + 20 bits).
    auto out = codec.encode(addr, cmd, false);
    EXPECT_EQ(out.size(), 21U);

    // Decode a SIRC15 signal next — variant should hop again to SIRC15.
    SircCodec encoder15{SircCodec::Variant::SIRC15};
    auto items15 = encoder15.encode(0xAB, 0x3F, false);
    DecodeResult r15{};
    EXPECT_TRUE(codec.decode(items15.data(), items15.size(), r15));
    EXPECT_EQ(codec.variant(), SircCodec::Variant::SIRC15);
    EXPECT_EQ(codec.encode(0xAB, 0x3F, false).size(), 16U);  // start + 15 bits
}

// ============================================================
// Helper: emulate what real RX hardware captures by dropping the
// leading idle-space from a round-trip encode() output.
// Needed for RC5 (no leader, Manchester starts with a space for S1=1).
// ============================================================
static item_container_type stripLeadingSpace(const item_container_type& in)
{
    struct Seg {
        uint8_t lvl;
        uint16_t dur;
    };
    std::vector<Seg> stream;
    stream.reserve(in.size() * 2);
    for (const auto& it : in) {
        stream.push_back({it.level0, it.duration0});
        if (it.duration1 > 0) {
            stream.push_back({it.level1, it.duration1});
        }
    }
    if (!stream.empty() && stream.front().lvl == 0) {
        stream.erase(stream.begin());
    }
    item_container_type out;
    out.reserve((stream.size() + 1) / 2);
    for (size_t i = 0; i < stream.size(); i += 2) {
        m5::unit::gpio::m5_rmt_item_t item{};
        item.duration0 = stream[i].dur;
        item.level0    = stream[i].lvl;
        if (i + 1 < stream.size()) {
            item.duration1 = stream[i + 1].dur;
            item.level1    = stream[i + 1].lvl;
        }
        out.push_back(item);
    }
    return out;
}

// ============================================================
// Rc5Codec encode/decode round-trip tests
// ============================================================
class TestRc5Codec : public ::testing::Test {
protected:
    Rc5Codec codec;
};

TEST_F(TestRc5Codec, Properties)
{
    EXPECT_EQ(codec.type(), CodecType::RC5);
    EXPECT_EQ(codec.carrierFrequencyHz(), 36000U);
}

TEST_F(TestRc5Codec, EncodeDecodeBasic)
{
    codec.setToggle(false);
    auto items = codec.encode(0x05, 0x10, false);
    EXPECT_GT(items.size(), 0U);

    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::RC5);
    EXPECT_EQ(result.address, 0x05U);
    EXPECT_EQ(result.command, 0x10U);
    EXPECT_EQ(result.bits, 14U);
    EXPECT_FALSE(result.toggle);
}

TEST_F(TestRc5Codec, ToggleBit)
{
    codec.setToggle(true);
    auto items = codec.encode(0x00, 0x01, false);

    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_TRUE(result.toggle);
}

TEST_F(TestRc5Codec, RC5XExtendedCommand)
{
    // Command > 63 uses RC5X (S2 = ~Cmd[6])
    codec.setToggle(false);
    auto items = codec.encode(0x01, 0x50, false);  // cmd=80, bit6=1

    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.command, 0x50U);
}

TEST_F(TestRc5Codec, MultipleValues)
{
    struct TestCase {
        uint16_t address;
        uint16_t command;
        bool toggle;
    };
    const TestCase cases[] = {
        {0x00, 0x00, false}, {0x1F, 0x3F, false}, {0x05, 0x10, true}, {0x01, 0x50, false},  // RC5X (s2=0)
        {0x1F, 0x7F, true},                                                                 // RC5X + toggle
        {0x10, 0x40, false}, {0x0A, 0x2A, false},                                           // RC5X boundary (cmd=0x40)
    };
    for (const auto& tc : cases) {
        codec.setToggle(tc.toggle);
        auto items = codec.encode(tc.address, tc.command, false);
        DecodeResult result{};
        EXPECT_TRUE(codec.decode(items.data(), items.size(), result))
            << "addr=0x" << std::hex << tc.address << " cmd=0x" << tc.command;
        EXPECT_EQ(result.address, tc.address);
        EXPECT_EQ(result.command, tc.command);
        EXPECT_EQ(result.toggle, tc.toggle);
    }
}

TEST_F(TestRc5Codec, EdgeCommands)
{
    codec.setToggle(false);
    uint16_t commands[] = {0x00, 0x3F, 0x40, 0x7F};  // RC5/RC5X boundaries
    for (auto cmd : commands) {
        auto items = codec.encode(0x05, cmd, false);
        DecodeResult result{};
        EXPECT_TRUE(codec.decode(items.data(), items.size(), result)) << "cmd=0x" << std::hex << cmd;
        EXPECT_EQ(result.command, cmd);
    }
}

TEST_F(TestRc5Codec, RXNoLeadingSpace)
{
    // Emulate real RX capture (leading idle-space stripped by hardware).
    codec.setToggle(false);
    struct TestCase {
        uint16_t address;
        uint16_t command;
    };
    const TestCase cases[] = {
        {0x05, 0x10},  // s2=1 (no S1-S2 merge)
        {0x01, 0x50},  // s2=0 (S1.mark + S2.mark merged into FULL_BIT)
        {0x1F, 0x3F},  // all-ones address / command
    };
    for (const auto& tc : cases) {
        auto items = codec.encode(tc.address, tc.command, false);
        auto rx    = stripLeadingSpace(items);
        DecodeResult result{};
        EXPECT_TRUE(codec.decode(rx.data(), rx.size(), result))
            << "addr=0x" << std::hex << tc.address << " cmd=0x" << tc.command;
        EXPECT_EQ(result.address, tc.address);
        EXPECT_EQ(result.command, tc.command);
    }
}

TEST_F(TestRc5Codec, DecodeRejectsTooShort)
{
    DecodeResult result{};
    gpio::m5_rmt_item_t items[1]{};
    EXPECT_FALSE(codec.decode(items, 1, result));
    EXPECT_FALSE(codec.decode(nullptr, 0, result));
}

TEST_F(TestRc5Codec, DecodeRejectsGarbage)
{
    gpio::m5_rmt_item_t garbage[30]{};
    for (auto& item : garbage) {
        item.duration0 = 100;
        item.level0    = 1;
        item.duration1 = 100;
        item.level1    = 0;
    }
    DecodeResult result{};
    EXPECT_FALSE(codec.decode(garbage, 30, result));
}

// ============================================================
// Rc6Codec encode/decode round-trip tests
// ============================================================
class TestRc6Codec : public ::testing::Test {
protected:
    Rc6Codec codec;
};

TEST_F(TestRc6Codec, Properties)
{
    EXPECT_EQ(codec.type(), CodecType::RC6);
    EXPECT_EQ(codec.carrierFrequencyHz(), 36000U);
}

TEST_F(TestRc6Codec, EncodeDecodeBasic)
{
    codec.setToggle(false);
    auto items = codec.encode(0x04, 0x3C, false);
    EXPECT_GT(items.size(), 0U);

    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::RC6);
    EXPECT_EQ(result.address, 0x04U);
    EXPECT_EQ(result.command, 0x3CU);
    EXPECT_FALSE(result.toggle);
}

TEST_F(TestRc6Codec, ToggleBit)
{
    codec.setToggle(true);
    auto items = codec.encode(0x00, 0x01, false);

    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_TRUE(result.toggle);
}

TEST_F(TestRc6Codec, MultipleValues)
{
    struct TestCase {
        uint16_t address;
        uint16_t command;
        bool toggle;
    };
    const TestCase cases[] = {
        {0x00, 0x00, false}, {0xFF, 0xFF, false}, {0x04, 0x3C, true},
        {0x80, 0x01, false}, {0x01, 0x80, true},  {0xAA, 0x55, false},
    };
    for (const auto& tc : cases) {
        codec.setToggle(tc.toggle);
        auto items = codec.encode(tc.address, tc.command, false);
        DecodeResult result{};
        EXPECT_TRUE(codec.decode(items.data(), items.size(), result))
            << "addr=0x" << std::hex << tc.address << " cmd=0x" << tc.command;
        EXPECT_EQ(result.address, tc.address);
        EXPECT_EQ(result.command, tc.command);
        EXPECT_EQ(result.toggle, tc.toggle);
    }
}

TEST_F(TestRc6Codec, DecodeRejectsTooShort)
{
    DecodeResult result{};
    gpio::m5_rmt_item_t items[1]{};
    EXPECT_FALSE(codec.decode(items, 1, result));
    EXPECT_FALSE(codec.decode(nullptr, 0, result));
}

TEST_F(TestRc6Codec, DecodeRejectsGarbage)
{
    gpio::m5_rmt_item_t garbage[30]{};
    for (auto& item : garbage) {
        item.duration0 = 100;
        item.level0    = 1;
        item.duration1 = 100;
        item.level1    = 0;
    }
    DecodeResult result{};
    EXPECT_FALSE(codec.decode(garbage, 30, result));
}

// ============================================================
// PanasonicCodec encode/decode round-trip tests
// ============================================================
class TestPanasonicCodec : public ::testing::Test {
protected:
    PanasonicCodec codec;
};

TEST_F(TestPanasonicCodec, Properties)
{
    EXPECT_EQ(codec.type(), CodecType::Panasonic);
    EXPECT_EQ(codec.carrierFrequencyHz(), 36700U);
}

TEST_F(TestPanasonicCodec, EncodeDecodeBasic)
{
    uint16_t address = 0x4004;
    uint16_t command = 0xBCBD;
    auto items       = codec.encode(address, command, false);
    EXPECT_GT(items.size(), 0U);

    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::Panasonic);
    EXPECT_EQ(result.address, address);
    EXPECT_EQ(result.command, command);
    EXPECT_EQ(result.bits, 48U);
    EXPECT_FALSE(result.repeat);
}

TEST_F(TestPanasonicCodec, MultipleValues)
{
    struct TestCase {
        uint16_t address;
        uint16_t command;
    };
    TestCase cases[] = {
        {0x4004, 0x0100},
        {0x4004, 0xBCBD},
        {0x2002, 0x1234},
    };

    for (const auto& tc : cases) {
        auto items = codec.encode(tc.address, tc.command, false);
        DecodeResult result{};
        EXPECT_TRUE(codec.decode(items.data(), items.size(), result))
            << "addr=0x" << std::hex << tc.address << " cmd=0x" << tc.command;
        EXPECT_EQ(result.address, tc.address);
        EXPECT_EQ(result.command, tc.command);
    }
}

TEST_F(TestPanasonicCodec, DecodeRejectsTooShort)
{
    DecodeResult result{};
    gpio::m5_rmt_item_t items[1]{};
    EXPECT_FALSE(codec.decode(items, 1, result));
    EXPECT_FALSE(codec.decode(nullptr, 0, result));
}

TEST_F(TestPanasonicCodec, DecodeRejectsGarbage)
{
    gpio::m5_rmt_item_t garbage[50]{};
    for (auto& item : garbage) {
        item.duration0 = 100;
        item.level0    = 1;
        item.duration1 = 100;
        item.level1    = 0;
    }
    DecodeResult result{};
    EXPECT_FALSE(codec.decode(garbage, 50, result));
}

// ============================================================
// MitsubishiCodec encode/decode round-trip tests
// ============================================================
class TestMitsubishiCodec : public ::testing::Test {
protected:
    MitsubishiCodec codec;
};

TEST_F(TestMitsubishiCodec, Properties)
{
    EXPECT_EQ(codec.type(), CodecType::Mitsubishi);
    EXPECT_EQ(codec.carrierFrequencyHz(), 33000U);
}

TEST_F(TestMitsubishiCodec, EncodeDecodeBasic)
{
    // Mitsubishi: addr=0, cmd=16bit data
    uint16_t command = 0xE240;
    auto items       = codec.encode(0, command, false);
    EXPECT_GT(items.size(), 0U);

    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::Mitsubishi);
    EXPECT_EQ(result.address, 0U);
    EXPECT_EQ(result.command, command);
    EXPECT_EQ(result.bits, 16U);
}

TEST_F(TestMitsubishiCodec, MultipleValues)
{
    uint16_t commands[] = {0xE240, 0x1234, 0xFFFF, 0x0001};

    for (auto cmd : commands) {
        auto items = codec.encode(0, cmd, false);
        DecodeResult result{};
        EXPECT_TRUE(codec.decode(items.data(), items.size(), result)) << "cmd=0x" << std::hex << cmd;
        EXPECT_EQ(result.command, cmd);
    }
}

TEST_F(TestMitsubishiCodec, DecodeRejectsTooShort)
{
    DecodeResult result{};
    gpio::m5_rmt_item_t items[1]{};
    EXPECT_FALSE(codec.decode(items, 1, result));
    EXPECT_FALSE(codec.decode(nullptr, 0, result));
}

TEST_F(TestMitsubishiCodec, DecodeRejectsGarbage)
{
    gpio::m5_rmt_item_t garbage[20]{};
    for (auto& item : garbage) {
        item.duration0 = 5000;  // Too long for Mitsubishi bit mark (~300us)
        item.level0    = 1;
        item.duration1 = 100;
        item.level1    = 0;
    }
    DecodeResult result{};
    EXPECT_FALSE(codec.decode(garbage, 20, result));
}

// ============================================================
// AutoDetectCodec tests
// ============================================================
class TestAutoDetect : public ::testing::Test {
protected:
    AutoDetectCodec codec;
    NecCodec nec;
    SircCodec sirc12{SircCodec::Variant::SIRC12};
    Rc5Codec rc5;
    Rc6Codec rc6;
    PanasonicCodec panasonic;
    MitsubishiCodec mitsubishi;
};

TEST_F(TestAutoDetect, DetectsNEC)
{
    auto items = nec.encode(0x04, 0x08, false);
    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::NEC);
    EXPECT_EQ(result.address, 0x04U);
    EXPECT_EQ(result.command, 0x08U);
}

TEST_F(TestAutoDetect, DetectsSIRC)
{
    auto items = sirc12.encode(0x01, 0x15, false);
    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::SIRC);
    EXPECT_EQ(result.address, 0x01U);
    EXPECT_EQ(result.command, 0x15U);
}

TEST_F(TestAutoDetect, DetectsRC6)
{
    rc6.setToggle(false);
    auto items = rc6.encode(0x04, 0x3C, false);
    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::RC6);
    EXPECT_EQ(result.address, 0x04U);
    EXPECT_EQ(result.command, 0x3CU);
}

TEST_F(TestAutoDetect, DetectsRC5)
{
    rc5.setToggle(false);
    auto items = rc5.encode(0x05, 0x10, false);
    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::RC5);
    EXPECT_EQ(result.address, 0x05U);
    EXPECT_EQ(result.command, 0x10U);
}

TEST_F(TestAutoDetect, DetectsRC5X)
{
    // RC5X path: cmd > 63, s2=0, S1.mark + S2.mark merged into FULL_BIT
    rc5.setToggle(false);
    auto items = rc5.encode(0x01, 0x50, false);
    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::RC5);
    EXPECT_EQ(result.address, 0x01U);
    EXPECT_EQ(result.command, 0x50U);
}

TEST_F(TestAutoDetect, DetectsPanasonic)
{
    auto items = panasonic.encode(0x4004, 0xBCBD, false);
    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::Panasonic);
    EXPECT_EQ(result.address, 0x4004U);
    EXPECT_EQ(result.command, 0xBCBDU);
}

TEST_F(TestAutoDetect, DetectsMitsubishi)
{
    auto items = mitsubishi.encode(0, 0xE240, false);
    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::Mitsubishi);
    EXPECT_EQ(result.command, 0xE240U);
}

TEST_F(TestAutoDetect, NECRepeat)
{
    auto items = nec.encodeRepeat();
    DecodeResult result{};
    EXPECT_TRUE(codec.decode(items.data(), items.size(), result));
    EXPECT_EQ(result.protocol, CodecType::NEC);
    EXPECT_TRUE(result.repeat);
}

// ============================================================
// ir_rmt_items helper tests
// ============================================================
class TestIRRmtItems : public ::testing::Test {
};

TEST_F(TestIRRmtItems, MakeItem)
{
    auto item = makeItem(9000, 4500);
    EXPECT_EQ(item.duration0, 9000U);
    EXPECT_EQ(item.level0, 1U);
    EXPECT_EQ(item.duration1, 4500U);
    EXPECT_EQ(item.level1, 0U);
}

TEST_F(TestIRRmtItems, MakeTerminator)
{
    auto item = makeTerminator(560);
    EXPECT_EQ(item.duration0, 560U);
    EXPECT_EQ(item.level0, 1U);
    EXPECT_EQ(item.duration1, 0U);
    EXPECT_EQ(item.level1, 0U);
}

TEST_F(TestIRRmtItems, MatchDuration)
{
    EXPECT_TRUE(matchDuration(560, 560, 200));
    EXPECT_TRUE(matchDuration(500, 560, 200));
    EXPECT_TRUE(matchDuration(700, 560, 200));
    EXPECT_FALSE(matchDuration(300, 560, 200));
    EXPECT_FALSE(matchDuration(800, 560, 200));
}

TEST_F(TestIRRmtItems, PulseDistanceRoundTrip)
{
    item_container_type items;
    uint32_t data_in = 0xDEADBEEF;
    encodePulseDistance(items, data_in, 32, 560, 1690, 560, true);
    EXPECT_EQ(items.size(), 32U);

    uint32_t data_out = 0;
    uint32_t consumed = decodePulseDistance(items.data(), items.size(), data_out, 32, 560, 1690, 560, 200, true);
    EXPECT_EQ(consumed, 32U);
    EXPECT_EQ(data_out, data_in);
}

TEST_F(TestIRRmtItems, PulseWidthRoundTrip)
{
    item_container_type items;
    uint32_t data_in = 0x0A5;  // 12-bit
    encodePulseWidth(items, data_in, 12, 1200, 600, 600, true);
    EXPECT_EQ(items.size(), 12U);

    uint32_t data_out = 0;
    uint32_t consumed = decodePulseWidth(items.data(), items.size(), data_out, 12, 1200, 600, 600, 150, true);
    EXPECT_EQ(consumed, 12U);
    EXPECT_EQ(data_out, data_in);
}

// ============================================================
// UnitIR component tests (requires hardware)
// ============================================================
class TestUnitIR : public GPIOComponentTestBase<UnitIR> {
protected:
    virtual UnitIR* get_instance() override
    {
        auto ptr = new m5::unit::UnitIR();
        return ptr;
    }
};

// Basic properties
TEST_F(TestUnitIR, BasicProperties)
{
    EXPECT_STREQ(unit->deviceName(), "UnitIR");
    EXPECT_TRUE(unit->canAccessGPIO());
    EXPECT_FALSE(unit->canAccessI2C());
}

// Config: getter/setter round-trip only. Note that begin() has already been called
// by GPIOComponentTestBase, so these values are NOT applied to the running RMT
// channels — only the cached _cfg is exercised here.
TEST_F(TestUnitIR, ConfigGetterSetter)
{
    auto cfg                = unit->config();
    cfg.rx_ring_buffer_size = 2048;
    cfg.rx_idle_threshold   = 15000;
    cfg.rx_filter_threshold = 100;
    unit->config(cfg);
    auto cfg2 = unit->config();
    EXPECT_EQ(cfg2.rx_ring_buffer_size, 2048U);
    EXPECT_EQ(cfg2.rx_idle_threshold, 15000U);
    EXPECT_EQ(cfg2.rx_filter_threshold, 100U);
}

// TX/RX state
TEST_F(TestUnitIR, TxRxState)
{
    // After begin via GPIOComponentTestBase, both TX and RX should be available
    EXPECT_TRUE(unit->hasTX());
    EXPECT_TRUE(unit->hasRX());
}

// Codec management
TEST_F(TestUnitIR, CodecManagement)
{
    // Default is AutoDetectCodec (type = Unknown)
    EXPECT_EQ(unit->codec().type(), CodecType::Unknown);

    // Set a different codec
    NecCodec nec;
    unit->setCodec(nec);
    EXPECT_EQ(unit->codec().type(), CodecType::NEC);

    // Reset to default
    unit->resetCodec();
    EXPECT_EQ(unit->codec().type(), CodecType::Unknown);
}

// RX initial state
TEST_F(TestUnitIR, RxInitialState)
{
    EXPECT_TRUE(unit->empty());
    EXPECT_EQ(unit->available(), 0U);
}

// Flush
TEST_F(TestUnitIR, Flush)
{
    unit->flush();
    EXPECT_TRUE(unit->empty());
}

// Update
TEST_F(TestUnitIR, Update)
{
    unit->update(true);
    // No signal expected, should remain empty
    EXPECT_TRUE(unit->empty());
}

// Optional self-loopback test (disabled by default).
// To enable, rename to `SelfLoopback` and ensure the IR LED is aimed at the
// VS1838B (e.g. Unit IR module powered on both TX and RX pins, or an external
// loopback mirror). GoogleTest recognises the `DISABLED_` prefix and skips it
// during normal runs; execute with `--gtest_also_run_disabled_tests` when
// hardware is configured.
TEST_F(TestUnitIR, DISABLED_SelfLoopback)
{
    if (!unit->hasTX() || !unit->hasRX()) {
        GTEST_SKIP() << "Loopback requires both TX and RX configured";
    }
    NecCodec nec;
    unit->setCodec(nec);
    unit->flush();

    const uint16_t address = 0x04;
    const uint16_t command = 0x42;
    EXPECT_TRUE(unit->send(address, command, 1));

    // Poll for up to ~300 ms for the receiver to capture the frame
    bool received = false;
    for (int i = 0; i < 60 && !received; ++i) {
        m5::utility::delay(5);
        unit->update();
        received = !unit->empty();
    }

    EXPECT_TRUE(received);
    if (received) {
        const auto& r = unit->latest();
        EXPECT_EQ(r.protocol, CodecType::NEC);
        EXPECT_EQ(r.address, address);
        EXPECT_EQ(r.command, command);
    }
}
