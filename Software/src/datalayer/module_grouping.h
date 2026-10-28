#ifndef MODULE_GROUPING_H
#define MODULE_GROUPING_H

#include <stdint.h>
#include <stdio.h>

// How a pack's bricks (the entries of cell_voltages_mV, in index order) are
// grouped into modules. The battery driver fills it, the web pages only read it.
//
// module_count == 0 means unknown, and then nothing here may be trusted. A
// driver must only fill it when it is sure of the layout, so an incomplete or
// unexpected brick count has to leave it unknown.
//
// To support another battery type: keep a table of bricks per module for every
// brick count you know, and when number_of_cells changes call
//   info.module_grouping = ModuleGrouping::from_sizes(table, modules, info.number_of_cells);
// which stays unknown unless the sizes add up to the brick count exactly.
// See TESLA-MODULES.h for an example.
struct ModuleGrouping {
  static constexpr uint8_t MAX_MODULES = 16;

  uint8_t module_count = 0;
  uint8_t bricks_in_module[MAX_MODULES] = {};

  bool known() const { return module_count > 0; }

  uint16_t brick_total() const {
    uint16_t total = 0;
    for (uint8_t i = 0; i < module_count; i++) {
      total += bricks_in_module[i];
    }
    return total;
  }

  static ModuleGrouping from_sizes(const uint8_t* sizes, uint8_t count, uint16_t brick_count) {
    ModuleGrouping g;
    if (sizes == nullptr || count == 0 || count > MAX_MODULES) {
      return g;
    }
    uint16_t total = 0;
    for (uint8_t i = 0; i < count; i++) {
      if (sizes[i] == 0) {
        return g;
      }
      total += sizes[i];
    }
    if (total != brick_count) {
      return g;
    }
    g.module_count = count;
    for (uint8_t i = 0; i < count; i++) {
      g.bricks_in_module[i] = sizes[i];
    }
    return g;
  }

  // Appends the sizes as a JSON array, for example [23,25,25,23]. Out is
  // anything with += (const char*), such as String or std::string.
  template <typename Out>
  void append_json(Out& out) const {
    out += "[";
    for (uint8_t i = 0; i < module_count; i++) {
      char num[8];
      snprintf(num, sizeof(num), i ? ",%u" : "%u", (unsigned)bricks_in_module[i]);
      out += num;
    }
    out += "]";
  }
};

#endif
