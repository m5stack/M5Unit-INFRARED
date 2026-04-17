/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for UnitIR (U002) / Built-in IR
  Receives IR remote signals, displays waveform and decoded info on screen,
  and prints to Serial.

  Note: StickS3 built-in IR RX may pick up noise when powered via USB.
  Battery-only operation reduces noise significantly.
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedINFRARED.h>
#include <M5Utility.h>
#include <unit/ir/auto_detect_codec.hpp>
#include <numeric>

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::UnitIR unit;

LGFX_Sprite sprite;

const char* codecTypeName(m5::unit::ir::CodecType t)
{
    switch (t) {
        case m5::unit::ir::CodecType::NEC:
            return "NEC";
        case m5::unit::ir::CodecType::SIRC:
            return "SIRC";
        case m5::unit::ir::CodecType::RC5:
            return "RC5";
        case m5::unit::ir::CodecType::RC6:
            return "RC6";
        case m5::unit::ir::CodecType::Panasonic:
            return "Panasonic";
        case m5::unit::ir::CodecType::Mitsubishi:
            return "Mitsubishi";
        case m5::unit::ir::CodecType::Raw:
            return "Raw";
        default:
            return "Unknown";
    }
}

void drawWaveform(const m5::unit::ir::item_container_type& items, int32_t x0, int32_t y0, int32_t w, int32_t h)
{
    if (items.empty() || w < 2 || h < 4) {
        return;
    }

    // Calculate total duration for scaling
    const uint32_t total_us = std::accumulate(
        items.begin(), items.end(), uint32_t{0},
        [](uint32_t acc, const m5::unit::gpio::m5_rmt_item_t& it) { return acc + it.duration0 + it.duration1; });
    if (total_us == 0) {
        return;
    }

    int32_t y_high = y0 + 2;
    int32_t y_low  = y0 + h - 3;
    int32_t prev_x = x0;
    bool prev_high = false;

    // Baseline (low)
    sprite.drawFastHLine(x0, y_low, w, 1);

    for (const auto& item : items) {
        // Mark (carrier ON = high)
        int32_t mark_px = static_cast<int32_t>(static_cast<uint64_t>(item.duration0) * w / total_us);
        if (mark_px < 1) {
            mark_px = 1;
        }

        // Transition: low -> high
        if (!prev_high && prev_x >= x0) {
            sprite.drawFastVLine(prev_x, y_high, y_low - y_high, 1);
        }
        // High line
        sprite.drawFastHLine(prev_x, y_high, mark_px, 1);
        prev_x += mark_px;
        prev_high = true;

        // Space (carrier OFF = low)
        if (item.duration1 > 0) {
            int32_t space_px = static_cast<int32_t>(static_cast<uint64_t>(item.duration1) * w / total_us);
            if (space_px < 1) {
                space_px = 1;
            }

            // Transition: high -> low
            sprite.drawFastVLine(prev_x, y_high, y_low - y_high, 1);
            // Low line
            sprite.drawFastHLine(prev_x, y_low, space_px, 1);
            prev_x += space_px;
            prev_high = false;
        }

        if (prev_x >= x0 + w) {
            break;
        }
    }
}

void updateDisplay(const m5::unit::ir::DecodeResult* result, const m5::unit::ir::item_container_type* items)
{
    auto w = sprite.width();
    auto h = sprite.height();

    sprite.fillSprite(0);
    sprite.setTextColor(1);

    // Adaptive text size
    float text_scale = (w >= 480) ? 2.0f : (w >= 200) ? 1.5f : 1.0f;
    sprite.setTextSize(text_scale);

    if (result) {
        auto line_h = sprite.fontHeight() + 2;
        int32_t ty  = 2;

        char buf[32];
        snprintf(buf, sizeof(buf), "P:%s", codecTypeName(result->protocol));
        sprite.drawString(buf, 2, ty);
        ty += line_h;
        snprintf(buf, sizeof(buf), "A:0x%04X", result->address);
        sprite.drawString(buf, 2, ty);
        ty += line_h;
        snprintf(buf, sizeof(buf), "C:0x%04X", result->command);
        sprite.drawString(buf, 2, ty);
        ty += line_h;
        // RC5/RC6 show the toggle bit alongside the bit count
        bool has_toggle =
            (result->protocol == m5::unit::ir::CodecType::RC5 || result->protocol == m5::unit::ir::CodecType::RC6);
        if (has_toggle) {
            snprintf(buf, sizeof(buf), "B:%u T:%d%s", result->bits, result->toggle ? 1 : 0,
                     result->repeat ? " RPT" : "");
        } else {
            snprintf(buf, sizeof(buf), "B:%u%s", result->bits, result->repeat ? " RPT" : "");
        }
        sprite.drawString(buf, 2, ty);
        ty += line_h + 2;

        // Waveform area
        if (items && !items->empty()) {
            sprite.drawFastHLine(0, ty, w, 1);
            ty += 2;
            drawWaveform(*items, 2, ty, w - 4, h - ty - 2);
        }
    } else {
        sprite.setTextDatum(middle_center);
        sprite.drawString("Waiting for IR...", w / 2, h / 2);
        sprite.setTextDatum(top_left);
    }

    lcd.startWrite();
    sprite.pushSprite(&lcd, 0, 0);
    lcd.endWrite();
}

