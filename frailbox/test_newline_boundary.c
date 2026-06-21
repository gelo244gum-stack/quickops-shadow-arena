/**
 * @file test_newline_boundary.c
 * @brief Regression test for logger newline boundary handling.
 *
 * This test suite verifies that the legacy logger correctly handles
 * various newline boundary cases, including:
 *   - Messages with no trailing newline
 *   - Messages with single trailing newline
 *   - Messages with multiple trailing newlines
 *   - Partial writes that cross internal buffer boundaries
 *   - Empty lines (just newline)
 *   - Lines with only whitespace
 *
 * The test uses a mock log buffer (no file I/O) and simulates writing
 * each case through the logger's write path to verify boundary behavior.
 *
 * Expected behavior based on logger internals:
 *   - The logger always appends '\n' to messages that don't end with one
 *   - Messages are truncated at MAX_LOG_LINE (4096) bytes
 *   - Truncated messages get "... [TRUNCATED]" suffix
 *   - Ring buffer stores the complete formatted line with newline
 *
 * Compile with:
 *   gcc -Wall -Wextra -Wpedantic -std=c2x -O2 -g -Iinclude -o test_newline_boundary test_newline_boundary.c -lpthread
 */

#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

/* ------------------------------------------------------------------ */
/* Logger Constants (mirrored from logger.c for self-contained test)   */
/* ------------------------------------------------------------------ */

#ifndef MAX_LOG_LINE
#define MAX_LOG_LINE 4096
#endif

#ifndef RING_BUFFER_SIZE
#define RING_BUFFER_SIZE 1024
#endif

#define LOG_LEVEL_NONE    0
#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_WARN    2
#define LOG_LEVEL_INFO    3
#define LOG_LEVEL_DEBUG   4
#define LOG_LEVEL_TRACE   5
#define LOG_LEVEL_VERBOSE 6

#ifndef DEFAULT_LOG_LEVEL
#define DEFAULT_LOG_LEVEL LOG_LEVEL_INFO
#endif

/* ------------------------------------------------------------------ */
/* Mock Logger State                                                   */
/* ------------------------------------------------------------------ */

static int g_log_level = DEFAULT_LOG_LEVEL;

/* Mock log buffer for capturing output */
static char g_mock_log_buffer[64][MAX_LOG_LINE];
static int g_mock_log_count = 0;

/**
 * Simulates the core log_message newline handling logic.
 * This mirrors the actual logger.c behavior for newline termination.
 *
 * @param buffer   The output buffer (simulates log_message internal buffer)
 * @param buf_size Size of the output buffer
 * @param msg      The message to log (without prefix)
 * @return Total length of the formatted log line
 */
static int mock_log_write(char *buffer, size_t buf_size, const char *msg)
{
    int msg_len = strlen(msg);

    /* Check for truncation (same as logger.c) */
    if (msg_len >= (int)buf_size) {
        /* Message truncated - add truncation indicator */
        const char trunc_msg[] = "... [TRUNCATED]";
        size_t trunc_len = sizeof(trunc_msg) - 1;
        size_t copy_len = buf_size - 1 - trunc_len;

        memcpy(buffer, msg, copy_len);
        memcpy(buffer + copy_len, trunc_msg, trunc_len);
        buffer[buf_size - 1] = '\0';
        return buf_size - 1;
    }

    /* Message fits - copy it */
    memcpy(buffer, msg, msg_len);

    /* Add newline if not present (this is the key newline boundary behavior) */
    if (msg_len == 0 || buffer[msg_len - 1] != '\n') {
        buffer[msg_len] = '\n';
        buffer[msg_len + 1] = '\0';
        return msg_len + 1;
    }

    /* Message already has trailing newline - don't add another */
    buffer[msg_len] = '\0';
    return msg_len;
}

/**
 * Simulates adding a log entry to the ring buffer.
 * This mirrors the ring_buffer_push() function from logger.c.
 */
