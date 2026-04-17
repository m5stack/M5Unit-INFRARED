/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file M5UnitUnifiedINFRARED.hpp
  @brief Main header of M5Unit-INFRARED

  @mainpage M5Unit-INFRARED
  Library for M5Unit-INFRARED using M5UnitUnified.
*/
#ifndef M5_UNIT_UNIFIED_INFRARED_HPP
#define M5_UNIT_UNIFIED_INFRARED_HPP

#include "unit/unit_STHS34PF80.hpp"
#include "unit/unit_AS312.hpp"
#include "unit/unit_ITR9606.hpp"
#include "unit/unit_IR.hpp"

/*!
  @namespace m5
  @brief Top level namespace of M5Stack
 */
namespace m5 {
/*!
  @namespace unit
  @brief Unit-related namespace
 */
namespace unit {
//! @brief Alias for M5Stack's M5-TMOSPIR unit (internally uses STHS34PF80)
using UnitTmosPIR = m5::unit::UnitSTHS34PF80;
//! @brief Alias for M5Stack's Unit PIR (SKU: U004, internally uses AS312)
using UnitPIR = m5::unit::UnitAS312;
//! @brief Alias for M5Stack's Hat PIR (SKU: U054, internally uses AS312)
using HatPIR = m5::unit::UnitAS312;
//! @brief Alias for M5Stack's Unit OP90 (SKU: U057) / Unit OP180 (SKU: U058, internally uses ITR9606)
using UnitOP = m5::unit::UnitITR9606;

}  // namespace unit
}  // namespace m5
#endif
