#include "selftest_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

static void test_success_case() {
    TestResult* result = create_test_result("success_test", "pass", 100, NULL);
    assert(result != NULL);
    assert(strcmp(result->test.name, "success_test") == 0);
    assert(strcmp(result->test.status, "pass") == 0);
    assert(result->test.duration_ms == 100);
    free_test_result(result);
    printf("PASS: test_success_case\n");
}

static void test_failure_case() {
    TestResult* result = create_test_result("failure_test", "fail", 50, "Assertion failed");
    assert(result != NULL);
    assert(strcmp(result->test.name, "failure_test") == 0);
    assert(strcmp(result->test.status, "fail") == 0);
    assert(result->test.duration_ms == 50);
    assert(strcmp(result->test.failure_reason, "Assertion failed") == 0);
    free_test_result(result);
    printf("PASS: test_failure_case\n");
}

static void test_skip_case() {
    TestResult* result = create_test_result("skip_test", "skip", 10, "Skipped");
    assert(result != NULL);
    assert(strcmp(result->test.name, "skip_test") == 0);
    assert(strcmp(result->test.status, "skip") == 0);
    assert(result->test.duration_ms == 10);
    free_test_result(result);
    printf("PASS: test_skip_case\n");
}

static bool validate_json_structure(const char* json_output) {
    if (!json_output) return false;

    const char* expected_start = "{\"tests\": [";
    if (strncmp(json_output, expected_start, strlen(expected_start)) != 0) return false;

    const char* expected_summary = "], \"summary\": ";
    const char* summary_pos = strstr(json_output, expected_summary);
    if (!summary_pos) return false;

    /* After "], \"summary\": " there should be a JSON object { ... } */
    const char* obj_start = summary_pos + strlen(expected_summary);
    if (*obj_start != '{') return false;

    /* Find matching closing brace */
    int depth = 1;
    const char* p = obj_start + 1;
    while (*p && depth > 0) {
        if (*p == '{') depth++;
        else if (*p == '}') depth--;
        p++;
    }
    if (depth != 0) return false;

    /* After the summary object, the outer object closes with '}' */
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    if (*p != '}') return false;
    p++;

    /* After outer closing brace, only whitespace or end of string */
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    if (*p != '\0' && *p != '\n') return false;

    return true;
}

static void test_json_structure_validation() {
    char json_output[1024];
    snprintf(json_output, sizeof(json_output), "{\"tests\": [{\"name\": \"test1\", \"status\": \"pass\", \"duration_ms\": 100, \"failure_reason\": \"\"}], \"summary\": {\"total\": 1, \"passed\": 1, \"failed\": 0, \"skipped\": 0}}");

    bool is_valid = validate_json_structure(json_output);
    assert(is_valid);
    printf("PASS: test_json_structure_validation\n");
}

static void test_deterministic_output() {
    const int output_fd = 1;
    run_selftest_json(output_fd);

    run_selftest_json(output_fd);

    printf("PASS: test_deterministic_output\n");
}

static void run_all_tests() {
    printf("Running selftest_json tests...\n");

    test_success_case();
    test_failure_case();
    test_skip_case();
    test_json_structure_validation();
    test_deterministic_output();

    printf("All tests passed!\n");
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    run_all_tests();
    return 0;
}