#include <gtest/gtest.h>
#include <cstring>
#include <vector>

#include "../Software/src/battery/TESLA-MODULES.h"
#include "../Software/src/devboard/display/display_modes.h"

namespace {

void expect_row(const char* row, const char* text) {
  ASSERT_NE(row, nullptr);
  ASSERT_EQ(std::strlen(row), 21u);
  ASSERT_EQ(std::strncmp(row, text, std::strlen(text)), 0);
  for (size_t i = std::strlen(text); i < 21; i++) {
    EXPECT_EQ(row[i], ' ') << text;
  }
}

// Expected 1px gap column indexes for a 128px-centered strip of n cells plus grouping gaps.
std::vector<int> expected_gap_xs(uint16_t n, const ModuleGrouping& grouping) {
  std::vector<int> gaps;
  if (!grouping.known() || grouping.module_count < 2 || grouping.brick_total() != n) {
    return gaps;
  }
  const uint16_t gap_count = (uint16_t)(grouping.module_count - 1);
  if ((uint16_t)(n + gap_count) > 128) {
    return gaps;
  }
  int x = (int)((128u - (n + gap_count)) / 2u);
  uint16_t taken = 0;
  uint8_t module_i = 0;
  for (uint16_t i = 0; i < n; i++) {
    if (taken == grouping.bricks_in_module[module_i]) {
      gaps.push_back(x);
      x++;
      module_i++;
      taken = 0;
    }
    x++;
    taken++;
  }
  return gaps;
}

void expect_layout(const ModuleGrouping& grouping, const uint16_t* mv, uint16_t n, const uint8_t column[128]) {
  const auto gaps = expected_gap_xs(n, grouping);
  const uint16_t width = (uint16_t)(n + gaps.size());
  const int origin = (int)((128u - width) / 2u);
  const int end = origin + (int)width;
  size_t gi = 0;
  for (int x = 0; x < 128; x++) {
    const bool is_gap = gi < gaps.size() && x == gaps[gi];
    if (is_gap) {
      EXPECT_EQ(column[x], 0) << "gap at " << x;
      gi++;
      continue;
    }
    if (x >= origin && x < end) {
      EXPECT_GE(column[x], CELL_BAR_MIN_PX) << "cell at " << x;
    } else {
      EXPECT_EQ(column[x], 0) << "padding at " << x;
    }
  }
  EXPECT_EQ(gi, gaps.size());
}

}  // namespace

TEST(DisplayModeTests, MissingAndUnknownValuesAreStatus) {
  EXPECT_EQ(DISPLAY_MODE_DEFAULT_RAW, static_cast<uint32_t>(DisplayMode::Status));
  EXPECT_EQ(display_mode_from_raw(DISPLAY_MODE_DEFAULT_RAW), DisplayMode::Status);
  EXPECT_EQ(display_mode_from_raw(4), DisplayMode::Status);
  EXPECT_EQ(display_mode_from_raw(99), DisplayMode::Status);
  EXPECT_EQ(display_mode_from_raw(0xFFFFFFFFu), DisplayMode::Status);
}

TEST(DisplayModeTests, KnownValuesRoundTrip) {
  EXPECT_EQ(display_mode_from_raw(0), DisplayMode::Status);
  EXPECT_EQ(display_mode_from_raw(1), DisplayMode::Health);
  EXPECT_EQ(display_mode_from_raw(2), DisplayMode::CellBars);
  EXPECT_EQ(display_mode_from_raw(3), DisplayMode::Cycle);
  EXPECT_STREQ(display_mode_name(DisplayMode::Status), "status-pages");
  EXPECT_STREQ(display_mode_name(DisplayMode::Health), "health");
  EXPECT_STREQ(display_mode_name(DisplayMode::CellBars), "cell-bars");
  EXPECT_STREQ(display_mode_name(DisplayMode::Cycle), "cycle");
}

