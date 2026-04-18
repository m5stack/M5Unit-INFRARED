/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file ir_rmt_items.cpp
  @brief RMT item helper functions for IR protocols
*/
#include "ir_rmt_items.hpp"

namespace m5 {
namespace unit {
namespace ir {

void encodePulseDistance(item_container_type& items, uint32_t data, uint8_t bits, uint16_t bit_mark, uint16_t one_space,
                         uint16_t zero_space, bool lsb_first)
{
    for (uint8_t i = 0; i < bits; ++i) {
        bool bit;
        if (lsb_first) {
            bit = (data >> i) & 1;
        } else {
            bit = (data >> (bits - 1 - i)) & 1;
        }
        items.push_back(makeItem(bit_mark, bit ? one_space : zero_space));
    }
}

uint32_t decodePulseDistance(const gpio::m5_rmt_item_t* items, uint32_t num, uint32_t& data, uint8_t bits,
                             uint16_t bit_mark, uint16_t one_space, uint16_t zero_space, uint16_t tolerance,
                             bool lsb_first)
{
    if (num < bits) {
        return 0;
    }

    uint32_t result = 0;
    for (uint8_t i = 0; i < bits; ++i) {
        if (!matchDuration(items[i].duration0, bit_mark, tolerance)) {
            return 0;
        }
        bool is_one  = matchDuration(items[i].duration1, one_space, tolerance);
        bool is_zero = matchDuration(items[i].duration1, zero_space, tolerance);
        if (!is_one && !is_zero) {
            return 0;
        }
        if (is_one) {
            if (lsb_first) {
                result |= (1U << i);
            } else {
                result |= (1U << (bits - 1 - i));
            }
        }
    }
    data = result;
    return bits;
}

void encodePulseWidth(item_container_type& items, uint32_t data, uint8_t bits, uint16_t one_mark, uint16_t zero_mark,
                      uint16_t bit_space, bool lsb_first)
{
    for (uint8_t i = 0; i < bits; ++i) {
        bool bit;
        if (lsb_first) {
            bit = (data >> i) & 1;
        } else {
            bit = (data >> (bits - 1 - i)) & 1;
        }
        items.push_back(makeItem(bit ? one_mark : zero_mark, bit_space));
    }
}

uint32_t decodePulseWidth(const gpio::m5_rmt_item_t* items, uint32_t num, uint32_t& data, uint8_t bits,
                          uint16_t one_mark, uint16_t zero_mark, uint16_t bit_space, uint16_t tolerance, bool lsb_first)
{
    if (num < bits) {
        return 0;
    }

    uint32_t result = 0;
    for (uint8_t i = 0; i < bits; ++i) {
        if (!matchDuration(items[i].duration1, bit_space, tolerance)) {
            // Last item may have duration1 == 0 (no trailing space)
            if (i != bits - 1 || items[i].duration1 != 0) {
                return 0;
            }
        }
        bool is_one  = matchDuration(items[i].duration0, one_mark, tolerance);
        bool is_zero = matchDuration(items[i].duration0, zero_mark, tolerance);
        if (!is_one && !is_zero) {
            return 0;
        }
        if (is_one) {
            if (lsb_first) {
                result |= (1U << i);
            } else {
                result |= (1U << (bits - 1 - i));
            }
        }
    }
    data = result;
    return bits;
}

void encodeManchester(item_container_type& items, uint32_t data, uint8_t bits, uint16_t half_bit, bool rc5_polarity,
                      bool msb_first)
{
    // Step 1: Build raw segment sequence (alternating levels)
    struct Segment {
        uint16_t dur;
        uint8_t lvl;  // 1=mark, 0=space
    };
    // Max segments: bits * 2 (each bit = 2 half-bits)
    Segment raw[64];
    uint32_t raw_count = 0;

    for (uint8_t i = 0; i < bits && raw_count + 1 < 64; ++i) {
        bool bit_val;
        if (msb_first) {
            bit_val = (data >> (bits - 1 - i)) & 1;
        } else {
            bit_val = (data >> i) & 1;
        }

        // RC5: 0 = mark+space, 1 = space+mark
        // RC6: 0 = space+mark, 1 = mark+space
        bool first_half_mark = rc5_polarity ? !bit_val : bit_val;

        raw[raw_count++] = {half_bit, static_cast<uint8_t>(first_half_mark ? 1 : 0)};
        raw[raw_count++] = {half_bit, static_cast<uint8_t>(first_half_mark ? 0 : 1)};
    }

    if (raw_count == 0) {
        return;
    }

    // Step 2: Merge adjacent same-level segments
    Segment merged[64];
    uint32_t merged_count = 1;
    merged[0]             = raw[0];

    for (uint32_t i = 1; i < raw_count; ++i) {
        if (raw[i].lvl == merged[merged_count - 1].lvl) {
            merged[merged_count - 1].dur += raw[i].dur;
        } else {
            merged[merged_count++] = raw[i];
        }
    }

    // Step 3: Pack into RMT items (2 segments per item, preserving actual levels)
    for (uint32_t i = 0; i + 1 < merged_count; i += 2) {
        gpio::m5_rmt_item_t item{};
        item.duration0 = merged[i].dur;
        item.level0    = merged[i].lvl;
        item.duration1 = merged[i + 1].dur;
        item.level1    = merged[i + 1].lvl;
        items.push_back(item);
    }
    // Odd trailing segment
    if (merged_count & 1) {
        gpio::m5_rmt_item_t item{};
        item.duration0 = merged[merged_count - 1].dur;
        item.level0    = merged[merged_count - 1].lvl;
        item.duration1 = 0;
        item.level1    = 0;
        items.push_back(item);
    }
}

uint32_t decodeManchester(const gpio::m5_rmt_item_t* items, uint32_t num, uint32_t& data, uint8_t bits,
                          uint16_t half_bit, uint16_t tolerance, bool rc5_polarity, bool msb_first)
{
    if (num == 0) {
        return 0;
    }

    uint16_t full_bit = half_bit * 2;

    // Flatten RMT items into a sequence of (duration, level) half-bits
    // Then decode Manchester transitions
    uint32_t result   = 0;
    uint8_t bit_count = 0;
    uint32_t item_idx = 0;
    bool in_duration1 = false;  // false = processing duration0, true = duration1

    auto get_duration = [&]() -> int32_t {
        if (item_idx >= num) {
            return -1;
        }
        uint16_t dur;
        uint8_t level;
        if (!in_duration1) {
            dur   = items[item_idx].duration0;
            level = items[item_idx].level0;
            (void)level;
            in_duration1 = true;
        } else {
            dur          = items[item_idx].duration1;
            in_duration1 = false;
            ++item_idx;
        }
        return dur;
    };

    // We decode by looking at transitions.
    // Each Manchester bit has a guaranteed transition at the midpoint.
    // A half-bit duration means the signal continues; a full-bit means it transitions at the boundary too.
    int32_t remaining = get_duration();
    if (remaining < 0) {
        return 0;
    }

    bool current_level = (items[0].level0 != 0);

    while (bit_count < bits) {
        // First half of bit
        if (remaining <= 0) {
            remaining = get_duration();
            if (remaining < 0) {
                break;
            }
            current_level = !current_level;
        }

        bool first_half_high = current_level;

        if (matchDuration(remaining, half_bit, tolerance)) {
            remaining = 0;
        } else if (matchDuration(remaining, full_bit, tolerance)) {
            remaining -= half_bit;
        } else {
            break;  // Timing mismatch
        }

        // Second half of bit (must be opposite level)
        if (remaining <= 0) {
            remaining = get_duration();
            if (remaining < 0) {
                // Last bit may end without trailing duration
                if (bit_count == bits - 1) {
                    bool bit_val;
                    if (rc5_polarity) {
                        bit_val = !first_half_high;  // RC5: mark-first = 0
                    } else {
                        bit_val = first_half_high;  // RC6: mark-first = 1
                    }
                    if (msb_first) {
                        result |= (bit_val ? 1U : 0U) << (bits - 1 - bit_count);
                    } else {
                        result |= (bit_val ? 1U : 0U) << bit_count;
                    }
                    ++bit_count;
                    break;
                }
                break;
            }
            current_level = !current_level;
        }

        if (matchDuration(remaining, half_bit, tolerance)) {
            remaining = 0;
        } else if (matchDuration(remaining, full_bit, tolerance)) {
            remaining -= half_bit;
        } else {
            break;
        }

        // Determine bit value from first half level
        bool bit_val;
        if (rc5_polarity) {
            bit_val = !first_half_high;  // RC5: mark(high)-first = 0, space(low)-first = 1
        } else {
            bit_val = first_half_high;  // RC6: mark(high)-first = 1, space(low)-first = 0
        }

        if (msb_first) {
            result |= (bit_val ? 1U : 0U) << (bits - 1 - bit_count);
        } else {
            result |= (bit_val ? 1U : 0U) << bit_count;
        }
        ++bit_count;
    }

    if (bit_count != bits) {
        return 0;
    }
    data = result;
    return item_idx;
}

}  // namespace ir
}  // namespace unit
}  // namespace m5
