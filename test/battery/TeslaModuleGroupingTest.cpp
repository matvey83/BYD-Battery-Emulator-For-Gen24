#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "../../Software/src/battery/TESLA-BATTERY.h"
#include "../../Software/src/battery/TESLA-LEGACY-BATTERY.h"
#include "../../Software/src/battery/TESLA-MODULES.h"
#include "../../Software/src/datalayer/datalayer.h"
#include "../../Software/src/datalayer/module_grouping.h"

// Bricks-per-module grouping: the generic structure, the Tesla layout tables, and the
// two Tesla drivers filling the datalayer from CAN frames.

namespace {

std::vector<uint8_t> Sizes(const ModuleGrouping& g) {
  return std::vector<uint8_t>(g.bricks_in_module, g.bricks_in_module + g.module_count);
}

std::string Json(const ModuleGrouping& g) {
  std::string out;
  g.append_json(out);
  return out;
}

struct Layout {
  TeslaPackFamily family;
  uint16_t bricks;
  std::vector<uint8_t> sizes;
};

const std::vector<Layout>& Layouts() {
  static const std::vector<Layout> layouts = {
      {TeslaPackFamily::Model3YClass, 106, {25, 28, 28, 25}},
      {TeslaPackFamily::Model3YClass, 108, {26, 28, 28, 26}},
      {TeslaPackFamily::LegacySX, 96, std::vector<uint8_t>(16, 6)},
      {TeslaPackFamily::LegacySX, 84, std::vector<uint8_t>(14, 6)},
  };
  return layouts;
}

CAN_frame Frame(uint32_t id) {
  CAN_frame frame = {};
  frame.ID = id;
  frame.DLC = 8;
  return frame;
}

// 0x401 brick voltages. Status 0x2A is a full frame of three bricks, 0x02 the short
// last frame with one. The count comes from the status bytes, so the voltages stay 0.
CAN_frame BrickFrame(uint8_t mux, uint8_t flags) {
  CAN_frame frame = Frame(0x401);
  frame.data.u8[0] = mux;
  frame.data.u8[1] = flags;
  return frame;
}

// A full scan plus the second mux-0 frame that publishes the count.
void ScanFullFrames(TeslaBattery* pack, uint8_t last_mux) {
  for (uint8_t mux = 0; mux <= last_mux; mux++) {
    pack->handle_incoming_can_frame(BrickFrame(mux, 0x2A));
  }
  pack->handle_incoming_can_frame(BrickFrame(0, 0x2A));
}

// 0x6F2 frame carrying `bricks_present` non-zero bricks (0 to 4) out of its four slots.
CAN_frame LegacyCellFrame(uint8_t mux, int bricks_present) {
  const uint16_t raw = 12000;  // about 3.66 V, any non-zero value will do
  const uint16_t r[4] = {
      (uint16_t)(bricks_present > 0 ? raw : 0),
      (uint16_t)(bricks_present > 1 ? raw : 0),
      (uint16_t)(bricks_present > 2 ? raw : 0),
      (uint16_t)(bricks_present > 3 ? raw : 0),
  };
  CAN_frame f = Frame(0x6F2);
  f.data.u8[0] = mux;
  f.data.u8[1] = r[0] & 0xFF;
  f.data.u8[2] = ((r[0] >> 8) & 0x3F) | ((r[1] & 0x03) << 6);
  f.data.u8[3] = (r[1] >> 2) & 0xFF;
  f.data.u8[4] = ((r[1] >> 10) & 0x0F) | ((r[2] & 0x0F) << 4);
  f.data.u8[5] = (r[2] >> 4) & 0xFF;
  f.data.u8[6] = ((r[2] >> 12) & 0x03) | ((r[3] & 0x3F) << 2);
  f.data.u8[7] = (r[3] >> 6) & 0xFF;
  return f;
}

// Two full mux cycles of a pack with `bricks` series bricks, then the mux 0 that closes the second.
void FeedLegacyPack(TeslaLegacyBattery& pack, uint16_t bricks) {
  for (auto& v : datalayer.battery.status.cell_voltages_mV) {
    v = 0;
  }
  const uint8_t frames = (bricks + 3) / 4;
  for (int cycle = 0; cycle < 2; cycle++) {
    for (uint8_t mux = 0; mux < frames; mux++) {
      const int present = (mux * 4 + 4 <= bricks) ? 4 : bricks - mux * 4;
      pack.handle_incoming_can_frame(LegacyCellFrame(mux, present));
    }
  }
  pack.handle_incoming_can_frame(LegacyCellFrame(0, 4));
}

}  // namespace

// ---- The generic structure ----