TEST(DisplayCycleTests, PagesAdvanceEveryFourSeconds) {
  EXPECT_EQ(DISPLAY_CYCLE_PAGE_MS, 4000u);
  EXPECT_EQ(display_cycle_page(0), DisplayMode::Status);
  EXPECT_EQ(display_cycle_page(DISPLAY_CYCLE_PAGE_MS - 1), DisplayMode::Status);
  EXPECT_EQ(display_cycle_page(DISPLAY_CYCLE_PAGE_MS), DisplayMode::Health);
  EXPECT_EQ(display_cycle_page(DISPLAY_CYCLE_PAGE_MS * 2 - 1), DisplayMode::Health);
  EXPECT_EQ(display_cycle_page(DISPLAY_CYCLE_PAGE_MS * 2), DisplayMode::CellBars);
  EXPECT_EQ(display_cycle_page(DISPLAY_CYCLE_PAGE_MS * 3 - 1), DisplayMode::CellBars);
  EXPECT_EQ(display_cycle_page(DISPLAY_CYCLE_PAGE_MS * 3), DisplayMode::Status);
  EXPECT_EQ(display_cycle_page(DISPLAY_CYCLE_PAGE_MS * 4), DisplayMode::Health);

  // A large elapsed value stays on the same three-page ring.
  const uint32_t near_wrap = 0xFFFFF000u;
  EXPECT_EQ(display_cycle_page(near_wrap), display_cycle_page(near_wrap % (DISPLAY_CYCLE_PAGE_MS * 3)));
}

TEST(HealthFormatTests, HalfUpBoundaries) {
  EXPECT_EQ(round_half_up(149, 100), 1u);  // 1.49
  EXPECT_EQ(round_half_up(150, 100), 2u);  // 1.50
  EXPECT_EQ(round_half_up(0, 10), 0u);
}

TEST(HealthFormatTests, MockFrameAndFullPrecisionCac) {
  HealthInputs in{};
  in.soc = true;
  in.soc_pptt = 7200;
  in.capacity = true;
  in.capacity_x10 = 821;
  in.soh = true;
  in.soh_current_x10 = 821;
  in.soh_new_x10 = 1000;
  in.imbalance = true;
  in.imbalance_mV = 18;
  // 178.4-181.6 Ah. Rounded endpoints would be 178 and 182 (delta 4); the real delta is 3.2 Ah.
  in.cac = true;
  in.cac_min_x10 = 1784;
  in.cac_max_x10 = 1816;
  in.cac_avg_x10 = 1800;
  in.socimb = true;
  in.soc_min_x10 = 710;
  in.soc_max_x10 = 720;

  HealthRows rows{};
  format_health(in, rows);
  expect_row(rows.line[0], "SOC 72%");
  expect_row(rows.line[1], "CAPACITY 82.1kWh");
  expect_row(rows.line[2], "SOH 82% (82.1/100kWh)");
  expect_row(rows.line[3], "CELL IMBAL 18mV");
  expect_row(rows.line[4], "CAC 178 - 182Ah");
  expect_row(rows.line[5], "CAC IMBAL 3Ah  2%");
  expect_row(rows.line[6], "SOC IMBAL 1%");
  expect_row(rows.line[7], "");
}

TEST(HealthFormatTests, CacPercentUsesUnroundedDeltaAndAverage) {
  HealthInputs in{};
  in.cac = true;
  in.cac_min_x10 = 1784;
  in.cac_max_x10 = 1816;
  // 3.2 / 213.4 Ah = 1.499% -> 1. Subtracting the rounded endpoints (4 Ah) would show 2%.
  in.cac_avg_x10 = 2134;
  in.socimb = true;
  in.soc_min_x10 = 104;  // 10.4%
  in.soc_max_x10 = 116;  // 11.6%; delta 1.2 -> 1, not 12-10

  HealthRows rows{};
  format_health(in, rows);
  expect_row(rows.line[4], "CAC 178 - 182Ah");
  expect_row(rows.line[5], "CAC IMBAL 3Ah  1%");
  expect_row(rows.line[6], "SOC IMBAL 1%");
}

TEST(HealthFormatTests, ValidZeroAndUnavailable) {
  HealthInputs zeros{};
  zeros.soc = true;
  zeros.soc_pptt = 0;
  zeros.imbalance = true;
  zeros.imbalance_mV = 0;
  zeros.socimb = true;
  zeros.soc_min_x10 = 500;
  zeros.soc_max_x10 = 500;

  HealthRows rows{};
  format_health(zeros, rows);
  expect_row(rows.line[0], "SOC 0%");
  expect_row(rows.line[1], "CAPACITY --");
  expect_row(rows.line[2], "SOH --");
  expect_row(rows.line[3], "CELL IMBAL 0mV");
  expect_row(rows.line[4], "CAC --");
  expect_row(rows.line[5], "CAC IMBAL --");
  expect_row(rows.line[6], "SOC IMBAL 0%");

  HealthInputs none{};
  format_health(none, rows);
  expect_row(rows.line[0], "SOC --");
  expect_row(rows.line[3], "CELL IMBAL --");
  expect_row(rows.line[6], "SOC IMBAL --");
}

