#include <sstream>

#include "engine.hpp"
#include "json.hpp"
#include "mini_test.hpp"
#include "prices.hpp"
#include "rules.hpp"

using Lines = std::vector<std::string>;

// ---------- helpers ----------

static Lines run(const std::string& csv, const std::string& rulesJson, std::vector<std::string>* warn = nullptr) {
    std::vector<std::string> w;
    std::istringstream in(csv);
    const PriceBook book = parsePrices(in, w);
    Lines out;
    for (const Alert& a : evaluate(book, parseRules(rulesJson), w)) out.push_back(formatAlert(a));
    if (warn) *warn = w;
    return out;
}

static std::string day(int d) { return std::string("2026-09-") + (d < 10 ? "0" : "") + std::to_string(d); }

// One instrument, closes on consecutive days starting 2026-09-01.
static std::string csv(const std::string& inst, const std::vector<double>& closes) {
    std::string s = "date,instrument,close\n";
    for (std::size_t i = 0; i < closes.size(); ++i)
        s += day(static_cast<int>(i) + 1) + "," + inst + "," + std::to_string(closes[i]) + "\n";
    return s;
}

static std::string levelRule(const std::string& id, const std::string& inst, const std::string& dir, const std::string& level) {
    return "{\"id\":\"" + id + "\",\"instrument\":\"" + inst + "\",\"type\":\"level\",\"direction\":\"" + dir + "\",\"level\":" + level + "}";
}
static std::string maRule(const std::string& id, const std::string& inst, int s, int l, const std::string& dir) {
    return "{\"id\":\"" + id + "\",\"instrument\":\"" + inst + "\",\"type\":\"ma_cross\",\"short\":" + std::to_string(s) +
           ",\"long\":" + std::to_string(l) + ",\"direction\":\"" + dir + "\"}";
}
static std::string rulesOf(const Lines& rules) {
    std::string s = "[";
    for (std::size_t i = 0; i < rules.size(); ++i) s += (i ? "," : "") + rules[i];
    return s + "]";
}

static void expectLines(const Lines& got, const Lines& want) {
    if (got == want) return;
    std::string msg = "output mismatch\n  got:";
    for (const auto& l : got) msg += "\n    " + l;
    msg += "\n  want:";
    for (const auto& l : want) msg += "\n    " + l;
    throw std::runtime_error(msg);
}

// ---------- the example from the brief ----------

TEST(brief_example) {
    const std::string prices =
        "date,instrument,close\n"
        "2026-09-01,INFY,1520.50\n2026-09-01,TCS,3890.00\n"
        "2026-09-02,INFY,1534.10\n2026-09-02,TCS,3902.25\n"
        "2026-09-03,INFY,1498.00\n";
    const std::string rules = rulesOf({levelRule("r1", "INFY", "below", "1500"), maRule("r2", "TCS", 5, 20, "above")});
    expectLines(run(prices, rules), {"2026-09-03 r1 INFY level below 1500"});
}

// ---------- ordering of the input ----------

TEST(out_of_order_rows_give_the_same_result_as_sorted_rows) {
    const std::string shuffled =
        "date,instrument,close\n2026-09-03,INFY,1498.00\n2026-09-01,INFY,1520.50\n2026-09-02,INFY,1534.10\n";
    expectLines(run(shuffled, rulesOf({levelRule("r1", "INFY", "below", "1500")})), {"2026-09-03 r1 INFY level below 1500"});
}

TEST(each_instrument_is_judged_only_against_its_own_previous_day) {
    // A has no row on day 2, B has no row on day 1/3: "previous day" means previous row of that instrument.
    const std::string prices =
        "date,instrument,close\n2026-09-01,A,11\n2026-09-02,B,50\n2026-09-03,A,9\n2026-09-04,B,5\n";
    expectLines(run(prices, rulesOf({levelRule("r1", "A", "below", "10"), levelRule("r2", "B", "above", "10")})),
                {"2026-09-03 r1 A level below 10"});
}

