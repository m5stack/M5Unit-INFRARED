/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file mitsubishi_codec.hpp
  @brief Mitsubishi 16-bit IR protocol codec (headerless format)
*/
#ifndef M5_UNIT_INFRARED_IR_MITSUBISHI_CODEC_HPP
#define M5_UNIT_INFRARED_IR_MITSUBISHI_CODEC_HPP

#include "ir_codec.hpp"

namespace m5 {
namespace unit {
namespace ir {

/*!
  @class MitsubishiCodec
  @brief Mitsubishi 16-bit IR protocol encoder/decoder
  @details
  - Carrier: 33 kHz
  - Modulation: Pulse Distance (no leader/header)
  - Bit order: MSB first
  - 16 bits, data sent twice with ~28ms gap
  - No leader pulse (data starts immediately)

  @par Timing
  | Element | Mark (us) | Space (us) |
  |---------|-----------|------------|
  | Bit "1" | 300       | 2100       |
  | Bit "0" | 300       | 900        |
  | Stop    | 300       | -          |
  | Gap     | -         | ~28500     |

  @par API mapping
  - address = 0 (unused)
  - command = full 16-bit data value

  @note encode() emits a single 16-bit frame. Real Mitsubishi remotes always send the
        frame twice per keypress, so minFrames() returns 2 and UnitIR::send() replicates
        the frame automatically (with frameGapUs() ≈ 28.5ms between them).
 */
class MitsubishiCodec : public IRCodec {
public:
    //! @brief Constructor
    MitsubishiCodec() : IRCodec(CodecType::Mitsubishi)
    {
    }

    //! @brief Encode a single 16-bit Mitsubishi frame (UnitIR::send() replicates per minFrames())
    item_container_type encode(uint16_t address, uint16_t command, bool repeat) override;
    //! @brief Decode a single 16-bit Mitsubishi frame (16 data items + trailing stop mark)
    bool decode(const gpio::m5_rmt_item_t* items, uint32_t num, DecodeResult& result) override;

    //! @brief Carrier frequency for Mitsubishi (33 kHz)
    uint32_t carrierFrequencyHz() const override
    {
        return 33000;
    }

    //! @brief Mitsubishi requires the frame to be sent twice per keypress
    uint8_t minFrames() const override
    {
        return 2;
    }

    //! @brief Inter-frame gap ≈ 28.5 ms as used by real Mitsubishi remotes
    uint16_t frameGapUs() const override
    {
        return 28500;
    }

private:
    static constexpr uint16_t BIT_MARK{300};
    static constexpr uint16_t ONE_SPACE{2100};
    static constexpr uint16_t ZERO_SPACE{900};
    static constexpr uint16_t TOLERANCE{250};
};

}  // namespace ir
}  // namespace unit
}  // namespace m5
#endif
