#ifndef _WIN32  // popen and sys/wait.h
#include <gtest/gtest.h>

#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "../../Software/src/devboard/webserver/cellmonitor_js.h"

// Runs the pure helpers of the cell monitor page script (cellmonitor_js.h, the very text the page
// serves) under node with a few asserts. Skipped when node is not installed.

namespace {

// Appended to the page's helpers. Throws, and so exits non-zero, on the first failed assert.
const char js_checks[] = R"js(
const assert = require('assert');
const range = n => [...Array(n).keys()];
const gaps = q => q.map((c, i) => c < 0 ? i : -1).filter(i => i >= 0);
const cells = q => q.filter(c => c >= 0);

// The Modules row: exact text for a known grouping, nothing for an unknown one.
assert.strictEqual(modulesRow([23, 25, 25, 23]), '<br>Modules: 4 (23 / 25 / 25 / 23 bricks)');
assert.strictEqual(modulesRow(undefined), '');

// Gap columns at the boundaries, and every cell once, in order.
let q = columns(96, [23, 25, 25, 23]);
assert.strictEqual(q.length, 99);
assert.deepStrictEqual(gaps(q), [23, 49, 75]);
assert.deepStrictEqual(cells(q), range(96));

// No gaps when the grouping is unknown, a single module, or the sizes do not add up to the bars drawn.
assert.deepStrictEqual(columns(96, undefined), range(96));
assert.deepStrictEqual(columns(96, [96]), range(96));
assert.deepStrictEqual(columns(95, [23, 25, 25, 23]), range(95));
assert.deepStrictEqual(columns(97, [23, 25, 25, 23]), range(97));
assert.deepStrictEqual(columns(0, [23, 25, 25, 23]), []);

// Pointer lookup: columns map to cells, and a gap or a point outside the graph gives no cell.
assert.strictEqual(cellAt(q, 0), 0);
assert.strictEqual(cellAt(q, 22.5 / 99), 22);   // last bar of module 1
assert.strictEqual(cellAt(q, 23.5 / 99), -1);   // the gap
assert.strictEqual(cellAt(q, 24.5 / 99), 23);   // first bar of module 2, not 24
assert.strictEqual(cellAt(q, 98.5 / 99), 95);
assert.ok(!(cellAt(q, 1) >= 0));
assert.strictEqual(cellAt(columns(96, undefined), 0.5), 48);
)js";

bool node_available() {
  FILE* pipe = popen("node --version 2>&1", "r");
  if (!pipe) {
    return false;
  }
  char buffer[64];
  const bool printed = fgets(buffer, sizeof(buffer), pipe) != nullptr;
  const int status = pclose(pipe);
  return printed && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

}  // namespace

TEST(CellMonitorJsTest, ModulesRowGapColumnsAndPointerLookup) {
  if (!node_available()) {
    GTEST_SKIP() << "node is not installed";
  }

  const std::filesystem::path script =
      std::filesystem::temp_directory_path() / ("cellmonitor_js_test_" + std::to_string(getpid()) + ".js");
  {
    std::ofstream out(script);
    out << cellmonitor_logic_js << js_checks;
  }

  std::string output;
  FILE* pipe = popen(("node " + script.string() + " 2>&1").c_str(), "r");
  ASSERT_NE(pipe, nullptr);
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe)) {
    output += buffer;
  }
  const int status = pclose(pipe);
  std::filesystem::remove(script);

  EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0) << output;
}

#endif