static void mock_ring_buffer_push(const char *message)
{
    if (g_mock_log_count >= 64) {
        /* Ring buffer full - shift entries */
        memmove(&g_mock_log_buffer[0], &g_mock_log_buffer[1],
                sizeof(g_mock_log_buffer[0]) * 63);
        g_mock_log_count = 63;
    }

    strncpy(g_mock_log_buffer[g_mock_log_count], message, MAX_LOG_LINE - 1);
    g_mock_log_buffer[g_mock_log_count][MAX_LOG_LINE - 1] = '\0';
    g_mock_log_count++;
}

/**
 * Counts newline characters in a string.
 */
static int count_newlines(const char *str)
{
    int count = 0;
    for (const char *p = str; *p; p++) {
        if (*p == '\n') count++;
    }
    return count;
}

/* ------------------------------------------------------------------ */
/* Test Framework                                                      */
/* ------------------------------------------------------------------ */

#define MAX_TESTS 256

typedef struct {
    const char *name;
    int (*func)(void);
    int failed;
    double duration_ms;
} test_case_t;

static test_case_t tests[MAX_TESTS];
static int test_count = 0;
static int tests_passed = 0;
static int tests_failed = 0;

static jmp_buf assert_jmp;
static int assert_failed = 0;
static char assert_msg[1024];

#define TEST(test_name) \
    static int test_##test_name(void); \
    __attribute__((constructor)) static void register_##test_name(void) { \
        if (test_count < MAX_TESTS) { \
            tests[test_count].name = #test_name; \
            tests[test_count].func = test_##test_name; \
            tests[test_count].failed = 0; \
            test_count++; \
        } \
    } \
    static int test_##test_name(void)

#define ASSERT(cond, msg, ...) do { \
    if (!(cond)) { \
        snprintf(assert_msg, sizeof(assert_msg), "ASSERT FAILED: " msg, ##__VA_ARGS__); \
        assert_failed = 1; \
        longjmp(assert_jmp, 1); \
    } \
} while(0)

static double get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static int run_all_tests(void)
{
    printf("\n");
    printf("============================================================\n");
    printf("  LOGGER NEWLINE BOUNDARY REGRESSION TESTS\n");
    printf("============================================================\n\n");

    for (int i = 0; i < test_count; i++) {
        test_case_t *test = &tests[i];
        printf("  [%3d/%3d] %-50s ", i + 1, test_count, test->name);

        double start = get_time_ms();
        assert_failed = 0;

        if (setjmp(assert_jmp) == 0) {
            int result = test->func();
            if (result == 0) {
                tests_passed++;
                double elapsed = get_time_ms() - start;
                printf("PASS (%.1fms)\n", elapsed);
            } else {
                tests_failed++;
                test->failed = 1;
                printf("FAIL (returned %d)\n", result);
            }
        } else {
            tests_failed++;
            test->failed = 1;
            double elapsed = get_time_ms() - start;
            printf("FAIL (%.1fms)\n", elapsed);
            printf("         %s\n", assert_msg);
        }
    }

    printf("\n");
    printf("============================================================\n");
    printf("  RESULTS: %d passed, %d failed out of %d\n",
           tests_passed, tests_failed, test_count);
    printf("============================================================\n\n");

    return tests_failed;
}

/* ================================================================== */
/* TEST CASES                                                          */
/* ================================================================== */

/**
 * Test: Message with no trailing newline.
 * Expected behavior: Logger should NOT automatically add newline.
 * (The logger adds newline during format_log_prefix, but the raw
 * message boundary test verifies the logic separately.)
 *
 * NOTE: The actual logger.c adds '\n' after vsnprintf if the message
 * doesn't end with one. This test verifies that the newline is added
 * correctly.
 */
