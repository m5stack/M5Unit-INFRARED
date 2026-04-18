/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for UnitIR (U002) / Built-in IR
  Single-button menu:
  - Hold: Move to next menu item (Send -> Addr -> Cmd -> Proto -> Send ...)
  - Click: Execute
      Send  = transmit
      Addr  = randomize to a new valid value (protocol-dependent mask)
      Cmd   = randomize to a new valid value (protocol-dependent mask)
      Proto = cycle protocol. Selecting SIRC rotates variant SIRC12 -> 15 -> 20 on each visit.
  Mitsubishi has no address field; the ADDR row is hidden and skipped.
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedINFRARED.h>
#include <M5Utility.h>
#include <unit/ir/nec_codec.hpp>
#include <unit/ir/sirc_codec.hpp>
#include <unit/ir/rc5_codec.hpp>
#include <unit/ir/rc6_codec.hpp>
#include <unit/ir/panasonic_codec.hpp>
#include <unit/ir/mitsubishi_codec.hpp>

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::UnitIR unit;

LGFX_Sprite sprite;

// Protocol codecs (user-managed instances)
m5::unit::ir::NecCodec nec_codec;
// SIRC is initialized to SIRC20 so that the first visit to the SIRC menu
// rotates forward to SIRC12 (see setProtocol()).
m5::unit::ir::SircCodec sirc_codec{m5::unit::ir::SircCodec::Variant::SIRC20};
m5::unit::ir::Rc5Codec rc5_codec;
m5::unit::ir::Rc6Codec rc6_codec;
m5::unit::ir::PanasonicCodec panasonic_codec;
m5::unit::ir::MitsubishiCodec mitsubishi_codec;

// Menu
enum class Menu : uint8_t { Send = 0, Addr, Cmd, Proto, Count };
Menu current_menu{Menu::Send};

uint8_t protocol_index{};
uint16_t ir_address{0x00};
uint16_t ir_command{0x1F};
const char* last_status{""};

// Repeat tracking: if same protocol+addr+cmd as last send, auto-repeat
uint8_t last_protocol{0xFF};
uint16_t last_address{0xFFFF};
uint16_t last_command{0xFFFF};
uint16_t repeat_count{};

const char* protocol_names[] = {"NEC", "SIRC", "RC5", "RC6", "Panasonic", "Mitsubishi"};
const char* menu_labels[]    = {"SEND", "ADDR", "CMD", "PROTO"};

// Per-protocol valid bit-widths (match codec encode/decode expectations)
struct ProtocolRange {
    uint16_t addr_mask;
    uint16_t cmd_mask;
};
const ProtocolRange protocol_ranges[] = {
    {0x00FF, 0x00FF},  // NEC (8-bit addr, 8-bit cmd)
    {0x001F, 0x007F},  // SIRC12 (overridden by currentRange() per variant)
    {0x001F, 0x003F},  // RC5 (5-bit addr, 6-bit cmd; RC5X adds bit6)
    {0x00FF, 0x00FF},  // RC6 (8-bit addr, 8-bit cmd; Mode 0)
    {0xFFFF, 0xFFFF},  // Panasonic (16-bit addr, 16-bit cmd)
    {0x0000, 0xFFFF},  // Mitsubishi (no addr, 16-bit cmd)
};

constexpr uint8_t PROTO_SIRC = 1;

ProtocolRange currentRange()
{
    if (protocol_index == PROTO_SIRC) {
        switch (sirc_codec.variant()) {
            case m5::unit::ir::SircCodec::Variant::SIRC12:
                return {0x001F, 0x007F};  // 5-bit addr, 7-bit cmd
            case m5::unit::ir::SircCodec::Variant::SIRC15:
                return {0x00FF, 0x007F};  // 8-bit addr, 7-bit cmd
            case m5::unit::ir::SircCodec::Variant::SIRC20:
                return {0x1FFF, 0x007F};  // 5-bit addr + 8-bit ext, 7-bit cmd
        }
    }
    return protocol_ranges[protocol_index];
}

const char* currentProtoLabel()
{
    if (protocol_index == PROTO_SIRC) {
        switch (sirc_codec.variant()) {
            case m5::unit::ir::SircCodec::Variant::SIRC12:
                return "SIRC12";
            case m5::unit::ir::SircCodec::Variant::SIRC15:
                return "SIRC15";
            case m5::unit::ir::SircCodec::Variant::SIRC20:
                return "SIRC20";
        }
    }
    return protocol_names[protocol_index];
}

bool isAddrMenuHidden()
{
    return currentRange().addr_mask == 0;
}