// ---------- level rules ----------

TEST(level_fires_once_per_crossing_and_again_only_after_recrossing) {
    expectLines(run(csv("X", {11, 12, 9, 8, 11, 7}), rulesOf({levelRule("a", "X", "below", "10")})),
                {"2026-09-03 a X level below 10", "2026-09-06 a X level below 10"});
}

TEST(level_above) {
    expectLines(run(csv("X", {9, 11, 12, 9, 13}), rulesOf({levelRule("a", "X", "above", "10")})),
                {"2026-09-02 a X level above 10", "2026-09-05 a X level above 10"});
}

TEST(level_does_not_fire_when_series_starts_on_the_target_side) {
    expectLines(run(csv("X", {9, 8, 7}), rulesOf({levelRule("a", "X", "below", "10")})), {});
}

TEST(level_single_row_never_fires) {
    expectLines(run(csv("X", {9}), rulesOf({levelRule("a", "X", "below", "10")})), {});
}

TEST(close_exactly_on_the_level_is_neither_side) {
    const std::string r = rulesOf({levelRule("a", "X", "below", "10")});
    expectLines(run(csv("X", {11, 10, 9}), r), {"2026-09-03 a X level below 10"});  // went through the level
    expectLines(run(csv("X", {9, 10, 9}), r), {});                                   // never was above
    expectLines(run(csv("X", {11, 10, 11}), r), {});                                 // touched, stayed above
}

TEST(level_output_prints_numbers_without_trailing_zeros) {
    expectLines(run(csv("X", {1600, 1400}), rulesOf({levelRule("a", "X", "below", "1500.25")})), {"2026-09-02 a X level below 1500.25"});
    expectLines(run(csv("X", {1, 0.05}), rulesOf({levelRule("a", "X", "below", "0.1")})), {"2026-09-02 a X level below 0.1"});
}

// ---------- moving-average cross rules ----------

TEST(ma_cross_fires_on_the_day_of_the_cross_only) {
    // short=2, long=3. Hand-computed: day5 short 9.5 > long 9 (was 7.5 < 8): above. day7 short 9.5 < long 10.33 (was 13 > 11): below.
    const std::string prices = csv("X", {10, 9, 8, 7, 12, 14, 5, 4});
    expectLines(run(prices, rulesOf({maRule("m1", "X", 2, 3, "above"), maRule("m2", "X", 2, 3, "below")})),
                {"2026-09-05 m1 X ma_cross 2/3 above", "2026-09-07 m2 X ma_cross 2/3 below"});
}

TEST(ma_cross_is_undefined_until_the_long_window_is_full) {
    const std::string both = rulesOf({maRule("u", "X", 2, 3, "above"), maRule("d", "X", 2, 3, "below")});
    expectLines(run(csv("X", {10, 9}), both), {});      // fewer days than the long window
    expectLines(run(csv("X", {10, 9, 8}), both), {});   // first defined day: nothing to cross from
    // 19 days against a 5/20 rule: a version that treated a missing long MA as 0 would fire here.
    std::vector<double> nineteen;
    for (int i = 0; i < 19; ++i) nineteen.push_back(100 + (i % 2 ? 5 : -5) + i);
    expectLines(run(csv("X", nineteen), rulesOf({maRule("u", "X", 5, 20, "above"), maRule("d", "X", 5, 20, "below")})), {});
}

TEST(ma_cross_on_flat_prices_never_fires) {
    expectLines(run(csv("X", {100, 100, 100, 100, 100, 100}),
                    rulesOf({maRule("u", "X", 2, 3, "above"), maRule("d", "X", 2, 3, "below")})), {});
}

TEST(ma_cross_works_with_out_of_order_rows) {
    const std::string prices =
        "date,instrument,close\n2026-09-05,X,12\n2026-09-03,X,8\n2026-09-01,X,10\n2026-09-04,X,7\n2026-09-02,X,9\n";
    expectLines(run(prices, rulesOf({maRule("m1", "X", 2, 3, "above")})), {"2026-09-05 m1 X ma_cross 2/3 above"});
}

