/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_IR.hpp
  @brief IR remote control unit (TX + RX) for M5UnitUnified
*/
#ifndef M5_UNIT_INFRARED_UNIT_IR_HPP
#define M5_UNIT_INFRARED_UNIT_IR_HPP

#include <M5UnitComponent.hpp>
#include "ir/auto_detect_codec.hpp"

namespace m5 {
namespace unit {

/*!
  @class m5::unit::UnitIR
  @brief IR remote control transceiver unit
  @details Supports Unit IR (SKU: U002) via Grove and built-in IR transmitters (e.g., M5StickC Plus2 GPIO 9).
  Uses ESP32 RMT peripheral for precise carrier modulation (TX) and timing capture (RX).

  @par Usage (Unit IR U002 via Grove)
  @code
  m5::unit::UnitUnified Units;
  m5::unit::UnitIR ir;
  // Port B preferred, fallback to Port A
  auto pin_rx = M5.getPin(m5::pin_name_t::port_b_in);
  auto pin_tx = M5.getPin(m5::pin_name_t::port_b_out);
  if (pin_rx < 0 || pin_tx < 0) {
      pin_rx = M5.getPin(m5::pin_name_t::port_a_pin1);
      pin_tx = M5.getPin(m5::pin_name_t::port_a_pin2);
  }
  Units.add(ir, pin_rx, pin_tx);
  Units.begin();
  ir.send(0x00, 0x1F);  // NEC send
  @endcode

  @par Usage (Built-in IR, TX only, e.g. M5StickC Plus2)
  @code
  m5::unit::UnitUnified Units;
  m5::unit::UnitIR ir;
  if (!Units.add(ir, -1, 19) || !Units.begin()) {  // CPlus2: TX=G19
      // error
  }
  ir.send(0x00, 0x1F);
  @endcode

  @par Usage (Built-in IR, TX+RX, e.g. M5StickS3)
  @code
  m5::unit::UnitUnified Units;
  m5::unit::UnitIR ir;
  if (!Units.add(ir, 42, 46) || !Units.begin()) {  // StickS3: RX=G42, TX=G46
      // error
  }
  @endcode

  @par Built-in IR pin assignments
  | Device          | TX GPIO | RX GPIO |
  |-----------------|---------|---------|
  | M5StickC        | 9       | -       |
  | M5StickC Plus   | 9       | -       |
  | M5StickC Plus2  | 19      | -       |
  | M5StickS3       | 46      | 42      |
  | Atom Lite       | 12      | -       |
  | Atom Matrix     | 12      | -       |
  | AtomU           | 12      | -       |
  | AtomS3          | 4       | -       |
  | AtomS3 Lite     | 4       | -       |
  | AtomS3U         | 12      | -       |
  | AtomS3R         | 47      | -       |
  | Atom VoiceS3R   | 47      | -       |
  | M5Capsule       | 4       | -       |
  | NanoC6          | 3       | -       |
  | NessoN1         | 9       | -       |
  | Cardputer       | 44      | -       |
  | Cardputer ADV   | 44      | -       |
 */
class UnitIR : public Component {
    M5_UNIT_COMPONENT_HPP_BUILDER(UnitIR, 0x00);

public:
    /*!
      @struct config_t
      @brief Settings for begin
     */
    struct config_t {
        //! RMT tick resolution in nanoseconds (default: 1000 = 1us)
        uint32_t tick_ns{1000};
        //! RX ring buffer size in bytes
        uint16_t rx_ring_buffer_size{1024};
        //! RX idle threshold in ticks (signal gap indicating end of frame)
        //! @note Default 20000us is sufficient for all standard IR protocols.
        //! NEC full frame ~67.5ms but inter-frame gap detection needs ~10-20ms.
        uint16_t rx_idle_threshold{20000};
        //! RX noise filter threshold in ticks
        //! @note Filters glitches shorter than this value. 200us is safe for IR signals.
        uint16_t rx_filter_threshold{200};
        //! Minimum RMT items for a valid frame (noise filter)
        //! @note Frames with fewer items are silently discarded.
        //! NEC=34, SIRC=13+, RC5=14, RC6=22+, Panasonic=50
        uint8_t rx_min_item_count{2};
        //! Invert RX signal levels for active-LOW receivers
        //! @note Most IR receivers (VS1838B on Unit IR, and most built-in IR receivers
        //!       such as StickS3) are active-LOW, so the default `true` is correct.
        bool rx_invert_level{true};
    };

    //! @brief Constructor
    UnitIR() : Component(0x00)
    {
    }

    /*!
      @brief Gets the config values
      @return Current configuration (by value)
     */
    config_t config() const
    {
        return _cfg;
    }
    /*!
      @brief Set the config values
      @param cfg New configuration to apply on next begin()
     */
    void config(const config_t& cfg)
    {
        _cfg = cfg;
    }

