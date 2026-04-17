/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file sirc_codec.cpp
  @brief Sony SIRC IR protocol codec
*/
#include "sirc_codec.hpp"
#include "ir_rmt_items.hpp"

namespace m5 {
namespace unit {
namespace ir {

namespace {
// Reverse the lowest `bits` bits of `v`. Used to match IRremoteESP8266's raw
// convention where the wire is transmitted LSB-first but the stored "raw"
// value is bit-reversed (i.e. first-sent bit ends up in the highest bit).
uint64_t reverse_bit_low(uint64_t v, uint8_t bits)
{
    uint64_t r = 0;
    for (uint8_t i = 0; i < bits; ++i) {
        r = (r << 1) | (v & 1);
        v >>= 1;
    }
    return r;
}
}  // namespace

item_container_type SircCodec::encode(uint16_t address, uint16_t command, bool repeat)
{
    (void)repeat;  // SIRC repeats by re-sending the full frame

    item_container_type items;
    uint8_t total_bits = static_cast<uint8_t>(_variant);

    items.reserve(1 + total_bits);

    // Start burst
    items.push_back(makeItem(START_MARK, START_SPACE));

    // Command: 7 bits LSB first
    encodePulseWidth(items, command & 0x7F, 7, ONE_MARK, ZERO_MARK, BIT_SPACE, true);

    // Address + extended depending on variant
    switch (_variant) {
        case Variant::SIRC12:
            // 5-bit address
            encodePulseWidth(items, address & 0x1F, 5, ONE_MARK, ZERO_MARK, BIT_SPACE, true);
            break;
        case Variant::SIRC15:
            // 8-bit address
            encodePulseWidth(items, address & 0xFF, 8, ONE_MARK, ZERO_MARK, BIT_SPACE, true);
            break;
        case Variant::SIRC20:
            // 5-bit address + 8-bit extended (extended in upper byte of address)
            encodePulseWidth(items, address & 0x1F, 5, ONE_MARK, ZERO_MARK, BIT_SPACE, true);
            encodePulseWidth(items, (address >> 5) & 0xFF, 8, ONE_MARK, ZERO_MARK, BIT_SPACE, true);
            break;
    }

    return items;
}

bool SircCodec::decode(const gpio::m5_rmt_item_t* items, uint32_t num, DecodeResult& result)
{
    if (num < 8) {  // Minimum: start + 7 command bits
        return false;
    }

    // Check start burst
    if (!matchDuration(items[0].duration0, START_MARK, TOLERANCE)) {
        return false;
    }
    if (!matchDuration(items[0].duration1, START_SPACE, TOLERANCE)) {
        return false;
    }

    // Determine variant by total item count (excluding start)
    // 12-bit: 12 data items, 15-bit: 15, 20-bit: 20
    uint8_t data_items = num - 1;

    // Try decoding command (7 bits) first
    uint32_t cmd_data = 0;
    uint32_t consumed =
        decodePulseWidth(items + 1, data_items, cmd_data, 7, ONE_MARK, ZERO_MARK, BIT_SPACE, TOLERANCE, true);
    if (consumed == 0) {
        return false;
    }

    // Try each variant from longest to shortest
    uint32_t addr_data = 0;
    Variant detected   = Variant::SIRC12;

    if (data_items >= 20) {
        // Try 20-bit: 7 cmd + 5 addr + 8 ext
        uint32_t addr5 = 0;
        uint32_t ext8  = 0;
        uint32_t c1 =
            decodePulseWidth(items + 8, data_items - 7, addr5, 5, ONE_MARK, ZERO_MARK, BIT_SPACE, TOLERANCE, true);
        if (c1 > 0) {
            uint32_t c2 =
                decodePulseWidth(items + 13, data_items - 12, ext8, 8, ONE_MARK, ZERO_MARK, BIT_SPACE, TOLERANCE, true);
            if (c2 > 0) {
                addr_data = (addr5 & 0x1F) | ((ext8 & 0xFF) << 5);
                detected  = Variant::SIRC20;
            }
        }
    }

    if (detected != Variant::SIRC20 && data_items >= 15) {
        // Try 15-bit: 7 cmd + 8 addr
        uint32_t addr8 = 0;
        consumed =
            decodePulseWidth(items + 8, data_items - 7, addr8, 8, ONE_MARK, ZERO_MARK, BIT_SPACE, TOLERANCE, true);
        if (consumed > 0) {
            addr_data = addr8 & 0xFF;
            detected  = Variant::SIRC15;
        }
    }

    if (detected != Variant::SIRC20 && detected != Variant::SIRC15 && data_items >= 12) {
        // 12-bit: 7 cmd + 5 addr
        uint32_t addr5 = 0;
        consumed =
            decodePulseWidth(items + 8, data_items - 7, addr5, 5, ONE_MARK, ZERO_MARK, BIT_SPACE, TOLERANCE, true);
        if (consumed > 0) {
            addr_data = addr5 & 0x1F;
            detected  = Variant::SIRC12;
        } else {
            return false;
        }
    }

    // Sync internal variant so a subsequent encode() emits the same-width frame.
    _variant = detected;

    uint16_t cmd7    = static_cast<uint16_t>(cmd_data & 0x7F);
    uint8_t nbits    = static_cast<uint8_t>(detected);
    uint64_t logical = (static_cast<uint64_t>(addr_data) << 7) | cmd7;

    result.protocol = CodecType::SIRC;
    result.address  = addr_data;
    result.command  = cmd7;
    result.bits     = nbits;
    result.repeat   = false;
    result.toggle   = false;
    // Raw is the wire-bit sequence (first-sent bit placed in the highest bit of
    // the result). This matches IRremoteESP8266's `decode_results::value` — the
    // pre-`reverseBits` form held immediately after accumulating LSB-first wire
    // bits via `data = (data << 1) | bit`.
    result.raw = reverse_bit_low(logical, nbits);

    return true;
}

}  // namespace ir
}  // namespace unit
}  // namespace m5
