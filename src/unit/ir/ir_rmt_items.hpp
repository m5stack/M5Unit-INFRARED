/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file ir_rmt_items.hpp
  @brief RMT item helper functions for IR protocols
*/
#ifndef M5_UNIT_INFRARED_IR_RMT_ITEMS_HPP
#define M5_UNIT_INFRARED_IR_RMT_ITEMS_HPP

#include "ir_codec.hpp"

namespace m5 {
namespace unit {
namespace ir {

/*!
  @brief Create an RMT item representing a mark (carrier burst) followed by a space (silence)
  @param mark_us Mark duration in microseconds
  @param space_us Space duration in microseconds
  @return RMT item
 */
inline gpio::m5_rmt_item_t makeItem(uint16_t mark_us, uint16_t space_us)
{
    gpio::m5_rmt_item_t item{};
    item.duration0 = mark_us;
    item.level0    = 1;  // Mark: carrier ON
    item.duration1 = space_us;
    item.level1    = 0;  // Space: carrier OFF
    return item;
}

/*!
  @brief Create a terminator RMT item (mark only, no space)
  @param mark_us Mark duration in microseconds
  @return RMT item with zero-length space
 */
inline gpio::m5_rmt_item_t makeTerminator(uint16_t mark_us)
{
    return makeItem(mark_us, 0);
}

/*!
  @brief Check if a duration matches an expected value within tolerance
  @param actual Measured duration in ticks (microseconds at 1MHz)
  @param expected Expected duration in microseconds
  @param tolerance Acceptable deviation in microseconds
  @return true if within tolerance
 */
inline bool matchDuration(uint16_t actual, uint16_t expected, uint16_t tolerance)
{
    return (actual >= expected - tolerance) && (actual <= expected + tolerance);
}

/*!
  @brief Encode a sequence of bits as pulse-distance modulated RMT items (NEC-style)
  @details Each bit is encoded as: mark(bit_mark) + space(one_space or zero_space)
  @param[out] items Output container (items are appended)
  @param data Raw data value
  @param bits Number of bits to encode
  @param bit_mark Mark duration for each bit in microseconds
  @param one_space Space duration for logic "1" in microseconds
  @param zero_space Space duration for logic "0" in microseconds
  @param lsb_first If true, encode LSB first; otherwise MSB first
 */
void encodePulseDistance(item_container_type& items, uint32_t data, uint8_t bits, uint16_t bit_mark, uint16_t one_space,
                         uint16_t zero_space, bool lsb_first = true);

/*!
  @brief Decode pulse-distance modulated RMT items into a data value
  @param items RMT items to decode
  @param num Number of items
  @param[out] data Decoded data value
  @param bits Expected number of bits
  @param bit_mark Expected mark duration in microseconds
  @param one_space Expected space duration for logic "1" in microseconds
  @param zero_space Expected space duration for logic "0" in microseconds
  @param tolerance Acceptable timing deviation in microseconds
  @param lsb_first If true, decode as LSB first; otherwise MSB first
  @return Number of items consumed, or 0 on failure
 */
uint32_t decodePulseDistance(const gpio::m5_rmt_item_t* items, uint32_t num, uint32_t& data, uint8_t bits,
                             uint16_t bit_mark, uint16_t one_space, uint16_t zero_space, uint16_t tolerance,
                             bool lsb_first = true);

/*!
  @brief Encode a sequence of bits as pulse-width modulated RMT items (SIRC-style)
  @details Each bit is encoded as: mark(one_mark or zero_mark) + space(bit_space)
  @param[out] items Output container (items are appended)
  @param data Raw data value
  @param bits Number of bits to encode
  @param one_mark Mark duration for logic "1" in microseconds
  @param zero_mark Mark duration for logic "0" in microseconds
  @param bit_space Space duration for each bit in microseconds
  @param lsb_first If true, encode LSB first; otherwise MSB first
 */
void encodePulseWidth(item_container_type& items, uint32_t data, uint8_t bits, uint16_t one_mark, uint16_t zero_mark,
                      uint16_t bit_space, bool lsb_first = true);

/*!
  @brief Decode pulse-width modulated RMT items into a data value
  @param items RMT items to decode
  @param num Number of items
  @param[out] data Decoded data value
  @param bits Expected number of bits
  @param one_mark Expected mark duration for logic "1" in microseconds
  @param zero_mark Expected mark duration for logic "0" in microseconds
  @param bit_space Expected space duration in microseconds
  @param tolerance Acceptable timing deviation in microseconds
  @param lsb_first If true, decode as LSB first; otherwise MSB first
  @return Number of items consumed, or 0 on failure
 */
uint32_t decodePulseWidth(const gpio::m5_rmt_item_t* items, uint32_t num, uint32_t& data, uint8_t bits,
                          uint16_t one_mark, uint16_t zero_mark, uint16_t bit_space, uint16_t tolerance,
                          bool lsb_first = true);

/*!
  @brief Encode a sequence of bits as Manchester (bi-phase) modulated RMT items (RC5-style)
  @details Each bit period = 2 half-bits. RC5 convention:
  - Logic "0": Mark(half) + Space(half) (high-to-low transition at midpoint)
  - Logic "1": Space(half) + Mark(half) (low-to-high transition at midpoint)
  @param[out] items Output container (items are appended)
  @param data Raw data value
  @param bits Number of bits to encode
  @param half_bit Half-bit duration in microseconds (889us for RC5)
  @param rc5_polarity If true, use RC5 polarity (0=mark-space, 1=space-mark).
         If false, use RC6 polarity (0=space-mark, 1=mark-space).
  @param msb_first If true, encode MSB first (RC5/RC6 standard)
 */
void encodeManchester(item_container_type& items, uint32_t data, uint8_t bits, uint16_t half_bit,
                      bool rc5_polarity = true, bool msb_first = true);

/*!
  @brief Decode Manchester (bi-phase) modulated RMT items
  @details Accepts both single-transition items (half+half in one item) and
  split items (consecutive half-bit items). Handles edge merging at bit boundaries.
  @param items RMT items to decode
  @param num Number of items
  @param[out] data Decoded data value
  @param bits Expected number of bits
  @param half_bit Expected half-bit duration in microseconds
  @param tolerance Acceptable timing deviation in microseconds
  @param rc5_polarity If true, use RC5 polarity. If false, use RC6 polarity.
  @param msb_first If true, decode MSB first
  @return Number of items consumed, or 0 on failure
 */
uint32_t decodeManchester(const gpio::m5_rmt_item_t* items, uint32_t num, uint32_t& data, uint8_t bits,
                          uint16_t half_bit, uint16_t tolerance, bool rc5_polarity = true, bool msb_first = true);

}  // namespace ir
}  // namespace unit
}  // namespace m5
#endif
