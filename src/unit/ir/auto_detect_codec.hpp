/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file auto_detect_codec.hpp
  @brief Auto-detecting multi-protocol IR codec
*/
#ifndef M5_UNIT_INFRARED_IR_AUTO_DETECT_CODEC_HPP
#define M5_UNIT_INFRARED_IR_AUTO_DETECT_CODEC_HPP

#include "nec_codec.hpp"
#include "sirc_codec.hpp"
#include "rc5_codec.hpp"
#include "rc6_codec.hpp"
#include "panasonic_codec.hpp"
#include "mitsubishi_codec.hpp"
#include <memory>

namespace m5 {
namespace unit {
namespace ir {

/*!
  @class AutoDetectCodec
  @brief Auto-detecting multi-protocol IR decoder
  @details Tries multiple IR protocols in order during decode().
  Detection priority:
  1. NEC         — 9 ms leader mark
  2. Panasonic   — 3.5 ms leader mark
  3. RC6         — 2.67 ms leader mark
  4. SIRC        — 2.4 ms leader mark
  5. Mitsubishi  — no leader, short 300us bit mark
  6. RC5         — no leader, ~889us Manchester half-bit

  For encode(), delegates to the codec matching the last successfully decoded protocol,
  defaulting to NEC.
 */
class AutoDetectCodec : public IRCodec {
public:
    //! @brief Constructor
    AutoDetectCodec() : IRCodec(CodecType::Unknown)
    {
    }

    /*!
      @brief Encode using the last detected protocol's codec (default: NEC)
      @param address Device address for the target protocol
      @param command Command code for the target protocol
      @param repeat  True to emit the repeat frame (only NEC has a dedicated one)
      @return Encoded RMT items
     */
    item_container_type encode(uint16_t address, uint16_t command, bool repeat) override
    {
        return _last_codec->encode(address, command, repeat);
    }

    /*!
      @brief Try decoding with each protocol codec in priority order
      @param items RMT items from receiver
      @param num   Number of items
      @param[out] result Populated when a protocol matches
      @return True on successful decode
     */
    bool decode(const gpio::m5_rmt_item_t* items, uint32_t num, DecodeResult& result) override;

    /*!
      @brief Carrier frequency of the last detected protocol
      @return Frequency in Hz (default NEC: 38 kHz)
     */
    uint32_t carrierFrequencyHz() const override
    {
        return _last_codec->carrierFrequencyHz();
    }

    ///@name Access individual codecs
    ///@{
    //! @brief Get NEC codec instance
    NecCodec& nec()
    {
        return _nec;
    }
    //! @brief Get Sony SIRC codec instance
    SircCodec& sirc()
    {
        return _sirc;
    }
    //! @brief Get Philips RC5 codec instance
    Rc5Codec& rc5()
    {
        return _rc5;
    }
    //! @brief Get Philips RC6 codec instance
    Rc6Codec& rc6()
    {
        return _rc6;
    }
    //! @brief Get Panasonic / Kaseikyo codec instance
    PanasonicCodec& panasonic()
    {
        return _panasonic;
    }
    //! @brief Get Mitsubishi codec instance
    MitsubishiCodec& mitsubishi()
    {
        return _mitsubishi;
    }
    ///@}

    /*!
      @brief Get the codec that last successfully decoded
      @return Pointer to the active codec (NecCodec on startup)
     */
    IRCodec* lastCodec()
    {
        return _last_codec;
    }

private:
    NecCodec _nec{};
    SircCodec _sirc{};
    Rc5Codec _rc5{};
    Rc6Codec _rc6{};
    PanasonicCodec _panasonic{};
    MitsubishiCodec _mitsubishi{};
    IRCodec* _last_codec{&_nec};
};

}  // namespace ir
}  // namespace unit
}  // namespace m5
#endif
