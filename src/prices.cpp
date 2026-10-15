#include "prices.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace {

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const auto b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(ws) - b + 1);
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Plain comma split (no quoted fields: dates, tickers and prices never need them).
std::vector<std::string> splitCsv(const std::string& line) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (true) {
        const auto comma = line.find(',', start);
        out.push_back(trim(line.substr(start, comma == std::string::npos ? comma : comma - start)));
        if (comma == std::string::npos) return out;
        start = comma + 1;
    }
}

// A close must be a finite number greater than zero.
bool parseClose(const std::string& s, double& out) {
    if (s.empty() || s.find_first_of("xX") != std::string::npos) return false;  // no hex floats
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return *end == '\0' && std::isfinite(out) && out > 0;
}

}  // namespace

bool isValidDate(const std::string& d) {
    if (d.size() != 10 || d[4] != '-' || d[7] != '-') return false;
    for (std::size_t i : {0, 1, 2, 3, 5, 6, 8, 9})
        if (!std::isdigit(static_cast<unsigned char>(d[i]))) return false;
    const int y = std::stoi(d.substr(0, 4));
    const int m = std::stoi(d.substr(5, 2));
    const int day = std::stoi(d.substr(8, 2));
    static const int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12 || day < 1) return false;
    const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return day <= daysInMonth[m - 1] + (m == 2 && leap ? 1 : 0);
}

PriceBook parsePrices(std::istream& in, std::vector<std::string>& warnings) {
    // date -> close per instrument; std::map sorts by date and collapses duplicates for us.
    std::map<std::string, std::map<std::string, double>> sorted;

    int cDate = -1, cInstrument = -1, cClose = -1;
    std::size_t columns = 0;
    bool haveHeader = false;
    int lineNo = 0;
    std::string line;

    while (std::getline(in, line)) {
        ++lineNo;
        if (lineNo == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);  // UTF-8 BOM
        if (trim(line).empty()) continue;
        const auto f = splitCsv(line);

        if (!haveHeader) {
            for (std::size_t i = 0; i < f.size(); ++i) {
                const std::string name = lower(f[i]);
                if (name == "date") cDate = static_cast<int>(i);
                else if (name == "instrument") cInstrument = static_cast<int>(i);
                else if (name == "close") cClose = static_cast<int>(i);
            }
            if (cDate < 0 || cInstrument < 0 || cClose < 0)
                throw std::runtime_error("prices header must contain the columns: date, instrument, close");
            columns = f.size();
            haveHeader = true;
            continue;
        }

        const std::string where = "prices line " + std::to_string(lineNo) + ": ";
        if (f.size() != columns) { warnings.push_back(where + "wrong number of columns, row skipped"); continue; }
        const std::string& date = f[cDate];
        const std::string& instrument = f[cInstrument];
        double close = 0;
        if (!isValidDate(date)) { warnings.push_back(where + "invalid date '" + date + "', row skipped"); continue; }
        if (instrument.empty()) { warnings.push_back(where + "empty instrument, row skipped"); continue; }
        if (!parseClose(f[cClose], close)) { warnings.push_back(where + "invalid close '" + f[cClose] + "', row skipped"); continue; }

        const bool inserted = sorted[instrument].insert_or_assign(date, close).second;
        if (!inserted) warnings.push_back(where + "duplicate " + instrument + " " + date + ", the later row wins");
    }
    if (!haveHeader) throw std::runtime_error("prices file is empty (no header row)");

    PriceBook book;
    for (const auto& [instrument, byDate] : sorted)
        for (const auto& [date, close] : byDate) book[instrument].push_back({date, close});
    return book;
}

PriceBook loadPrices(const std::string& path, std::vector<std::string>& warnings) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open prices file '" + path + "'");
    return parsePrices(in, warnings);
}
