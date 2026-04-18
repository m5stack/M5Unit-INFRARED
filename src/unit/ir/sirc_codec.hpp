/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file sirc_codec.hpp
  @brief Sony SIRC IR protocol codec (12/15/20-bit)
*/
#ifndef M5_UNIT_INFRARED_IR_SIRC_CODEC_HPP
#define M5_UNIT_INFRARED_IR_SIRC_CODEC_HPP

#include "ir_codec.hpp"

namespace m5 {
namespace unit {
namespace ir {

/*!
  @class SircCodec
  @brief Sony SIRC IR protocol encoder/decoder
  @details
  - Carrier: 40 kHz, duty 1/3
  - Modulation: Pulse Width (space constant, mark varies)
  - Bit order: LSB first
  - 12-bit: 7-bit command + 5-bit address
  - 15-bit: 7-bit command + 8-bit address
  - 20-bit: 7-bit command + 5-bit address + 8-bit extended

  @par Timing
  | Element  | Mark (us) | Space (us) |
  |----------|-----------|------------|
  | Start    | 2400      | 600        |
  | Bit "1"  | 1200      | 600        |
  | Bit "0"  | 600       | 600        |
 */
class SircCodec : public IRCodec {
public:
    //! @brief SIRC protocol variants
    enum class Variant : uint8_t {
        SIRC12 = 12,
        SIRC15 = 15,
        SIRC20 = 20,
    };

    /*!
      @brief Constructor
      @param v Initial SIRC variant (default SIRC12)
     */
    explicit SircCodec(Variant v = Variant::SIRC12) : IRCodec(CodecType::SIRC), _variant(v)
    {
    }

    //! @brief Encode a Sony SIRC frame for the currently selected variant
    item_container_type encode(uint16_t address, uint16_t command, bool repeat) override;
    //! @brief Decode a Sony SIRC frame and, on success, sync the active variant
    bool decode(const gpio::m5_rmt_item_t* items, uint32_t num, DecodeResult& result) override;

    //! @brief Carrier frequency for Sony SIRC (40 kHz)
    uint32_t carrierFrequencyHz() const override
    {
        return 40000;
    }

    //! @brief Sony SIRC requires the frame to be sent at least 3 times per keypress
    uint8_t minFrames() const override
    {
        return 3;
    }

    //! @brief Inter-frame gap ≈ 30 ms (keeps the 45 ms start-to-start cycle)
    uint16_t frameGapUs() const override
    {
        return 30000;
    }

    /*!
      @brief Get current variant
      @return Variant currently used for encode()
     */
    Variant variant() const
    {
        return _variant;
    }
    /*!
      @brief Set variant (12 / 15 / 20-bit)
      @param v New variant to apply on subsequent encode() calls
     */
    void setVariant(Variant v)
    {
        _variant = v;
    }

private:
    Variant _variant;

    static constexpr uint16_t START_MARK{2400};
    static constexpr uint16_t START_SPACE{600};
    static constexpr uint16_t ONE_MARK{1200};
    static constexpr uint16_t ZERO_MARK{600};
    static constexpr uint16_t BIT_SPACE{600};
    static constexpr uint16_t TOLERANCE{200};
};

}  // namespace ir
}  // namespace unit
}  // namespace m5
#endif
