/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Simple display example using M5UnitUnified for UnitTmosPIR
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedINFRARED.h>
#include <M5Utility.h>
#include <M5HAL.hpp>  // For NessoN1
#include <cassert>
#include <cmath>

using namespace m5::unit::sths34pf80;

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::UnitTmosPIR unit;

// Dark theme palette indices
enum Pal : uint8_t {
    BG        = 0,   // #141420
    PANEL_BG  = 1,   // #1E1E32
    BORDER    = 2,   // #3A3A50
    LABEL     = 3,   // #707888
    VALUE     = 4,   // #E0E8F0
    TEMP_OBJ  = 5,   // #FFB040 amber
    TEMP_AMB  = 6,   // #40C0FF cyan
    TEMP_COMP = 7,   // #C0E040 lime
    DET_PRES  = 8,   // #00E878 green
    DET_MOT   = 9,   // #4088FF blue
    DET_SHOCK = 10,  // #FF5050 red
    BAR_BG    = 11,  // #202038
    MARKER_TH = 12,  // #FFE040 yellow
    MARKER_HY = 13,  // #FF8040 orange
    IND_OFF   = 14,  // #2A2A40
    WHITE     = 15,  // #FFFFFF
};

// Animated bar for signed 16-bit detection values
struct BarI16 {
    BarI16(const uint32_t th, const uint32_t hy, const uint8_t color_idx, const int32_t disp_max = 5000)
        : _thres(th), _hyst(hy), _color(color_idx), _disp_max(disp_max)
    {
    }

    void to(const int16_t val)
    {
        _to      = val;
        _counter = 8;
        _add     = (_to - _val) / _counter;
    }

    void update()
    {
        if (_to != _val) {
            if (_counter--) {
                _val += _add;
            } else {
                _val = _to;
            }
        }
        if (_flash > 0) {
            --_flash;
        }
    }

    void flash()
    {
        _flash = 6;
    }

    void render(LovyanGFX* dst, const int32_t x, const int32_t y, const uint32_t w, const uint32_t h)
    {
        uint32_t r = std::max((uint32_t)2, h / 6);

        // Bar background (subtle brighten on flash)
        uint8_t bg = (_flash > 0) ? (uint8_t)Pal::BORDER : (uint8_t)Pal::BAR_BG;
        dst->fillRoundRect(x, y, w, h, r, bg);

        // Active bar from center
        int32_t center = w / 2;
        float ratio    = (float)_val / (float)_disp_max;
        int32_t bar_px = (int32_t)(center * ratio);

        if (bar_px > 0) {
            int32_t bw = std::min(bar_px, (int32_t)center - 1);
            dst->fillRect(x + center, y + 1, bw, h - 2, _color);
        } else if (bar_px < 0) {
            int32_t bw = std::min(-bar_px, (int32_t)center - 1);
            dst->fillRect(x + center - bw, y + 1, bw, h - 2, _color);
        }

        // Center line (origin = 0)
        dst->drawFastVLine(x + center, y, h, Pal::BORDER);

        // Threshold marker (triangle down from top)
        int32_t tri_sz = std::max((int32_t)2, (int32_t)(h / 5));
        int32_t th_x   = x + center + (int32_t)(center * ((float)_thres / (float)_disp_max));
        th_x           = std::max(x + tri_sz, std::min(th_x, (int32_t)(x + (int32_t)w - tri_sz)));
        dst->fillTriangle(th_x - tri_sz, y, th_x + tri_sz, y, th_x, y + tri_sz, Pal::MARKER_TH);

        // Hysteresis marker (triangle up from bottom)
        int32_t hy_val = (int32_t)_thres - (int32_t)_hyst;
        int32_t hy_x   = x + center + (int32_t)(center * ((float)hy_val / (float)_disp_max));
        hy_x           = std::max(x + tri_sz, std::min(hy_x, (int32_t)(x + (int32_t)w - tri_sz)));
        int32_t bot    = y + h - 1;
        dst->fillTriangle(hy_x - tri_sz, bot, hy_x + tri_sz, bot, hy_x, bot - tri_sz, Pal::MARKER_HY);
    }

    uint32_t _thres{}, _hyst{};
    uint8_t _color{};
    int32_t _disp_max{};
    float _val{}, _to{}, _add{};
    uint32_t _counter{};
    uint8_t _flash{};
};

