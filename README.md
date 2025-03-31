# M5Unit - INFRARED

## Overview

Library for INFRARED using [M5UnitUnified](https://github.com/m5stack/M5UnitUnified).  
M5UnitUnified is a library for unified handling of various M5 units products.

### SKU: U185
Unit TMOS PIR is a high-sensitivity infrared sensor unit for presence and motion detection, utilizing the STHS34PF80 chip solution. 

It communicates with M5 devices via I2C (default address: 0x5A). 

The working principle is based on the blackbody radiation principle described by Planck's law, which not only monitors ambient temperature but also detects human presence and motion. 

## Related Link
See also examples using conventional methods here.

- [Unit TMOS PIR & Datasheet](https://docs.m5stack.switch-science.com/en/unit/UNIT-TMOS%20PIR)

### Required Libraries:
- [M5UnitUnified](https://github.com/m5stack/M5UnitUnified)
- [M5Utility](https://github.com/m5stack/M5Utility)
- [M5HAL](https://github.com/m5stack/M5HAL)

## License

- [M5Unit-INFRARED - MIT](LICENSE)

## Examples
See also [examples/UnitUnified](examples/UnitUnified)

### Doxygen document
[GitHub Pages](https://m5stack.github.io/M5Unit-INFRARED/)

If you want to generate documents on your local machine, execute the following command

```
bash docs/doxy.sh
```

It will output it under docs/html  
If you want to output Git commit hashes to html, do it for the git cloned folder.

#### Required
- [Doxyegn](https://www.doxygen.nl/)
- [pcregrep](https://formulae.brew.sh/formula/pcre2)
- [Git](https://git-scm.com/) (Output commit hash to html)

