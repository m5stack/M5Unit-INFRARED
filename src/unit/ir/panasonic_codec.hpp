/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file panasonic_codec.hpp
  @brief Panasonic (Kaseikyo) 48-bit IR protocol codec
*/
#ifndef M5_UNIT_INFRARED_IR_PANASONIC_CODEC_HPP
#define M5_UNIT_INFRARED_IR_PANASONIC_CODEC_HPP

#include "ir_codec.hpp"

namespace m5 {
namespace unit {
namespace ir {

/*!
  @class PanasonicCodec
  @brief Panasonic (Kaseikyo) 48-bit IR protocol encoder/decoder
  @details
  - Carrier: 36.7 kHz, duty 1/3
  - Modulation: Pulse Distance (same as NEC)
  - Bit order: LSB first
  - 48 bits: Customer Code (16) + Data (32)
  - Customer code for Panasonic TV: 0x4004
  - Data: device(8) + subdevice(8) + function(8) + checksum(8)

  @par Timing
  | Element | Mark (us) | Space (us) |
  |---------|-----------|------------|
  | Leader  | 3500      | 1750       |
  | Bit "1" | 425       | 1275       |
  | Bit "0" | 425       | 425        |
  | Stop    | 425       | -          |

  @par API mapping (simple `encode(address, command, repeat)`)
  - address   = manufacturer / customer code (e.g. 0x4004 for Panasonic TV)
  - command   = `(device << 8) | function`, subdevice is fixed to 0
  - checksum  = `device ^ subdevice ^ function` (auto-computed)
  - result.raw (decode) holds the full 48-bit frame for post-processing.

  For full control (non-zero subdevice, custom manufacturer codes), use the
  static helper `encodePanasonic(manufacturer, device, subdevice, function)`
  and feed the returned 48-bit value to `encodeRaw48()`.
 */
class PanasonicCodec : public IRCodec {
public:
    //! @brief Constructor
    PanasonicCodec() : IRCodec(CodecType::Panasonic)
    {
    }

    //! @brief Encode a 48-bit Panasonic frame from (manufacturer=address, device+function=command)
    item_container_type encode(uint16_t address, uint16_t command, bool repeat) override;
    //! @brief Decode a 48-bit Panasonic frame; `result.raw` receives the full 48-bit value
    bool decode(const gpio::m5_rmt_item_t* items, uint32_t num, DecodeResult& result) override;

    //! @brief Carrier frequency for Panasonic (36.7 kHz)
    uint32_t carrierFrequencyHz() const override
    {
        return 36700;
    }

    /*!
      @brief Encode with explicit 48-bit raw data
      @param data48 Full 48-bit Panasonic frame (customer + data)
      @return RMT items
     */
    item_container_type encodeRaw48(uint64_t data48);

    /*!
      @brief Build a 48-bit Panasonic / Kaseikyo frame from structured fields
      @param manufacturer 16-bit manufacturer / customer code (e.g. 0x4004 for Panasonic TV)
      @param device       8-bit device identifier
      @param subdevice    8-bit sub-device
      @param function     8-bit function
      @return 48-bit raw frame with correct checksum (`device ^ subdevice ^ function`)
      @note Pair with `encodeRaw48()` to transmit the result, or decode -> compare
            against this to verify a captured frame at application level.
     */
    static uint64_t encodePanasonic(uint16_t manufacturer, uint8_t device, uint8_t subdevice, uint8_t function)
    {
        uint8_t checksum = static_cast<uint8_t>(device ^ subdevice ^ function);
        return (static_cast<uint64_t>(manufacturer) << 32) | (static_cast<uint64_t>(device) << 24) |
               (static_cast<uint64_t>(subdevice) << 16) | (static_cast<uint64_t>(function) << 8) |
               static_cast<uint64_t>(checksum);
    }

private:
    static constexpr uint16_t LEADER_MARK{3500};
    static constexpr uint16_t LEADER_SPACE{1750};
    static constexpr uint16_t BIT_MARK{425};
    static constexpr uint16_t ONE_SPACE{1275};
    static constexpr uint16_t ZERO_SPACE{425};
    static constexpr uint16_t TOLERANCE{200};
};

}  // namespace ir
}  // namespace unit
}  // namespace m5
#endif