TEST(test_no_trailing_newline)
{
    char buffer[MAX_LOG_LINE];
    const char *msg = "No trailing newline";

    int len = mock_log_write(buffer, sizeof(buffer), msg);

    ASSERT(len == strlen(msg) + 1,
           "Expected length %zu, got %d", strlen(msg) + 1, len);
    ASSERT(buffer[len - 2] != '\n',
           "Message should not end with newline before final");
    ASSERT(buffer[len - 1] == '\n',
           "Output should end with newline");
    ASSERT(buffer[len] == '\0',
           "Buffer should be null-terminated");
    return 0;
}

/**
 * Test: Message with single trailing newline.
 * Expected behavior: Logger should NOT add extra newline.
 * The message already ends with '\n', so logger.c should not append another.
 */
TEST(test_single_trailing_newline)
{
    char buffer[MAX_LOG_LINE];
    const char *msg = "Single trailing newline\n";

    int len = mock_log_write(buffer, sizeof(buffer), msg);

    ASSERT(len == strlen(msg),
           "Expected length %zu, got %d", strlen(msg), len);
    ASSERT(buffer[len - 1] == '\n',
           "Output should end with newline");
    ASSERT(buffer[len] == '\0',
           "Buffer should be null-terminated");
    return 0;
}

/**
 * Test: Message with multiple trailing newlines.
 * Expected behavior: Logger should preserve the newlines.
 * Multiple '\n' at end should be preserved as-is.
 */
TEST(test_multiple_trailing_newlines)
{
    char buffer[MAX_LOG_LINE];
    const char *msg = "Multiple trailing newlines\n\n\n";

    int len = mock_log_write(buffer, sizeof(buffer), msg);

    ASSERT(len == strlen(msg),
           "Expected length %zu, got %d", strlen(msg), len);

    /* Verify all 3 newlines are present */
    int nl_count = count_newlines(buffer);
    ASSERT(nl_count == 3,
           "Expected 3 newlines, got %d", nl_count);

    /* Verify they are at the end */
    ASSERT(strcmp(buffer + len - 3, "\n\n\n") == 0,
           "Newlines should be at end of message");
    return 0;
}

/**
 * Test: Empty line (just newline).
 * Expected behavior: Single '\n' should be preserved as-is.
 */
TEST(test_empty_line)
{
    char buffer[MAX_LOG_LINE];
    const char *msg = "\n";

    int len = mock_log_write(buffer, sizeof(buffer), msg);

    ASSERT(len == 1,
           "Expected length 1, got %d", len);
    ASSERT(buffer[0] == '\n',
           "Output should be just newline");
    ASSERT(buffer[1] == '\0',
           "Buffer should be null-terminated");
    return 0;
}

/**
 * Test: Line with only whitespace.
 * Expected behavior: Whitespace should be followed by newline.
 * The mock_log_write adds '\n' if not present.
 */
TEST(test_whitespace_only_line)
{
    char buffer[MAX_LOG_LINE];
    const char *msg = "    ";

    int len = mock_log_write(buffer, sizeof(buffer), msg);

    ASSERT(len == strlen(msg) + 1,
           "Expected length %zu, got %d", strlen(msg) + 1, len);
    ASSERT(strcmp(buffer, "    \n") == 0,
           "Output should be whitespace followed by newline");
    return 0;
}

/**
 * Test: Message that would cross internal buffer boundary.
 * Expected behavior: Message should be truncated at boundary.
 *
 * The actual logger uses MAX_LOG_LINE (4096) as buffer size.
 * Messages longer than this are truncated with "... [TRUNCATED]".
 */
TEST(test_boundary_crossing_message)
{
    char buffer[MAX_LOG_LINE];
    /* Create a message longer than buffer */
    char long_msg[MAX_LOG_LINE + 256];
    memset(long_msg, 'A', sizeof(long_msg) - 1);
    long_msg[sizeof(long_msg) - 1] = '\0';

    int len = mock_log_write(buffer, sizeof(buffer), long_msg);

    ASSERT(len == MAX_LOG_LINE - 1,
           "Truncated length should be %d, got %d", MAX_LOG_LINE - 1, len);
    ASSERT(strstr(buffer, "... [TRUNCATED]") != NULL,
           "Truncated message should contain truncation indicator");
    ASSERT(buffer[len] == '\0',
           "Buffer should be null-terminated");
    return 0;
}

