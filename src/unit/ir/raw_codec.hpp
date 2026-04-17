/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file raw_codec.hpp
  @brief Raw mark/space IR codec (no protocol)
*/
#ifndef M5_UNIT_INFRARED_IR_RAW_CODEC_HPP
#define M5_UNIT_INFRARED_IR_RAW_CODEC_HPP

#include "ir_codec.hpp"
#include "ir_rmt_items.hpp"

namespace m5 {
namespace unit {
namespace ir {

/*!
  @class RawCodec
  @brief Raw mark/space pass-through codec
  @details Encodes/decodes raw mark/space timing pairs without protocol interpretation.
  Use for unknown protocols or direct RMT item manipulation.
 */
class RawCodec : public IRCodec {
public:
    /*!
      @brief Constructor
      @param carrier_hz Carrier frequency in Hz for transmission (default 38 kHz)
     */
    explicit RawCodec(uint32_t carrier_hz = 38000) : IRCodec(CodecType::Raw), _carrier_hz(carrier_hz)
    {
    }

    //! @brief encode() is not meaningful for Raw; returns empty
    item_container_type encode(uint16_t, uint16_t, bool) override
    {
        return {};
    }

    /*!
      @brief decode() stores raw item count in `bits` and tags protocol as Raw
      @param items RMT items from receiver
      @param num Number of items
      @param[out] result Populated with `CodecType::Raw` and `bits` = min(num, 255) on success
      @return True when `items` is non-null and `num > 0`; false otherwise
     */
    bool decode(const gpio::m5_rmt_item_t* items, uint32_t num, DecodeResult& result) override
    {
        if (!items || num == 0) {
            return false;
        }
        result.protocol = CodecType::Raw;
        result.address  = 0;
        result.command  = 0;
        result.bits     = static_cast<uint8_t>(num > 255 ? 255 : num);
        result.repeat   = false;
        result.toggle   = false;
        return true;
    }

    /*!
      @brief Carrier frequency configured at construction
      @return Frequency in Hz
     */
    uint32_t carrierFrequencyHz() const override
    {
        return _carrier_hz;
    }

    /*!
      @brief Encode raw mark/space pairs into RMT items
      @param mark_space_us Array of alternating mark/space durations (microseconds)
      @param count Number of entries. Pairs (count/2) become full (mark, space) items;
             an odd trailing entry is emitted as a terminator (mark, 0).
      @return Packed RMT items
     */
    item_container_type encodeRaw(const uint16_t* mark_space_us, uint32_t count)
    {
        item_container_type items;
        items.reserve(count / 2 + 1);
        for (uint32_t i = 0; i + 1 < count; i += 2) {
            items.push_back(makeItem(mark_space_us[i], mark_space_us[i + 1]));
        }
        if (count & 1) {
            items.push_back(makeTerminator(mark_space_us[count - 1]));
        }
        return items;
    }

private:
    uint32_t _carrier_hz;
};

}  // namespace ir
}  // namespace unit
}  // namespace m5
#endif
