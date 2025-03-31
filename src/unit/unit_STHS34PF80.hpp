/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_STHS34PF80.hpp
  @brief STHS34PF80 Unit for M5UnitUnified
*/
#ifndef M5_UNIT_INFRARED_UNIT_STHS34PF80_HPP
#define M5_UNIT_INFRARED_UNIT_STHS34PF80_HPP

#include <M5UnitComponent.hpp>
#include <m5_utility/container/circular_buffer.hpp>
#include <limits>  // NaN
#include <array>

namespace m5 {
namespace unit {

/*!
  @namespace sths34pf80
  @brief For STHS34PF80
 */
namespace sths34pf80 {

/*!
  @enum LowPassFilter
  @brief Low-pass filter configuration (LPF_)
  @note ODR/9 > ODR/20 > ODR/50 ...
 */
enum class LowPassFilter : uint8_t {
    ODR9,    //!< ODR/9
    ODR20,   //!< ODR/20
    ODR50,   //!< ODR/50
    ODR100,  //!< ODR/100
    ODR200,  //!< ODR/200
    ODR400,  //!< ODR/400
    ODR800,  //!< ODR/800
};

/*!
  @enum AmbientTemperatureAverage
  @brief The number of averaged samples for ambient temperature (AVG_T)
 */
enum class AmbientTemperatureAverage : uint8_t {
    Samples8,  //!< 8 samples (as default)
    Samples4,  //!< 4 samples
    Samples2,  //!< 2 samples
    Samples1,  //!< No averaging
};

/*!
  @enum ObjectTemperatureAverage
  @brief The number of averaged samples for object temperature (AVG_TMOS)
  @note The maximum ODR that can be set varies depending on this value
 */
enum class ObjectTemperatureAverage : uint8_t {
    Samples2,     //!< 2 samples, 90 RMS noise
    Samples8,     //!< 8 samples, 50 RMS noise
    Samples32,    //!< 32 samples, 25 RMS noise
    Samples128,   //!< 128 samples, 20 RMS noise (as default)
    Samples256,   //!< 256 samples, 15 RMS noise
    Samples512,   //!< 512 samples, 12 RMS noise
    Samples1024,  //!< 1024 samples, 11 RMS noise
    Samples2048,  //!< 2048 samples, 10 RMS noise
};

/*!
  @enum Gain
  @brief Gain mode
 */
enum class Gain : uint8_t {
    Wide,            //!< Wide mode
    Default = 0x07,  //!< Default mode (High sensitivity)
};

/*!
  @enum ODR
  @brief Output data rate configuration
*/
enum class ODR : uint8_t {
    PowerDown,  //!< Power-down mode
    Rate0_25,   //!< Every 4000 ms
    Rate0_5,    //!< Every 2000 ms
    Rate1,      //!< Every 1000 ms
    Rate2,      //!< Every 500 ms
    Rate4,      //!< Every 250 ms
    Rate8,      //!< Every 126 ms
    Rate15,     //!< Every 66.67 ms
    Rate30,     //!< Every 33.33 ms
};

/*!
  @struct Data
  @brief Measurement data group
 */
struct Data {
    static constexpr uint8_t PRES_FLAG{0x04};
    static constexpr uint8_t MOT_FLAG{0x02};
    static constexpr uint8_t TAMB_SHOCK_FLAG{0x01};

    /*!
      Raw data
      |From|To|Description|
      |---|---|---|
      |0|1|TOBJECT|
      |2|3|TAMBIENT|
      |4|5|TOBJ_COMP (Disabled if wide mode)|
      |6|7|TPRESENCE|
      |8|9|TMOTION|
      |10|11|TAMB_SHOCK|
      |12|12|Detection flags|
    */
    std::array<uint8_t, 13> raw{};
    uint16_t sensitivity{};  //!< Sensitivity value (NOT RAW)