// ---------- output ordering ----------

TEST(output_is_ordered_by_date_then_numeric_rule_id) {
    const std::string prices =
        "date,instrument,close\n2026-09-01,A,11\n2026-09-02,A,9\n2026-09-01,B,11\n2026-09-02,B,11\n2026-09-03,B,9\n";
    const std::string rules = rulesOf({levelRule("r10", "A", "below", "10"), levelRule("r2", "A", "below", "10"),
                                       levelRule("r1", "A", "below", "10"), levelRule("r0", "B", "below", "10")});
    expectLines(run(prices, rules), {"2026-09-02 r1 A level below 10", "2026-09-02 r2 A level below 10",
                                     "2026-09-02 r10 A level below 10", "2026-09-03 r0 B level below 10"});
}

TEST(rule_ids_compare_naturally) {
    CHECK(idLess("r2", "r10"));
    CHECK(!idLess("r10", "r2"));
    CHECK(!idLess("r1", "r1"));
    CHECK(idLess("2", "11"));
    CHECK(idLess("a9", "b1"));
    CHECK(idLess("r1", "r1a"));
    CHECK(idLess("r01", "r1") != idLess("r1", "r01"));  // zero padding: still a strict, deterministic order
}

// ---------- CSV robustness ----------

TEST(malformed_csv_rows_are_skipped_with_a_warning_each) {
    const std::string text =
        "date,instrument,close\n2026-09-01,INFY,100\noops\n2026-13-01,INFY,100\n2026-02-30,INFY,100\n"
        "2026-09-02,INFY,abc\n2026-09-03,INFY,-5\n2026-09-04,,100\n2026-09-05,INFY,nan\n2026-09-06,INFY,101\n";
    std::vector<std::string> w;
    std::istringstream in(text);
    const PriceBook book = parsePrices(in, w);
    CHECK(book.at("INFY").size() == 2);
    CHECK(book.at("INFY")[0].date == "2026-09-01");
    CHECK(book.at("INFY")[1].date == "2026-09-06");
    CHECK(w.size() == 7);
}

TEST(duplicate_date_for_an_instrument_keeps_the_later_row) {
    std::vector<std::string> w;
    std::istringstream in("date,instrument,close\n2026-09-01,A,10\n2026-09-01,A,20\n");
    const PriceBook book = parsePrices(in, w);
    CHECK(book.at("A").size() == 1);
    CHECK(book.at("A")[0].close == 20);
    CHECK(w.size() == 1);
}

TEST(csv_tolerates_bom_crlf_blank_lines_spaces_and_column_order) {
    std::vector<std::string> w;
    std::istringstream in("\xEF\xBB\xBF" "close,Date, instrument\r\n5, 2026-09-01 ,A\r\n\r\n6,2026-09-02,A\r\n");
    const PriceBook book = parsePrices(in, w);
    CHECK(book.at("A").size() == 2);
    CHECK(book.at("A")[0].date == "2026-09-01" && book.at("A")[0].close == 5);
    CHECK(book.at("A")[1].date == "2026-09-02" && book.at("A")[1].close == 6);
    CHECK(w.empty());
}

TEST(csv_without_a_usable_header_is_an_error) {
    std::vector<std::string> w;
    std::istringstream missingColumn("date,instrument\n2026-09-01,A\n");
    CHECK_THROWS(parsePrices(missingColumn, w));
    std::istringstream empty("");
    CHECK_THROWS(parsePrices(empty, w));
}

TEST(header_only_csv_is_fine_and_gives_no_alerts) {
    expectLines(run("date,instrument,close\n", rulesOf({levelRule("a", "X", "below", "10")})), {});
}

