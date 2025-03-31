/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_STHS34PF80.cpp
  @brief STHS34PF80 Unit for M5UnitUnified
*/
#include "unit_STHS34PF80.hpp"
#include <M5Utility.hpp>

using namespace m5::utility::mmh3;
using namespace m5::unit::types;
using namespace m5::unit::sths34pf80;
using namespace m5::unit::sths34pf80::command;
using m5::unit::types::elapsed_time_t;

namespace {
constexpr uint8_t DEVICE_IDENTIFICATION_VALUE{0xD3};

constexpr uint8_t GAIN_WIDE_MODE{0x81};
constexpr uint8_t GAIN_DEFAULT_MODE{0xF1};

constexpr int16_t SENSITIVITY_MAX{4080};

constexpr uint8_t CTRL0_RESERVED{0x81};
constexpr uint8_t CTRL1_BDU{0x10};
constexpr uint8_t CTRL2_ONE_SHOT{0x01};

constexpr uint8_t ENABLE_FUNC_CFG_ACCESS{0x10};
constexpr uint8_t FUNC_CFG_READ{0x20};
constexpr uint8_t FUNC_CFG_WRITE{0x40};

constexpr uint8_t ALGO_ENABLE_RESET{0x01};

inline constexpr uint16_t raw_to_sensitivity(const int8_t raw)
{
    return static_cast<uint16_t>(raw * 16 + 2048);
}

inline constexpr int8_t sensitivity_to_raw(const uint16_t s)
{
    return static_cast<int8_t>(((int)s - 2048) / 16);
}

constexpr uint32_t interval_table[] = {
    0, 4000, 2000, 1000, 500, 250, 125, 66, 33,
};

// ODR Maximum configurable value depends on AVG_TMOS
constexpr ODR max_odr_table[] = {
    ODR::Rate30, ODR::Rate30, ODR::Rate30, ODR::Rate8, ODR::Rate4, ODR::Rate2, ODR::Rate1, ODR::Rate0_5,
};

// For singleshot (Typcal)
constexpr uint32_t wait_table[] = {5, 6, 9, 20, 36, 67, 128, 252};

}  // namespace

