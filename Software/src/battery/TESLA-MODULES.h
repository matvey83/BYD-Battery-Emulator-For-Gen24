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

// Capacity when new from 0x392 mux 1 bits 40-49, in 0.1 kWh. 0, 0.1 kWh, 10-bit
// SNA (1023), and values outside 20.0-100.0 kWh are not used for grouping.
inline bool tesla_energy_when_new_plausible_x10(uint16_t energy_x10) {
  return energy_x10 >= 200 && energy_x10 <= 1000;
}

inline ModuleGrouping tesla_module_grouping(TeslaPackFamily family, uint16_t bricks, bool sx_chassis = false,
                                            uint16_t energy_when_new_x10 = 0) {
  // Long Range / Performance NCA 96s46p. Sizes sourced, module order guessed (see 1. above).
  static constexpr uint8_t SIZES_96_LR[] = {23, 25, 25, 23};
  // Standard Range NCA 96s31p. 4x24 is uncertain: Electrek describes the same 23/25
  // split as LR; a TMC post says 4x24 and calls it a guess. There is no SR CAN log
  // to confirm. Used only when capacity when new is in the SR band.
  static constexpr uint8_t SIZES_96_SR[] = {24, 24, 24, 24};
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
  // 96-brick 3/Y: brick count cannot tell Long Range NCA 96s46p (23+25+25+23) from
  // Standard Range NCA 96s31p (4x24, see uncertainty note on SIZES_96_SR). Capacity
  // when new (0x392 mux 1 bits 40-49, 0.1 kWh) is the discriminator: >= 65.0 kWh is
  // LR, 45.0-60.0 kWh is SR, otherwise (including Mid Range ~62 kWh, missing, or
  // implausible values) the pack stays ungrouped. 0x292 is not used; on some
  // firmware that field is 0 or 0.1 kWh.
  if (sx_chassis) {
    return ModuleGrouping();
  }
  switch (bricks) {
    case 96:
      if (!tesla_energy_when_new_plausible_x10(energy_when_new_x10)) {
        return ModuleGrouping();
      }
      if (energy_when_new_x10 >= 650) {
        return ModuleGrouping::from_sizes(SIZES_96_LR, sizeof(SIZES_96_LR), bricks);
      }
      if (energy_when_new_x10 >= 450 && energy_when_new_x10 <= 600) {
        return ModuleGrouping::from_sizes(SIZES_96_SR, sizeof(SIZES_96_SR), bricks);
      }
      return ModuleGrouping();
    case 106:
      return ModuleGrouping::from_sizes(SIZES_106, sizeof(SIZES_106), bricks);
    case 108:
      return ModuleGrouping::from_sizes(SIZES_108, sizeof(SIZES_108), bricks);
    default:
      return ModuleGrouping();
  }
}

#endif
