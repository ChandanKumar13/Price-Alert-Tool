// A 30-line test harness so the tests build with no downloads (just a C++20 compiler).
#pragma once
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mini {
struct Case { std::string name; void (*fn)(); };
inline std::vector<Case>& cases() { static std::vector<Case> c; return c; }
struct Register { Register(const char* n, void (*f)()) { cases().push_back({n, f}); } };

inline int runAll() {
    std::size_t failed = 0;
    for (const auto& c : cases()) {
        try { c.fn(); std::cout << "[ ok ] " << c.name << "\n"; }
        catch (const std::exception& e) { ++failed; std::cout << "[FAIL] " << c.name << ": " << e.what() << "\n"; }
    }
    std::cout << cases().size() - failed << "/" << cases().size() << " tests passed\n";
    return failed == 0 ? 0 : 1;
}
}  // namespace mini

#define TEST(name) static void name(); static mini::Register reg_##name(#name, name); static void name()
#define CHECK(cond) do { if (!(cond)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": CHECK failed: " #cond); } while (0)
#define CHECK_THROWS(expr) do { bool threw_ = false; try { (void)(expr); } catch (const std::exception&) { threw_ = true; } \
    if (!threw_) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": expected an exception from: " #expr); } while (0)