TEST(HealthFormatTests, SohDropsParensWhenFullLineDoesNotFit) {
  HealthInputs in{};
  in.soh = true;
  // "SOH 100% (82.1/82.5kWh)" is 23 chars. 82.1/82.5 is 99.5% -> 100.
  in.soh_current_x10 = 821;
  in.soh_new_x10 = 825;

  HealthRows rows{};
  format_health(in, rows);
  expect_row(rows.line[2], "SOH 100% 82.1/82.5");
}

TEST(HealthFormatTests, RejectsReversedCacAndOverlongSoh) {
  HealthInputs in{};
  in.cac = true;
  in.cac_min_x10 = 1816;
  in.cac_max_x10 = 1784;
  in.cac_avg_x10 = 1800;
  in.soh = true;
  in.soh_current_x10 = 381;  // "SOH 4% (38.1/999.9kWh)" is 22 chars; drop parens+unit.
  in.soh_new_x10 = 9999;

  HealthRows rows{};
  format_health(in, rows);
  expect_row(rows.line[2], "SOH 4% 38.1/999.9");
  expect_row(rows.line[4], "CAC --");
  expect_row(rows.line[5], "CAC IMBAL --");
}

TEST(HealthFormatTests, CacPercentUnavailableWhenAverageMissing) {
  HealthInputs in{};
  in.cac = true;
  in.cac_min_x10 = 1780;
  in.cac_max_x10 = 1820;
  in.cac_avg_x10 = 0;

  HealthRows rows{};
  format_health(in, rows);
  expect_row(rows.line[4], "CAC 178 - 182Ah");
  expect_row(rows.line[5], "CAC IMBAL 4Ah --%");
}

TEST(HealthFormatTests, CacImbalanceTwoDigitValuesFit) {
  HealthInputs in{};
  in.cac = true;
  // "CAC IMBAL 12Ah  7%" is 17 characters, so no shortened label is needed.
  in.cac_min_x10 = 1780;
  in.cac_max_x10 = 1900;
  in.cac_avg_x10 = 1800;

  HealthRows rows{};
  format_health(in, rows);
  expect_row(rows.line[5], "CAC IMBAL 12Ah  7%");
}

TEST(HealthFormatTests, SocRoundsHalfUp) {
  HealthInputs in{};
  in.soc = true;
  in.soc_pptt = 7249;
  HealthRows rows{};
  format_health(in, rows);
  expect_row(rows.line[0], "SOC 72%");
  in.soc_pptt = 7250;
  format_health(in, rows);
  expect_row(rows.line[0], "SOC 73%");
  in.soc_pptt = 10000;
  format_health(in, rows);
  expect_row(rows.line[0], "SOC 100%");
}

TEST(HealthFormatTests, HoldKeepsEachFieldForThirtyMinutes) {
  HealthHold hold{};
  HealthInputs live{};
  live.soc = true;
  live.soc_pptt = 7200;
  live.imbalance = true;
  live.imbalance_mV = 0;

  const uint32_t t0 = 1000;
  HealthInputs shown = health_hold_step(hold, live, t0);
  EXPECT_TRUE(shown.soc);
  EXPECT_EQ(shown.soc_pptt, 7200);
  EXPECT_TRUE(shown.imbalance);
  EXPECT_EQ(shown.imbalance_mV, 0u);
  EXPECT_FALSE(shown.cac);

  HealthInputs gone{};
  shown = health_hold_step(hold, gone, t0 + DISPLAY_VALUE_HOLD_MS);
  EXPECT_TRUE(shown.soc);
  EXPECT_EQ(shown.soc_pptt, 7200);
  EXPECT_TRUE(shown.imbalance);

  shown = health_hold_step(hold, gone, t0 + DISPLAY_VALUE_HOLD_MS + 1);
  EXPECT_FALSE(shown.soc);
  EXPECT_FALSE(shown.imbalance);
  EXPECT_FALSE(shown.capacity);
}

TEST(CellBarTests, HeaderMatchesMock) {
  char line[22];
  ASSERT_TRUE(format_cell_header(line, 3912, 3930));
  EXPECT_STREQ(line, "3.912-3.930V  18mV");
  ASSERT_TRUE(format_cell_header(line, 3912, 3912));
  EXPECT_STREQ(line, "3.912-3.912V  0mV");
}

TEST(CellBarTests, ZeroCellIsAStubAndExcludedFromMinMax) {
  const uint16_t mv[] = {3912, 3920, 0, 3912};
  const CellScale scale = cell_voltage_scale(mv, 4);
  EXPECT_TRUE(scale.have);
  EXPECT_EQ(scale.min_mV, 3912);
  EXPECT_EQ(scale.max_mV, 3920);
  EXPECT_EQ(scale.range_mV, 48u);
  EXPECT_GT(cell_bar_height(3920, scale), cell_bar_height(3912, scale));
  EXPECT_EQ(cell_bar_height(0, scale), CELL_BAR_MIN_PX);
}

