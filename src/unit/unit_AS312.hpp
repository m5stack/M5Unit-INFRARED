/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_AS312.hpp
  @brief AS312 Unit for M5UnitUnified
*/
#ifndef M5_UNIT_INFRARED_UNIT_AS312_HPP
#define M5_UNIT_INFRARED_UNIT_AS312_HPP

#include <M5UnitComponent.hpp>

namespace m5 {
namespace unit {

/*!
  @class m5::unit::UnitAS312
  @brief AS312 digital PIR motion sensor unit
  @details The AS312 is a passive infrared (PIR) sensor with digital output.
  It detects motion by sensing changes in infrared radiation from human bodies or warm objects.
  The sensor outputs HIGH when motion is detected, LOW otherwise.
  Timing is fixed internally: ~2.3s hold time, ~2.3s blocking time.
*/
class UnitAS312 : public Component {
    M5_UNIT_COMPONENT_HPP_BUILDER(UnitAS312, 0x00);

public:
    //! @brief AS312 hold time in milliseconds (~2.3s)
    static constexpr uint32_t HOLD_TIME_MS{2300};

    /*!
      @struct config_t
      @brief Settings for begin
     */
    struct config_t {
        //! Polling interval for update() in milliseconds (default: 500ms)
        //! @note Must be less than HOLD_TIME_MS (2300ms) to avoid missing detection events
        uint32_t interval{500};
    };

    explicit UnitAS312() : Component(0x00)
    {
    }

    //! @brief Gets the config values
    config_t config() const
    {
        return _cfg;
    }
    //! @brief Set the config values
    void config(const config_t& cfg)
    {
        _cfg = cfg;
    }

    bool begin() override;
    void update(const bool force = false) override;

    ///@name Detection state
    ///@{
    /*!
      @brief Current detection state
      @return true if motion is currently detected (pin is HIGH)
    */
    inline bool isDetected() const
    {
        return _detected;
    }
    /*!
      @brief Rising edge detection
      @return true if a new detection occurred since the last update (LOW -> HIGH transition)
    */
    inline bool wasDetected() const
    {
        return _was_detected;
    }
    /*!
      @brief Falling edge detection
      @return true if detection ended since the last update (HIGH -> LOW transition)
    */
    inline bool wasReleased() const
    {
        return _was_released;
    }
    ///@}

    ///@name Direct reading
    ///@{
    /*!
      @brief Read the sensor pin state directly
      @param[out] detected true if motion is detected
      @return True if successful
    */
    bool readDetection(bool& detected);
    ///@}

private:
    config_t _cfg{};
    bool _detected{};
    bool _prev_detected{};
    bool _was_detected{};
    bool _was_released{};
};

}  // namespace unit
}  // namespace m5
#endif