    inline int16_t object() const
    {
        return static_cast<int16_t>((raw[1] << 8) | raw[0]);
    }
    inline float objectTemperature() const
    {
        return sensitivity ? object() / (float)sensitivity : std::numeric_limits<float>::quiet_NaN();
    }
    inline int16_t ambient() const
    {
        return static_cast<int16_t>((raw[3] << 8) | raw[2]);
    }
    inline float ambientTemperature() const
    {
        // It is possible withoutsensitivity, but the absence of sensitivity is an error
        return sensitivity ? ambient() / 100.f /* Fixed value */ : std::numeric_limits<float>::quiet_NaN();
    }
    inline int16_t compensated_object() const
    {
        return static_cast<int16_t>((raw[5] << 8) | raw[4]);
    }
    inline float compensatedObjectTemperature() const
    {
        return sensitivity ? compensated_object() / (float)sensitivity : std::numeric_limits<float>::quiet_NaN();
    }
    inline int16_t presence() const
    {
        return static_cast<int16_t>((raw[7] << 8) | raw[6]);
    }
    inline int16_t motion() const
    {
        return static_cast<int16_t>((raw[9] << 8) | raw[8]);
    }
    inline int16_t ambient_shock() const
    {
        return static_cast<int16_t>((raw[11] << 8) | raw[10]);
    }
    inline bool isPresence() const
    {
        return raw[12] & PRES_FLAG;
    }
    inline bool isMotion() const
    {
        return raw[12] & MOT_FLAG;
    }
    inline bool isAmbientShock() const
    {
        return raw[12] & TAMB_SHOCK_FLAG;
    }
};
}  // namespace sths34pf80

/*!
  @class m5::unit::UnitSTHS34PF80
  @brief STHS34PF80 unit
*/
class UnitSTHS34PF80 : public Component, public PeriodicMeasurementAdapter<UnitSTHS34PF80, sths34pf80::Data> {
    M5_UNIT_COMPONENT_HPP_BUILDER(UnitSTHS34PF80, 0x5A);

public:
    //! @brief Get the maximum ODR value that can be set
    static sths34pf80::ODR maximum_odr(const sths34pf80::ObjectTemperatureAverage avg_tmos);

    /*!
      @struct config_t
      @brief Settings for begin
     */
    struct config_t {
        //! Start periodic measurement on begin?
        bool start_periodic{true};
        //! Gain mode if start on begin
        sths34pf80::Gain mode{sths34pf80::Gain::Default};
        //! ODR if start on begin
        sths34pf80::ODR odr{sths34pf80::ODR::Rate30};
        //! Using compensated value if start on begin (Valid only if mode is default mode)
        bool comp_type{true};
        //! Using absolute value for detect presence if start on begin
        bool abs{false};
        //! Amibient  samples if start on begin
        sths34pf80::AmbientTemperatureAverage avg_t{sths34pf80::AmbientTemperatureAverage::Samples8};
        //! Object samples if start on begin
        sths34pf80::ObjectTemperatureAverage avg_tmos{sths34pf80::ObjectTemperatureAverage::Samples32};
    };

    explicit UnitSTHS34PF80(const uint8_t addr = DEFAULT_ADDRESS)
        : Component(addr), _data{new m5::container::CircularBuffer<sths34pf80::Data>(1)}
    {
        auto ccfg  = component_config();
        ccfg.clock = 400 * 1000U;
        component_config(ccfg);
    }
    virtual ~UnitSTHS34PF80()
    {
    }

    virtual bool begin() override;
    virtual void update(const bool force = false) override;

    ///@name Settings for begin
    ///@{
    /*! @brief Gets the configration */
    inline config_t config()
    {
        return _cfg;
    }
    //! @brief Set the configration
    inline void config(const config_t& cfg)
    {
        _cfg = cfg;
    }
    ///@}

    ///@name Properties
    ///@{
    //! @brief Gets the inner sensitivity
    uint16_t sensitivity() const
    {
        return _sensitivity;
    }
    ///@}

    ///@name Measurement data by periodic
    ///@{
    //! @brief Oldest TOBJECT
    inline int16_t object() const
    {
        return !empty() ? oldest().object() : 0;
    }
    //! @brief Oldest object temperature
    inline float objectTemperature() const
    {
        return !empty() ? oldest().objectTemperature() : std::numeric_limits<float>::quiet_NaN();
    }
    //! @brief Oldest TAMBIENT
    inline int16_t ambient() const
    {
        return !empty() ? oldest().ambient() : 0;
    }
    //! @brief Oldest ambient temperature
    inline float ambientTemperature() const
    {
        return !empty() ? oldest().ambientTemperature() : std::numeric_limits<float>::quiet_NaN();
    }
    //! @brief Oldest TOBJ_COMP
    inline int16_t compensated_object() const
    {
        return !empty() ? oldest().compensated_object() : 0;
    }
    //! @brief Oldest compensated object temperature
    inline float compensatedObjectTemperature() const
    {
        return !empty() ? oldest().compensatedObjectTemperature() : std::numeric_limits<float>::quiet_NaN();
    }
    //! @brief Oldest TPRESENCE
    inline int16_t presence() const
    {
        return !empty() ? oldest().presence() : 0;
    }
    //! @brief Oldest TMOTION
    inline int16_t motion() const
    {
        return !empty() ? oldest().motion() : 0;
    }
    //! @brief Oldest TAMB_SHOCK
    inline int16_t ambient_shock() const
    {
        return !empty() ? oldest().ambient_shock() : 0;
    }
    //! @brief Oldest presence detefction
    inline bool isPresence() const
    {
        return !empty() ? oldest().isPresence() : false;
    }
    //! @brief Oldest motion detefction
    inline bool isMotion() const
    {
        return !empty() ? oldest().isMotion() : false;
    }
    //! @brief Oldest ambient shock detefction
    inline bool isAmbientShock() const
    {
        return !empty() ? oldest().isAmbientShock() : false;
    }
    ///@}