// Dashboard view
struct View {
    View(const uint32_t w, const uint32_t h, const uint16_t thres_p, const uint8_t hys_p, const uint16_t thres_m,
         const uint8_t hys_m, const uint16_t thres_a, const uint8_t hys_a)
        : _disp_w(w), _disp_h(h)
    {
        // Cap sprite size to fit in internal SRAM (~60KB at 4-bit)
        constexpr uint32_t MAX_PIXELS = 120000;
        _wid                          = w;
        _hgt                          = h;
        if (_wid * _hgt > MAX_PIXELS) {
            float scale = std::sqrt((float)MAX_PIXELS / (float)(_wid * _hgt));
            _wid        = (uint32_t)(_wid * scale);
            _hgt        = (uint32_t)(_hgt * scale);
        }
        _small = (_wid < 200);

        constexpr RGBColor color_table[16] = {
            RGBColor(0x14, 0x14, 0x20),  // 0  BG
            RGBColor(0x1E, 0x1E, 0x32),  // 1  PANEL_BG
            RGBColor(0x3A, 0x3A, 0x50),  // 2  BORDER
            RGBColor(0x70, 0x78, 0x88),  // 3  LABEL
            RGBColor(0xE0, 0xE8, 0xF0),  // 4  VALUE
            RGBColor(0xFF, 0xB0, 0x40),  // 5  TEMP_OBJ
            RGBColor(0x40, 0xC0, 0xFF),  // 6  TEMP_AMB
            RGBColor(0xC0, 0xE0, 0x40),  // 7  TEMP_COMP
            RGBColor(0x00, 0xE8, 0x78),  // 8  DET_PRES
            RGBColor(0x40, 0x88, 0xFF),  // 9  DET_MOT
            RGBColor(0xFF, 0x50, 0x50),  // 10 DET_SHOCK
            RGBColor(0x20, 0x20, 0x38),  // 11 BAR_BG
            RGBColor(0xFF, 0xE0, 0x40),  // 12 MARKER_TH
            RGBColor(0xFF, 0x80, 0x40),  // 13 MARKER_HY
            RGBColor(0x2A, 0x2A, 0x40),  // 14 IND_OFF
            RGBColor(0xFF, 0xFF, 0xFF),  // 15 WHITE
        };

        _sprite.setPsram(false);
        _sprite.setColorDepth(4);  // 16 colors
        auto r = _sprite.createSprite(_wid, _hgt);
        assert(r);
        auto pal = _sprite.getPalette();
        for (auto&& c : color_table) {
            *pal++ = c;
        }

        // Font selection based on screen size
        if (_hgt >= 240) {
            _sprite.setFont(&fonts::FreeSans9pt7b);
        } else if (_wid >= 240) {
            _sprite.setFont(&fonts::AsciiFont8x16);
        } else {
            _sprite.setFont(&fonts::Font0);
        }
        _fhgt = _sprite.fontHeight();

        // Layout margins
        _margin  = std::max((uint32_t)2, _wid / 80);
        _panel_r = std::max((uint32_t)3, _wid / 60);
        _inner_w = _wid - _margin * 2;

        // Temperature section: ~35%
        _temp_y = _margin;
        _temp_h = _small ? (_hgt * 28 / 100) : (_hgt * 35 / 100);

        // Detection section + status strip fill the rest
        _det_y          = _temp_y + _temp_h + _margin;
        uint32_t remain = _hgt - _det_y - _margin;
        if (_small) {
            _det_h    = remain;
            _status_h = 0;
        } else {
            _status_h = std::max(_fhgt + 8, _hgt * 13 / 100);
            _det_h    = remain - _status_h - _margin;
        }
        _status_y = _det_y + _det_h + _margin;

        // Detection content layout
        uint32_t det_header    = _fhgt + 4;
        uint32_t det_content_h = _det_h - det_header - _margin;
        _det_row_h             = det_content_h / 3;
        _det_content_y         = _det_y + det_header;

        // Bar geometry: [Label][  Bar  ][Value]
        const char* max_label = _small ? "S" : "Amb.Shock";
        _label_w              = _sprite.textWidth(max_label) + 6;
        _value_w              = _sprite.textWidth(" -32768") + 4;
        _bar_w                = _inner_w - _margin * 2 - _label_w - _value_w;
        _bar_x                = _margin * 2 + _label_w;

        _presence = new BarI16(thres_p, hys_p, Pal::DET_PRES);
        assert(_presence);
        _motion = new BarI16(thres_m, hys_m, Pal::DET_MOT);
        assert(_motion);
        _shock = new BarI16(thres_a, hys_a, Pal::DET_SHOCK, 100);
        assert(_shock);
    }

