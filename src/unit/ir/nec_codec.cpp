/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file nec_codec.cpp
  @brief NEC / Extended NEC IR protocol codec
*/
#include "nec_codec.hpp"
#include "ir_rmt_items.hpp"

namespace m5 {
namespace unit {
namespace ir {

item_container_type NecCodec::encode(uint16_t address, uint16_t command, bool repeat)
{
    if (repeat) {
        return encodeRepeat();
    }

    // Build 32-bit NEC frame
    uint32_t data;
    uint8_t addr_lo = address & 0xFF;
    uint8_t addr_hi = (address >> 8) & 0xFF;

    if (address > 0xFF) {
        // Extended NEC: 16-bit address, no inversion
        data = addr_lo | (addr_hi << 8) | (static_cast<uint32_t>(command & 0xFF) << 16) |
               (static_cast<uint32_t>(~command & 0xFF) << 24);
    } else {
        // Standard NEC: 8-bit address + inverted
        data = addr_lo | (static_cast<uint32_t>(~addr_lo & 0xFF) << 8) | (static_cast<uint32_t>(command & 0xFF) << 16) |
               (static_cast<uint32_t>(~command & 0xFF) << 24);
    }

    return encodeRaw(data);
}

item_container_type NecCodec::encodeRaw(uint32_t data)
{
    item_container_type items;
    items.reserve(34);  // leader + 32 bits + stop

    // Leader code
    items.push_back(makeItem(LEADER_MARK, LEADER_SPACE));

    // 32 data bits (LSB first, pulse distance)
    encodePulseDistance(items, data, 32, BIT_MARK, ONE_SPACE, ZERO_SPACE, true);

    // Stop bit
    items.push_back(makeTerminator(BIT_MARK));

    return items;
}

item_container_type NecCodec::encodeRepeat()
{
    item_container_type items;
    items.reserve(2);

    // Repeat: 9ms mark + 2.25ms space + 560us stop
    items.push_back(makeItem(LEADER_MARK, REPEAT_SPACE));
    items.push_back(makeTerminator(BIT_MARK));

    return items;
}

bool NecCodec::decode(const gpio::m5_rmt_item_t* items, uint32_t num, DecodeResult& result)
{
    if (num < 2) {
        return false;
    }

    // Check leader mark
    if (!matchDuration(items[0].duration0, LEADER_MARK, TOLERANCE)) {
        return false;
    }

    // Check for repeat code: 9ms mark + 2.25ms space + stop
    if (matchDuration(items[0].duration1, REPEAT_SPACE, TOLERANCE)) {
        if (num >= 2 && matchDuration(items[1].duration0, BIT_MARK, TOLERANCE)) {
            result.protocol = CodecType::NEC;
            result.repeat   = true;
            result.bits     = 0;
            return true;
        }
        return false;
    }

    // Check leader space (4.5ms)
    if (!matchDuration(items[0].duration1, LEADER_SPACE, TOLERANCE)) {
        return false;
    }

    // Need at least leader + 32 bits + stop = 34 items
    if (num < 34) {
        return false;
    }

    // Decode 32 data bits
    uint32_t data = 0;
    uint32_t consumed =
        decodePulseDistance(items + 1, num - 1, data, 32, BIT_MARK, ONE_SPACE, ZERO_SPACE, TOLERANCE, true);
    if (consumed == 0) {
        return false;
    }

    // Extract fields
    uint8_t addr_lo = data & 0xFF;
    uint8_t addr_hi = (data >> 8) & 0xFF;
    uint8_t cmd     = (data >> 16) & 0xFF;
    uint8_t cmd_inv = (data >> 24) & 0xFF;

    // Validate command (cmd + ~cmd must equal 0xFF)
    if ((cmd ^ cmd_inv) != 0xFF) {
        return false;
    }

    result.protocol = CodecType::NEC;
    result.command  = cmd;
    result.raw      = data;
    result.bits     = 32;
    result.repeat   = false;
    result.toggle   = false;

    // Check if standard or extended NEC
    if ((addr_lo ^ addr_hi) == 0xFF) {
        // Standard NEC: 8-bit address
        result.address = addr_lo;
    } else {
        // Extended NEC: 16-bit address
        result.address = addr_lo | (addr_hi << 8);
    }

    return true;
}

}  // namespace ir
}  // namespace unit
}  // namespace m5