    ///@name Periodic measurement
    ///@{
    /*!
      @brief Start periodic measurement
      @param mode Gain mode
      @param odr Object data rate
      @param comp_type Enables the embedded linear algorithm if true (Valid only in default gain mode)
      @param abs Selects the absolute value in the presence detection algorithm if true
      @return True if successful
      @note Object data rate maximum configurable value depends on AVG_TMOS
      |ObjectTemperatureAverage(AVG_TMOS)| Maximum ODR |
      |---|---|
      |Samples2|ODR30|
      |Samples8|ODR30|
      |Samples32|ODR30|
      |Samples128|ODR8|
      |Samples256|ODR4|
      |Samples512|ODR2|
      |Samples1024|ODR1|
      |Samples2048|ODR0_5|
    */
    inline bool startPeriodicMeasurement(const sths34pf80::Gain mode, const sths34pf80::ODR odr,
                                         const bool comp_type = true, const bool abs = false)
    {
        return PeriodicMeasurementAdapter<UnitSTHS34PF80, sths34pf80::Data>::startPeriodicMeasurement(mode, odr,
                                                                                                      comp_type, abs);
    }
    /*!
      @brief Stop periodic measurement
      @details To be PowerDown mode
      @return True if successful
    */
    inline bool stopPeriodicMeasurement()
    {
        return PeriodicMeasurementAdapter<UnitSTHS34PF80, sths34pf80::Data>::stopPeriodicMeasurement();
    }
    ///@}

    ///@name Single shot measurement
    ///@{
    /*!
      @brief Measurement single shot
      @param[out] data Measuerd data
      @param avg_t The number of averaged samples for ambient temperature
      @param avg_tmos The number of averaged samples for object temperature
      @return True if successful
      @note Using current SENS_DATA
      Only TOBJECT, TAMBIENT, and TOBJ_COMP are valid data for single-shot measurement
      Blocked until measurement ends or timeout occurs
      Measurement time depends on avg_tmos
      @warning During periodic detection runs, an error is returned
    */
    bool measureSingleshot(sths34pf80::Data& data, const sths34pf80::AmbientTemperatureAverage avg_t,
                           const sths34pf80::ObjectTemperatureAverage avg_tmos);
    ///@}

    ///@name Settings
    ///@{
    /*!
      @brief Read the avarage trim
      @param[out] avg_t The number of averaged samples for ambient temperature
      @param[out] avg_tmos The number of averaged samples for object temperature
      @return True if successful
     */
    bool readAverageTrim(sths34pf80::AmbientTemperatureAverage& avg_t, sths34pf80::ObjectTemperatureAverage& avg_tmos);
    /*!
      @brief Write the avarage trim
      @param avg_t The number of averaged samples for ambient temperature
      @param avg_tmos The number of averaged samples for object temperature
      @return True if successful
      @note The maximum ODR that can be set varies depending on avg_trim value
      @warning During periodic detection runs, an error is returned
     */
    bool writeAverageTrim(const sths34pf80::AmbientTemperatureAverage avg_t,
                          const sths34pf80::ObjectTemperatureAverage avg_tmos);

    /*!
      @brief Read the gain mode
      @param[out] mode Gain
      @return True if successful
     */
    bool readGainMode(sths34pf80::Gain& mode);
    /*!
      @brief Write the gain mode
      @param mode Gain
      @return True if successful
      @warning During periodic detection runs, an error is returned
    */
    bool writeGainMode(const sths34pf80::Gain mode);