namespace m5 {
namespace unit {
// class UnitSTHS34PF80
const char UnitSTHS34PF80::name[] = "UnitSTHS34PF80";
const types::uid_t UnitSTHS34PF80::uid{"UnitSTHS34PF80"_mmh3};
const types::uid_t UnitSTHS34PF80::attr{0};

sths34pf80::ODR UnitSTHS34PF80::maximum_odr(const ObjectTemperatureAverage avg_tmos)
{
    return max_odr_table[m5::stl::to_underlying(avg_tmos)];
}

bool UnitSTHS34PF80::begin()
{
    auto ssize = stored_size();
    assert(ssize && "stored_size must be greater than zero");
    if (ssize != _data->capacity()) {
        _data.reset(new m5::container::CircularBuffer<Data>(ssize));
        if (!_data) {
            M5_LIB_LOGE("Failed to allocate");
            return false;
        }
    }

    m5::utility::delay(3);  // Need wait 2.5ms for boot

    // Check ID
    uint8_t id{};
    if (!readRegister8(WHO_AM_I_REG, id, 0) || id != DEVICE_IDENTIFICATION_VALUE) {
        M5_LIB_LOGE("Cannot detect sths34pf80 %02X", id);
        return false;
    }

    // Read the sensitivity
    if (!readSensitivity(_sensitivity)) {
        M5_LIB_LOGE("Failed to read sensitivity");
        return false;
    }
    return _cfg.start_periodic ? (writeAverageTrim(_cfg.avg_t, _cfg.avg_tmos) &&
                                  startPeriodicMeasurement(_cfg.mode, _cfg.odr, _cfg.comp_type, _cfg.abs))
                               : true;
}

void UnitSTHS34PF80::update(const bool force)
{
    _updated = false;
    if (inPeriodic()) {
        elapsed_time_t at{m5::utility::millis()};
        if (force || !_latest || at >= _latest + _interval) {
            Data d{};
            _updated = is_data_ready() && read_measurement(d);
            if (_updated) {
                _latest       = m5::utility::millis();
                d.sensitivity = _sensitivity;
                _data->push_back(d);
            }
        }
    }
}

bool UnitSTHS34PF80::start_periodic_measurement(const sths34pf80::Gain mode, const sths34pf80::ODR odr,
                                                const bool comp_type, const bool abs)
{
    if (inPeriodic()) {
        return false;
    }
    if (odr == ODR::PowerDown) {
        M5_LIB_LOGE("odr must be other than PowerDown");
        return false;
    }

    AmbientTemperatureAverage avg_t{};
    ObjectTemperatureAverage avg_tmos{};
    uint8_t acfg{}, v{};

    // Check if odr is a valid value
    if (!readAverageTrim(avg_t, avg_tmos)) {
        M5_LIB_LOGE("Failed to readAverageTrim");
        return false;
    }
    if (odr > maximum_odr(avg_tmos)) {
        M5_LIB_LOGE("ODR cannot be set because the number of samples in avg_tmos is too large %u,%u,%u", avg_tmos, odr,
                    maximum_odr(avg_tmos));
        return false;
    }

    //
    acfg = (comp_type ? 0x04 : 0x00) | (abs ? 0x02 : 0x00);
    v    = CTRL1_BDU | m5::stl::to_underlying(odr);  // Enabled BDU and ODR

    _periodic = writeGainMode(mode) && write_algorithm_config(acfg) && resetAlgorithm() && writeRegister8(CTRL1_REG, v);
    if (_periodic) {
        _latest   = 0;
        _interval = interval_table[m5::stl::to_underlying(odr)];
    }
    return _periodic;
}

bool UnitSTHS34PF80::stop_periodic_measurement()
{
    if (!inPeriodic()) {
        return true;
    }

    uint8_t fs{};
    // 1. Read the FUNC_STATUS (25h) register
    if (readRegister8(FUNC_STATUS_REG, fs, 0)) {
        auto timeout_at = m5::utility::millis() + 4100;
        // 2. Wait for the DRDY bit in the STATUS (23h) register to be set to 1
        do {
            if (is_data_ready()) {
                // 3. Set ODR[3:0] bits of the CTRL1 (20h) register to 0000b
                // 4. Read the FUNC_STATUS (25h) register
                if (write_odr(ODR::PowerDown) && readRegister8(FUNC_STATUS_REG, fs, 0)) {
                    _periodic = false;
                    _latest   = 0;
                    break;
                }
            }
            m5::utility::delay(1);
        } while (m5::utility::millis() <= timeout_at);
    }
    return !_periodic;
}

bool UnitSTHS34PF80::measureSingleshot(sths34pf80::Data& data, const sths34pf80::AmbientTemperatureAverage avg_t,
                                       const sths34pf80::ObjectTemperatureAverage avg_tmos)
{
    data = Data{};

    if (!guard_in_periodic(__func__)) {
        return false;
    }

    uint8_t c2{};
    if (writeAverageTrim(avg_t, avg_tmos) && write_odr(ODR::PowerDown) && readRegister8(CTRL2_REG, c2, 0) &&
        writeRegister8(CTRL2_REG, c2 | CTRL2_ONE_SHOT)) {
        m5::utility::delay(wait_table[m5::stl::to_underlying(avg_tmos)]);
        auto timeout_at = m5::utility::millis() + wait_table[m5::stl::to_underlying(avg_tmos)];
        do {
            // Check CTRL2_ONE_SHOT is cleared and ready to read data
            if (readRegister8(CTRL2_REG, c2, 0) && ((c2 & CTRL2_ONE_SHOT) == 0) && is_data_ready() &&
                read_measurement(data, false)) {
                data.sensitivity = _sensitivity;
                return true;
            }
            m5::utility::delay(1);
        } while (m5::utility::millis() <= timeout_at);
    }
    return false;
}

bool UnitSTHS34PF80::readLowPassFilter(sths34pf80::LowPassFilter& lpf_p_m, sths34pf80::LowPassFilter& lpf_m,
                                       sths34pf80::LowPassFilter& lpf_p, sths34pf80::LowPassFilter& lpf_a_t)
{
    lpf_p_m = lpf_m = lpf_p = lpf_a_t = LowPassFilter::ODR9;

    uint8_t v[2]{};
    if (readRegister(LPF1_REG, v, 2, 0)) {
        lpf_p_m = static_cast<LowPassFilter>((v[0] >> 3) & 0x07);
        lpf_m   = static_cast<LowPassFilter>(v[0] & 0x07);
        lpf_p   = static_cast<LowPassFilter>((v[1] >> 3) & 0x07);
        lpf_a_t = static_cast<LowPassFilter>(v[1] & 0x07);
        return true;
    }
    return false;
}

bool UnitSTHS34PF80::writeLowPassFilter(const sths34pf80::LowPassFilter lpf_p_m, const sths34pf80::LowPassFilter lpf_m,
                                        const sths34pf80::LowPassFilter lpf_p, const sths34pf80::LowPassFilter lpf_a_t)
{
    if (!guard_in_periodic(__func__)) {
        return false;
    }

    if (m5::stl::to_underlying(lpf_p_m) >= m5::stl::to_underlying(lpf_m) ||
        m5::stl::to_underlying(lpf_p_m) >= m5::stl::to_underlying(lpf_p)) {
        M5_LIB_LOGE(
            "The value of lpf_p_m(ODR/n) must be greater than lpf_m and lpf_p %u,%u,%u\n"
            "Not the value of the enum, but the greater or lesser meaning it represents. ODR/9 > ODR/20 >...",
            lpf_p_m, lpf_m, lpf_p);
        return false;
    }

    uint8_t v[2]{};
    v[0] = (m5::stl::to_underlying(lpf_p_m) << 3) | m5::stl::to_underlying(lpf_m);
    v[1] = (m5::stl::to_underlying(lpf_p) << 3) | m5::stl::to_underlying(lpf_a_t);
    return writeRegister(LPF1_REG, v, 2);
}

bool UnitSTHS34PF80::readAverageTrim(sths34pf80::AmbientTemperatureAverage& avg_t,
                                     sths34pf80::ObjectTemperatureAverage& avg_tmos)
{
    avg_t    = AmbientTemperatureAverage::Samples8;
    avg_tmos = ObjectTemperatureAverage::Samples2;

    uint8_t v{};
    if (readRegister8(AVG_TRIM_REG, v, 0)) {
        avg_t    = static_cast<AmbientTemperatureAverage>((v >> 4) & 0x03);
        avg_tmos = static_cast<ObjectTemperatureAverage>(v & 0x07);
        return true;
    }
    return false;
}

bool UnitSTHS34PF80::writeAverageTrim(const sths34pf80::AmbientTemperatureAverage avg_t,
                                      const sths34pf80::ObjectTemperatureAverage avg_tmos)
{
    if (!guard_in_periodic(__func__)) {
        return false;
    }
    uint8_t v{};
    v = (m5::stl::to_underlying(avg_t) << 4) | m5::stl::to_underlying(avg_tmos);
    return writeRegister8(AVG_TRIM_REG, v);
}

bool UnitSTHS34PF80::readGainMode(sths34pf80::Gain& mode)
{
    mode = Gain::Wide;
    uint8_t v{};
    if (readRegister8(CTRL0_REG, v, 0) && ((v == GAIN_DEFAULT_MODE || v == GAIN_WIDE_MODE))) {
        mode = static_cast<Gain>((v >> 4) & 0x07);
        return true;
    }
    return false;
}

bool UnitSTHS34PF80::writeGainMode(const sths34pf80::Gain mode)
{
    if (!guard_in_periodic(__func__)) {
        return false;
    }

    uint8_t v{};
    v = CTRL0_RESERVED | (m5::stl::to_underlying(mode) << 4);
    return writeRegister8(CTRL0_REG, v);
}

bool UnitSTHS34PF80::readSensitivityRaw(int8_t& raw)
{
    raw = -128;
    return readRegister8(SENS_DATA_REG, (uint8_t&)raw, 0);
}

bool UnitSTHS34PF80::readSensitivity(uint16_t& value)
{
    value = 0;
    int8_t raw{};
    if (readSensitivityRaw(raw)) {
        value = raw_to_sensitivity(raw);
        return true;
    }
    return false;
}

bool UnitSTHS34PF80::writeSensitivityRaw(const int8_t raw)
{
    if (!guard_in_periodic(__func__)) {
        return false;
    }

    if (writeRegister8(SENS_DATA_REG, (uint8_t)raw)) {
        _sensitivity = raw_to_sensitivity(raw);
        return true;
    }
    return false;
}

bool UnitSTHS34PF80::writeSensitivity(const uint16_t value)
{
    if (value > SENSITIVITY_MAX) {
        M5_LIB_LOGE("Sensitivity must be between 0 and %u (%u)", SENSITIVITY_MAX, value);
        return false;
    }
    return writeSensitivityRaw(sensitivity_to_raw(value));
}

bool UnitSTHS34PF80::readObjectDataRate(sths34pf80::ODR& odr)
{
    odr = ODR::PowerDown;

    uint8_t v{};
    if (readRegister8(CTRL1_REG, v, 0)) {
        v &= 0x0F;
        odr = static_cast<ODR>(v & 0x08 ? 0x08 : v);  // 1xxxb is ODR30
        return true;
    }
    return false;
}

bool UnitSTHS34PF80::softReset()
{
    constexpr uint8_t BOOT{0x80};

    if ((inPeriodic() ? stopPeriodicMeasurement() : write_odr(ODR::PowerDown)) && writeRegister8(CTRL2_REG, BOOT)) {
        m5::utility::delay(3);  // Wait at least 2.5 ms
        return resetAlgorithm() && readSensitivity(_sensitivity);
    }
    return false;
}

bool UnitSTHS34PF80::resetAlgorithm()
{
    return guard_in_periodic(__func__) && write_embedded_register8(RESET_ALGO_REG, ALGO_ENABLE_RESET);
}

bool UnitSTHS34PF80::readPresenceThreshold(uint16_t& thres)
{
    return guard_in_periodic(__func__) && read_embedded_register16LE(PRESENCE_THS_REG, thres);
}

bool UnitSTHS34PF80::writePresenceThreshold(const uint16_t thres)
{
    if (!guard_in_periodic(__func__)) {
        return false;
    }
    if (thres & 0x8000) {
        M5_LIB_LOGE("Threshold must be 15 bits %04X", thres);
        return false;
    }
    return write_embedded_register16LE(PRESENCE_THS_REG, thres);
}

bool UnitSTHS34PF80::readMotionThreshold(uint16_t& thres)
{
    return guard_in_periodic(__func__) && read_embedded_register16LE(MOTION_THS_REG, thres);
}

bool UnitSTHS34PF80::writeMotionThreshold(const uint16_t thres)
{
    if (!guard_in_periodic(__func__)) {
        return false;
    }
    if (thres & 0x8000) {
        M5_LIB_LOGE("Threshold must be 15 bits %04X", thres);
        return false;
    }
    return write_embedded_register16LE(MOTION_THS_REG, thres);
}

bool UnitSTHS34PF80::readAmbientShockThreshold(uint16_t& thres)
{
    return guard_in_periodic(__func__) && read_embedded_register16LE(TAMB_SHOCK_THS_REG, thres);
}

bool UnitSTHS34PF80::writeAmbientShockThreshold(const uint16_t thres)
{
    if (!guard_in_periodic(__func__)) {
        return false;
    }
    if (thres & 0x8000) {
        M5_LIB_LOGE("Threshold must be 15 bits %04X", thres);
        return false;
    }
    return write_embedded_register16LE(TAMB_SHOCK_THS_REG, thres);
}

bool UnitSTHS34PF80::readPresenceHysteresis(uint8_t& hyst)
{
    return guard_in_periodic(__func__) && read_embedded_register8(HYST_PRESENCE_REG, hyst);
}

bool UnitSTHS34PF80::writePresenceHysteresis(const uint8_t hyst)
{
    if (!guard_in_periodic(__func__)) {
        return false;
    }

    uint16_t thres{};
    if (readPresenceThreshold(thres)) {
        if (hyst >= thres) {
            M5_LIB_LOGE("Hyst must be smaller than thres. %u %u", hyst, thres);
            return false;
        }
        return write_embedded_register8(HYST_PRESENCE_REG, hyst);
    }
    return false;
}

bool UnitSTHS34PF80::readMotionHysteresis(uint8_t& hyst)
{
    return guard_in_periodic(__func__) && read_embedded_register8(HYST_MOTION_REG, hyst);
}

bool UnitSTHS34PF80::writeMotionHysteresis(const uint8_t hyst)
{
    if (!guard_in_periodic(__func__)) {
        return false;
    }

    uint16_t thres{};
    if (readMotionThreshold(thres)) {
        if (hyst >= thres) {
            M5_LIB_LOGE("Hyst must be smaller than thres. %u %u", hyst, thres);
            return false;
        }
        return write_embedded_register8(HYST_MOTION_REG, hyst);
    }
    return false;
}

bool UnitSTHS34PF80::readAmbientShockHysteresis(uint8_t& hyst)
{
    return guard_in_periodic(__func__) && read_embedded_register8(HYST_TAMB_SHOCK_REG, hyst);
}

bool UnitSTHS34PF80::writeAmbientShockHysteresis(const uint8_t hyst)
{
    if (!guard_in_periodic(__func__)) {
        return false;
    }

    uint16_t thres{};
    if (readAmbientShockThreshold(thres)) {
        if (hyst >= thres) {
            M5_LIB_LOGE("Hyst must be smaller than thres. %u %u", hyst, thres);
            return false;
        }
        return write_embedded_register8(HYST_TAMB_SHOCK_REG, hyst);
    }
    return false;
}

bool UnitSTHS34PF80::readAlgorithmConfig(uint8_t& v)
{
    return guard_in_periodic(__func__) && read_embedded_register8(ALGO_CONFIG_REG, v);
}

bool UnitSTHS34PF80::write_algorithm_config(const uint8_t v)
{
    return guard_in_periodic(__func__) && write_embedded_register8(ALGO_CONFIG_REG, v);
}

//
bool UnitSTHS34PF80::write_odr(const sths34pf80::ODR odr)
{
    uint8_t v{};
    AmbientTemperatureAverage avg_t{};
    ObjectTemperatureAverage avg_tmos{};

    if (readAverageTrim(avg_t, avg_tmos) && readRegister8(CTRL1_REG, v, 0)) {
        if (odr > maximum_odr(avg_tmos)) {
            M5_LIB_LOGE("ODR cannot be set because the number of samples in avg_tmos is too large %u,%u", avg_tmos,
                        odr);
            return false;
        }
        // Keep other bits
        v = (v & ~0x0F) | m5::stl::to_underlying(odr);
        return writeRegister8(CTRL1_REG, v);
    }
    return false;
}

bool UnitSTHS34PF80::is_data_ready()
{
    constexpr uint8_t DRDY{0x04};
    uint8_t v{};
    return readRegister8(STATUS_REG, v, 0) && (v & DRDY);
}

bool UnitSTHS34PF80::read_measurement(sths34pf80::Data& d, const bool full)
{
    return readRegister(TOBJECT_L_REG, d.raw.data(), 4, 0) &&
           (full ? (readRegister(TOBJ_COMP_L_REG, d.raw.data() + 4, 8, 0) &&
                    readRegister8(FUNC_STATUS_REG, d.raw[12], 0))
                 : readRegister(TOBJ_COMP_L_REG, d.raw.data() + 4, 2, 0));
}

bool UnitSTHS34PF80::read_embedded_register(const uint8_t ereg, uint8_t* rbuf, const uint32_t len)
{
    auto count = len;

    // To Power-down mode
    if (!(inPeriodic() ? stopPeriodicMeasurement() : write_odr(ODR::PowerDown))) {
        return false;
    }
    // Enable function register and page to read
    if (writeRegister8(CTRL2_REG, ENABLE_FUNC_CFG_ACCESS)) {
        if (writeRegister8(PAGE_RW_REG, FUNC_CFG_READ)) {
            // Repeat reading
            uint8_t cur = ereg;
            while (count) {
                uint8_t v{};
                if (!writeRegister8(FUNC_CFG_ADDR_REG, cur++) || !readRegister8(FUNC_CFG_DATA_REG, v, 0)) {
                    M5_LIB_LOGE("Failed to read %u", len - count);
                    break;
                }
                *rbuf++ = v;
                --count;
            }
            return writeRegister8(PAGE_RW_REG, 0x00) && writeRegister8(CTRL2_REG, 0x00) && (count == 0);
        }
        return writeRegister8(CTRL2_REG, 0x00) && false;
    }
    return false;
}

bool UnitSTHS34PF80::write_embedded_register(const uint8_t ereg, const uint8_t* buf, const uint32_t len)
{
    auto count = len;

    // To Power-down mode
    if (!(inPeriodic() ? stopPeriodicMeasurement() : write_odr(ODR::PowerDown))) {
        return false;
    }
    // Enable function register and page to write
    if (writeRegister8(CTRL2_REG, ENABLE_FUNC_CFG_ACCESS)) {
        if (writeRegister8(PAGE_RW_REG, FUNC_CFG_WRITE) && writeRegister8(FUNC_CFG_ADDR_REG, ereg)) {
            // Repeat writing
            while (count) {
                if (!writeRegister8(FUNC_CFG_DATA_REG, *buf++)) {
                    M5_LIB_LOGE("Failed to write %u", len - count);
                    break;
                }
                --count;
            }
            return writeRegister8(PAGE_RW_REG, 0x00) && writeRegister8(CTRL2_REG, 0x00) && (count == 0);
        }
        return writeRegister8(CTRL2_REG, 0x00) && false;
    }
    return false;
}

bool UnitSTHS34PF80::guard_in_periodic(const char* fname)
{
    if (inPeriodic()) {
        M5_LIB_LOGD("%s() Periodic measurements are running", fname ? fname : "");
        return false;
    }
    return true;
}

}  // namespace unit
}  // namespace m5
