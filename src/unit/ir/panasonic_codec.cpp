/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file panasonic_codec.cpp
  @brief Panasonic (Kaseikyo) 48-bit IR protocol codec
*/
#include "panasonic_codec.hpp"
#include "ir_rmt_items.hpp"

namespace {

// Reverse bits within a byte (LSB<->MSB)
inline uint8_t reverse_byte(uint8_t b)
{
    b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4);
    b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
    b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);
    return b;
}

}  // namespace

namespace m5 {
namespace unit {
namespace ir {

item_container_type PanasonicCodec::encode(uint16_t address, uint16_t command, bool repeat)
{
    (void)repeat;  // Panasonic has no special repeat code

    // Treat `command` as (device << 8) | function, with subdevice = 0.
    // Checksum is computed from the three data bytes.
    uint8_t device    = static_cast<uint8_t>((command >> 8) & 0xFF);
    uint8_t function  = static_cast<uint8_t>(command & 0xFF);
    uint8_t subdevice = 0;
    return encodeRaw48(encodePanasonic(address, device, subdevice, function));
}

item_container_type PanasonicCodec::encodeRaw48(uint64_t data48)
{
    item_container_type items;
    items.reserve(50);  // leader + 48 bits + stop

    // Leader
    items.push_back(makeItem(LEADER_MARK, LEADER_SPACE));

    // Encode 6 bytes, each byte LSB first, in MSB-byte order
    // data48 layout: byte0(MSB) byte1 byte2 byte3 byte4 byte5(LSB)
    for (int8_t byte_idx = 5; byte_idx >= 0; --byte_idx) {
        uint8_t byte_val  = (data48 >> (byte_idx * 8)) & 0xFF;
        uint32_t reversed = reverse_byte(byte_val);
        encodePulseDistance(items, reversed, 8, BIT_MARK, ONE_SPACE, ZERO_SPACE, true);
    }

    // Stop bit
    items.push_back(makeTerminator(BIT_MARK));

    return items;
}

bool PanasonicCodec::decode(const gpio::m5_rmt_item_t* items, uint32_t num, DecodeResult& result)
{
    if (num < 50) {  // leader + 48 bits + stop
        return false;
    }

    // Check leader
    if (!matchDuration(items[0].duration0, LEADER_MARK, TOLERANCE)) {
        return false;
    }
    if (!matchDuration(items[0].duration1, LEADER_SPACE, TOLERANCE)) {
        return false;
    }

    // Decode 6 bytes, each 8 bits LSB first, assemble in MSB-byte order
    uint64_t data48      = 0;
    uint32_t item_offset = 1;  // Skip leader

    for (uint8_t byte_idx = 0; byte_idx < 6; ++byte_idx) {
        uint32_t byte_raw = 0;
        uint32_t consumed = decodePulseDistance(items + item_offset, num - item_offset, byte_raw, 8, BIT_MARK,
                                                ONE_SPACE, ZERO_SPACE, TOLERANCE, true);
        if (consumed == 0) {
            return false;
        }
        // Reverse bits to get conventional MSB-first-per-byte representation
        uint8_t byte_val = reverse_byte(static_cast<uint8_t>(byte_raw));
        data48           = (data48 << 8) | byte_val;
        item_offset += 8;
    }

    // data48 layout: [man_hi | man_lo | device | subdevice | function | checksum]
    // Round-trip the simple API: command == (device << 8) | function.
    // Full access to subdevice and checksum is via result.raw.
    uint16_t customer = static_cast<uint16_t>((data48 >> 32) & 0xFFFF);
    uint8_t device    = static_cast<uint8_t>((data48 >> 24) & 0xFF);
    uint8_t function  = static_cast<uint8_t>((data48 >> 8) & 0xFF);

    result.protocol = CodecType::Panasonic;
    result.address  = customer;
    result.command  = static_cast<uint16_t>((static_cast<uint16_t>(device) << 8) | function);
    result.raw      = data48;
    result.bits     = 48;
    result.repeat   = false;
    result.toggle   = false;

    return true;
}

}  // namespace ir
}  // namespace unit
}  // namespace m5
