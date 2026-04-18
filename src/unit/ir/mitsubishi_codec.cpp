/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file mitsubishi_codec.cpp
  @brief Mitsubishi 16-bit IR protocol codec
*/
#include "mitsubishi_codec.hpp"
#include "ir_rmt_items.hpp"

namespace m5 {
namespace unit {
namespace ir {

item_container_type MitsubishiCodec::encode(uint16_t address, uint16_t command, bool repeat)
{
    (void)address;
    (void)repeat;

    // Mitsubishi has no address field; the protocol is 16-bit data only.
    // The full 16-bit payload goes in `command`; `address` is unused.
    uint16_t data16 = command;

    item_container_type items;
    items.reserve(18);  // 16 bits + stop (no leader)

    // Single 16-bit frame MSB first, pulse distance. UnitIR::send() replicates per minFrames().
    encodePulseDistance(items, data16, 16, BIT_MARK, ONE_SPACE, ZERO_SPACE, false);
    items.push_back(makeTerminator(BIT_MARK));

    return items;
}

bool MitsubishiCodec::decode(const gpio::m5_rmt_item_t* items, uint32_t num, DecodeResult& result)
{
    // Mitsubishi has no leader. 16 data items + 1 trailing stop mark = 17 items minimum.
    if (num < 17) {
        return false;
    }

    // Quick check: first item should have short mark (~300us) and long or short space
    if (items[0].duration0 > 500) {
        return false;  // Not Mitsubishi (likely has a leader pulse)
    }

    // Decode 16 bits MSB first
    uint32_t data     = 0;
    uint32_t consumed = decodePulseDistance(items, num, data, 16, BIT_MARK, ONE_SPACE, ZERO_SPACE, TOLERANCE, false);
    if (consumed == 0) {
        return false;
    }

    // Verify trailing stop mark (BIT_MARK wide, no pairing space required).
    if (!matchDuration(items[16].duration0, BIT_MARK, TOLERANCE)) {
        return false;
    }

    // Mitsubishi has no address field; the full 16-bit value goes into command.
    result.protocol = CodecType::Mitsubishi;
    result.address  = 0;
    result.command  = static_cast<uint16_t>(data);
    result.raw      = data;
    result.bits     = 16;
    result.repeat   = false;
    result.toggle   = false;

    return true;
}

}  // namespace ir
}  // namespace unit
}  // namespace m5
