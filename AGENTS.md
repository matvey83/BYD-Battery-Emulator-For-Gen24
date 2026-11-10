# Notes for agents working in this fork

This fork is [matvey83/BYD-Battery-Emulator-For-Gen24](https://github.com/matvey83/BYD-Battery-Emulator-For-Gen24), tracking [dalathegreat/Battery-Emulator](https://github.com/dalathegreat/Battery-Emulator). Open pull requests against this fork's `main` only. Upstream docs still apply: `README.md`, `CONTRIBUTING.md`. Where they disagree with the code, follow the code.

## Layout and build

Firmware is PlatformIO Arduino for ESP32. `platformio.ini` sets `src_dir = ./Software`. `Software/Software.cpp` pins `core_loop` to `esp32hal->CORE_FUNCTION_CORE()` and connectivity/MQTT to `WIFICORE()`. The defaults in `Software/src/devboard/hal/hal.h` are core 1 and core 0. `CONTRIBUTING.md` says the opposite; do not copy that sentence.

`pio run -e lilygo_2CAN_330` compiles the LilyGo T-2CAN image (other envs are listed in `platformio.ini`). A successful compile is not hardware validation. Do not flash a board or do any high-voltage test unless the owner explicitly approves that action.

## Unit tests

GoogleTest under `test/`. `test/CMakeLists.txt` needs CMake 3.28 and C++20 and downloads googletest v1.17.0. The commands CI runs (`.github/workflows/unit-tests.yml`) are:

```sh
cd test && mkdir build && cd build && cmake .. && cmake --build . && ctest
./tests
```

`CONTRIBUTING.md` still describes an in-source `sudo cmake` plus apt `libgtest-dev`. That is not what CI or `CMakeLists.txt` do. `ctest` runs each case in its own process (`gtest_discover_tests`). `./tests` runs them in one process, which is what a local run does.

## Web UI conventions

Follow these on Tesla and related pages. Main does not implement all of them yet (the main card still prints `SOC` at 2 decimals, one `Total capacity` row, and no 30-minute SOC hold).

- Show `0` when zero is a valid reading. Use `N/A` only for invalid or missing data (not yet received, or an all-ones SNA).
- Round SOC on the main card to 1 decimal.
- Label the pack UI SOC `Battery SOC`. Use `Brick SOC` only on brick-SOC rows.
- Show both `Total capacity when new` and `Total capacity`. Do not switch one label between the two meanings.
- A stale value such as SOC may stay on screen for up to 30 minutes after the data stops, then it must be cleared or shown unavailable.

## OLED (on main)

SSD1306/SSD1309 128x64 I2C support landed upstream in [PR 3233](https://github.com/dalathegreat/Battery-Emulator/pull/3233) (`5812e502`) and is on this `main`. Code: `Software/src/devboard/display/display.cpp`, shared init `ssd130x_init.h`, called from `init_display()` in `Software/Software.cpp`. It is compiled out on `SMALL_FLASH_DEVICE`. The panel is probed at `0x3C`, then `0x3D`. Turn it on in Settings; the label still says SSD1306. On a LilyGo T-2CAN that option is `GPIOOPT1::I2C_DISPLAY_SSD1306` (SDA GPIO1, SCL GPIO2). The same option exists for T-CAN485 (`GPIOOPT4`, pins 14/15) and Waveshare (`GPIOOPT6`, GPIO1/GPIO2). A 2.42" SSD1309 on a T-2CAN is the panel this was brought up with; that hardware pairing is not named in the source.

## Tesla packs

Drivers on main: `TeslaBattery` is both `Tesla Model 3/Y` and `Tesla Model S/X` (`TESLA-BATTERY.cpp`). `TeslaLegacyBattery` is `Tesla Model S/X 2012-2020` (`TESLA-LEGACY-BATTERY.cpp`). There is no brick-to-module grouping on main: no grouping arrays, and nothing is shown as a module map. When grouping is added, fill those arrays only when the brick count exactly matches a known layout; otherwise leave them empty and show grouping as unknown.

Verified series counts, not module maps:

- Legacy: `0x6F2` carries four brick voltages per mux. After one full cycle the series count is the last non-zero slot. The comment names 96S full packs and 84S/83S early/smaller packs. `setup()` sets `number_of_cells = 14` only until that count arrives.
- 3/Y (chassis type > 1): the comment says NCM/A is 96s and LFP is 102–108s. Chemistry flips to LFP when `number_of_cells > 101`. Bricks come from `0x401`: status `0x2A` is three cells, `0x02` is a one-cell tail.

Not in this tree (do not treat as implemented): legacy packs as 14 modules × 6 bricks (84), and Plaid/Palladium (2021+ S/X) as 5 modules × 22 bricks (110). Plaid/Palladium module grouping is intentionally not to be implemented.

### CAC and "full pack when new" on main

`TeslaBattery` does not decode CAC amp-hours. `BMS_a143_SW_CAC_Change` is one alert bit (`0x320` mux 2, byte 3 bit 2). SOH is hardcoded to 99% (`soh_pptt = 9900`), which the unit test uses as proof there is no CAC reading. Cell voltages are published; the cell monitor drops cells still at 0 mV.

`TeslaLegacyBattery` stores only CAC minimum, from `0x7E2` mux 0, and uses it for SOH (`BMS_CAC_min / 2316`, new pack 231.6 Ah). Average, maximum, per-brick CAC, and brick SOC are not decoded. The minimum starts at the new-pack value, so SOH reads 100% before any frame.

Beginning-of-life energy on main is `0x292` bits 40–49, scale 0.1 kWh (`battery_beginning_of_life`), shown as "Battery Beginning of Life". `0x392` mux 1 does not decode bits 40–49; it decodes module type, pack mass, and platform max bus voltage. Nominal full pack energy is `0x352` (muxed 16-bit fields on newer firmware, 11-bit 0.1 kWh fields on older firmware).

Not on main, present only on the unmerged branch `tesla_read_CAC` (verified there, do not describe it as current `main`): `0x392` mux 1 bits 40–49 as full-pack-when-new energy, 0.1 kWh, preferred over `0x292` when plausible; pack CAC min/avg/max (not every brick) from `0x3B2` mux 0 on 3/Y, with two bit layouts; legacy `0x7E2` mux 0 avg/min/max CAC and mux 7 brick SOC min/max.

## Planned, not implemented

1. Optional OLED "battery health" page. A persisted web Settings checkbox, default off, display only. Do not change the BOOT button. Show SOC, Capacity, SOH with current/new kWh, cell imbalance in mV, CAC min–max Ah, CAC imbalance in Ah and %, and SOC imbalance in %. Use `--` for unknown values.
2. Startup logs for an OLED that sometimes stays blank after restart. Cause unknown. Log reset reason, display config, each I2C init step and error, and the first frame. Today `init_display()` only prints a single failure line per failed step.

## Commits

Keep commits small and focused. Do not commit CAN logs, secrets, or private pack data.
