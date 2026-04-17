/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file rc6_codec.hpp
  @brief Philips RC6 Mode 0 IR protocol codec
*/
#ifndef M5_UNIT_INFRARED_IR_RC6_CODEC_HPP
#define M5_UNIT_INFRARED_IR_RC6_CODEC_HPP

#include "ir_codec.hpp"

namespace m5 {
namespace unit {
namespace ir {

/*!
  @class Rc6Codec
  @brief Philips RC6 Mode 0 IR protocol encoder/decoder
  @details
  - Carrier: 36 kHz, duty 1/3
  - Modulation: Manchester (bi-phase), opposite polarity to RC5
  - 1t = 444us (16 cycles of 36kHz)
  - Leader: 6t mark + 2t space
  - Frame: SB(1) + Mode(3) + Trailer(1,2t) + Address(8) + Command(8) = 21 bits
  - RC6 polarity: "0" = space+mark, "1" = mark+space (opposite of RC5)
  - Trailer bit uses 2t half-bit width (double the normal 1t)
 */
class Rc6Codec : public IRCodec {
public:
    //! @brief Constructor
    Rc6Codec() : IRCodec(CodecType::RC6)
    {
    }

    //! @brief Encode an RC6 Mode 0 frame (21 bits: SB+Mode+Trailer+Addr+Cmd)
    item_container_type encode(uint16_t address, uint16_t command, bool repeat) override;
    //! @brief Decode an RC6 Mode 0 Manchester frame (with 2t-wide trailer bit)
    bool decode(const gpio::m5_rmt_item_t* items, uint32_t num, DecodeResult& result) override;

    //! @brief Carrier frequency for RC6 (36 kHz)
    uint32_t carrierFrequencyHz() const override
    {
        return 36000;
    }

    /*!
      @brief Toggle bit state
      @return Current toggle (trailer-bit) value
     */
    bool toggle() const
    {
        return _toggle;
    }
    /*!
      @brief Set toggle bit
      @param t New toggle value; the next encode() emits this bit as the trailer
     */
    void setToggle(bool t)
    {
        _toggle = t;
    }
    //! @brief Flip toggle bit
    void flipToggle()
    {
        _toggle = !_toggle;
    }

private:
    bool _toggle{};

    static constexpr uint16_t T_UNIT{444};        // 1t
    static constexpr uint16_t LEADER_MARK{2666};  // 6t
    static constexpr uint16_t LEADER_SPACE{889};  // 2t
    static constexpr uint16_t HALF_BIT{444};      // 1t for normal bits
    static constexpr uint16_t HALF_BIT_TR{889};   // 2t for trailer bit
    static constexpr uint16_t TOLERANCE{150};
};

}  // namespace ir
}  // namespace unit
}  // namespace m5
#endif