TEST(missing_files_are_errors) {
    std::vector<std::string> w;
    CHECK_THROWS(loadPrices("/definitely/not/here.csv", w));
    CHECK_THROWS(loadRules("/definitely/not/here.json"));
}

TEST(date_validation) {
    CHECK(isValidDate("2026-09-01"));
    CHECK(isValidDate("2024-02-29"));
    CHECK(!isValidDate("2026-02-29"));
    CHECK(!isValidDate("2100-02-29"));
    CHECK(isValidDate("2000-02-29"));
    CHECK(!isValidDate("2026-9-1"));
    CHECK(!isValidDate("2026-04-31"));
    CHECK(!isValidDate("01-09-2026"));
    CHECK(!isValidDate("2026-00-10"));
}

// ---------- rules robustness ----------

TEST(rule_for_an_instrument_without_prices_warns_and_prints_nothing) {
    std::vector<std::string> w;
    expectLines(run(csv("X", {1, 2}), rulesOf({levelRule("a", "ZZZ", "below", "10")}), &w), {});
    CHECK(w.size() == 1);
}

TEST(invalid_rules_are_rejected_with_errors) {
    CHECK_THROWS(parseRules("{}"));                                                                         // not an array
    CHECK_THROWS(parseRules("[1]"));                                                                        // not an object
    CHECK_THROWS(parseRules(rulesOf({levelRule("a", "X", "sideways", "10")})));                             // bad direction
    CHECK_THROWS(parseRules("[{\"id\":\"a\",\"instrument\":\"X\",\"type\":\"rsi\",\"direction\":\"above\"}]"));  // unknown type
    CHECK_THROWS(parseRules(rulesOf({maRule("a", "X", 20, 5, "above")})));                                  // short >= long
    CHECK_THROWS(parseRules(rulesOf({maRule("a", "X", 5, 5, "above")})));
    CHECK_THROWS(parseRules(rulesOf({maRule("a", "X", 0, 5, "above")})));                                   // window < 1
    CHECK_THROWS(parseRules("[{\"id\":\"a\",\"instrument\":\"X\",\"type\":\"ma_cross\",\"short\":2.5,\"long\":9,\"direction\":\"above\"}]"));
    CHECK_THROWS(parseRules(rulesOf({levelRule("a", "X", "below", "\"1500\"")})));                          // level not a number
    CHECK_THROWS(parseRules(rulesOf({levelRule("a", "X", "below", "1"), levelRule("a", "Y", "below", "1")})));  // duplicate id
    CHECK_THROWS(parseRules("[{\"instrument\":\"X\",\"type\":\"level\",\"direction\":\"above\",\"level\":1}]"));  // missing id
    CHECK_THROWS(parseRules("[{"));                                                                         // broken JSON
    CHECK(parseRules("[]").empty());
}

// ---------- JSON parser ----------

TEST(json_parses_the_supported_subset) {
    const json::Value v = json::parse(" {\"a\":[1,2.5,-3e2,true,null,\"x\\n\\\"y\"], \"b\":{}} ");
    const auto& a = v.object.at("a").array;
    CHECK(a.size() == 6);
    CHECK(a[0].number == 1 && a[1].number == 2.5 && a[2].number == -300);
    CHECK(a[3].type == json::Value::Type::Bool && a[3].boolean);
    CHECK(a[4].type == json::Value::Type::Null);
    CHECK(a[5].str == "x\n\"y");
    CHECK(v.object.at("b").object.empty());
}

TEST(json_rejects_malformed_input) {
    for (const char* bad : {"[1,]", "[1 2]", "{\"a\" 1}", "{\"a\":1,}", "\"abc", "[] x", "", "nul", "NaN", "-inf", "1e999", "[\"\\u0041\"]", "{1:2}"})
        CHECK_THROWS(json::parse(bad));
    CHECK_THROWS(json::parse(std::string(500, '[')));  // nesting limit, not a stack overflow
}

int main() { return mini::runAll(); }