#if defined(USING_BUILTIN_IR)
// Get built-in IR RX pin for the current board
int8_t get_builtin_ir_rx_pin()
{
    switch (M5.getBoard()) {
        case m5::board_t::board_M5StickS3:
            return 42;
        default:
            return -1;
    }
}
#endif

}  // namespace

void setup()
{
    M5.begin();
    M5.setTouchButtonHeightByRatio(100);

    // Landscape
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

    // Sprite: 1-bit depth, minimal memory
    sprite.setColorDepth(1);
    sprite.setPsram(false);
    if (!sprite.createSprite(lcd.width(), lcd.height())) {
        sprite.setPsram(true);
        sprite.createSprite(lcd.width(), lcd.height());
    }

    // PlotToSerial is RX-only: passing pin_tx = -1 keeps the IR LED unclaimed
    // and prevents any accidental TX activity from this example.
    int8_t pin_tx = -1;
#if defined(USING_BUILTIN_IR)
    int8_t pin_rx = get_builtin_ir_rx_pin();
    if (pin_rx < 0) {
        M5_LOGE("No built-in IR RX on this board");
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }
    // StickS3: built-in IR RX needs disable speaker, internal pullup, and EXT_5V
    if (M5.getBoard() == m5::board_t::board_M5StickS3) {
        M5.Speaker.end();
        M5.Power.setExtOutput(true);
        gpio_set_pull_mode((gpio_num_t)pin_rx, GPIO_PULLUP_ONLY);
    }
    M5_LOGI("Built-in IR: RX=%d", pin_rx);
#else
    // Unit IR (U002): Port B preferred, fallback to Port A
    int8_t pin_rx = M5.getPin(m5::pin_name_t::port_b_in);
    if (pin_rx < 0) {
        M5_LOGW("PortB is not available, using PortA");
        pin_rx = M5.getPin(m5::pin_name_t::port_a_pin1);
    }
    M5_LOGI("UnitIR GPIO: RX:%d", pin_rx);
#endif

    if (!Units.add(unit, pin_rx, pin_tx) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

    M5_LOGI("M5UnitUnified initialized");
    M5_LOGI("%s", Units.debugInfo().c_str());

    updateDisplay(nullptr, nullptr);
}

void loop()
{
    M5.update();
    Units.update();

    if (unit.available()) {
        auto& r = unit.latest();

        // Serial output: decoded result
        M5.Log.printf(">Protocol:%s\n>Address:0x%04X\n>Command:0x%04X\n>Bits:%u\n>Repeat:%d\n>Raw:0x%08X%08X\n",
                      codecTypeName(r.protocol), r.address, r.command, r.bits, r.repeat ? 1 : 0,
                      static_cast<uint32_t>(r.raw >> 32), static_cast<uint32_t>(r.raw & 0xFFFFFFFF));
        // RC5/RC6 carry a toggle bit in each frame (flipped on every new key press)
        if (r.protocol == m5::unit::ir::CodecType::RC5 || r.protocol == m5::unit::ir::CodecType::RC6) {
            M5.Log.printf(">Toggle:%d\n", r.toggle ? 1 : 0);
        }

        // Raw RMT dump for debugging (especially RC5/RC6)
        auto* raw    = unit.rawItems();
        uint32_t cnt = unit.rawItemCount();
        if (raw && cnt > 0) {
            M5.Log.printf("RMT[%u]:", cnt);
            for (uint32_t i = 0; i < cnt; ++i) {
                M5.Log.printf(" {%u,%u,%u,%u}", raw[i].duration0, raw[i].level0, raw[i].duration1, raw[i].level1);
            }
            M5.Log.printf("\n");
        }

        // Unknown: log only, skip LCD update
        if (r.protocol == m5::unit::ir::CodecType::Unknown) {
            return;
        }

        // Waveform display: use the actual RX capture (covers SIRC15/20, Panasonic
        // fields that encode() cannot round-trip faithfully from (address, command)).
        m5::unit::ir::item_container_type items;
        if (raw && cnt > 0) {
            items.assign(raw, raw + cnt);
        }

        updateDisplay(&r, &items);
    }
}
