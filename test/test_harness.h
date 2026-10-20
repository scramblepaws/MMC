#pragma once

#include <QString>
#include <cstdio>

// A tiny check runner. No test framework, just counts.
struct TestRunner
{
    int checks = 0;
    int failures = 0;
    const char* current = "";

    void begin(const char* name)
    {
        current = name;
        std::printf("== %s\n", name);
    }

    void check(bool ok, const char* expression, const char* file, int line)
    {
        ++checks;
        if (!ok) {
            ++failures;
            std::printf("   FAIL %s (%s:%d)\n", expression, file, line);
        }
    }

    void checkEqual(const QString& actual, const QString& expected, const char* expression,
                    const char* file, int line)
    {
        ++checks;
        if (actual != expected) {
            ++failures;
            std::printf("   FAIL %s: got \"%s\", wanted \"%s\" (%s:%d)\n",
                        expression, qPrintable(actual), qPrintable(expected), file, line);
        }
    }

    void checkEqual(qsizetype actual, qsizetype expected, const char* expression,
                    const char* file, int line)
    {
        ++checks;
        if (actual != expected) {
            ++failures;
            std::printf("   FAIL %s: got %lld, wanted %lld (%s:%d)\n",
                        expression, static_cast<long long>(actual),
                        static_cast<long long>(expected), file, line);
        }
    }
};

#define CHECK(cond) runner.check((cond), #cond, __FILE__, __LINE__)
#define CHECK_EQ(actual, expected) \
    runner.checkEqual((actual), (expected), #actual, __FILE__, __LINE__)
