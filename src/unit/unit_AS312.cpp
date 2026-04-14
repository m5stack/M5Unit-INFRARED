/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_AS312.cpp
  @brief AS312 Unit for M5UnitUnified
*/
#include "unit_AS312.hpp"
#include <M5Utility.hpp>

using namespace m5::utility::mmh3;
using namespace m5::unit::types;

namespace m5 {
namespace unit {

const char UnitAS312::name[] = "UnitAS312";
const types::uid_t UnitAS312::uid{"UnitAS312"_mmh3};
const types::attr_t UnitAS312::attr{attribute::AccessGPIO};

bool UnitAS312::begin()
{
    if (_cfg.interval >= HOLD_TIME_MS) {
        M5_LIB_LOGE("Interval %u ms >= hold time %u ms. Detection events may be missed", _cfg.interval, HOLD_TIME_MS);
        return false;
    }

    auto aptr = adapter();
    if (!aptr) {
        M5_LIB_LOGE("Adapter is null");
        return false;
    }

    auto err = aptr->pinModeRX(gpio::Mode::InputPullup);
    if (err != m5::hal::error::error_t::OK) {
        M5_LIB_LOGE("Failed to set pin mode: %d", (int)err);
        return false;
    }

    _interval = _cfg.interval;

    return true;
}

void UnitAS312::update(const bool force)
{
    _updated      = false;
    _was_detected = false;
    _was_released = false;

    auto now = m5::utility::millis();
    if (!force && (now - _latest) < _interval) {
        return;
    }

    bool det{};
    if (readDetection(det)) {
        _latest = now;
        if (det != _detected) {
            _updated      = true;
            _was_detected = (det && !_prev_detected);
            _was_released = (!det && _prev_detected);
        }
        _prev_detected = _detected;
        _detected      = det;
    }
}

bool UnitAS312::readDetection(bool& detected)
{
    detected  = false;
    auto aptr = adapter();
    if (!aptr) {
        return false;
    }
    bool raw{};
    auto err = aptr->readDigitalRX(raw);
    if (err != m5::hal::error::error_t::OK) {
        return false;
    }
    detected = raw;  // Active HIGH: HIGH = detected, LOW = no detection
    return true;
}

}  // namespace unit
}  // namespace m5
