/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file rc6_codec.cpp
  @brief Philips RC6 Mode 0 IR protocol codec
*/
#include "rc6_codec.hpp"
#include "ir_rmt_items.hpp"

namespace m5 {
namespace unit {
namespace ir {

item_container_type Rc6Codec::encode(uint16_t address, uint16_t command, bool repeat)
{
    (void)repeat;

    item_container_type items;
    items.reserve(40);

    // Leader (non-Manchester): 6t mark + 2t space
    items.push_back(makeItem(LEADER_MARK, LEADER_SPACE));

    // Build the entire 21-bit Manchester frame as a single raw sequence so the
    // odd-trailing terminator from each section does not appear mid-frame (which
    // would cut RMT TX short). Using separate encodeManchester() calls per
    // section caused this: Addr / Cmd values with an odd merged-segment count
    // injected (dur1=0) terminator items into the middle of the transmission.
    //
    // Frame layout (MSB first): SB(1) + Mode(3) + Trailer(1) + Addr(8) + Cmd(8)
    // Trailer bit (index 4) uses 2t-wide half-bits; all other bits use 1t.
    uint32_t frame = (1U << 20) |                                        // SB = 1
                     (0U << 17) |                                        // Mode = 000
                     (static_cast<uint32_t>(_toggle ? 1U : 0U) << 16) |  // Trailer
                     (static_cast<uint32_t>(address & 0xFF) << 8) |      // Address
                     static_cast<uint32_t>(command & 0xFF);              // Command

    struct Seg {
        uint8_t lvl;
        uint16_t dur;
    };
    Seg raw[64];
    uint32_t raw_count = 0;
    for (uint8_t i = 0; i < 21 && raw_count + 1 < 64; ++i) {
        uint16_t hb  = (i == 4) ? HALF_BIT_TR : HALF_BIT;
        bool bit_val = (frame >> (20 - i)) & 1;
        // RC6 polarity: bit=1 -> mark+space, bit=0 -> space+mark
        bool first_mark  = bit_val;
        raw[raw_count++] = {static_cast<uint8_t>(first_mark ? 1 : 0), hb};
        raw[raw_count++] = {static_cast<uint8_t>(first_mark ? 0 : 1), hb};
    }

    // Merge adjacent same-level segments (RC6 produces 1t+2t and 2t+1t merges
    // around the Trailer, plus standard 1t+1t merges elsewhere).
    Seg merged[64];
    uint32_t merged_count = 1;
    merged[0]             = raw[0];
    for (uint32_t i = 1; i < raw_count; ++i) {
        if (raw[i].lvl == merged[merged_count - 1].lvl) {
            merged[merged_count - 1].dur += raw[i].dur;
        } else {
            merged[merged_count++] = raw[i];
        }
    }

    // Pack 2 segments per RMT item; odd trailing becomes the end-of-frame terminator.
    for (uint32_t i = 0; i + 1 < merged_count; i += 2) {
        gpio::m5_rmt_item_t item{};
        item.duration0 = merged[i].dur;
        item.level0    = merged[i].lvl;
        item.duration1 = merged[i + 1].dur;
        item.level1    = merged[i + 1].lvl;
        items.push_back(item);
    }
    if (merged_count & 1) {
        gpio::m5_rmt_item_t item{};
        item.duration0 = merged[merged_count - 1].dur;
        item.level0    = merged[merged_count - 1].lvl;
        item.duration1 = 0;
        item.level1    = 0;
        items.push_back(item);
    }

    return items;
}

bool Rc6Codec::decode(const gpio::m5_rmt_item_t* items, uint32_t num, DecodeResult& result)
{
    if (num < 6) {
        return false;
    }

    // Check leader: 6t mark + 2t space
    if (!matchDuration(items[0].duration0, LEADER_MARK, TOLERANCE * 3)) {
        return false;
    }
    if (!matchDuration(items[0].duration1, LEADER_SPACE, TOLERANCE * 2)) {
        return false;
    }

    // Flatten all durations after leader into a simple array
    // Each RMT item contributes 2 durations (duration0 + duration1)
    // Max: (num-1) * 2 durations
    uint16_t flat[128];
    uint8_t flat_level[128];  // 1=mark, 0=space
    uint32_t flat_count = 0;

    for (uint32_t i = 1; i < num && flat_count < 126; ++i) {
        flat[flat_count]       = items[i].duration0;
        flat_level[flat_count] = items[i].level0;
        ++flat_count;
        if (items[i].duration1 > 0) {
            flat[flat_count]       = items[i].duration1;
            flat_level[flat_count] = items[i].level1;
            ++flat_count;
        }
    }

    if (flat_count < 4) {
        return false;
    }

    // Parse Manchester from flat duration array
    // RC6 Mode 0: SB(1) + Mode(3) + Trailer(1,2t) + Address(8) + Command(8) = 21 bits
    // Bit order: MSB first
    // RC6 polarity: "1" = mark first, "0" = space first

    uint32_t fi        = 0;  // flat index
    int32_t remaining  = flat[fi];
    bool is_mark       = (flat_level[fi] != 0);
    uint8_t bit_count  = 0;
    uint32_t frame     = 0;
    uint8_t total_bits = 21;

    auto advance = [&]() -> bool {
        ++fi;
        if (fi >= flat_count) {
            return false;
        }
        remaining = flat[fi];
        is_mark   = (flat_level[fi] != 0);
        return true;
    };

    while (bit_count < total_bits) {
        // Determine half-bit width: trailer bit (index 4) uses 2t, others use 1t
        uint16_t hb  = (bit_count == 4) ? HALF_BIT_TR : HALF_BIT;
        uint16_t tol = (bit_count == 4) ? TOLERANCE * 2 : TOLERANCE;

        // Width of the next bit's first half, used to detect boundary merges
        // (Mode[0]<->Trailer and Trailer<->Addr[7] can produce 1t+2t = 3t spans).
        uint16_t next_hb  = (bit_count + 1 == 4) ? HALF_BIT_TR : HALF_BIT;
        uint16_t next_tol = (bit_count + 1 == 4) ? TOLERANCE * 2 : TOLERANCE;

        // First half of bit
        if (remaining <= 0) {
            if (!advance()) {
                break;
            }
        }

        bool first_half_mark = is_mark;

        if (matchDuration(remaining, hb, tol)) {
            remaining = 0;
        } else if (matchDuration(remaining, hb * 2, tol)) {
            remaining -= hb;
        } else {
            break;
        }

        // Second half of bit (opposite level). Same-level carry-over into the
        // NEXT bit's first half is allowed, so accept `hb + next_hb` durations
        // (covers normal 1t+1t and Trailer-boundary 1t+2t / 2t+1t merges).
        if (remaining <= 0) {
            if (!advance()) {
                // Last bit may end without trailing duration
                if (bit_count == total_bits - 1) {
                    bool bit_val = first_half_mark;  // RC6: mark-first = 1
                    frame        = (frame << 1) | (bit_val ? 1U : 0U);
                    ++bit_count;
                    break;
                }
                break;
            }
        }

        if (matchDuration(remaining, hb, tol)) {
            remaining = 0;
        } else if (matchDuration(remaining, hb + next_hb, tol + next_tol)) {
            remaining -= hb;
        } else {
            break;
        }

        // RC6: mark-first = "1", space-first = "0"
        bool bit_val = first_half_mark;
        frame        = (frame << 1) | (bit_val ? 1U : 0U);
        ++bit_count;
    }

    if (bit_count != total_bits) {
        return false;
    }

    // Extract fields from frame (MSB first, 21 bits):
    // SB(1) + Mode(3) + Trailer(1) + Address(8) + Command(8)
    uint8_t sb   = (frame >> 20) & 0x01;
    uint8_t mode = (frame >> 17) & 0x07;
    bool tgl     = ((frame >> 16) & 0x01) != 0;
    uint8_t addr = (frame >> 8) & 0xFF;
    uint8_t cmd  = frame & 0xFF;

    if (sb != 1 || mode != 0) {
        return false;
    }

    result.protocol = CodecType::RC6;
    result.address  = addr;
    result.command  = cmd;
    result.bits     = 21;
    result.repeat   = false;
    result.toggle   = tgl;
    result.raw      = frame;

    return true;
}

}  // namespace ir
}  // namespace unit
}  // namespace m5
