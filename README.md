## Build & Run

- **Needs:** a C++20 compiler (g++ 10+, clang 12+, MSVC 2019+). Nothing else.
- **Run:** `make && ./alerts prices.csv rules.json`
- **Tests:** `make test` (26 tests)
- **Without make:** `g++ -std=c++20 -O2 -Isrc src/*.cpp -o alerts` (list the files by hand on Windows)
- Alerts go to stdout, warnings/errors to stderr. Exit code: 0 ok, 1 bad input, 2 wrong usage.

## How I used the assistant
- **CSV & Rules Parsers (`src/prices.*`, `src/json.*`, `src/rules.*`):** *"Write zero-dependency C++20 parsers for the prices CSV (handling UTF-8 BOM, flexible column order, leap-year date validation, out-of-order sorting, and duplicate dates) and a recursive-descent JSON reader that strictly validates `level` and `ma_cross` rule schemas."*

- **Crossing Engine & Ordering (`src/engine.*`):** *"Implement a stateful `CrossDetector` for `level` and `ma_cross` evaluation where moving averages are only defined once the full `long` window is populated, and sort fired alerts by date then natural alphanumeric rule ID (`r2` before `r10`)."*

- **Verification Suite (`tests/test_alerts.cpp`):** *"Write a self-contained 26-test harness without external libraries covering boundary touches, warm-up windows, flat series, dirty CSV rows, and malformed JSON rules."*

## What the assistant got wrong, and what I changed

- **Boundary touches (`CrossDetector::update`):** The assistant initially used `<=` / `>=`, which caused a touch on the level followed by a move back to the same side (`9 -> 10 -> 9` for `below 10`) to fire falsely. I changed `CrossDetector` to treat values within `eps` of zero as neutral (`side == 0`) without overwriting `lastSide_`.

- **Running-sum float drift (`windowMean`):** The assistant suggested a sliding window sum (`sum += close[i] - close[i - w]`), which accumulates floating-point rounding error over long series and can cause false crosses on flat prices. I changed `windowMean()` to sum the window directly each day and added a `1e-9` epsilon to `ma_cross`.

- **Rule ID tie-breaking (`idLess`):** The assistant initially used `std::stoll` with a string fallback, which fails on prefixed IDs like `"r2"` vs `"r10"`. I replaced it with `idLess()` to compare embedded digit runs numerically while ignoring leading zeros.