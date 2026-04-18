# M5Unit - INFRARED

## Overview

Library for INFRARED using [M5UnitUnified](https://github.com/m5stack/M5UnitUnified).  
M5UnitUnified is a library for unified handling of various M5 units products.

### SKU: U185
Unit TMOS PIR is a high-sensitivity infrared sensor unit for presence and motion detection, utilizing the STHS34PF80 chip solution. It communicates with M5 devices via I2C (default address: 0x5A). The working principle is based on the blackbody radiation principle described by Planck's law, which not only monitors ambient temperature but also detects human presence and motion. The sensor can distinguish between stationary and moving objects, with an 80-degree field of view, providing a wide detection range. Additionally, it supports adjustable sampling frequency and gain modes to meet the needs of different application scenarios. It is suitable for various applications such as alarm systems, smart lighting, and occupancy detection.

### SKU: U004
Unit PIR is a high-performance passive pyroelectric infrared detector. Adopting pyroelectric infrared sensing technology, it judges movements by detecting changes in infrared radiation emitted by the human body or objects. This unit communicates via the Grove HY2.0-4P interface. It outputs a high level when an infrared signal is detected, and features a 2-second delay and a re-triggerable mechanism (continuous detection after triggering will extend the high-level duration). It boasts a detection distance of 500 cm and a wide sensing angle of less than 100°. Equipped with LEGO-compatible mounting holes, it can be flexibly assembled with LEGO structures or fixed using screws. It is suitable for human-sensing lighting, security alarms, smart home automatic control and other application scenarios requiring motion detection.

### SKU: U054
Hat PIR is a human body infrared sensor compatible with M5SticKC. It is a "Passive Pyroelectric Infrared Detector" that works by detecting infrared radiation emitted or reflected by humans or objects. When infrared is detected, it outputs a high level signal and delays for a period of time (during which the high level is maintained and repeat triggers are allowed) until the trigger signal disappears (returns to low level).

### SKU: U057
Unit OP90 is a 90° non-contact photoelectric limit switch. The unit has an infrared transmitter and receiver located on opposite sides. During normal operation, the transmitter continuously emits an infrared signal to the receiver. When an object passes between them and blocks the infrared signal, the output terminal will generate an action signal to detect the object's passage. It is commonly used in mechanical control systems as a safety interlock or photoelectric counter.

### SKU: U058
Unit OP180 is a 180° non-contact photoelectric limit switch. The unit has an infrared transmitter and receiver located on opposite sides. During normal operation, the transmitter continuously emits an infrared signal to the receiver. When an object passes between them and blocks the infrared signal, the output terminal will generate an action signal to detect the object's passage. It is commonly used in mechanical control systems as a safety interlock or a photoelectric counter.

### SKU: U002
Unit IR is a compact short-range photoelectric transceiver unit integrating both infrared transmission and reception. It features a 940nm infrared emitting diode and a 38kHz hardware-demodulating receiver, supporting modulated transmission and automatic demodulation reception for standard infrared protocols such as NEC. The unit communicates via a Grove HY2.0-4P interface with an effective range of less than 5 meters, and incorporates LEGO-compatible mounting holes for flexible integration with LEGO structures or screw-based installation. Suitable for smart home control, infrared remote learning, and short-range inter-device communication.

## Related Link
See also examples using conventional methods here.

- [Unit TMOS PIR & Datasheet](https://docs.m5stack.com/en/unit/UNIT-TMOS%20PIR)
- [Unit PIR & Datasheet](https://docs.m5stack.com/en/unit/PIR)
- [Hat PIR & Datasheet](https://docs.m5stack.com/en/hat/hat-pir)
- [Unit OP90 & Datasheet](https://docs.m5stack.com/en/unit/OP.90)
- [Unit OP180 & Datasheet](https://docs.m5stack.com/en/unit/OP180)
- [Unit IR & Datasheet](https://docs.m5stack.com/en/unit/ir)

### Required Libraries:
- [M5UnitUnified](https://github.com/m5stack/M5UnitUnified)
- [M5Utility](https://github.com/m5stack/M5Utility)
- [M5HAL](https://github.com/m5stack/M5HAL)

## License

- [M5Unit-INFRARED - MIT](LICENSE)

## Support via [PbHub](https://docs.m5stack.com/en/unit/pbhub_1.1)

|Unit|Support|Note|
|---|---|---|
|UnitTmosPIR|NG|I2C with complex registers not supported by PbHub (usable via [PaHub](https://docs.m5stack.com/en/unit/Unit-PaHub%20v2.1))|
|UnitPIR|OK||
|HatPIR|NG|Hat form factor (not Grove)|
|UnitOP|OK||
|UnitIR|NG|RMT (hardware-demodulated) not supported by PbHub|

See also [M5Unit-HUB](https://github.com/m5stack/M5Unit-HUB)

## Examples
See also [examples/UnitUnified](examples/UnitUnified)

- **UnitTmosPIR (U185)**: `PlotToSerial`, `SimpleDisplay`
- **UnitPIR (U004) / HatPIR (U054)**: `PlotToSerial`, `ViaPbHub` (UnitPIR only)
- **UnitOP (U057/U058)**: `PlotToSerial`, `ViaPbHub`
- **UnitIR (U002)**: `PlotToSerial`, `SendIR` (both support `-DUSING_BUILTIN_IR` for built-in IR)

### For ArduinoIDE settings
You must choose a define symbol for the unit you will use.
(Rewrite source or specify with compile options)

- UnitPIR / HatPIR (PlotToSerial)
```cpp
// *************************************************************
// Choose one define symbol to match the unit you are using
// *************************************************************
#if !defined(USING_UNIT_PIR) && !defined(USING_HAT_PIR)
// For UnitPIR (U004)
// #define USING_UNIT_PIR
// For HatPIR (U054)
// #define USING_HAT_PIR
#endif
```

- UnitIR (PlotToSerial / SendIR)

By default, the examples use the Grove-connected Unit IR (U002).
To use a board's built-in IR transmitter/receiver instead, define `USING_BUILTIN_IR`
(either uncomment in the source or pass `-DUSING_BUILTIN_IR` as a compile option).

```cpp
// Uncomment to use a board's built-in IR instead of Unit IR (U002)
// #define USING_BUILTIN_IR
```

Supported boards for built-in IR:
StickC / StickCPlus / StickCPlus2 / StickS3,
Atom (Lite / Matrix / U / S3 / S3 Lite / S3U / S3R), AtomEchoS3R,
Capsule, NanoC6, NessoN1, Cardputer / CardputerADV (17 boards).


### Doxygen document
[GitHub Pages](https://m5stack.github.io/M5Unit-INFRARED/)

If you want to generate documents on your local machine, execute the following command

```
bash docs/doxy.sh
```

It will output it under docs/html  
If you want to output Git commit hashes to html, do it for the git cloned folder.

#### Required
- [Doxygen](https://www.doxygen.nl/)
- [pcregrep](https://formulae.brew.sh/formula/pcre2)
- [Git](https://git-scm.com/) (Output commit hash to html)