void setProtocol(uint8_t idx)
{
    protocol_index = idx % 6;
    switch (protocol_index) {
        case 0:
            unit.setCodec(nec_codec);
            break;
        case 1: {
            // Rotate variant on every visit: SIRC12 -> SIRC15 -> SIRC20 -> SIRC12 ...
            using V   = m5::unit::ir::SircCodec::Variant;
            V current = sirc_codec.variant();
            V next    = (current == V::SIRC12) ? V::SIRC15 : (current == V::SIRC15) ? V::SIRC20 : V::SIRC12;
            sirc_codec.setVariant(next);
            unit.setCodec(sirc_codec);
            break;
        }
        case 2:
            unit.setCodec(rc5_codec);
            break;
        case 3:
            unit.setCodec(rc6_codec);
            break;
        case 4:
            unit.setCodec(panasonic_codec);
            break;
        case 5:
            unit.setCodec(mitsubishi_codec);
            break;
    }
    // Clamp current values to the new protocol's valid bit-width
    auto range = currentRange();
    ir_address &= range.addr_mask;
    ir_command &= range.cmd_mask;
    // If the hidden Addr row is currently selected, advance past it
    if (isAddrMenuHidden() && current_menu == Menu::Addr) {
        current_menu = Menu::Cmd;
    }
}

void updateDisplay()
{
    auto w = sprite.width();
    auto h = sprite.height();

    sprite.fillSprite(0);
    sprite.setTextColor(1);
    sprite.setTextDatum(middle_center);

    float scale_large = (w >= 480) ? 3.0f : (w >= 200) ? 2.0f : 1.5f;
    float scale_small = (w >= 480) ? 2.0f : (w >= 200) ? 1.5f : 1.0f;

    int32_t cx     = w / 2;
    int32_t row_h  = h / 6;
    int32_t base_y = row_h;

    // Menu rows
    for (uint8_t i = 0; i < static_cast<uint8_t>(Menu::Count); ++i) {
        // Hide Addr row for protocols with no address (e.g., Mitsubishi)
        if (static_cast<Menu>(i) == Menu::Addr && isAddrMenuHidden()) {
            continue;
        }
        bool selected = (i == static_cast<uint8_t>(current_menu));
        sprite.setTextSize(selected ? scale_large : scale_small);

        char buf[32];
        switch (static_cast<Menu>(i)) {
            case Menu::Send:
                snprintf(buf, sizeof(buf), "%sSEND", selected ? "> " : "  ");
                break;
            case Menu::Addr:
                snprintf(buf, sizeof(buf), "%sA:0x%04X", selected ? "> " : "  ", ir_address);
                break;
            case Menu::Cmd:
                snprintf(buf, sizeof(buf), "%sC:0x%04X", selected ? "> " : "  ", ir_command);
                break;
            case Menu::Proto: {
                char tgl[8] = "";
                if (protocol_index == 2) {
                    snprintf(tgl, sizeof(tgl), " T:%d", rc5_codec.toggle() ? 1 : 0);
                } else if (protocol_index == 3) {
                    snprintf(tgl, sizeof(tgl), " T:%d", rc6_codec.toggle() ? 1 : 0);
                }
                snprintf(buf, sizeof(buf), "%s%s%s", selected ? "> " : "  ", currentProtoLabel(), tgl);
                break;
            }
            default:
                break;
        }
        sprite.drawString(buf, cx, base_y + row_h * i);
    }

    // RPT count (display only, not a menu item)
    if (repeat_count > 0) {
        sprite.setTextSize(scale_small);
        char rpt_buf[16];
        snprintf(rpt_buf, sizeof(rpt_buf), "RPT:%u", repeat_count);
        sprite.drawString(rpt_buf, cx, base_y + row_h * static_cast<uint8_t>(Menu::Count));
    }

    // Status line
    sprite.setTextSize(scale_small);
    sprite.drawString(last_status, cx, h - row_h / 2);

    lcd.startWrite();
    sprite.pushSprite(&lcd, 0, 0);
    lcd.endWrite();
}

void logStatus()
{
    M5.Log.printf("[%s] A:0x%04X C:0x%04X  <menu:%s>\n", currentProtoLabel(), ir_address, ir_command,
                  menu_labels[static_cast<uint8_t>(current_menu)]);
}

#if defined(USING_BUILTIN_IR)
int8_t get_builtin_ir_tx_pin()
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
        case m5::board_t::board_M5AtomEchoS3R:
            return 47;
        case m5::board_t::board_M5NanoC6:
            return 3;
        case m5::board_t::board_M5Cardputer:
        case m5::board_t::board_M5CardputerADV:
            return 44;
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

    // SendIR is TX-only: passing pin_rx = -1 avoids self-reception / RX buffer overflow
    // on devices where TX and RX share the same Unit IR (VS1838B next to the IR LED).
    int8_t pin_rx = -1;
#if defined(USING_BUILTIN_IR)
    int8_t pin_tx = get_builtin_ir_tx_pin();
    if (pin_tx < 0) {
        M5_LOGE("No built-in IR TX on this board");
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }
    // StickS3: EXT_5V must be enabled for IR TX
    if (M5.getBoard() == m5::board_t::board_M5StickS3) {
        M5.Power.setExtOutput(true);
    }
    M5_LOGI("Built-in IR: TX=%d", pin_tx);