    void drawPanel(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const char* title)
    {
        _sprite.fillRoundRect(x, y, w, h, _panel_r, Pal::PANEL_BG);
        _sprite.drawRoundRect(x, y, w, h, _panel_r, Pal::BORDER);
        _sprite.setTextColor((uint8_t)Pal::LABEL);
        _sprite.setCursor(x + _margin + 2, y + 2);
        _sprite.print(title);
        _sprite.drawFastHLine(x + _margin, y + _fhgt + 1, w - _margin * 2, Pal::BORDER);
    }

    void drawTempRow(uint32_t x, uint32_t y, uint32_t w, const char* label, float temp, uint8_t color)
    {
        _sprite.setTextColor((uint8_t)Pal::LABEL);
        _sprite.setCursor(x, y);
        _sprite.print(label);

        char buf[16];
        if (std::isnan(temp)) {
            snprintf(buf, sizeof(buf), "  ---  ");
        } else {
            snprintf(buf, sizeof(buf), "%5.1f C", temp);
        }
        int32_t tw = _sprite.textWidth(buf);
        _sprite.setTextColor(color);
        _sprite.setCursor(x + w - tw, y);
        _sprite.print(buf);
    }

    void drawDetRow(uint32_t row, const char* label, int16_t raw, uint8_t color, BarI16* bar)
    {
        uint32_t ry     = _det_content_y + _det_row_h * row;
        uint32_t text_y = ry + (_det_row_h - _fhgt) / 2;

        // Clear row area (inset 1px from panel border)
        _sprite.fillRect(_margin + 1, ry, _inner_w - 2, _det_row_h, Pal::PANEL_BG);

        // Label (colored, left)
        _sprite.setTextColor(color);
        _sprite.setCursor(_margin * 2, text_y);
        _sprite.print(label);

        // Value (bright, right)
        char buf[10];
        snprintf(buf, sizeof(buf), "%6d", raw);
        int32_t vw = _sprite.textWidth(buf);
        _sprite.setTextColor((uint8_t)Pal::VALUE);
        _sprite.setCursor(_margin + _inner_w - _margin - vw - 2, text_y);
        _sprite.print(buf);

        // Bar (between label and value)
        uint32_t bar_h = std::max((uint32_t)8, _det_row_h * 3 / 5);
        uint32_t bar_y = ry + (_det_row_h - bar_h) / 2;
        bar->render(&_sprite, _bar_x, bar_y, _bar_w, bar_h);
    }

    void apply(const Data& d)
    {
        _sprite.fillScreen(Pal::BG);

        _prev_p = _is_p;
        _prev_m = _is_m;
        _prev_a = _is_a;
        _is_p   = d.isPresence();
        _is_m   = d.isMotion() && d.motion() > 0;
        _is_a   = d.isAmbientShock();

        // Flash on detection edges
        if (_is_p && !_prev_p) _presence->flash();
        if (_is_m && !_prev_m) {
            _motion->flash();
            M5.Speaker.tone(4000, 20);
        }
        if (_is_a && !_prev_a) _shock->flash();

        // --- Temperature Panel ---
        drawPanel(_margin, _temp_y, _inner_w, _temp_h, "Temperature");

        uint32_t tx        = _margin * 2 + 2;
        uint32_t temp_top  = _temp_y + _fhgt + 4;
        uint32_t temp_area = _temp_h - _fhgt - 6;
        uint32_t temp_rh   = temp_area / 3;
        uint32_t tw        = _inner_w - _margin * 2 - 4;

        const char* lbl_obj  = _small ? "O" : "Object";
        const char* lbl_amb  = _small ? "A" : "Ambient";
        const char* lbl_comp = _small ? "C" : "Compensated";

        drawTempRow(tx, temp_top, tw, lbl_obj, d.objectTemperature(), Pal::TEMP_OBJ);
        drawTempRow(tx, temp_top + temp_rh, tw, lbl_amb, d.ambientTemperature(), Pal::TEMP_AMB);
        drawTempRow(tx, temp_top + temp_rh * 2, tw, lbl_comp, d.compensatedObjectTemperature(), Pal::TEMP_COMP);

        // --- Detection Panel ---
        drawPanel(_margin, _det_y, _inner_w, _det_h, "Detection");

        _presence->to(d.presence());
        _motion->to(d.motion());
        _shock->to(d.ambient_shock());

        _raw[0] = d.presence();
        _raw[1] = d.motion();
        _raw[2] = d.ambient_shock();

        // --- Status Strip ---
        if (!_small && _status_h > 0) {
            uint32_t sec_w = _wid / 3;
            uint32_t ind_r = std::max((uint32_t)4, _status_h / 4);
            uint32_t cy    = _status_y + _status_h / 2;

            struct Ind {
                const char* text;
                bool active;
                uint8_t color;
            };
            Ind inds[] = {
                {"Presence", _is_p, Pal::DET_PRES},
                {"Motion", _is_m, Pal::DET_MOT},
                {"Shock", _is_a, Pal::DET_SHOCK},
            };

            for (uint32_t i = 0; i < 3; ++i) {
                auto& id       = inds[i];
                uint32_t sx    = sec_w * i;
                uint32_t lw    = _sprite.textWidth(id.text);
                uint32_t total = ind_r * 2 + 6 + lw;
                uint32_t cx    = sx + (sec_w - total) / 2;

                _sprite.fillCircle(cx + ind_r, cy, ind_r, id.active ? id.color : (uint8_t)Pal::IND_OFF);
                _sprite.setTextColor(id.active ? id.color : (uint8_t)Pal::LABEL);
                _sprite.setCursor(cx + ind_r * 2 + 6, cy - _fhgt / 2);
                _sprite.print(id.text);
            }
        }
    }

