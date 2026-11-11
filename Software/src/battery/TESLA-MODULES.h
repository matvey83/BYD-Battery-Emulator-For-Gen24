#ifndef TESLA_MODULES_H
#define TESLA_MODULES_H

#include "../datalayer/module_grouping.h"

// Bricks per module for the Tesla packs. Returns unknown unless the brick count
// is exactly one of the layouts below, so a partly counted pack gets no grouping.
//
// Two things below are NOT confirmed:
// 1. The order of the modules along the brick index is a guess. Only the sizes
//    are sourced (teardowns, logs); the tables put the smaller end modules first
//    and last (symmetric). A real pack may differ, then only the order changes.
// 2. The legacy S/X 84 brick layout (14 modules of 6) is derived from "6 bricks
//    per module", there is no capture of such a pack yet.
enum class TeslaPackFamily : uint8_t {
  Model3YClass,  // TeslaBattery: Model 3/Y. The 2021+ Model S/X on the same BMS is not supported.
  LegacySX,      // TeslaLegacyBattery: Model S/X 2012-2020
};

inline ModuleGrouping tesla_module_grouping(TeslaPackFamily family, uint16_t bricks, bool sx_chassis = false) {
  // LFP. Sizes sourced, module order guessed (see 1. above).
  static constexpr uint8_t SIZES_106[] = {25, 28, 28, 25};
  // LFP. Sizes sourced, module order guessed.
  static constexpr uint8_t SIZES_108[] = {26, 28, 28, 26};
  // Legacy S/X modules hold 6 bricks each, all equal, so the order does not matter.
  // 96 bricks is 16 modules.
  static constexpr uint8_t SIZES_LEGACY_96[] = {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6};
  // DERIVED, NOT SEEN ON A REAL PACK (see 2. above): 84 bricks is 14 modules of 6.
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
  // So a Model S/X chassis gets no grouping, and the 106/108 tables are for 3/Y LFP.
  //
  // 96-brick 3/Y is left ungrouped: Long Range NCA 96s46p is 23+25+25+23, but Standard
  // Range NCA 96s31p is 4x24, and the brick count cannot tell them apart. Capacity when
  // new would separate them (LR ~75+ kWh, SR ~50-55 kWh), but that value is not
  // reliably available at grouping time (0x392 mux 1 is not decoded here, and 0x292
  // reads 0 or 0.1 kWh on some firmware).
  if (sx_chassis) {
    return ModuleGrouping();
  }
  switch (bricks) {
    case 106:
      return ModuleGrouping::from_sizes(SIZES_106, sizeof(SIZES_106), bricks);
    case 108:
      return ModuleGrouping::from_sizes(SIZES_108, sizeof(SIZES_108), bricks);
    default:
      return ModuleGrouping();
  }
}

#endif
