/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file nec_codec.hpp
  @brief NEC / Extended NEC IR protocol codec
*/
#ifndef M5_UNIT_INFRARED_IR_NEC_CODEC_HPP
#define M5_UNIT_INFRARED_IR_NEC_CODEC_HPP

#include "ir_codec.hpp"

namespace m5 {
namespace unit {
namespace ir {

/*!
  @class NecCodec
  @brief NEC / Extended NEC IR protocol encoder/decoder
  @details
  - Carrier: 38 kHz, duty 1/3
  - Modulation: Pulse Distance (mark duration constant, space varies)
  - Bit order: LSB first
  - Standard NEC: 8-bit address + inverted + 8-bit command + inverted = 32 bits
  - Extended NEC: 16-bit address (no inversion) + 8-bit command + inverted = 32 bits
  - Repeat code: 9ms mark + 2.25ms space + 560us mark

  @par Timing
  | Element | Mark (us) | Space (us) |
  |---------|-----------|------------|
  | Leader  | 9000      | 4500       |
  | Bit "1" | 560       | 1690       |
  | Bit "0" | 560       | 560        |
  | Repeat  | 9000      | 2250       |
  | Stop    | 560       | -          |
 */
class NecCodec : public IRCodec {
public:
    //! @brief Constructor
    NecCodec() : IRCodec(CodecType::NEC)
    {
    }

    //! @brief Encode a standard or extended NEC frame, or the 2-item repeat frame when `repeat` is true
    item_container_type encode(uint16_t address, uint16_t command, bool repeat) override;
    //! @brief Decode a NEC full frame or repeat frame
    bool decode(const gpio::m5_rmt_item_t* items, uint32_t num, DecodeResult& result) override;

    //! @brief Carrier frequency for NEC (38 kHz)
    uint32_t carrierFrequencyHz() const override
    {
        return 38000;
    }

    /*!
      @brief Encode with explicit 32-bit raw data
      @param data 32-bit NEC frame (address + ~address + command + ~command)
      @return RMT items
     */
    item_container_type encodeRaw(uint32_t data);

    /*!
      @brief Encode NEC repeat code
      @return RMT items for repeat frame
     */
    item_container_type encodeRepeat();

private:
    // Timing constants in microseconds (RMT tick = 1 us)
    static constexpr uint16_t LEADER_MARK{9000};
    static constexpr uint16_t LEADER_SPACE{4500};
    static constexpr uint16_t BIT_MARK{560};
    static constexpr uint16_t ONE_SPACE{1690};
    static constexpr uint16_t ZERO_SPACE{560};
    static constexpr uint16_t REPEAT_SPACE{2250};
    static constexpr uint16_t TOLERANCE{200};
};

}  // namespace ir
}  // namespace unit
}  // namespace m5
#endif