TEST(CellBarTests, WebScaleTwentyMillivoltSpread) {
  // Web: lo = 3892, sc = 180/60 = 3, heights 80 and 140 of 200px.
  // OLED: (20*180 + 20*60)*56/12000 = 22.4 -> 22; (40*180 + 20*60)*56/12000 = 39.2 -> 39.
  const uint16_t mv[] = {3912, 3932};
  const CellScale scale = cell_voltage_scale(mv, 2);
  EXPECT_EQ(scale.range_mV, 60u);
  EXPECT_EQ(cell_bar_height(3912, scale), 22);
  EXPECT_EQ(cell_bar_height(3932, scale), 39);
}

TEST(CellBarTests, AllEqualCellsAreThirtyPixels) {
  // delta 0, range 40: (20*180 + 20*40)*56/8000 = 30.8 -> 30.
  const uint16_t mv[] = {3900, 3900, 3900, 3900};
  const CellScale scale = cell_voltage_scale(mv, 4);
  EXPECT_EQ(scale.range_mV, 40u);
  uint8_t column[128];
  ASSERT_TRUE(layout_cell_bars(ModuleGrouping(), mv, 4, column));
  int drawn = 0;
  for (int x = 0; x < 128; x++) {
    if (column[x] != 0) {
      EXPECT_EQ(column[x], 30) << x;
      drawn++;
    }
  }
  EXPECT_EQ(drawn, 4);
}

TEST(CellBarTests, AllZeroPackHasNoBars) {
  const uint16_t mv[] = {0, 0, 0};
  EXPECT_FALSE(cell_voltage_scale(mv, 3).have);
  uint8_t column[128];
  EXPECT_FALSE(layout_cell_bars(ModuleGrouping(), mv, 3, column));
}

TEST(CellBarTests, GapsOnlyWhenTheModuleLayoutIsExact) {
  uint16_t mv[128];
  for (int i = 0; i < 128; i++) {
    mv[i] = 4000;
  }
  uint8_t column[128];

  const auto legacy96 = tesla_module_grouping(TeslaPackFamily::LegacySX, 96);
  ASSERT_TRUE(layout_cell_bars(legacy96, mv, 96, column));
  // 16 modules of 6, 15 gaps, width 111, origin 8. All known Tesla layouts fit with 1px gaps.
  EXPECT_EQ(legacy96.module_count, 16);
  EXPECT_EQ(column[8 + 6], 0);
  expect_layout(legacy96, mv, 96, column);

  const auto legacy84 = tesla_module_grouping(TeslaPackFamily::LegacySX, 84);
  ASSERT_TRUE(layout_cell_bars(legacy84, mv, 84, column));
  // 14 modules of 6, width 97, origin 15.
  expect_layout(legacy84, mv, 84, column);

  const auto legacy83 = tesla_module_grouping(TeslaPackFamily::LegacySX, 83);
  EXPECT_FALSE(legacy83.known());
  ASSERT_TRUE(layout_cell_bars(legacy83, mv, 83, column));
  expect_layout(legacy83, mv, 83, column);

  // 96-brick 3/Y stays ungrouped: brick count cannot tell 23+25+25+23 from 4x24.
  const auto y96_unknown = tesla_module_grouping(TeslaPackFamily::Model3YClass, 96);
  EXPECT_FALSE(y96_unknown.known());
  ASSERT_TRUE(layout_cell_bars(y96_unknown, mv, 96, column));
  expect_layout(y96_unknown, mv, 96, column);

  // The 23+25+25+23 layout still fits (width 99, origin 14) when grouping is known.
  const uint8_t y96_lr_sizes[] = {23, 25, 25, 23};
  const auto y96 = ModuleGrouping::from_sizes(y96_lr_sizes, 4, 96);
  ASSERT_TRUE(layout_cell_bars(y96, mv, 96, column));
  EXPECT_EQ(column[14 + 23], 0);
  expect_layout(y96, mv, 96, column);

  const auto y106 = tesla_module_grouping(TeslaPackFamily::Model3YClass, 106);
  ASSERT_TRUE(layout_cell_bars(y106, mv, 106, column));
  expect_layout(y106, mv, 106, column);

  const auto y108 = tesla_module_grouping(TeslaPackFamily::Model3YClass, 108);
  ASSERT_TRUE(layout_cell_bars(y108, mv, 108, column));
  // 26+28+28+26, width 111, origin 8.
  expect_layout(y108, mv, 108, column);

  const auto unknown110 = ModuleGrouping();
  ASSERT_TRUE(layout_cell_bars(unknown110, mv, 110, column));
  expect_layout(unknown110, mv, 110, column);

  const auto sx96 = tesla_module_grouping(TeslaPackFamily::Model3YClass, 96, true);
  EXPECT_FALSE(sx96.known());
  ASSERT_TRUE(layout_cell_bars(sx96, mv, 96, column));
  expect_layout(sx96, mv, 96, column);

  mv[3] = 0;
  ASSERT_TRUE(layout_cell_bars(y96, mv, 96, column));
  // A 0 mV cell is still a 2px bar, not a blank gap.
  EXPECT_EQ(column[14 + 3], CELL_BAR_MIN_PX);
  EXPECT_GE(column[14], CELL_BAR_MIN_PX);
  EXPECT_EQ(column[14 + 23], 0);
  expect_layout(y96, mv, 96, column);

  // Grouping that does not add up to the brick count is ignored.
  const uint8_t bad_sizes[] = {23, 25, 25, 22};
  const auto mismatched = ModuleGrouping::from_sizes(bad_sizes, 4, 96);
  EXPECT_FALSE(mismatched.known());
  ASSERT_TRUE(layout_cell_bars(mismatched, mv, 96, column));
  expect_layout(mismatched, mv, 96, column);
}

