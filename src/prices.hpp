// Loading the prices CSV into per-instrument series sorted by date.
#pragma once
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

struct Bar {
    std::string date;  // always a valid YYYY-MM-DD, so string order == date order
    double close;
};

// instrument -> bars sorted by date, one bar per date
using PriceBook = std::map<std::string, std::vector<Bar>>;

bool isValidDate(const std::string& d);

// Bad data rows are skipped and described in `warnings`; a bad/missing header throws.
PriceBook parsePrices(std::istream& in, std::vector<std::string>& warnings);
PriceBook loadPrices(const std::string& path, std::vector<std::string>& warnings);
