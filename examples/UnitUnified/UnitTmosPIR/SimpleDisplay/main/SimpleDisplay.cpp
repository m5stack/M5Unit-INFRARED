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
#include <cassert>

using namespace m5::unit::sths34pf80;

namespace {
auto& lcd = M5.Display;
m5::unit::UnitUnified Units;
m5::unit::UnitTmosPIR unit;

struct BarI16 {
    BarI16(const uint32_t w, const uint32_t h, const uint32_t th, const uint32_t hy, const int32_t disp_min = 0,
           const int32_t disp_max = 5000)
        : _wid{w},
          _hgt{h},
          _thres{th},
          _hyst{hy},
          _disp_min{disp_min},
          _disp_max{disp_max},
          _disp_range{disp_max - disp_min},
          _val{-32768},
          _to{-32768}
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
    }

    void render(LovyanGFX* dst, const uint32_t x, const uint32_t y)
    {
        dst->fillRect(x, y, _wid, _hgt, 0);
        int32_t w = _wid * ((_val + _disp_range / 2) / _disp_range);
        w         = std::max(std::min(w, (int32_t)_wid), (int32_t)0);

        uint8_t pal = _val < (_thres - _hyst) ? 12 : _val > _thres ? 13 : 14;
        dst->fillRect(x, y, w, _hgt >> 1, pal);
        dst->drawRect(x, y, _wid, _hgt >> 1, 7);

        dst->drawFastVLine(x + _wid / 2, y, _hgt, 7 /* white */);  // origin (0)

        int32_t xx = _wid * (((float)_thres + _disp_range / 2) / _disp_range);
        xx         = std::max(std::min(xx, (int32_t)_wid), (int32_t)0);
        dst->drawFastVLine(x + xx, y, _hgt, 1 /* blue */);  // Thres

        xx = _wid * (((float)(_thres - _hyst) + _disp_range / 2) / _disp_range);
        xx = std::max(std::min(xx, (int32_t)_wid), (int32_t)0);
        dst->drawFastVLine(x + xx, y, _hgt, 4 /* red */);  // Hyst
    }

    uint32_t _wid{}, _hgt{};
    uint32_t _thres{}, _hyst{};
    uint8_t _pal{};
    int32_t _disp_min{}, _disp_max{}, _disp_range{};

    float _val{}, _to{}, _add{};
    uint32_t _counter{};
};

struct View {
    View(const uint32_t w, const uint32_t h, const uint16_t thres_p, const uint8_t hys_p, const uint16_t thres_m,
         const uint8_t hys_m, const uint16_t thres_a, const uint8_t hys_a)
        : _wid{w}, _hgt{h}
    {
        constexpr RGBColor color_table[16] = {
            RGBColor(0, 0, 0),      RGBColor(0, 0, 255),     RGBColor(0, 255, 0),     RGBColor(0, 255, 255),
            RGBColor(255, 0, 0),    RGBColor(255, 0, 255),   RGBColor(255, 255, 0),   RGBColor(255, 255, 255),
            RGBColor(64, 64, 64),   RGBColor(128, 128, 128), RGBColor(192, 192, 192), RGBColor(0, 0, 0),
            RGBColor(204, 128, 64), RGBColor(64, 204, 128),  RGBColor(128, 64, 204),  RGBColor(0, 0, 0)};

        _sprite.setPsram(false);
        _sprite.setColorDepth(4);  // 16 colors
        auto r = _sprite.createSprite(w, h);
        assert(r);
        auto pal = _sprite.getPalette();
        for (auto&& c : color_table) {
            *pal++ = c;
        }
        if (_hgt >= 240) {
            _sprite.setFont(&fonts::FreeMono9pt7b);
        } else {
            if (_wid < 240) {
                _sprite.setFont(&fonts::Font0);
            } else {
                _sprite.setFont(&fonts::AsciiFont8x16);
            }
        }
        auto fhgt = _sprite.fontHeight();
        _presence = new BarI16(_wid, fhgt, thres_p, hys_p);
        assert(_presence);
        _motion = new BarI16(_wid, fhgt, thres_m, hys_m);
        assert(_motion);
        _shock = new BarI16(_wid, fhgt, thres_a, hys_a, 0, 100);
        assert(_shock);
    }

