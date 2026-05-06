/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_IR.cpp
  @brief IR remote control unit for M5UnitUnified
*/
#include "unit_IR.hpp"
#include <M5Utility.hpp>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <esp_heap_caps.h>

#if defined(M5_UNIT_UNIFIED_USING_RMT_V2)
#include <driver/rmt_tx.h>  // includes rmt_common.h (rmt_apply_carrier)
#include <esp_private/esp_clk.h>
#else
#include <driver/rmt.h>
#include <esp32/clk.h>
#endif

using namespace m5::utility::mmh3;
using namespace m5::unit::types;
using namespace m5::unit::gpio;

namespace m5 {
namespace unit {

const char UnitIR::name[] = "UnitIR";
const types::uid_t UnitIR::uid{"UnitIR"_mmh3};
const types::attr_t UnitIR::attr{attribute::AccessGPIO};

bool UnitIR::begin()
{
    // Codec timing constants assume 1us per RMT tick. Reject other resolutions.
    if (_cfg.tick_ns != 1000) {
        M5_LIB_LOGE("tick_ns must be 1000 (1us); got %u", _cfg.tick_ns);
        return false;
    }

    auto aptr = adapter();
    if (!aptr) {
        M5_LIB_LOGE("Adapter is null");
        return false;
    }

    auto* ad = asAdapter<AdapterGPIO>(Adapter::Type::GPIO);
    if (!ad) {
        M5_LIB_LOGE("Not a GPIO adapter");
        return false;
    }

    bool tx_valid = (static_cast<int>(ad->tx_pin()) >= 0);
    bool rx_valid = (static_cast<int>(ad->rx_pin()) >= 0);

    if (!tx_valid && !rx_valid) {
        M5_LIB_LOGE("Neither TX nor RX pin configured");
        return false;
    }

    // Determine RMT mode based on valid pins
    Mode mode;
    if (tx_valid && rx_valid) {
        mode = Mode::RmtRXTX;
    } else if (tx_valid) {
        mode = Mode::RmtTX;
    } else {
        mode = Mode::RmtRX;
    }

    // Build adapter config
    adapter_config_t cfg{};
    cfg.mode = mode;

    // TX config
    if (tx_valid) {
        cfg.tx.tick_ns             = _cfg.tick_ns;
        cfg.tx.mem_blocks          = 1;
        cfg.tx.idle_output_enabled = true;
        cfg.tx.idle_level_high     = false;
        cfg.tx.with_dma            = false;
        cfg.tx.loop_enabled        = false;
    }

    // RX config
    if (rx_valid) {
        // Allocate RX buffer (4-byte aligned for RMT v2)
        uint16_t buf_bytes = (_cfg.rx_ring_buffer_size + 3) & ~3;
        auto* rx_buf       = static_cast<uint8_t*>(heap_caps_aligned_alloc(4, buf_bytes, MALLOC_CAP_8BIT));
        if (!rx_buf) {
            M5_LIB_LOGE("Failed to allocate rx buffer (%u bytes)", buf_bytes);
            return false;
        }
        _rx_buffer.reset(rx_buf);
        _rx_buffer_size = buf_bytes;

        cfg.rx.tick_ns = _cfg.tick_ns;
#if defined(M5_UNIT_UNIFIED_USING_RMT_V2)
        cfg.rx.mem_blocks = 2;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
        cfg.rx.mem_blocks = 1;
#else
        cfg.rx.mem_blocks = 4;
#endif
        cfg.rx.ring_buffer_size       = buf_bytes;
        cfg.rx.filter_enabled         = true;
        cfg.rx.filter_ticks_threshold = _cfg.rx_filter_threshold;
        cfg.rx.idle_ticks_threshold   = _cfg.rx_idle_threshold;
    }

    if (!ad->begin(cfg)) {
        M5_LIB_LOGE("Failed to begin AdapterGPIO");
        return false;
    }

    _has_tx = tx_valid;
    _has_rx = rx_valid;

    // Apply carrier modulation for TX
    if (_has_tx) {
        if (!apply_carrier()) {
            M5_LIB_LOGW("Failed to apply carrier, TX may not work correctly");
        }
    }

    M5_LIB_LOGI("UnitIR begin: TX=%s RX=%s codec=%u", _has_tx ? "yes" : "no", _has_rx ? "yes" : "no",
                static_cast<uint8_t>(_codec->type()));

    return true;
}

void UnitIR::update(const bool force)
{
    _updated        = false;
    _rx_available   = false;
    _raw_items      = nullptr;
    _raw_item_count = 0;

    if (!_has_rx) {
        return;
    }

    if (read_rx()) {
        _updated      = true;
        _rx_available = true;
        _latest       = m5::utility::millis();
    }
}

bool UnitIR::send(uint16_t address, uint16_t command, uint8_t frames)
{
    if (!_has_tx) {
        M5_LIB_LOGE("TX not available");
        return false;
    }

    // Update carrier frequency for current codec (may have changed via setCodec)
    apply_carrier();

    // Encode a single frame
    auto single = _codec->encode(address, command, false);
    if (single.empty()) {
        M5_LIB_LOGE("Encode failed");
        return false;
    }

    uint8_t n = frames ? frames : _codec->minFrames();
    if (n <= 1) {
        return sendRaw(single.data(), single.size());
    }

    // Build N-frame burst: replicate the frame, inserting frameGapUs() between copies.
    // The gap is applied by overwriting the last RMT item's duration1/level1 of every
    // non-final frame copy (that slot normally holds the post-stop idle space).
    uint16_t gap = _codec->frameGapUs();
    ir::item_container_type burst;
    burst.reserve(single.size() * n);
    for (uint8_t i = 0; i < n; ++i) {
        for (const auto& it : single) {
            burst.push_back(it);
        }
        if (i < n - 1 && !burst.empty() && gap > 0) {
            burst.back().duration1 = gap;
            burst.back().level1    = 0;
        }
    }
    return sendRaw(burst.data(), burst.size());
}

bool UnitIR::sendRaw(const gpio::m5_rmt_item_t* items, uint32_t num)
{
    if (!_has_tx || !items || num == 0) {
        return false;
    }

    // Estimate timeout: sum of all durations + margin
    uint32_t total_us = 0;
    for (uint32_t i = 0; i < num; ++i) {
        total_us += items[i].duration0 + items[i].duration1;
    }
    total_us += 10000;  // 10ms margin
    TickType_t wait = pdMS_TO_TICKS((total_us + 999) / 1000);

    return writeWithTransaction(reinterpret_cast<const uint8_t*>(items), num * sizeof(m5_rmt_item_t), wait) ==
           m5::hal::error::error_t::OK;
}

void UnitIR::flush()
{
    _rx_available   = false;
    _latest_result  = ir::DecodeResult{};
    _raw_items      = nullptr;
    _raw_item_count = 0;
}

bool UnitIR::read_rx()
{
    if (!_rx_buffer) {
        return false;
    }

    auto buffer_size = _rx_buffer_size;
    auto* buff       = _rx_buffer.get();

    if (readWithTransaction(buff, buffer_size) != m5::hal::error::error_t::OK) {
        return false;
    }

    // First 2 bytes = data length
    uint16_t len{};
    std::memcpy(&len, buff, sizeof(len));

    // Sanity: payload must fit and align on RMT item boundary
    if (static_cast<size_t>(len) + sizeof(len) > buffer_size) {
        M5_LIB_LOGW("rx len %u exceeds buffer %zu", len, buffer_size);
        return false;
    }
    if (len % sizeof(m5_rmt_item_t) != 0) {
        M5_LIB_LOGW("rx len %u not a multiple of %zu", len, sizeof(m5_rmt_item_t));
        return false;
    }

    uint16_t inum = len / sizeof(m5_rmt_item_t);
    if (inum < _cfg.rx_min_item_count) {
        return false;
    }

    // buff is 4-byte aligned (heap_caps_aligned_alloc); buff+2 is only 2-byte aligned but
    // ESP32/Xtensa and ESP32-C6/RISC-V both tolerate unaligned 32-bit word access for
    // RMT items, and in practice we only read/write 16-bit fields (duration0/1). Keep
    // as-is to avoid an extra copy. cppcheck/UBSan will flag this; intentional.
    auto* items = reinterpret_cast<m5::unit::gpio::m5_rmt_item_t*>(buff + 2);

    // Invert RX signal levels for active-LOW receivers (e.g. VS1838B)
    if (_cfg.rx_invert_level) {
        for (uint16_t i = 0; i < inum; ++i) {
            items[i].level0 = !items[i].level0;
            items[i].level1 = !items[i].level1;
        }
    }

    // Store raw items (pointer valid until next read_rx call)
    _raw_items      = items;
    _raw_item_count = inum;

    // Try decoding with current codec
    ir::DecodeResult result{};
    if (_codec->decode(items, inum, result)) {
        _latest_result = result;
        return true;
    }

    // Decode failed but raw items are still available for debugging
    _latest_result          = ir::DecodeResult{};
    _latest_result.protocol = ir::CodecType::Unknown;
    return true;
}

bool UnitIR::apply_carrier()
{
    auto* ad = asAdapter<AdapterGPIO>(Adapter::Type::GPIO);
    if (!ad) {
        return false;
    }

    uint32_t freq = _codec->carrierFrequencyHz();
    float duty    = _codec->carrierDuty();

    M5_LIB_LOGI("Applying carrier: %u Hz, duty %.2f", freq, duty);

#if defined(M5_UNIT_UNIFIED_USING_RMT_V2)
    // RMT v2: apply carrier config to TX channel handle
    auto handle = static_cast<rmt_channel_handle_t>(ad->impl()->rmtTxHandle());
    if (!handle) {
        M5_LIB_LOGE("No RMT TX handle");
        return false;
    }
    rmt_carrier_config_t carrier_cfg{};
    carrier_cfg.duty_cycle                = duty;
    carrier_cfg.frequency_hz              = freq;
    carrier_cfg.flags.polarity_active_low = false;
    carrier_cfg.flags.always_on           = false;
    return rmt_apply_carrier(handle, &carrier_cfg) == ESP_OK;
#else
    // RMT v1: set carrier via legacy API
    int ch = ad->impl()->rmtTxChannel();
    if (ch < 0 || ch >= RMT_CHANNEL_MAX) {
        M5_LIB_LOGE("Invalid RMT TX channel: %d", ch);
        return false;
    }

    // Calculate high/low level counts from APB clock
    uint32_t apb_hz = esp_clk_apb_freq();
    uint32_t period = apb_hz / freq;  // Total period in APB ticks
    uint16_t high = static_cast<uint16_t>(period * duty);
    uint16_t low = static_cast<uint16_t>(period - high);

    auto err = rmt_set_tx_carrier(static_cast<rmt_channel_t>(ch), true, high, low, RMT_CARRIER_LEVEL_HIGH);
    if (err != ESP_OK) {
        M5_LIB_LOGE("rmt_set_tx_carrier failed: %d", err);
        return false;
    }
    return true;
#endif
}

}  // namespace unit
}  // namespace m5
