# ESP-IDF component manifest for M5Unit-INFRARED.
# Upstream: https://github.com/m5stack/M5Unit-INFRARED
#
# REQUIRES (public) -- headers included by our public headers (src/**.hpp):
#   M5UnitUnified : <M5UnitComponent.hpp>
#   M5Utility     : <m5_utility/container/circular_buffer.hpp> (unit_STHS34PF80.hpp)
# PRIV_REQUIRES (private) -- used only in .cpp:
#   driver        : RMT v1 (driver/rmt.h) / v2 (driver/rmt_tx.h) in unit_IR.cpp
# freertos / heap / esp_hw_support are part of the common requirements
# (auto-added to every component) and are not listed explicitly.
file(GLOB_RECURSE SRCS "src/*.cpp")
idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "src"
    REQUIRES M5UnitUnified M5Utility
    PRIV_REQUIRES driver
)
