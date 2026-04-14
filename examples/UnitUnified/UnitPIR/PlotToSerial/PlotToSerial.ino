/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for UnitPIR / HatPIR (AS312)
*/

// *************************************************************
// Choose one define symbol to match the unit you are using
// *************************************************************
#if !defined(USING_UNIT_PIR) && !defined(USING_HAT_PIR)
// For UnitPIR (U004)
// #define USING_UNIT_PIR
// For HatPIR (U054)
// #define USING_HAT_PIR
#endif

#include "main/PlotToSerial.cpp"