    void apply(const Data& d)
    {
        _sprite.fillScreen(0);

        _prev_p = _is_p;
        _prev_m = _is_m;
        _prev_a = _is_a;
        _is_p   = d.isPresence();
        _is_m   = d.isMotion() && d.motion() > 0;
        //_is_m   = d.isMotion(); //If you want to detect negative motion as well
        _is_a = d.isAmbientShock();

        // was detected motion
        if (_is_m && !_prev_m) {
            M5.Speaker.tone(4000, 20);
        }

        uint32_t fhgt = _sprite.fontHeight();
        uint32_t y{};

        _sprite.setCursor(0, y);
        _sprite.setTextColor(6);
        _sprite.printf(" OBJ: %3.2f %6d", d.objectTemperature(), d.object());
        y += fhgt;
        _sprite.setCursor(0, y);
        _sprite.setTextColor(5);
        _sprite.printf("OBJC: %3.2f %6d", d.compensatedObjectTemperature(), d.compensated_object());
        y += fhgt;
        _sprite.setCursor(0, y);
        _sprite.setTextColor(4);
        _sprite.printf(" AMB: %3.2f %6d", d.ambientTemperature(), d.ambient());
        y += fhgt;

        _sprite.setCursor(0, y);
        _sprite.setTextColor(3);
        _sprite.printf("PRES: %6d %c", d.presence(), _is_p ? '@' : ' ');
        y += fhgt * 2;
        _sprite.setCursor(0, y);
        _sprite.setTextColor(2);
        _sprite.printf(" MOT: %6d %c", d.motion(), _is_m ? '@' : ' ');
        y += fhgt * 2;
        _sprite.setCursor(0, y);
        _sprite.setTextColor(1);
        _sprite.printf("ASHK: %6d %c", d.ambient_shock(), _is_a ? '@' : ' ');
        y += fhgt * 2;

        _presence->to(d.presence());
        _motion->to(d.motion());
        _shock->to(d.ambient_shock());
    }

    void update()
    {
        _presence->update();
        _motion->update();
        _shock->update();

        uint32_t fhgt = _sprite.fontHeight();
        _presence->render(&_sprite, 0, fhgt * 4);
        _motion->render(&_sprite, 0, fhgt * 6);
        _shock->render(&_sprite, 0, fhgt * 8);
    }

    void push(LovyanGFX* dst, const int32_t x = 0, const int32_t y = 0)
    {
        _sprite.pushSprite(dst, x, y);
    }

    LGFX_Sprite _sprite{};
    uint32_t _wid{}, _hgt{};

    bool _prev_p{}, _is_p{};
    bool _prev_m{}, _is_m{};
    bool _prev_a{}, _is_a{};

    BarI16* _presence{};
    BarI16* _motion{};
    BarI16* _shock{};
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
    // The screen shall be in landscape mode if exists
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

    auto pin_num_sda = M5.getPin(m5::pin_name_t::port_a_sda);
    auto pin_num_scl = M5.getPin(m5::pin_name_t::port_a_scl);
    M5_LOGI("getPin: SDA:%u SCL:%u", pin_num_sda, pin_num_scl);
    Wire.end();
    Wire.begin(pin_num_sda, pin_num_scl, 400 * 1000U);

    auto cfg           = unit.config();
    cfg.start_periodic = false;
    //    cfg.mode           = Gain::Wide;  // If using wide mode
    unit.config(cfg);

    if (!Units.add(unit, Wire) || !Units.begin()) {
        lcd.clear(TFT_RED);
        M5_LOGE("Failed to begin");
        while (true) {
            m5::utility::delay(10000);
        }
    }
    M5_LOGI("M5UnitUnified has been begun");
    M5_LOGI("%s", Units.debugInfo().c_str());

    set_params();

    view = new View(lcd.width(), lcd.height(), thres_p, hys_p, thres_m, hys_m, thres_a, hys_a);
    assert(view);

    // Since resetAlgorithm() is called in startPeriodicMeasurement(), the settings are applied
    unit.startPeriodicMeasurement(cfg.mode, cfg.odr, cfg.comp_type, cfg.abs);

    lcd.startWrite();
    lcd.clear(0);
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
    view->push(&lcd);
}
