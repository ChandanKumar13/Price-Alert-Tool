// Loading and validating the rules JSON.
#pragma once
#include <string>
#include <vector>

enum class Direction { Above, Below };
enum class RuleType { Level, MaCross };

struct Rule {
    std::string id;
    std::string instrument;
    RuleType type = RuleType::Level;
    Direction direction = Direction::Above;
    double level = 0;      // Level rules
    int shortWindow = 0;   // MaCross rules
    int longWindow = 0;
};

// Throws std::runtime_error with a readable message on any invalid rule.
std::vector<Rule> parseRules(const std::string& jsonText);
std::vector<Rule> loadRules(const std::string& path);
