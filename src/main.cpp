// alerts <prices.csv> <rules.json>
// Alerts go to stdout; warnings and errors go to stderr.
#include <algorithm>
#include <iostream>

#include "engine.hpp"
#include "prices.hpp"
#include "rules.hpp"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: " << (argc > 0 ? argv[0] : "alerts") << " <prices.csv> <rules.json>\n";
        return 2;
    }
    try {
        std::vector<std::string> warnings;
        const PriceBook book = loadPrices(argv[1], warnings);
        const std::vector<Rule> rules = loadRules(argv[2]);

        for (const Alert& a : evaluate(book, rules, warnings)) std::cout << formatAlert(a) << '\n';

        constexpr std::size_t kMaxShown = 20;
        for (std::size_t i = 0; i < std::min(warnings.size(), kMaxShown); ++i)
            std::cerr << "warning: " << warnings[i] << '\n';
        if (warnings.size() > kMaxShown) std::cerr << "warning: ... and " << warnings.size() - kMaxShown << " more\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