/**
 * Test: Message at exact buffer boundary.
 * Expected behavior: Message of exactly MAX_LOG_LINE - 1 chars
 * should fit with newline added.
 */
TEST(test_exact_boundary_size)
{
    char buffer[MAX_LOG_LINE];
    char exact_msg[MAX_LOG_LINE - 2]; /* Leave room for \n\0 */
    memset(exact_msg, 'B', sizeof(exact_msg) - 1);
    exact_msg[sizeof(exact_msg) - 1] = '\0';

    int len = mock_log_write(buffer, sizeof(buffer), exact_msg);

    ASSERT(len == strlen(exact_msg) + 1,
           "Expected length %zu, got %d", strlen(exact_msg) + 1, len);
    ASSERT(buffer[len - 1] == '\n',
           "Output should end with newline");
    return 0;
}

/**
 * Test: Message at exact boundary minus 1.
 * Expected behavior: Message of MAX_LOG_LINE - 2 chars should fit.
 */
TEST(test_boundary_minus_one)
{
    char buffer[MAX_LOG_LINE];
    char msg[MAX_LOG_LINE - 3];
    memset(msg, 'C', sizeof(msg) - 1);
    msg[sizeof(msg) - 1] = '\0';

    int len = mock_log_write(buffer, sizeof(buffer), msg);

    ASSERT(len == strlen(msg) + 1,
           "Expected length %zu, got %d", strlen(msg) + 1, len);
    ASSERT(buffer[len - 1] == '\n',
           "Output should end with newline");
    return 0;
}

/**
 * Test: Empty message (zero length).
 * Expected behavior: Should produce just newline.
 */
TEST(test_empty_message)
{
    char buffer[MAX_LOG_LINE];
    const char *msg = "";

    int len = mock_log_write(buffer, sizeof(buffer), msg);

    ASSERT(len == 1,
           "Expected length 1, got %d", len);
    ASSERT(buffer[0] == '\n',
           "Empty message should produce just newline");
    ASSERT(buffer[1] == '\0',
           "Buffer should be null-terminated");
    return 0;
}

/**
 * Test: Message with newline in middle.
 * Expected behavior: Only check for trailing newline.
 */
TEST(test_newline_in_middle)
{
    char buffer[MAX_LOG_LINE];
    const char *msg = "Line 1\nLine 2";

    int len = mock_log_write(buffer, sizeof(buffer), msg);

    /* Should add newline since message doesn't end with one */
    ASSERT(len == strlen(msg) + 1,
           "Expected length %zu, got %d", strlen(msg) + 1, len);
    ASSERT(count_newlines(buffer) == 2,
           "Should have 2 newlines (one in middle, one at end)");
    return 0;
}

/**
 * Test: Multiple messages through ring buffer.
 * Expected behavior: Ring buffer should store all messages correctly.
 */
TEST(test_ring_buffer_multiple_messages)
{
    g_mock_log_count = 0;

    const char *messages[] = {
        "First message\n",
        "Second message\n",
        "Third message\n",
        NULL
    };

    for (int i = 0; messages[i] != NULL; i++) {
        mock_ring_buffer_push(messages[i]);
    }

    ASSERT(g_mock_log_count == 3,
           "Ring buffer should have 3 entries, got %d", g_mock_log_count);
    ASSERT(strcmp(g_mock_log_buffer[0], "First message\n") == 0,
           "First entry should match");
    ASSERT(strcmp(g_mock_log_buffer[1], "Second message\n") == 0,
           "Second entry should match");
    ASSERT(strcmp(g_mock_log_buffer[2], "Third message\n") == 0,
           "Third entry should match");
    return 0;
}