    /*!
      @brief Initialize the RMT channels (TX and/or RX) based on the adapter pins
      @return True on success, false if neither TX nor RX is configured or RMT init fails
      @note `config_t::tick_ns` must be 1000; other values are rejected because codec
            timing constants assume 1us per tick.
     */
    bool begin() override;
    /*!
      @brief Poll the RX ringbuffer and decode incoming frames
      @param force Unused (reserved for future periodic-update support)
     */
    void update(const bool force = false) override;

    ///@name Codec
    ///@{
    /*!
      @brief Get current codec
      @return Reference to the active codec (default: built-in AutoDetectCodec)
     */
    ir::IRCodec& codec()
    {
        return *_codec;
    }
    /*!
      @brief Set protocol codec
      @param codec Codec instance (must outlive UnitIR; typically a global/static variable)
      @note Default codec is a built-in AutoDetectCodec. Call resetCodec() to restore it.
     */
    void setCodec(ir::IRCodec& codec)
    {
        _codec = &codec;
    }
    //! @brief Reset to built-in AutoDetectCodec
    void resetCodec()
    {
        _codec = &_default_codec;
    }
    /*!
      @brief Get built-in AutoDetectCodec (for accessing individual protocol codecs)
      @return Reference to the internal AutoDetectCodec instance
     */
    ir::AutoDetectCodec& defaultCodec()
    {
        return _default_codec;
    }
    ///@}

    ///@name TX
    ///@{
    /*!
      @brief Send IR command using current codec
      @param address Device address
      @param command Command code
      @param frames Frame count per keypress.
             0 (default) uses the codec's `minFrames()` (NEC=1, Mitsubishi=2, Sony SIRC=3).
             Any value ≥ 1 overrides and sends exactly that many frames.
      @return True if successful
      @note Multiple frames are sent tightly in a single RMT burst with `codec.frameGapUs()`
            between them. To simulate press-and-hold (NEC repeat frame at ~110 ms intervals,
            etc.), loop externally using `encode(addr, cmd, true) + sendRaw()`.
      @note Carrier frequency and duty cycle are determined by the current codec.
     */
    bool send(uint16_t address, uint16_t command, uint8_t frames = 0);

    /*!
      @brief Send raw RMT items directly
      @param items RMT items
      @param num Number of items
      @return True if successful
      @note Uses carrier settings from the current codec.
     */
    bool sendRaw(const gpio::m5_rmt_item_t* items, uint32_t num);
    ///@}

    ///@name RX
    ///@{
    /*!
      @brief Number of decoded messages available since last update
      @return 1 if a decoded frame is pending, 0 otherwise
     */
    size_t available() const
    {
        return _rx_available ? 1 : 0;
    }
    /*!
      @brief True if no decoded messages
      @return True when no frame has been decoded since the last flush/update
     */
    bool empty() const
    {
        return !_rx_available;
    }
    /*!
      @brief Get latest decoded result
      @return Reference to the most recent DecodeResult (valid until next update)
     */
    const ir::DecodeResult& latest() const
    {
        return _latest_result;
    }
    //! @brief Clear received data (both DecodeResult and raw items)
    void flush();
    /*!
      @brief Get raw RMT items from last reception
      @return Pointer to the raw item array (valid until next update()), or nullptr if none
     */
    const gpio::m5_rmt_item_t* rawItems() const
    {
        return _raw_items;
    }
    /*!
      @brief Number of raw RMT items from last reception
      @return Count of valid items in rawItems(), 0 if none
     */
    uint32_t rawItemCount() const
    {
        return _raw_item_count;
    }
    ///@}

    ///@name State
    ///@{
    /*!
      @brief True if TX pin is configured and RMT TX channel initialized
      @return True if transmit is available
     */
    bool hasTX() const
    {
        return _has_tx;
    }
    /*!
      @brief True if RX pin is configured and RMT RX channel initialized
      @return True if receive is available
     */
    bool hasRX() const
    {
        return _has_rx;
    }
    ///@}

private:
    bool read_rx();
    bool apply_carrier();

    ir::AutoDetectCodec _default_codec{};
    ir::IRCodec* _codec{&_default_codec};
    config_t _cfg{};
    ir::DecodeResult _latest_result{};
    const gpio::m5_rmt_item_t* _raw_items{};
    uint32_t _raw_item_count{};
    bool _rx_available{};
    bool _has_tx{};
    bool _has_rx{};

    // RX buffer (4-byte aligned for RMT v2)
    struct FreeDeleter {
        void operator()(uint8_t* p) const
        {
            free(p);
        }
    };
    std::unique_ptr<uint8_t[], FreeDeleter> _rx_buffer{};
    size_t _rx_buffer_size{};
};

}  // namespace unit
}  // namespace m5
#endif
