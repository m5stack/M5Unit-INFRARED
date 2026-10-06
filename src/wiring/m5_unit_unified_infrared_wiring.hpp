/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file m5_unit_unified_infrared_wiring.hpp
  @brief Opt-in, header-only board-aware connection helpers specific to M5Unit-INFRARED
  @details Mirrors the M5UnitUnified core wiring helper (`m5::unit::wiring`) but holds connections
           that are product-specific to this library and therefore do NOT belong in the generic,
           survey-derived core tables:
           - HatPIR (U054): single DOUT input pin, whose GPIO differs from the generic Hat header
           - Built-in IR (board IR LED / receiver): per-board TX/RX GPIO and StickS3 power setup
  @note Include this LAST (it self-includes the core wiring helper, which detects M5Unified via
        `__M5UNIFIED_HPP__`). Example/test must `#include <M5Unified.h>` before this header.
*/
#ifndef M5_UNIT_UNIFIED_INFRARED_WIRING_HPP
#define M5_UNIT_UNIFIED_INFRARED_WIRING_HPP

#include <wiring/m5_unit_unified_wiring.hpp>  // M5UU core wiring (UnitUnified/Component/Wire + addI2C/addGPIO)
#include "../unit/unit_IR.hpp"                // UnitIR (addBuiltinIrRx sets its rx_pull)

namespace m5 {
namespace unit {
namespace infrared {
namespace wiring {

#if defined(__M5UNIFIED_HPP__)

///@name HatPIR (U054)
///@{
/*!
  @brief Add a HatPIR (AS312) on the board's Hat header DOUT pin
  @param units UnitUnified manager
  @param unit Unit Component to add (HatPIR / UnitAS312)
  @return True if successful
  @note HatPIR exposes a single digital-output (DOUT) line. Its GPIO is product-specific and does
        NOT match the generic 2-pin table in the core `addHatGPIO` (e.g. StickC: G36 here vs G26
        there), so it is kept in this library-local helper. tx is always -1 (input only).
*/
inline bool addHatPIR(UnitUnified& units, Component& unit)
{
    int in = -1;
    switch (M5.getBoard()) {
        case m5::board_t::board_M5StickC:
        case m5::board_t::board_M5StickCPlus:
        case m5::board_t::board_M5StickCPlus2:
        case m5::board_t::board_M5StackCoreInk:
            in = 36;
            break;
        case m5::board_t::board_M5StickS3:
            in = 1;
            break;
        case m5::board_t::board_ArduinoNessoN1:
            in = 2;
            break;
        default:
            M5_LIB_LOGE("wiring: addHatPIR unsupported board=0x%02x", (int)M5.getBoard());
            return false;
    }
    M5_LIB_LOGI("wiring: addHatPIR board=0x%02x in=%d", (int)M5.getBoard(), in);
    return units.add(unit, static_cast<int8_t>(in), static_cast<int8_t>(-1));
}
///@}

///@name Built-in IR (board IR LED / receiver)
///@{
/*!
  @brief Built-in IR transmit (LED) GPIO for the current board
  @return GPIO number, or -1 if the board has no built-in IR transmitter
*/
inline int8_t builtinIrTxPin()
{
    switch (M5.getBoard()) {
        case m5::board_t::board_M5StickC:
        case m5::board_t::board_M5StickCPlus:
        case m5::board_t::board_ArduinoNessoN1:
            return 9;
        case m5::board_t::board_M5StickCPlus2:
            return 19;
        case m5::board_t::board_M5StickS3:
            return 46;
        case m5::board_t::board_M5AtomLite:
        case m5::board_t::board_M5AtomMatrix:
        case m5::board_t::board_M5AtomU:
        case m5::board_t::board_M5AtomS3U:
            return 12;
        case m5::board_t::board_M5AtomS3:
        case m5::board_t::board_M5AtomS3Lite:
        case m5::board_t::board_M5Capsule:
            return 4;
        case m5::board_t::board_M5AtomS3R:
        case m5::board_t::board_M5AtomVoiceS3R:
            return 47;
        case m5::board_t::board_M5NanoC6:
        case m5::board_t::board_M5NanoH2:
            return 3;
        case m5::board_t::board_M5Cardputer:
        case m5::board_t::board_M5CardputerADV:
            return 44;
        default:
            return -1;
    }
}

/*!
  @brief Built-in IR receive GPIO for the current board (StickS3 only)
  @return GPIO number, or -1 if the board has no built-in IR receiver
*/
inline int8_t builtinIrRxPin()
{
    switch (M5.getBoard()) {
        case m5::board_t::board_M5StickS3:
            return 42;
        default:
            return -1;
    }
}

/*!
  @brief Add a unit on the board's built-in IR transmitter (TX only, rx = -1)
  @param units UnitUnified manager
  @param unit Unit Component to add (UnitIR)
  @return True if successful, false if the board has no built-in IR TX
  @note rx is -1 to avoid self-reception / RX buffer overflow on TX-only usage. StickS3 needs
        EXT_5V enabled to drive the IR LED.
*/
inline bool addBuiltinIrTx(UnitUnified& units, Component& unit)
{
    const int8_t tx = builtinIrTxPin();
    if (tx < 0) {
        M5_LIB_LOGE("wiring: no built-in IR TX on board=0x%02x", (int)M5.getBoard());
        return false;
    }
    if (M5.getBoard() == m5::board_t::board_M5StickS3) {
        M5.Power.setExtOutput(true);
    }
    M5_LIB_LOGI("wiring: addBuiltinIrTx tx=%d", tx);
    return units.add(unit, static_cast<int8_t>(-1), tx);
}

/*!
  @brief Add a unit on the board's built-in IR receiver (RX only, tx = -1)
  @param units UnitUnified manager
  @param unit UnitIR to add
  @return True if successful, false if the board has no built-in IR RX
  @note tx is -1 to keep the IR LED unclaimed. StickS3 needs the speaker disabled (shares the pin
        domain), EXT_5V enabled, and an internal pull-up on the RX pin (set via UnitIR::config_t::rx_pull,
        applied by the adapter at begin()).
*/
inline bool addBuiltinIrRx(UnitUnified& units, UnitIR& unit)
{
    const int8_t rx = builtinIrRxPin();
    if (rx < 0) {
        M5_LIB_LOGE("wiring: no built-in IR RX on board=0x%02x", (int)M5.getBoard());
        return false;
    }
    if (M5.getBoard() == m5::board_t::board_M5StickS3) {
        M5.Speaker.end();
        M5.Power.setExtOutput(true);
        auto cfg    = unit.config();
        cfg.rx_pull = gpio::RxPull::Up;
        unit.config(cfg);
    }
    M5_LIB_LOGI("wiring: addBuiltinIrRx rx=%d", rx);
    return units.add(unit, rx, static_cast<int8_t>(-1));
}
///@}

#endif  // __M5UNIFIED_HPP__

}  // namespace wiring
}  // namespace infrared
}  // namespace unit
}  // namespace m5
#endif  // M5_UNIT_UNIFIED_INFRARED_WIRING_HPP