TEST(CellBarTests, GapsDroppedWhenTheyWouldNotFit) {
  uint16_t mv[128];
  for (int i = 0; i < 128; i++) {
    mv[i] = 4000;
  }
  uint8_t column[128];
  const uint8_t sizes[] = {64, 64};
  const auto grouping = ModuleGrouping::from_sizes(sizes, 2, 128);
  ASSERT_TRUE(grouping.known());
  ASSERT_TRUE(layout_cell_bars(grouping, mv, 128, column));
  // 128 cells + 1 gap would be 129px, so the gap is dropped and every column is a bar.
  for (int x = 0; x < 128; x++) {
    EXPECT_GE(column[x], CELL_BAR_MIN_PX) << x;
  }
}

TEST(CellBarTests, MissingPackAndThirtyMinuteHold) {
  uint8_t column[128] = {};
  const uint16_t zeros[8] = {};
  const auto y96 = tesla_module_grouping(TeslaPackFamily::Model3YClass, 96);
  EXPECT_FALSE(layout_cell_bars(y96, zeros, 8, column));
  EXPECT_FALSE(layout_cell_bars(ModuleGrouping(), nullptr, 0, column));

  CellHold hold{};
  const uint16_t cells[] = {3912, 3930, 0, 3920};
  const auto legacy96 = tesla_module_grouping(TeslaPackFamily::LegacySX, 96);
  EXPECT_TRUE(cell_hold_step(hold, true, legacy96, cells, 4, 50));
  EXPECT_EQ(hold.count, 4u);
  EXPECT_EQ(hold.grouping.module_count, legacy96.module_count);
  EXPECT_TRUE(cell_hold_step(hold, false, ModuleGrouping(), nullptr, 0, 50 + DISPLAY_VALUE_HOLD_MS));
  EXPECT_TRUE(hold.known);
  EXPECT_EQ(hold.mv[1], 3930);
  EXPECT_FALSE(cell_hold_step(hold, false, ModuleGrouping(), nullptr, 0, 50 + DISPLAY_VALUE_HOLD_MS + 1));
  EXPECT_FALSE(hold.known);
  EXPECT_EQ(hold.count, 0u);
  EXPECT_FALSE(hold.grouping.known());
}

TEST(HealthFormatTests, HoldSurvivesMillisWrap) {
  HealthHold hold{};
  HealthInputs live{};
  live.capacity = true;
  live.capacity_x10 = 821;
  const uint32_t t0 = 0xFFFF0000u;
  HealthInputs shown = health_hold_step(hold, live, t0);
  EXPECT_TRUE(shown.capacity);

  HealthInputs gone{};
  const uint32_t at_limit = t0 + DISPLAY_VALUE_HOLD_MS;
  shown = health_hold_step(hold, gone, at_limit);
  EXPECT_TRUE(shown.capacity);
  EXPECT_EQ(shown.capacity_x10, 821);
  shown = health_hold_step(hold, gone, at_limit + 1);
  EXPECT_FALSE(shown.capacity);
}