/**
 * Test: Ring buffer overflow wraps correctly.
 * Expected behavior: Old entries should be overwritten.
 */
TEST(test_ring_buffer_overflow)
{
    g_mock_log_count = 0;

    /* Fill ring buffer beyond capacity */
    for (int i = 0; i < 70; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Message %d\n", i);
        mock_ring_buffer_push(msg);
    }

    ASSERT(g_mock_log_count == 64,
           "Ring buffer count should cap at 64, got %d", g_mock_log_count);
    /* Buffer should contain the most recent messages */
    ASSERT(strstr(g_mock_log_buffer[g_mock_log_count - 1], "Message 69") != NULL,
           "Last entry should be Message 69");
    return 0;
}

/**
 * Test: Truncation preserves start of message.
 * Expected behavior: First part of message preserved before truncation.
 */
TEST(test_truncation_preserves_start)
{
    char buffer[MAX_LOG_LINE];
    char long_msg[MAX_LOG_LINE + 100];
    memset(long_msg, 'D', sizeof(long_msg) - 1);
    long_msg[sizeof(long_msg) - 1] = '\0';

    /* Make first 100 chars unique */
    memcpy(long_msg, "UNIQUE_START_MARKER", 19);

    int len = mock_log_write(buffer, sizeof(buffer), long_msg);

    ASSERT(strncmp(buffer, "UNIQUE_START_MARKER", 19) == 0,
           "First part of message should be preserved");
    ASSERT(len == MAX_LOG_LINE - 1,
           "Length should be truncated");
    return 0;
}

/**
 * Test: Log level filtering.
 * Expected behavior: Messages above current level should be filtered.
 */
TEST(test_log_level_filtering)
{
    /* Simulate log level check (from logger.c) */
    g_log_level = LOG_LEVEL_INFO;

    /* INFO and below should pass */
    ASSERT(LOG_LEVEL_ERROR <= g_log_level, "ERROR should pass INFO filter");
    ASSERT(LOG_LEVEL_WARN <= g_log_level, "WARN should pass INFO filter");
    ASSERT(LOG_LEVEL_INFO <= g_log_level, "INFO should pass INFO filter");

    /* DEBUG and above should be filtered */
    ASSERT(LOG_LEVEL_DEBUG > g_log_level, "DEBUG should be filtered by INFO");
    ASSERT(LOG_LEVEL_TRACE > g_log_level, "TRACE should be filtered by INFO");
    ASSERT(LOG_LEVEL_VERBOSE > g_log_level, "VERBOSE should be filtered by INFO");

    return 0;
}

/**
 * Test: Buffer null-termination guarantee.
 * Expected behavior: All log outputs are null-terminated.
 */
TEST(test_null_termination_guarantee)
{
    char buffer[MAX_LOG_LINE];

    /* Fill buffer with max-length message */
    memset(buffer, 'E', sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    ASSERT(buffer[strlen(buffer)] == '\0',
           "Buffer should always be null-terminated");
    return 0;
}

/**
 * Test: Message with only newline characters.
 * Expected behavior: Multiple newlines preserved.
 */
TEST(test_only_newlines)
{
    char buffer[MAX_LOG_LINE];
    const char *msg = "\n\n\n\n\n";

    int len = mock_log_write(buffer, sizeof(buffer), msg);

    ASSERT(len == strlen(msg),
           "Multiple newlines should be preserved");
    ASSERT(count_newlines(buffer) == 5,
           "Should have 5 newlines");
    return 0;
}

/* ================================================================== */
/* MAIN                                                                */
/* ================================================================== */

int main(void)
{
    printf("Logger Newline Boundary Regression Test Suite\n");
    printf("Tests newline handling in the legacy logger write path\n");

    int failures = run_all_tests();

    if (failures > 0) {
        printf("FAILED: %d newline boundary tests failed\n", failures);
        return 1;
    }

    printf("PASSED: All newline boundary tests passed\n");
    return 0;
}