TEST(ModuleGroupingTest, DefaultIsUnknown) {
  ModuleGrouping g;
  EXPECT_FALSE(g.known());
  EXPECT_EQ(g.module_count, 0);
  EXPECT_EQ(g.brick_total(), 0);
  EXPECT_EQ(Json(g), "[]");
}

TEST(ModuleGroupingTest, FromSizesKeepsMatchingTable) {
  const uint8_t table[] = {10, 20, 30};
  ModuleGrouping g = ModuleGrouping::from_sizes(table, 3, 60);
  EXPECT_TRUE(g.known());
  EXPECT_EQ(Sizes(g), (std::vector<uint8_t>{10, 20, 30}));
  EXPECT_EQ(g.brick_total(), 60);
  EXPECT_EQ(Json(g), "[10,20,30]");
}

TEST(ModuleGroupingTest, FromSizesStaysUnknownUnlessItAddsUpExactly) {
  const uint8_t table[] = {10, 20, 30};
  EXPECT_FALSE(ModuleGrouping::from_sizes(table, 3, 0).known());
  EXPECT_FALSE(ModuleGrouping::from_sizes(table, 3, 59).known());
  EXPECT_FALSE(ModuleGrouping::from_sizes(table, 3, 61).known());
  EXPECT_FALSE(ModuleGrouping::from_sizes(table, 0, 60).known());
  EXPECT_FALSE(ModuleGrouping::from_sizes(nullptr, 3, 60).known());

  const uint8_t with_empty_module[] = {10, 0, 20};
  EXPECT_FALSE(ModuleGrouping::from_sizes(with_empty_module, 3, 30).known());

  uint8_t too_many[ModuleGrouping::MAX_MODULES + 1];
  for (auto& v : too_many) {
    v = 1;
  }
  EXPECT_TRUE(ModuleGrouping::from_sizes(too_many, ModuleGrouping::MAX_MODULES, ModuleGrouping::MAX_MODULES).known());
  EXPECT_FALSE(
      ModuleGrouping::from_sizes(too_many, ModuleGrouping::MAX_MODULES + 1, ModuleGrouping::MAX_MODULES + 1).known());
}

// ---- The Tesla layout tables ----

TEST(TeslaModuleGroupingTest, EveryLayoutAddsUpAndHasItsBoundaries) {
  for (const Layout& layout : Layouts()) {
    SCOPED_TRACE(layout.bricks);
    ModuleGrouping g = tesla_module_grouping(layout.family, layout.bricks);
    ASSERT_TRUE(g.known());
    EXPECT_EQ(Sizes(g), layout.sizes);
    EXPECT_EQ(g.module_count, layout.sizes.size());
    EXPECT_EQ(g.brick_total(), layout.bricks);
  }
}

TEST(TeslaModuleGroupingTest, ModuleBoundaries) {
  // The first brick of every module after the first, counted from 0.
  auto boundaries = [](const ModuleGrouping& g) {
    std::vector<unsigned> result;
    unsigned sum = 0;
    for (uint8_t i = 0; i + 1 < g.module_count; i++) {
      sum += g.bricks_in_module[i];
      result.push_back(sum);
    }
    return result;
  };
  EXPECT_EQ(boundaries(tesla_module_grouping(TeslaPackFamily::Model3YClass, 106)), (std::vector<unsigned>{25, 53, 81}));
  EXPECT_EQ(boundaries(tesla_module_grouping(TeslaPackFamily::Model3YClass, 108)), (std::vector<unsigned>{26, 54, 82}));
  EXPECT_EQ(boundaries(tesla_module_grouping(TeslaPackFamily::LegacySX, 84)),
            (std::vector<unsigned>{6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 66, 72, 78}));
}

TEST(TeslaModuleGroupingTest, Model3Y96StaysUnknown) {
  // Brick count alone cannot tell Long Range 96s46p (23+25+25+23) from Standard Range 96s31p (4x24).
  ModuleGrouping g = tesla_module_grouping(TeslaPackFamily::Model3YClass, 96);
  EXPECT_FALSE(g.known());
  EXPECT_EQ(g.module_count, 0);
}

TEST(TeslaModuleGroupingTest, NonMatchingCountsAreUnknown) {
  for (uint16_t bricks : {0, 1, 3, 14, 83, 85, 95, 97, 105, 107, 109, 110, 111, 112, 150, 255}) {
    SCOPED_TRACE(bricks);
    EXPECT_FALSE(tesla_module_grouping(TeslaPackFamily::Model3YClass, bricks, false).known());
    EXPECT_FALSE(tesla_module_grouping(TeslaPackFamily::Model3YClass, bricks, true).known());
    EXPECT_FALSE(tesla_module_grouping(TeslaPackFamily::LegacySX, bricks).known());
  }
}

