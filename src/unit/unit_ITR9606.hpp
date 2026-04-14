/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_ITR9606.hpp
  @brief ITR9606 Unit for M5UnitUnified
*/
#ifndef M5_UNIT_INFRARED_UNIT_ITR9606_HPP
#define M5_UNIT_INFRARED_UNIT_ITR9606_HPP

#include <M5UnitComponent.hpp>

namespace m5 {
namespace unit {

/*!
  @class m5::unit::UnitITR9606
  @brief ITR9606 infrared photointerrupter unit
  @details The ITR9606 is a transmissive photointerrupter consisting of an IR LED and a phototransistor.
  When an object passes through the slot and blocks the IR beam, the output changes state.
  Output is active LOW: LOW when blocked (object detected), HIGH when clear.
*/
class UnitITR9606 : public Component {
    M5_UNIT_COMPONENT_HPP_BUILDER(UnitITR9606, 0x00);

public:
    /*!
      @struct config_t
      @brief Settings for begin
     */
    struct config_t {
        //! Polling interval for update() in milliseconds (default: 50ms)
        //! @note Photointerrupter response is fast (~15us), so short intervals are useful
        uint32_t interval{50};
    };

    explicit UnitITR9606() : Component(0x00)
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
      @return true if an object is blocking the IR beam (pin is LOW)
    */
    inline bool isDetected() const
    {
        return _detected;
    }
    /*!
      @brief Rising edge detection (object entered the slot)
      @return true if a new detection occurred since the last update (clear -> blocked transition)
    */
    inline bool wasDetected() const
    {
        return _was_detected;
    }
    /*!
      @brief Falling edge detection (object left the slot)
      @return true if detection ended since the last update (blocked -> clear transition)
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
      @param[out] detected true if an object is blocking the IR beam
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