    /*!
      @brief Read the raw sensitivity
      @param[out] raw Raw value
      @return True if successful
     */
    bool readSensitivityRaw(int8_t& raw);
    /*!
      @brief Read the raw sensitivity
      @param[out] value Sensitivity
      @return True if successful
     */
    bool readSensitivity(uint16_t& value);
    /*!
      @brief Write the raw sensitivity
      @param raw Raw value
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool writeSensitivityRaw(const int8_t raw);
    /*!
      @brief Write the sensitivity
      @param value Sensitivity (0 - 4080)
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool writeSensitivity(const uint16_t value);

    /*!
      @brief Read the ODR
      @param odr ODR
      @return True if successful
     */
    bool readObjectDataRate(sths34pf80::ODR& odr);
    ///@}

    ///@warning Call to resetAlgorithm() is required to apply the value
    ///@name Algorithm settings
    ///@{
    /*!
      @brief Reset the algorithm
      @details Apply each value to the embedded linear algorithm for compensate for ambient temperature variations in
      the object temperature
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool resetAlgorithm();

    /*!
      @brief Read the low pass filter
      @param[out] lp_p_m For presence and motion detection
      @param[out] lpf_m For motion detection
      @param[out] lpf_p For presence detection
      @param[out] lpf_a_t For ambient temperature shock detection
      @return True if successful
     */
    bool readLowPassFilter(sths34pf80::LowPassFilter& lpf_p_m, sths34pf80::LowPassFilter& lpf_m,
                           sths34pf80::LowPassFilter& lpf_p, sths34pf80::LowPassFilter& lpf_a_t);
    /*!
      @brief Write the low pass filter
      @param lp_p_m For presence and motion detection
      @param lpf_m For motion detection
      @param lpf_p For presence detection
      @param lpf_a_t For ambient temperature shock detection
      @return True if successful
      @warning The value of ODR/n for lpf_p_m must be greater than the value of ODR/n for lpf_m and lpf_p
      @warning During periodic detection runs, an error is returned
    */
    bool writeLowPassFilter(const sths34pf80::LowPassFilter lpf_p_m, const sths34pf80::LowPassFilter lpf_m,
                            const sths34pf80::LowPassFilter lpf_p, const sths34pf80::LowPassFilter lpf_a);

    /*!
      @brief Read the threshold for presence detection
      @param[out] thres Threshold
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool readPresenceThreshold(uint16_t& thres);
    /*!
      @brief Write the threshold for presence detection
      @param thres Threshold (15 bits)
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool writePresenceThreshold(const uint16_t thres);
    /*!
      @brief Read the threshold for motion detection
      @param[out] thres Threshold
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool readMotionThreshold(uint16_t& thres);
    /*!
      @brief Write the threshold for motion detection
      @param thres Threshold (15 bits)
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool writeMotionThreshold(const uint16_t thres);
    /*!
      @brief Read the threshold for ambient shock detection
      @param[out] thres Threshold
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool readAmbientShockThreshold(uint16_t& thres);
    /*!
      @brief Write the threshold for ambient shock detection
      @param[out] thres Threshold
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool writeAmbientShockThreshold(const uint16_t thres);

    /*!
      @brief Read the hysteresis for presence detection
      @param[out] hyst Hysteresis
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool readPresenceHysteresis(uint8_t& hyst);
    /*!
      @brief Write the hysteresis for presence detection
      @param hyst Hysteresis
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool writePresenceHysteresis(const uint8_t hyst);
    /*!
      @brief Read the hysteresis for motion detection
      @param[out] hyst Hysteresis
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool readMotionHysteresis(uint8_t& hyst);
    /*!
      @brief Write the hysteresis for motion detection
      @param hyst Hysteresis
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool writeMotionHysteresis(const uint8_t hyst);
    /*!
      @brief Read the hysteresis for ambient shock detection
      @param[out] hyst Hysteresis
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool readAmbientShockHysteresis(uint8_t& hyst);
    /*!
      @brief Write the hysteresis for ambient shock detection
      @param[out] hyst Hysteresis
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool writeAmbientShockHysteresis(const uint8_t hyst);

    /*!
      @brief Read the algorithm configuration
      @param[out] v Value
      @return True if successful
      @warning During periodic detection runs, an error is returned
     */
    bool readAlgorithmConfig(uint8_t& v);
    ///@}

