#include "engine.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace {

std::string formatNumber(double v) {  // 1500 -> "1500", 1500.25 -> "1500.25"
    std::ostringstream os;
    os << std::setprecision(15) << v;
    return os.str();
}

const char* directionName(Direction d) { return d == Direction::Above ? "above" : "below"; }

// Mean of the `window` bars ending at index `end` (inclusive). Summed directly each time
// (not a running sum) so rounding error cannot build up over long series.
double windowMean(const std::vector<Bar>& bars, std::size_t end, int window) {
    double sum = 0;
    for (std::size_t k = end + 1 - static_cast<std::size_t>(window); k <= end; ++k) sum += bars[k].close;
    return sum / window;
}

void runLevel(const Rule& r, const std::vector<Bar>& bars, std::vector<Alert>& out) {
    const std::string text = r.instrument + " level " + directionName(r.direction) + " " + formatNumber(r.level);
    CrossDetector detector;
    for (const Bar& bar : bars)
        if (detector.update(bar.close - r.level, r.direction)) out.push_back({bar.date, r.id, text});
}

void runMaCross(const Rule& r, const std::vector<Bar>& bars, std::vector<Alert>& out) {
    const std::string text = r.instrument + " ma_cross " + std::to_string(r.shortWindow) + "/" +
                             std::to_string(r.longWindow) + " " + directionName(r.direction);
    CrossDetector detector(1e-9);
    // Index longWindow-1 is the first day with a full long window; nothing is computed before it.
    for (std::size_t i = static_cast<std::size_t>(r.longWindow) - 1; i < bars.size(); ++i) {
        const double diff = windowMean(bars, i, r.shortWindow) - windowMean(bars, i, r.longWindow);
        if (detector.update(diff, r.direction)) out.push_back({bars[i].date, r.id, text});
    }
}

}  // namespace

bool CrossDetector::update(double value, Direction target) {
    const int side = value > eps_ ? 1 : (value < -eps_ ? -1 : 0);
    if (side == 0) return false;
    const int wanted = target == Direction::Above ? 1 : -1;
    const bool crossed = side == wanted && lastSide_ == -wanted;
    lastSide_ = side;
    return crossed;
}

bool idLess(const std::string& a, const std::string& b) {
    const auto isDigit = [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; };
    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (isDigit(a[i]) && isDigit(b[j])) {
            std::size_t i2 = i, j2 = j;
            while (i2 < a.size() && isDigit(a[i2])) ++i2;
            while (j2 < b.size() && isDigit(b[j2])) ++j2;
            std::string na = a.substr(i, i2 - i), nb = b.substr(j, j2 - j);
            na.erase(0, std::min(na.find_first_not_of('0'), na.size()));  // ignore leading zeros
            nb.erase(0, std::min(nb.find_first_not_of('0'), nb.size()));
            if (na.size() != nb.size()) return na.size() < nb.size();  // more digits = bigger number
            if (na != nb) return na < nb;
            i = i2;
            j = j2;
        } else {
            if (a[i] != b[j]) return a[i] < b[j];
            ++i;
            ++j;
        }
    }
    if (a.size() - i != b.size() - j) return a.size() - i < b.size() - j;
    return a < b;  // same number, different zero padding: stay deterministic
}

std::vector<Alert> evaluate(const PriceBook& book, const std::vector<Rule>& rules, std::vector<std::string>& warnings) {
    std::vector<Alert> alerts;
    for (const Rule& r : rules) {
        const auto it = book.find(r.instrument);
        if (it == book.end()) {
            warnings.push_back("rule " + r.id + ": no prices for instrument '" + r.instrument + "'");
            continue;
        }
        if (r.type == RuleType::Level) runLevel(r, it->second, alerts);
        else runMaCross(r, it->second, alerts);
    }
    std::sort(alerts.begin(), alerts.end(), [](const Alert& x, const Alert& y) {
        if (x.date != y.date) return x.date < y.date;
        return idLess(x.ruleId, y.ruleId);
    });
    return alerts;
}

std::string formatAlert(const Alert& a) { return a.date + " " + a.ruleId + " " + a.text; }
