#ifndef TESLA_MODULES_H
#define TESLA_MODULES_H

#include "../datalayer/module_grouping.h"

// Bricks per module for the Tesla packs. Returns unknown unless the brick count
// is exactly one of the layouts below, so a partly counted pack gets no grouping.
//
// The order of the modules along the brick index is a symmetric guess (the two
// end modules are the smaller ones). The sizes come from teardowns and logs.
enum class TeslaPackFamily : uint8_t {
  Model3YClass,  // TeslaBattery: Model 3/Y. The 2021+ Model S/X on the same BMS is not supported.
  LegacySX,      // TeslaLegacyBattery: Model S/X 2012-2020
};

inline ModuleGrouping tesla_module_grouping(TeslaPackFamily family, uint16_t bricks, bool sx_chassis = false) {
  static constexpr uint8_t SIZES_96[] = {23, 25, 25, 23};
  static constexpr uint8_t SIZES_106[] = {25, 28, 28, 25};  // LFP
  static constexpr uint8_t SIZES_108[] = {26, 28, 28, 26};  // LFP
  // Legacy S/X modules hold 6 bricks each. 96 bricks is 16 modules. 84 bricks is
  // derived (14 modules): no capture of an 84 brick pack has been seen yet.
  static constexpr uint8_t SIZES_LEGACY_96[] = {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6};
  static constexpr uint8_t SIZES_LEGACY_84[] = {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6};

  if (family == TeslaPackFamily::LegacySX) {
    if (bricks == 96) {
      return ModuleGrouping::from_sizes(SIZES_LEGACY_96, sizeof(SIZES_LEGACY_96), bricks);
    }
    if (bricks == 84) {
      return ModuleGrouping::from_sizes(SIZES_LEGACY_84, sizeof(SIZES_LEGACY_84), bricks);
    }
    return ModuleGrouping();
  }

  // Model3YClass. 2021+ Model S/X (Plaid/Palladium, 110 bricks) is intentionally not
  // supported yet: the brick count logic cannot report 110 and there is no capture.
  // So a Model S/X chassis gets no grouping, and the 96/106/108 tables are for 3/Y.
  if (sx_chassis) {
    return ModuleGrouping();
  }
  switch (bricks) {
    case 96:
      return ModuleGrouping::from_sizes(SIZES_96, sizeof(SIZES_96), bricks);
    case 106:
      return ModuleGrouping::from_sizes(SIZES_106, sizeof(SIZES_106), bricks);
    case 108:
      return ModuleGrouping::from_sizes(SIZES_108, sizeof(SIZES_108), bricks);
    default:
      return ModuleGrouping();
  }
}

#endif
