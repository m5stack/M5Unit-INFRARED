/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file rc5_codec.hpp
  @brief Philips RC5 / RC5X IR protocol codec
*/
#ifndef M5_UNIT_INFRARED_IR_RC5_CODEC_HPP
#define M5_UNIT_INFRARED_IR_RC5_CODEC_HPP

#include "ir_codec.hpp"

namespace m5 {
namespace unit {
namespace ir {

/*!
  @class Rc5Codec
  @brief Philips RC5 / RC5X IR protocol encoder/decoder
  @details
  - Carrier: 36 kHz, duty 1/3
  - Modulation: Manchester (bi-phase)
  - Bit order: MSB first
  - RC5:  S1(1) + S2(1) + Toggle(1) + Address(5) + Command(6) = 14 bits
  - RC5X: S1(1) + S2=~Cmd6(1) + Toggle(1) + Address(5) + Command(6) = 14 bits, 128 commands
  - 1 bit = 1778us (64 cycles of 36kHz), half-bit = 889us
  - RC5 polarity: "0" = mark+space, "1" = space+mark
 */
class Rc5Codec : public IRCodec {
public:
    //! @brief Constructor
    Rc5Codec() : IRCodec(CodecType::RC5)
    {
    }

    //! @brief Encode an RC5 or RC5X frame (S2 = ~Cmd[6] when command ≥ 64)
    item_container_type encode(uint16_t address, uint16_t command, bool repeat) override;
    //! @brief Decode an RC5 / RC5X Manchester frame (14 bits including S1/S2/T)
    bool decode(const gpio::m5_rmt_item_t* items, uint32_t num, DecodeResult& result) override;

    //! @brief Carrier frequency for RC5 (36 kHz)
    uint32_t carrierFrequencyHz() const override
    {
        return 36000;
    }

    /*!
      @brief Toggle bit state (flipped on each new key press)
      @return Current toggle bit (false = 0, true = 1)
     */
    bool toggle() const
    {
        return _toggle;
    }
    /*!
      @brief Set toggle bit
      @param t New toggle value; the next encode() emits this bit verbatim
     */
    void setToggle(bool t)
    {
        _toggle = t;
    }
    //! @brief Flip toggle bit (call on each new key press)
    void flipToggle()
    {
        _toggle = !_toggle;
    }

private:
    bool _toggle{};

    static constexpr uint16_t HALF_BIT{889};
    static constexpr uint16_t FULL_BIT{1778};
    static constexpr uint16_t TOLERANCE{250};
};

}  // namespace ir
}  // namespace unit
}  // namespace m5
#endif
