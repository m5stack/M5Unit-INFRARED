/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file auto_detect_codec.cpp
  @brief Auto-detecting multi-protocol IR codec
*/
#include "auto_detect_codec.hpp"
#include "ir_rmt_items.hpp"

namespace m5 {
namespace unit {
namespace ir {

bool AutoDetectCodec::decode(const gpio::m5_rmt_item_t* items, uint32_t num, DecodeResult& result)
{
    if (!items || num < 2) {
        return false;
    }

    // Discriminate by leader pulse duration
    uint16_t leader_mark = items[0].duration0;

    // NEC: ~9000us leader mark
    if (matchDuration(leader_mark, 9000, 1000)) {
        if (_nec.decode(items, num, result)) {
            _last_codec = &_nec;
            return true;
        }
    }

    // Panasonic: ~3500us leader mark
    if (matchDuration(leader_mark, 3500, 500)) {
        if (_panasonic.decode(items, num, result)) {
            _last_codec = &_panasonic;
            return true;
        }
    }

    // RC6: ~2666us leader mark
    if (matchDuration(leader_mark, 2666, 400)) {
        if (_rc6.decode(items, num, result)) {
            _last_codec = &_rc6;
            return true;
        }
    }

    // SIRC: ~2400us leader mark
    if (matchDuration(leader_mark, 2400, 400)) {
        if (_sirc.decode(items, num, result)) {
            _last_codec = &_sirc;
            return true;
        }
    }

    // No leader protocols: discriminate by first mark duration
    // Mitsubishi: ~300us bit mark (short)
    // RC5: ~889us half-bit mark (longer)
    if (items[0].duration0 < 500) {
        // Try Mitsubishi first (short bit mark ~300us)
        if (_mitsubishi.decode(items, num, result)) {
            _last_codec = &_mitsubishi;
            return true;
        }
    }

    // RC5: No leader pulse (Manchester pattern, ~889us half-bit)
    if (_rc5.decode(items, num, result)) {
        _last_codec = &_rc5;
        return true;
    }

    return false;
}

}  // namespace ir
}  // namespace unit
}  // namespace m5