    void update()
    {
        _presence->update();
        _motion->update();
        _shock->update();

        const char* labels[] = {
            _small ? "P" : "Presence",
            _small ? "M" : "Motion",
            _small ? "S" : "Amb.Shock",
        };
        BarI16* bars[] = {_presence, _motion, _shock};

        for (uint32_t i = 0; i < 3; ++i) {
            drawDetRow(i, labels[i], _raw[i], bars[i]->_color, bars[i]);
        }

#if 0  // FPS counter (debug)
        ++_fps_count;
        uint32_t now = millis();
        if (now - _fps_time >= 1000) {
            _fps       = _fps_count;
            _fps_count = 0;
            _fps_time  = now;
        }
        char fps_buf[12];
        snprintf(fps_buf, sizeof(fps_buf), "%u fps", (unsigned)_fps);
        int32_t fw  = _sprite.textWidth(fps_buf);
        uint32_t fx = _margin + _inner_w - _margin - fw - 2;
        uint32_t fy = _det_y + 2;
        _sprite.fillRect(fx - 2, fy, fw + 4, _fhgt, Pal::PANEL_BG);
        _sprite.setTextColor((uint8_t)Pal::LABEL);
        _sprite.setCursor(fx, fy);
        _sprite.print(fps_buf);
#endif
    }

    void push(LovyanGFX* dst, const int32_t x = 0, const int32_t y = 0)
    {
        int32_t ox = (_disp_w - _wid) / 2 + x;
        int32_t oy = (_disp_h - _hgt) / 2 + y;
        _sprite.pushSprite(dst, ox, oy);
    }

    LGFX_Sprite _sprite{};
    uint32_t _disp_w{}, _disp_h{};
    uint32_t _wid{}, _hgt{};
    bool _small{};
    uint32_t _margin{}, _panel_r{}, _fhgt{};
    uint32_t _inner_w{};
    uint32_t _temp_y{}, _temp_h{};
    uint32_t _det_y{}, _det_h{};
    uint32_t _det_content_y{}, _det_row_h{};
    uint32_t _label_w{}, _value_w{}, _bar_w{}, _bar_x{};
    uint32_t _status_y{}, _status_h{};
    int16_t _raw[3]{};

    bool _prev_p{}, _is_p{};
    bool _prev_m{}, _is_m{};
    bool _prev_a{}, _is_a{};

    BarI16* _presence{};
    BarI16* _motion{};
    BarI16* _shock{};

    uint32_t _fps_time{};
    uint32_t _fps_count{};
    uint32_t _fps{};
};
View* view{};

