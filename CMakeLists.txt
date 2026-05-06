# ESP-IDF component manifest for M5Unit-INFRARED.
# Upstream: https://github.com/m5stack/M5Unit-INFRARED
#
# UnitIR uses ESP-IDF RMT directly (v1: driver/rmt.h, v2: driver/rmt_tx.h),
# both currently provided through the `driver` legacy umbrella.
file(GLOB_RECURSE SRCS "src/*.cpp")
idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "src"
    REQUIRES M5UnitUnified driver esp_driver_gpio
)
