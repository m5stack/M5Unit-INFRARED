# M5Unit - INFRARED

## Overview

Library for INFRARED using [M5UnitUnified](https://github.com/m5stack/M5UnitUnified).  
M5UnitUnified is a library for unified handling of various M5 units products.

### SKU: U185
Unit TMOS PIR is a high-sensitivity infrared sensor unit for presence and motion detection, utilizing the STHS34PF80 chip solution. 

It communicates with M5 devices via I2C (default address: 0x5A). 

The working principle is based on the blackbody radiation principle described by Planck's law, which not only monitors ambient temperature but also detects human presence and motion. 

### SKU: U004
Unit PIR is a passive infrared motion detection sensor unit utilizing the AS312 chip. It detects motion by sensing changes in infrared radiation from human bodies or warm objects via a single digital GPIO output.

### SKU: U054
Hat PIR is a passive infrared motion detection sensor for M5StickC series, utilizing the AS312 chip. Same functionality as Unit PIR in a Hat form factor.

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

