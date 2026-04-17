/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file ir_codec.hpp
  @brief IR protocol codec base class and types
*/
#ifndef M5_UNIT_INFRARED_IR_CODEC_HPP
#define M5_UNIT_INFRARED_IR_CODEC_HPP

#include <M5UnitComponent.hpp>
#include <cstdint>
#include <vector>

namespace m5 {
namespace unit {
namespace ir {

/*!
  @enum CodecType
  @brief Identifies the IR protocol codec implementation
  @note Additional protocols (e.g., Samsung, LG, JVC, Sharp, AEHA) may be added in future versions.
  Values up to 253 are reserved for built-in protocols. Use Custom (255) for user-defined protocols.
 */
enum class CodecType : uint8_t {
    NEC = 0,     //!< NEC / Extended NEC
    SIRC,        //!< Sony SIRC (12/15/20-bit)
    RC5,         //!< Philips RC5 / RC5X
    RC6,         //!< Philips RC6 Mode 0
    Panasonic,   //!< Panasonic / Kaseikyo (48-bit, customer code + data)
    Mitsubishi,  //!< Mitsubishi (16-bit, no leader, sent twice)
    Raw,         //!< Raw mark/space (no protocol)
    // Reserved for future protocols (Samsung, LG, JVC, Sharp, AEHA, etc.)
    Unknown = 254,  //!< Unknown protocol
    Custom  = 255,  //!< User-defined custom protocol
};

/*!
  @struct DecodeResult
  @brief Result of decoding a received IR frame
 */
struct DecodeResult {
    CodecType protocol{CodecType::Unknown};  //!< Detected protocol
    uint16_t address{};                      //!< Device address
    uint16_t command{};                      //!< Command code
    uint64_t raw{};                          //!< Raw encoded value (protocol-specific, up to 64 bits)
    uint8_t bits{};                          //!< Total bit count (NEC=32, SIRC=12/15/20, RC5=14, RC6=21)
    bool repeat{};                           //!< True if repeat frame (NEC repeat code / re-sent frame)
    bool toggle{};                           //!< Toggle bit (RC5/RC6 only)
};

using item_container_type = std::vector<m5::unit::gpio::m5_rmt_item_t>;  //!< RMT item container

/*!
  @class IRCodec
  @brief Abstract base class for IR protocol encoding/decoding
  @details Follows the Strategy pattern (like rf433::ProtocolCodec).
  Each protocol subclass implements encode/decode with protocol-specific timing.
 */
class IRCodec {
public:
    explicit IRCodec(CodecType t) : _type(t)
    {
    }
    virtual ~IRCodec() = default;

    /*!
      @brief Get codec type for safe downcasting
      @return CodecType enum value identifying the protocol
     */
    CodecType type() const
    {
        return _type;
    }

    ///@name TX
    ///@{
    /*!
      @brief Encode IR command into RMT items
      @param address Device address
      @param command Command code
      @param repeat If true, encode repeat frame instead of full frame
      @return RMT items for transmission
     */
    virtual item_container_type encode(uint16_t address, uint16_t command, bool repeat = false) = 0;

    /*!
      @brief Carrier frequency for this protocol in Hz
      @return Frequency (e.g., 38000 for NEC, 40000 for SIRC, 36000 for RC5/RC6)
     */
    virtual uint32_t carrierFrequencyHz() const = 0;

    /*!
      @brief Carrier duty cycle (0.0 - 1.0)
      @return Duty cycle (default 0.33)
     */
    virtual float carrierDuty() const
    {
        return 0.33f;
    }

    /*!
      @brief Number of frames that should be transmitted per keypress for this protocol
      @return Default 1; overridden by protocols that require multi-frame transmission
              (Sony SIRC = 3, Mitsubishi = 2)
     */
    virtual uint8_t minFrames() const
    {
        return 1;
    }

    /*!
      @brief Inter-frame gap (microseconds) when transmitting multiple frames in one burst
      @return Default 0; overridden by protocols that specify a gap
              (Sony SIRC ≈ 30000, Mitsubishi ≈ 28500)
     */
    virtual uint16_t frameGapUs() const
    {
        return 0;
    }
    ///@}

    ///@name RX
    ///@{
    /*!
      @brief Try to decode RMT items into an IR command
      @param items RMT items from receiver
      @param num Number of items
      @param[out] result Decoded result
      @return true if successfully decoded
     */
    virtual bool decode(const m5::unit::gpio::m5_rmt_item_t* items, uint32_t num, DecodeResult& result) = 0;
    ///@}

private:
    CodecType _type;
};

}  // namespace ir
}  // namespace unit
}  // namespace m5
#endif
