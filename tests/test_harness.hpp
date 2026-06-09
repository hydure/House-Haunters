#ifndef HH_TEST_HARNESS_HPP
#define HH_TEST_HARNESS_HPP
////////////////////////////////////////////////////////////
// test_harness.hpp
//
// Tiny in-tree test framework so we don't pull in a third-party
// dependency just to validate a couple of dozen pure-logic checks.
// Single-header, single-allocation registry, no exceptions in the hot
// path. Each test executable contains one `TEST_MAIN();` call, plus
// any number of `TEST_CASE("name") { ... }` blocks.
//
// Macros provided:
//   TEST_CASE("name")     - register a test function
//   CHECK(expr)           - record failure but keep going within the case
//   CHECK_EQ(a, b)        - CHECK with an "a == b" diagnostic
//   REQUIRE(expr)         - record failure and abort the current case
//   TEST_MAIN()           - emit the int main() that runs every TEST_CASE
//
// Exit code: 0 if every CHECK in every case passed, 1 otherwise.
// Uncaught exceptions count as a single failure on the offending case
// rather than crashing the binary -- useful when a stack of assertions
// fires inside a third-party header.
////////////////////////////////////////////////////////////

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace test_harness {

struct Result { int passed = 0; int failed = 0; };

struct TestCase {
    std::string name;
    std::function<void(Result&)> fn;
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void(Result&)> fn)
    {
        registry().push_back({name, std::move(fn)});
    }
};

inline int run_all()
{
    Result total;
    int failedCases = 0;
    for (const auto& tc : registry()) {
        std::cout << "[ RUN  ] " << tc.name << std::endl;
        Result r;
        try {
            tc.fn(r);
        }
        catch (const std::exception& ex) {
            std::cerr << "    UNCAUGHT EXCEPTION: " << ex.what() << std::endl;
            r.failed++;
        }
        catch (...) {
            std::cerr << "    UNCAUGHT EXCEPTION (unknown type)" << std::endl;
            r.failed++;
        }
        if (r.failed == 0) {
            std::cout << "[  OK  ] " << tc.name << " ("
                      << r.passed << " checks)" << std::endl;
        }
        else {
            std::cout << "[FAILED] " << tc.name << " ("
                      << r.passed << " passed, "
                      << r.failed << " failed)" << std::endl;
            ++failedCases;
        }
        total.passed += r.passed;
        total.failed += r.failed;
    }
    std::cout << "==========\n"
              << "Cases: " << registry().size() - failedCases << " ok / "
              << failedCases << " failed; "
              << "Checks: " << total.passed << " passed, "
              << total.failed << " failed" << std::endl;
    return failedCases == 0 ? 0 : 1;
}

} // namespace test_harness

// ---------- macro plumbing -------------------------------------------------

#define HH_TEST_CONCAT_INNER(a, b) a##b
#define HH_TEST_CONCAT(a, b) HH_TEST_CONCAT_INNER(a, b)

#define TEST_CASE(name)                                                                \
    static void HH_TEST_CONCAT(_test_fn_, __LINE__)(test_harness::Result&);            \
    static test_harness::Registrar HH_TEST_CONCAT(_test_reg_, __LINE__)(               \
        name, HH_TEST_CONCAT(_test_fn_, __LINE__));                                    \
    static void HH_TEST_CONCAT(_test_fn_, __LINE__)(test_harness::Result& _result)

#define CHECK(expr)                                                                    \
    do {                                                                               \
        if (expr) {                                                                    \
            _result.passed++;                                                          \
        }                                                                              \
        else {                                                                         \
            _result.failed++;                                                          \
            std::cerr << "    CHECK failed at " << __FILE__ << ":" << __LINE__         \
                      << ": " << #expr << std::endl;                                   \
        }                                                                              \
    } while (0)

#define CHECK_EQ(a, b)                                                                 \
    do {                                                                               \
        auto _a = (a);                                                                 \
        auto _b = (b);                                                                 \
        if (_a == _b) {                                                                \
            _result.passed++;                                                          \
        }                                                                              \
        else {                                                                         \
            _result.failed++;                                                          \
            std::ostringstream _oss;                                                   \
            _oss << "    CHECK_EQ failed at " << __FILE__ << ":" << __LINE__           \
                 << ": " << #a << " == " << #b << "\n"                                 \
                 << "        lhs: " << _a << "\n"                                      \
                 << "        rhs: " << _b;                                             \
            std::cerr << _oss.str() << std::endl;                                      \
        }                                                                              \
    } while (0)

#define REQUIRE(expr)                                                                  \
    do {                                                                               \
        if (expr) {                                                                    \
            _result.passed++;                                                          \
        }                                                                              \
        else {                                                                         \
            _result.failed++;                                                          \
            std::cerr << "    REQUIRE failed at " << __FILE__ << ":" << __LINE__       \
                      << ": " << #expr << " (aborting test case)" << std::endl;        \
            return;                                                                    \
        }                                                                              \
    } while (0)

#define TEST_MAIN()                                                                    \
    int main(int /*argc*/, char** /*argv*/)                                            \
    {                                                                                  \
        return test_harness::run_all();                                                \
    }

#endif // HH_TEST_HARNESS_HPP
