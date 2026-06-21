#pragma once

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TestCase {
    const char* name;
    const char* status; // "pass", "fail", or "skip"
    unsigned long duration_ms;
    const char* failure_reason;
} TestCase;

typedef struct TestSummary {
    size_t total;
    size_t passed;
    size_t failed;
    size_t skipped;
} TestSummary;

typedef struct TestResult {
    TestCase test;
    TestSummary summary;
    char* json_output;
    size_t json_output_len;
} TestResult;

void run_selftest_json(int output_fd);
TestResult* create_test_result(const char* name, const char* status, unsigned long duration_ms, const char* failure_reason);
void free_test_result(TestResult* result);

#ifdef __cplusplus
}
#endif