/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file M5UnitUnifiedINFRARED.hpp
  @brief Main header of M5UnitINFRARED

  @mainpage M5UnitINFRARED
  Library for UnitINFRARED using M5UnitUnified.
*/
#ifndef M5_UNIT_UNIFIED_INFRARED_HPP
#define M5_UNIT_UNIFIED_INFRARED_HPP

#include "unit/unit_STHS34PF80.hpp"

/*!
  @namespace m5
  @brief Top level namespace of M5stack
 */
namespace m5 {
/*!
  @namespace unit
  @brief Unit-related namespace
 */
namespace unit {
//! @brief Alias for M5Stack's M5-TMOSPIR unit (internally uses STHS34PF80)
using UnitTmosPIR = m5::unit::UnitSTHS34PF80;

}  // namespace unit
}  // namespace m5
#endif