// Known exactly for the expected counts and nothing else, for every count a uint8_t can hold.
TEST(TeslaModuleGroupingTest, OnlyTheExpectedCountsAreKnown) {
  for (unsigned bricks = 0; bricks <= 255; bricks++) {
    SCOPED_TRACE(bricks);
    const bool y_class = bricks == 106 || bricks == 108;
    EXPECT_EQ(tesla_module_grouping(TeslaPackFamily::Model3YClass, bricks, false).known(), y_class);
    EXPECT_FALSE(tesla_module_grouping(TeslaPackFamily::Model3YClass, bricks, true).known());  // S/X chassis
    EXPECT_EQ(tesla_module_grouping(TeslaPackFamily::LegacySX, bricks).known(), bricks == 96 || bricks == 84);
    EXPECT_EQ(tesla_module_grouping(TeslaPackFamily::LegacySX, bricks, true).known(), bricks == 96 || bricks == 84);
  }
}

// ---- TeslaBattery (3/Y) ----

class TeslaModuleGroupingBatteryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    saved_chassis_ = user_selected_tesla_GTW_chassisType;
    saved_cells_ = datalayer.battery.info.number_of_cells;
    saved_grouping_ = datalayer.battery.info.module_grouping;
    user_selected_tesla_GTW_chassisType = 2;  // Model 3
    datalayer.battery.info.number_of_cells = 0;
    datalayer.battery.info.module_grouping = ModuleGrouping();
  }
  void TearDown() override {
    user_selected_tesla_GTW_chassisType = saved_chassis_;
    datalayer.battery.info.number_of_cells = saved_cells_;
    datalayer.battery.info.module_grouping = saved_grouping_;
  }

  const ModuleGrouping& grouping() { return datalayer.battery.info.module_grouping; }

  uint16_t saved_chassis_ = 0;
  uint8_t saved_cells_ = 0;
  ModuleGrouping saved_grouping_;
};

TEST_F(TeslaModuleGroupingBatteryTest, UnknownBeforeAnyFrame) {
  TeslaBattery pack;
  pack.setup();
  EXPECT_EQ(datalayer.battery.info.number_of_cells, 0);
  EXPECT_FALSE(grouping().known());
}

TEST_F(TeslaModuleGroupingBatteryTest, Pack96StaysUnknown) {
  TeslaBattery pack;
  pack.setup();
  ScanFullFrames(&pack, 31);
  EXPECT_EQ(datalayer.battery.info.number_of_cells, 96);
  EXPECT_FALSE(grouping().known());
  EXPECT_EQ(Json(grouping()), "[]");
}

TEST_F(TeslaModuleGroupingBatteryTest, Pack106) {
  TeslaBattery pack;
  pack.setup();
  pack.handle_incoming_can_frame(BrickFrame(35, 0x02));
  EXPECT_EQ(datalayer.battery.info.number_of_cells, 106);
  EXPECT_EQ(Sizes(grouping()), (std::vector<uint8_t>{25, 28, 28, 25}));
}

TEST_F(TeslaModuleGroupingBatteryTest, Pack108) {
  TeslaBattery pack;
  pack.setup();
  ScanFullFrames(&pack, 35);
  EXPECT_EQ(datalayer.battery.info.number_of_cells, 108);
  EXPECT_EQ(Sizes(grouping()), (std::vector<uint8_t>{26, 28, 28, 26}));
}

TEST_F(TeslaModuleGroupingBatteryTest, ModelYChassis96AlsoStaysUnknown) {
  user_selected_tesla_GTW_chassisType = 3;
  TeslaBattery pack;
  pack.setup();
  ScanFullFrames(&pack, 31);
  EXPECT_EQ(datalayer.battery.info.number_of_cells, 96);
  EXPECT_FALSE(grouping().known());
}

TEST_F(TeslaModuleGroupingBatteryTest, UnexpectedCountsStayUnknown) {
  {
    TeslaBattery pack;  // a count that is only the first mux cycle
    pack.setup();
    pack.handle_incoming_can_frame(BrickFrame(0, 0x2A));
    pack.handle_incoming_can_frame(BrickFrame(0, 0x2A));
    EXPECT_EQ(datalayer.battery.info.number_of_cells, 3);
    EXPECT_FALSE(grouping().known());
  }
  {
    TeslaBattery pack;  // 36 full pages and a 1 brick page: 109 bricks
    pack.setup();
    pack.handle_incoming_can_frame(BrickFrame(36, 0x02));
    EXPECT_EQ(datalayer.battery.info.number_of_cells, 109);
    EXPECT_FALSE(grouping().known());
  }
  {
    TeslaBattery pack;  // 37 full pages: 111 bricks
    pack.setup();
    ScanFullFrames(&pack, 36);
    EXPECT_EQ(datalayer.battery.info.number_of_cells, 111);
    EXPECT_FALSE(grouping().known());
  }
}

