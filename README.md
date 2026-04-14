# M5Unit - INFRARED

## Overview

Library for INFRARED using [M5UnitUnified](https://github.com/m5stack/M5UnitUnified).  
M5UnitUnified is a library for unified handling of various M5 units products.

### SKU: U185
Unit TMOS PIR is a high-sensitivity infrared sensor unit for presence and motion detection, utilizing the STHS34PF80 chip solution. It communicates with M5 devices via I2C (default address: 0x5A). The working principle is based on the blackbody radiation principle described by Planck's law, which not only monitors ambient temperature but also detects human presence and motion. The sensor can distinguish between stationary and moving objects, with an 80-degree field of view, providing a wide detection range. Additionally, it supports adjustable sampling frequency and gain modes to meet the needs of different application scenarios. It is suitable for various applications such as alarm systems, smart lighting, and occupancy detection.

### SKU: U004
Unit PIR is a high-performance passive pyroelectric infrared (PIR) detector. It integrates the AS312 digital smart motion detector and adopts pyroelectric infrared sensing technology to determine motion by detecting changes in infrared radiation emitted by the human body or objects. This unit communicates via a Grove HY2.0-4P interface. When an infrared signal is detected, it outputs a high-level signal and features a 2-second delay along with a repeatable trigger mechanism (continuous detection after triggering will extend the high-level duration). It provides a detection distance of 500cm and a wide-angle sensing range of < 100°.

### SKU: U054
Hat PIR is a human body infrared sensor compatible with M5SticKC. It is a Passive Pyroelectric Infrared Detector that works by detecting infrared radiation emitted or reflected by humans or objects. When the sensor detects infrared, it outputs a high-level signal and maintains it for approximately 2 seconds.

### SKU: U057
Unit OP90 is a 90° non-contact photoelectric limit switch. The unit has an infrared transmitter and receiver located on opposite sides. During normal operation, the transmitter continuously emits an infrared signal to the receiver. When an object passes between them and blocks the infrared signal, the output terminal will generate an action signal to detect the object's passage. It is commonly used in mechanical control systems as a safety interlock or photoelectric counter.

### SKU: U058
Unit OP180 is a 180° non-contact photoelectric limit switch. The unit has an infrared transmitter and receiver located on opposite sides. During normal operation, the transmitter continuously emits an infrared signal to the receiver. When an object passes between them and blocks the infrared signal, the output terminal will generate an action signal to detect the object's passage. It is commonly used in mechanical control systems as a safety interlock or a photoelectric counter.

## Related Link
See also examples using conventional methods here.

- [Unit TMOS PIR & Datasheet](https://docs.m5stack.com/en/unit/UNIT-TMOS%20PIR)
- [Unit PIR & Datasheet](https://docs.m5stack.com/en/unit/PIR)
- [Hat PIR & Datasheet](https://docs.m5stack.com/en/hat/hat-pir)
- [Unit OP90 & Datasheet](https://docs.m5stack.com/en/unit/OP.90)
- [Unit OP180 & Datasheet](https://docs.m5stack.com/en/unit/OP180)

### Required Libraries:
- [M5UnitUnified](https://github.com/m5stack/M5UnitUnified)
- [M5Utility](https://github.com/m5stack/M5Utility)
- [M5HAL](https://github.com/m5stack/M5HAL)

## License

- [M5Unit-INFRARED - MIT](LICENSE)

## Examples
See also [examples/UnitUnified](examples/UnitUnified)

### For ArduinoIDE settings
For UnitPIR / HatPIR examples, you must choose a define symbol for the unit you will use.
(Rewrite source or specify with compile options)

- PlotToSerial
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

