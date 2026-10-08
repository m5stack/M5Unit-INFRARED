# "Target unit" selection for the UnitPIR PlotToSerial example (ESP-IDF).
# rsource'd from UnitPIR/PlotToSerial/main/Kconfig.projbuild. The chosen CONFIG_EXAMPLE_USING_*
# is mapped to the source-level macro in common/variant.cmake (shared with the Arduino build).
menu "M5Unit-INFRARED PIR example"

choice EXAMPLE_PIR_VARIANT
    prompt "Target unit"
    default EXAMPLE_USING_UNIT_PIR
    help
        Which AS312 PIR unit this example drives.

    config EXAMPLE_USING_UNIT_PIR
        bool "UnitPIR (U004, GPIO / GROVE)"
    config EXAMPLE_USING_HAT_PIR
        bool "HatPIR (U054, Hat header)"
        # Hat header hosts: StickC / StickC Plus / Plus2 / CoreInk (esp32), StickS3 (esp32s3),
        # NessoN1 (esp32c6)
        depends on IDF_TARGET_ESP32 || IDF_TARGET_ESP32S3 || IDF_TARGET_ESP32C6
endchoice

endmenu
