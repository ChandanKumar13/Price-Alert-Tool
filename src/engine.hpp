// The alert logic: crossing detection and rule evaluation.
#pragma once
#include <string>
#include <vector>

#include "prices.hpp"
#include "rules.hpp"

// Detects "crossed to the target side" from a stream of signed values
// (close - level, or shortMA - longMA). Positive = above, negative = below.
// A value within `eps` of zero is "on the line": it is neither side and does not
// change which side we were last on. So a touch followed by a move back to the
// same side is not a crossing, and a move through the line over several days is.
class CrossDetector {
public:
    explicit CrossDetector(double eps = 0) : eps_(eps) {}
    bool update(double value, Direction target);

private:
    double eps_;
    int lastSide_ = 0;  // +1 above, -1 below, 0 not seen yet
};

struct Alert {
    std::string date;
    std::string ruleId;
    std::string text;  // "INFY level below 1500" / "TCS ma_cross 5/20 above"
};

// Rule ids compare "naturally": r2 < r10 (digit runs compare as numbers).
bool idLess(const std::string& a, const std::string& b);

// Returns all alerts ordered by date, then rule id. Rules whose instrument has no prices add a warning.
std::vector<Alert> evaluate(const PriceBook& book, const std::vector<Rule>& rules, std::vector<std::string>& warnings);

std::string formatAlert(const Alert& a);
