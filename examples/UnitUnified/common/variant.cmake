# Map the Kconfig "Target unit" / "Target IR" choice (common/Kconfig.variant.*) to the source-level
# macro that both the Arduino and ESP-IDF builds use. Include this from an example's
# main/CMakeLists.txt *after* idf_component_register() (it needs ${COMPONENT_LIB}). Only the examples
# that have a variant (UnitPIR/PlotToSerial, UnitIR/PlotToSerial, UnitIR/SendIR) include this file;
# each of them rsource's exactly one Kconfig.variant.*, so only one of the choices below is defined.
set(M5UNIT_VARIANT "")
# UnitPIR/PlotToSerial (common/Kconfig.variant.pir)
if(CONFIG_EXAMPLE_USING_UNIT_PIR)
    set(M5UNIT_VARIANT USING_UNIT_PIR)
elseif(CONFIG_EXAMPLE_USING_HAT_PIR)
    set(M5UNIT_VARIANT USING_HAT_PIR)
# UnitIR/PlotToSerial, UnitIR/SendIR (common/Kconfig.variant.ir). UnitIR (GROVE) is the source
# default and needs no macro.
elseif(CONFIG_EXAMPLE_USING_BUILTIN_IR)
    set(M5UNIT_VARIANT USING_BUILTIN_IR)
endif()
if(M5UNIT_VARIANT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ${M5UNIT_VARIANT})
endif()