uint16_t thres_p{}, thres_m{}, thres_a{};
uint8_t hys_p{}, hys_m{}, hys_a{};
void set_params()
{
    auto cfg = unit.config();
    // Adjust for wide mode
    uint16_t s{};
    if (cfg.mode == Gain::Wide) {
        unit.readSensitivity(s);
        unit.writeSensitivity(s / 8);
    }

    // Low pass filter and Trim
    unit.writeLowPassFilter(LowPassFilter::ODR9, LowPassFilter::ODR200, LowPassFilter::ODR50, LowPassFilter::ODR50);
    unit.writeAverageTrim(cfg.avg_t, cfg.avg_tmos);

    // These parameters can only be read/written in Power-down mode
    unit.writePresenceThreshold(200);
    unit.writePresenceHysteresis(50);
    unit.writeMotionThreshold(500);
    unit.writeMotionHysteresis(100);
    unit.writeAmbientShockThreshold(10);
    unit.writeAmbientShockHysteresis(2);

    unit.readSensitivity(s);
    unit.readPresenceThreshold(thres_p);
    unit.readPresenceHysteresis(hys_p);
    unit.readMotionThreshold(thres_m);
    unit.readMotionHysteresis(hys_m);
    unit.readAmbientShockThreshold(thres_a);
    unit.readAmbientShockHysteresis(hys_a);
    M5_LOGI("SENS:%d P:%u,%u M:%u,%u A:%u,%u", s, thres_p, hys_p, thres_m, hys_m, thres_a, hys_a);
}
}  // namespace

void setup()
{
    M5.begin();
    M5.setTouchButtonHeightByRatio(100);

    // This example requires a non-EPD display
    if (lcd.isEPD() || lcd.width() == 0 || lcd.height() == 0) {
        M5_LOGE("No suitable display");
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

    // The screen shall be in landscape mode if exists
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

    auto board       = M5.getBoard();
    auto pin_num_sda = M5.getPin(m5::pin_name_t::port_a_sda);
    auto pin_num_scl = M5.getPin(m5::pin_name_t::port_a_scl);

    auto cfg           = unit.config();
    cfg.start_periodic = false;
    //    cfg.mode           = Gain::Wide;  // If using wide mode
    unit.config(cfg);

    // For NessoN1 GROVE
    if (board == m5::board_t::board_ArduinoNessoN1) {
        // Port A of the NessoN1 is QWIIC, then use portB (GROVE)
        pin_num_sda = M5.getPin(m5::pin_name_t::port_b_out);
        pin_num_scl = M5.getPin(m5::pin_name_t::port_b_in);
        M5_LOGI("getPin(NessoN1): SDA:%u SCL:%u", pin_num_sda, pin_num_scl);
        // Wire is used internally, so SoftwareI2C handles the unit
        m5::hal::bus::I2CBusConfig i2c_cfg;
        i2c_cfg.pin_sda = m5::hal::gpio::getPin(pin_num_sda);
        i2c_cfg.pin_scl = m5::hal::gpio::getPin(pin_num_scl);
        auto i2c_bus    = m5::hal::bus::i2c::getBus(i2c_cfg);
        M5_LOGI("Bus:%d", i2c_bus.has_value());
        if (!Units.add(unit, i2c_bus ? i2c_bus.value() : nullptr) || !Units.begin()) {
            M5_LOGE("Failed to begin");
            lcd.fillScreen(TFT_RED);
            while (true) {
                m5::utility::delay(10000);
            }
        }
    } else {
        // Using TwoWire
        M5_LOGI("getPin: SDA:%u SCL:%u", pin_num_sda, pin_num_scl);
        Wire.end();
        Wire.begin(pin_num_sda, pin_num_scl, 400 * 1000U);
        if (!Units.add(unit, Wire) || !Units.begin()) {
            M5_LOGE("Failed to begin");
            lcd.fillScreen(TFT_RED);
            while (true) {
                m5::utility::delay(10000);
            }
        }
    }
    M5_LOGI("M5UnitUnified has been begun");
    M5_LOGI("%s", Units.debugInfo().c_str());

    set_params();

    view = new View(lcd.width(), lcd.height(), thres_p, hys_p, thres_m, hys_m, thres_a, hys_a);
    assert(view);

    // Since resetAlgorithm() is called in startPeriodicMeasurement(), the settings are applied
    unit.startPeriodicMeasurement(cfg.mode, cfg.odr, cfg.comp_type, cfg.abs);

    lcd.fillScreen(0);
}

void loop()
{
    M5.update();
    Units.update();

    // Periodic
    if (unit.updated()) {
        auto d = unit.oldest();
        view->apply(d);
    }

    // Reset
    if (M5.BtnA.wasHold()) {
        M5.Speaker.tone(2000, 30);
        unit.stopPeriodicMeasurement();
        unit.softReset();
        set_params();  // Reset has parameters whose values are initialized or set from the OTP
        auto cfg = unit.config();
        unit.startPeriodicMeasurement(cfg.mode, cfg.odr, cfg.comp_type, cfg.abs);
        M5.Speaker.tone(2000, 30);
    }

    view->update();
    lcd.startWrite();
    view->push(&lcd);
    lcd.endWrite();
}
