/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file rc5_codec.cpp
  @brief Philips RC5 / RC5X IR protocol codec
*/
#include "rc5_codec.hpp"
#include "ir_rmt_items.hpp"

namespace m5 {
namespace unit {
namespace ir {

item_container_type Rc5Codec::encode(uint16_t address, uint16_t command, bool repeat)
{
    (void)repeat;  // RC5 repeats by re-sending with same toggle

    // Build 14-bit RC5 frame: S1(1) + S2(1) + T(1) + Addr(5) + Cmd(6)
    // RC5X: S2 = inverted 7th command bit
    uint8_t cmd6  = command & 0x3F;
    bool cmd_bit6 = (command >> 6) & 1;
    bool s2       = !cmd_bit6;  // RC5X: S2 = ~Cmd[6]; standard RC5: S2 = 1 (cmd < 64)

    uint32_t frame = 0;
    // MSB first: S1, S2, T, A4..A0, C5..C0
    frame |= (1U << 13);                                    // S1 = 1
    frame |= (s2 ? 1U : 0U) << 12;                          // S2
    frame |= (_toggle ? 1U : 0U) << 11;                     // Toggle
    frame |= (static_cast<uint32_t>(address & 0x1F)) << 6;  // Address (5 bits)
    frame |= cmd6;                                          // Command (6 bits)

    item_container_type items;
    items.reserve(28);  // Worst case: 14 bits x 2 half-bits

    encodeManchester(items, frame, 14, HALF_BIT, true, true);

    return items;
}

bool Rc5Codec::decode(const gpio::m5_rmt_item_t* items, uint32_t num, DecodeResult& result)
{
    if (num < 4) {
        return false;
    }

    // RC5 has no leader pulse. S1=1 starts with space+mark in Manchester.
    // The IR receiver's idle state is HIGH (no IR), so the leading space of S1
    // is indistinguishable from idle. The RMT capture starts at S1's mark.
    //
    // Strategy: Flatten all items into a duration stream, skip the first
    // mark (S1's second half), then decode 13 bits (S2..C5) from the
    // properly aligned stream. S1=1 is always true.

    // Flatten all durations into array with levels
    uint16_t flat[128];
    uint8_t flat_level[128];
    uint32_t flat_count = 0;

    for (uint32_t i = 0; i < num && flat_count < 126; ++i) {
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

    // Consume S1 (always "1" → space-then-mark). Two possible starting states:
    //   - Round-trip from encode(): flat[0] is the S1.space half, flat[1] is S1.mark
    //     (possibly merged with S2.mark into FULL_BIT when S2=0).
    //   - RX hardware: leading idle/space is stripped, so flat[0] is S1.mark
    //     (possibly merged into FULL_BIT with S2.mark).
    // After this block, any residual HALF_BIT in `remaining` is S2's first half.
    uint32_t fi;
    int32_t remaining;
    bool is_mark;

    if (flat_level[0] == 0) {
        // Leading space (S1 first half) must be exactly HALF_BIT
        if (!matchDuration(flat[0], HALF_BIT, TOLERANCE)) {
            return false;
        }
        fi = 1;
        if (fi >= flat_count) {
            return false;
        }
        remaining = flat[fi];
        is_mark   = (flat_level[fi] != 0);
        if (!is_mark) {
            return false;  // expected S1.mark after leading space
        }
    } else {
        // No leading space: flat[0] is S1.mark
        fi        = 0;
        remaining = flat[0];
        is_mark   = true;
    }

    // Consume S1's mark half-bit. Leaves HALF_BIT in `remaining` when S2.mark merged in.
    if (matchDuration(remaining, HALF_BIT, TOLERANCE)) {
        remaining = 0;
    } else if (matchDuration(remaining, FULL_BIT, TOLERANCE)) {
        remaining -= HALF_BIT;
    } else {
        return false;
    }

    uint8_t bit_count = 0;
    uint32_t frame    = 0;

    auto advance = [&]() -> bool {
        ++fi;
        if (fi >= flat_count) {
            return false;
        }
        remaining = flat[fi];
        is_mark   = (flat_level[fi] != 0);
        return true;
    };

    // Decode 13 bits (S2, T, Addr[5], Cmd[6])
    while (bit_count < 13) {
        // First half of bit
        if (remaining <= 0) {
            if (!advance()) {
                break;
            }
        }

        bool first_half_mark = is_mark;

        if (matchDuration(remaining, HALF_BIT, TOLERANCE)) {
            remaining = 0;
        } else if (matchDuration(remaining, FULL_BIT, TOLERANCE)) {
            remaining -= HALF_BIT;
        } else {
            break;
        }

        // Second half of bit
        if (remaining <= 0) {
            if (!advance()) {
                // Last bit may end without trailing duration
                if (bit_count == 12) {
                    bool bit_val = !first_half_mark;  // RC5: mark-first = 0
                    frame        = (frame << 1) | (bit_val ? 1U : 0U);
                    ++bit_count;
                    break;
                }
                break;
            }
        }

        if (matchDuration(remaining, HALF_BIT, TOLERANCE)) {
            remaining = 0;
        } else if (matchDuration(remaining, FULL_BIT, TOLERANCE)) {
            remaining -= HALF_BIT;
        } else {
            break;
        }

        // RC5: mark-first = "0", space-first = "1"
        bool bit_val = !first_half_mark;
        frame        = (frame << 1) | (bit_val ? 1U : 0U);
        ++bit_count;
    }

    if (bit_count != 13) {
        return false;
    }

    // Reconstruct full 14-bit frame: S1(1) + decoded 13 bits
    uint32_t full_frame = (1U << 13) | frame;

    // Extract fields
    bool s1      = (full_frame >> 13) & 1;  // Always 1
    bool s2      = (full_frame >> 12) & 1;
    bool tgl     = (full_frame >> 11) & 1;
    uint8_t addr = (full_frame >> 6) & 0x1F;
    uint8_t cmd6 = full_frame & 0x3F;

    (void)s1;

    // RC5X: 7th command bit = ~S2
    uint8_t cmd = cmd6 | (s2 ? 0 : 0x40);

    result.protocol = CodecType::RC5;
    result.address  = addr;
    result.command  = cmd;
    result.raw      = full_frame;
    result.bits     = 14;
    result.repeat   = false;
    result.toggle   = tgl;

    return true;
}

}  // namespace ir
}  // namespace unit
}  // namespace m5
