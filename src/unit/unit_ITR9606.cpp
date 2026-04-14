/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_ITR9606.cpp
  @brief ITR9606 Unit for M5UnitUnified
*/
#include "unit_ITR9606.hpp"
#include <M5Utility.hpp>

using namespace m5::utility::mmh3;
using namespace m5::unit::types;

namespace m5 {
namespace unit {

const char UnitITR9606::name[] = "UnitITR9606";
const types::uid_t UnitITR9606::uid{"UnitITR9606"_mmh3};
const types::attr_t UnitITR9606::attr{attribute::AccessGPIO};

bool UnitITR9606::begin()
{
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

void UnitITR9606::update(const bool force)
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

bool UnitITR9606::readDetection(bool& detected)
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
    detected = raw;  // Active HIGH: HIGH = blocked (detected), LOW = clear
    return true;
}

}  // namespace unit
}  // namespace m5
