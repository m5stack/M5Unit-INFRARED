# ESP-IDF component manifest for M5Unit-INFRARED.
# Upstream: https://github.com/m5stack/M5Unit-INFRARED
#
# UnitTmosPIR (STHS34PF80, I2C), UnitPIR / HatPIR (AS312, GPIO), UnitOP (ITR9606, GPIO) and
# UnitIR (IR remote TX/RX, RMT). The public headers include M5UnitUnified, M5Utility
# (m5_utility/container/circular_buffer.hpp) and driver (driver/gpio.h in the wiring helper), so
# all three are public requirements. The RMT driver used by unit_IR.cpp is part of driver.
file(GLOB_RECURSE SRCS "src/*.cpp")
idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "src"
    REQUIRES M5UnitUnified M5Utility driver
)