    //! @brief Software reset
    bool softReset();

protected:
    bool read_embedded_register(const uint8_t ereg, uint8_t* rbuf, const uint32_t len);
    bool write_embedded_register(const uint8_t ereg, const uint8_t* buf, const uint32_t len);
    inline bool read_embedded_register8(const uint8_t ereg, uint8_t& v)
    {
        return read_embedded_register(ereg, &v, 1);
    }
    inline bool write_embedded_register8(const uint8_t ereg, const uint8_t v)
    {
        return write_embedded_register(ereg, &v, 1);
    }
    bool read_embedded_register16LE(const uint8_t ereg, uint16_t& v)
    {
        uint8_t rbuf[2]{};
        if (read_embedded_register(ereg, rbuf, 2)) {
            v = rbuf[0] | (rbuf[1] << 8);
            return true;
        }
        return false;
    }
    bool write_embedded_register16LE(const uint8_t ereg, const uint16_t v)
    {
        uint8_t buf[2]{};
        buf[0] = v & 0xFF;
        buf[1] = (v >> 8) & 0xFF;
        return write_embedded_register(ereg, buf, 2);
    }

    bool start_periodic_measurement();
    bool start_periodic_measurement(const sths34pf80::Gain mode, const sths34pf80::ODR odr, const bool comp_type,
                                    const bool abs);
    bool stop_periodic_measurement();

    bool write_odr(const sths34pf80::ODR odr);
    bool write_algorithm_config(const uint8_t v);

    bool is_data_ready();
    bool read_measurement(sths34pf80::Data& d, const bool full = true);

    bool guard_in_periodic(const char* fname);

    M5_UNIT_COMPONENT_PERIODIC_MEASUREMENT_ADAPTER_HPP_BUILDER(UnitSTHS34PF80, sths34pf80::Data);

private:
    std::unique_ptr<m5::container::CircularBuffer<sths34pf80::Data>> _data{};
    uint16_t _sensitivity{};
    config_t _cfg{};
};

namespace sths34pf80 {
///@cond
namespace command {
constexpr uint8_t LPF1_REG{0x0C};         // R/W
constexpr uint8_t LPF2_REG{0x0D};         // R/W
constexpr uint8_t WHO_AM_I_REG{0x0F};     // R
constexpr uint8_t AVG_TRIM_REG{0x10};     // R/W
constexpr uint8_t CTRL0_REG{0x17};        // R/W
constexpr uint8_t SENS_DATA_REG{0x1D};    // R/W
constexpr uint8_t CTRL1_REG{0x20};        // R/W
constexpr uint8_t CTRL2_REG{0x21};        // R/W
constexpr uint8_t CTRL3_REG{0x22};        // R/W
constexpr uint8_t STATUS_REG{0x23};       // R
constexpr uint8_t FUNC_STATUS_REG{0x25};  // R

constexpr uint8_t TOBJECT_L_REG{0x26};     // R
constexpr uint8_t TOBJECT_H_REG{0x27};     // R
constexpr uint8_t TAMBIENT_L_REG{0x28};    // R
constexpr uint8_t TAMBIENT_H_REG{0x29};    // R
constexpr uint8_t TOBJ_COMP_L_REG{0x38};   // R
constexpr uint8_t TOBJ_COMP_H_REG{0x39};   // R
constexpr uint8_t TPRESENCE_L_REG{0x3A};   // R
constexpr uint8_t TPRESENCE_H_REG{0x3B};   // R
constexpr uint8_t TMOTION_L_REG{0x3C};     // R
constexpr uint8_t TMOTION_H_REG{0x3D};     // R
constexpr uint8_t TAMB_SHOCK_L_REG{0x3E};  // R
constexpr uint8_t TAMB_SHOCK_H_REG{0x3F};  // R

// Embedded functions
constexpr uint8_t FUNC_CFG_ADDR_REG{0x08};
constexpr uint8_t FUNC_CFG_DATA_REG{0x09};
constexpr uint8_t PAGE_RW_REG{0x11};

constexpr uint8_t PRESENCE_THS_REG{0x20};
constexpr uint8_t MOTION_THS_REG{0x22};
constexpr uint8_t TAMB_SHOCK_THS_REG{0x24};
constexpr uint8_t HYST_MOTION_REG{0x26};
constexpr uint8_t HYST_PRESENCE_REG{0x27};
constexpr uint8_t ALGO_CONFIG_REG{0x28};
constexpr uint8_t HYST_TAMB_SHOCK_REG{0x29};
constexpr uint8_t RESET_ALGO_REG{0x2A};

}  // namespace command
///@endcond
}  // namespace sths34pf80

}  // namespace unit
}  // namespace m5
#endif