TEST_F(TeslaModuleGroupingBatteryTest, ModelSXChassisHasNoGrouping) {
  for (uint16_t chassis : {0, 1}) {  // Model S, Model X (2021+): not supported, no grouping
    for (uint8_t last_mux : {31, 35}) {
      SCOPED_TRACE(chassis * 100 + last_mux);
      user_selected_tesla_GTW_chassisType = chassis;
      TeslaBattery pack;
      pack.setup();
      ScanFullFrames(&pack, last_mux);
      EXPECT_EQ(datalayer.battery.info.number_of_cells, last_mux == 31 ? 96 : 108);
      EXPECT_FALSE(grouping().known());
    }
  }
}

TEST_F(TeslaModuleGroupingBatteryTest, GroupingFollowsTheCountAndSetupClearsIt) {
  {
    TeslaBattery pack;
    pack.setup();
    ScanFullFrames(&pack, 35);
    EXPECT_TRUE(grouping().known());
    pack.handle_incoming_can_frame(BrickFrame(35, 0x02));  // the count moves to 106
    EXPECT_EQ(Sizes(grouping()), (std::vector<uint8_t>{25, 28, 28, 25}));
    pack.handle_incoming_can_frame(BrickFrame(36, 0x02));  // and to 109
    EXPECT_FALSE(grouping().known());
  }
  {
    TeslaBattery pack;
    pack.setup();
    ScanFullFrames(&pack, 35);
    ASSERT_TRUE(grouping().known());
  }
  datalayer.battery.info.number_of_cells = 0;
  TeslaBattery pack;
  pack.setup();
  EXPECT_FALSE(grouping().known());
}

// ---- TeslaLegacyBattery (S/X 2012-2020) ----

class TeslaModuleGroupingLegacyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    saved_cells_ = datalayer.battery.info.number_of_cells;
    saved_grouping_ = datalayer.battery.info.module_grouping;
  }
  void TearDown() override {
    datalayer.battery.info.number_of_cells = saved_cells_;
    datalayer.battery.info.module_grouping = saved_grouping_;
  }

  const ModuleGrouping& grouping() { return datalayer.battery.info.module_grouping; }

  uint8_t saved_cells_ = 0;
  ModuleGrouping saved_grouping_;
};

TEST_F(TeslaModuleGroupingLegacyTest, UnknownUntilTheCountIsRead) {
  TeslaLegacyBattery pack;
  pack.setup();  // a placeholder count of 14 is not 14 bricks
  EXPECT_FALSE(grouping().known());
}

TEST_F(TeslaModuleGroupingLegacyTest, Pack96) {
  TeslaLegacyBattery pack;
  pack.setup();
  FeedLegacyPack(pack, 96);
  EXPECT_EQ(datalayer.battery.info.number_of_cells, 96);
  EXPECT_EQ(Sizes(grouping()), std::vector<uint8_t>(16, 6));
  EXPECT_EQ(Json(grouping()), "[6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6]");
}

TEST_F(TeslaModuleGroupingLegacyTest, Pack84) {
  TeslaLegacyBattery pack;
  pack.setup();
  FeedLegacyPack(pack, 84);
  EXPECT_EQ(datalayer.battery.info.number_of_cells, 84);
  EXPECT_EQ(Sizes(grouping()), std::vector<uint8_t>(14, 6));
}

TEST_F(TeslaModuleGroupingLegacyTest, UnexpectedCountsStayUnknown) {
  for (uint16_t bricks : {4, 83, 88, 92, 95}) {
    SCOPED_TRACE(bricks);
    TeslaLegacyBattery pack;
    pack.setup();
    FeedLegacyPack(pack, bricks);
    EXPECT_EQ(datalayer.battery.info.number_of_cells, bricks);
    EXPECT_FALSE(grouping().known());
  }
}

TEST_F(TeslaModuleGroupingLegacyTest, SetupClearsAGroupingFromAnEarlierPack) {
  TeslaLegacyBattery first;
  first.setup();
  FeedLegacyPack(first, 96);
  ASSERT_TRUE(grouping().known());
  TeslaLegacyBattery second;
  second.setup();
  EXPECT_FALSE(grouping().known());
}