#else
    // Unit IR (U002): Port B preferred, fallback to Port A
    int8_t pin_tx = M5.getPin(m5::pin_name_t::port_b_out);
    if (pin_tx < 0) {
        M5_LOGW("PortB is not available, using PortA");
        pin_tx = M5.getPin(m5::pin_name_t::port_a_pin2);
    }
    M5_LOGI("UnitIR GPIO: TX:%d", pin_tx);
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

    last_status = "Hold:Menu Click:Exec";
    logStatus();
    updateDisplay();
}

void loop()
{
    M5.update();
    Units.update();

    bool need_update = false;

    if (M5.BtnA.wasHold()) {
        // Move to next menu item (skip Addr when the protocol has no address field)
        do {
            current_menu =
                static_cast<Menu>((static_cast<uint8_t>(current_menu) + 1) % static_cast<uint8_t>(Menu::Count));
        } while (current_menu == Menu::Addr && isAddrMenuHidden());
        M5.Log.printf("Menu -> %s\n", menu_labels[static_cast<uint8_t>(current_menu)]);
        last_status = menu_labels[static_cast<uint8_t>(current_menu)];
        need_update = true;
    }

    if (M5.BtnA.wasClicked()) {
        switch (current_menu) {
            case Menu::Send: {
                // Log encode/burst info on every press (initial and repeat)
                {
                    auto enc_single = unit.codec().encode(ir_address, ir_command, false);
                    auto enc_repeat = unit.codec().encode(ir_address, ir_command, true);
                    uint8_t mf      = unit.codec().minFrames();
                    M5.Log.printf("encode single=%u repeat=%u minFrames=%u total_tx=%u gapUs=%u\n",
                                  (unsigned)enc_single.size(), (unsigned)enc_repeat.size(), (unsigned)mf,
                                  (unsigned)(enc_single.size() * mf), (unsigned)unit.codec().frameGapUs());
                }

                // Auto-repeat if same protocol+addr+cmd as last send
                bool is_repeat =
                    (protocol_index == last_protocol && ir_address == last_address && ir_command == last_command);
                if (is_repeat && protocol_index == 0 /* NEC */) {
                    ++repeat_count;
                    M5.Log.printf("Repeat[%u] [%s] A:0x%04X C:0x%04X ...", repeat_count, currentProtoLabel(),
                                  ir_address, ir_command);
                    // Send repeat frame only
                    auto items = unit.codec().encode(ir_address, ir_command, true);
                    bool ok    = !items.empty() && unit.sendRaw(items.data(), items.size());
                    M5.Log.printf(ok ? " OK\n" : " FAIL\n");
                    last_status = ok ? "Repeat OK" : "FAIL";
                } else {
                    repeat_count  = 0;
                    last_protocol = protocol_index;
                    last_address  = ir_address;
                    last_command  = ir_command;
                    // Log the toggle that will be transmitted THIS send. The LCD shows
                    // the same value until the post-send flipToggle() advances it to
                    // the next value (i.e. "what will be sent on the NEXT press").
                    char tgl_log[12] = "";
                    if (protocol_index == 2) {
                        snprintf(tgl_log, sizeof(tgl_log), " T:%d", rc5_codec.toggle() ? 1 : 0);
                    } else if (protocol_index == 3) {
                        snprintf(tgl_log, sizeof(tgl_log), " T:%d", rc6_codec.toggle() ? 1 : 0);
                    }
                    M5.Log.printf("Sending [%s] A:0x%04X C:0x%04X%s ...", currentProtoLabel(), ir_address, ir_command,
                                  tgl_log);
                    if (unit.send(ir_address, ir_command)) {
                        M5.Log.printf(" OK\n");
                        last_status = "Sent OK";
                        // RC5 / RC6 flip the toggle bit AFTER send so repeats are
                        // distinguishable; the flipped value is "what the next new
                        // press will transmit" — this matches the LCD display.
                        if (protocol_index == 2) {
                            rc5_codec.flipToggle();
                        } else if (protocol_index == 3) {
                            rc6_codec.flipToggle();
                        }
                    } else {
                        M5.Log.printf(" FAIL\n");
                        last_status = "FAIL";
                    }
                }
                break;
            }
            case Menu::Addr:
                ir_address = esp_random() & currentRange().addr_mask;
                M5.Log.printf("Addr -> 0x%04X\n", ir_address);
                last_status = "Addr?";
                break;
            case Menu::Cmd:
                ir_command = esp_random() & currentRange().cmd_mask;
                M5.Log.printf("Cmd -> 0x%04X\n", ir_command);
                last_status = "Cmd?";
                break;
            case Menu::Proto:
                setProtocol(protocol_index + 1);
                M5.Log.printf("Proto -> %s\n", currentProtoLabel());
                last_status = currentProtoLabel();
                break;
            default:
                break;
        }
        logStatus();
        need_update = true;
    }

    if (need_update) {
        updateDisplay();
    }
}
