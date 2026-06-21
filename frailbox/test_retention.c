#define _GNU_SOURCE
#include "retention_report.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#define TEST_LOG_DIR "/tmp/frailbox_test_logs"

static int create_test_directory(void) {
    if (mkdir(TEST_LOG_DIR, 0755) != 0) {
        perror("mkdir");
        return -1;
    }
    return 0;
}

static void create_test_file(const char *filename, const char *content, int age_days) {
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", TEST_LOG_DIR, filename);

    FILE *file = fopen(full_path, "w");
    if (!file) {
        perror("fopen");
        return;
    }
    fprintf(file, "%s", content);
    fclose(file);

    struct stat st;
    if (stat(full_path, &st) == 0) {
        struct timespec ts;
        ts.tv_sec = st.st_mtime - (age_days * 24 * 60 * 60);
        ts.tv_nsec = 0;
        struct timespec times[2] = {ts, ts};
        utimensat(AT_FDCWD, full_path, times, 0);
    }
}

static void cleanup_test_directory(void) {
    char command[512];
    snprintf(command, sizeof(command), "rm -rf %s", TEST_LOG_DIR);
    system(command);
}

static int test_basic_retention_report(void) {
    printf("=== Test 1: Basic Retention Report ===\n");

    cleanup_test_directory();
    if (create_test_directory() != 0) {
        return 1;
    }

    create_test_file("log1.log", "Test log 1\n", 0);
    create_test_file("log2.log", "Test log 2\n", 5);
    create_test_file("log3.log", "Test log 3\n", 35);
    create_test_file("log4.log", "Test log 4\n", 40);

    generate_retention_report(TEST_LOG_DIR, 3, 30);

    cleanup_test_directory();
    printf("=== Test 1 Complete ===\n\n");
    return 0;
}

static int test_empty_directory(void) {
    printf("=== Test 2: Empty Directory ===\n");

    cleanup_test_directory();
    if (create_test_directory() != 0) {
        return 1;
    }

    generate_retention_report(TEST_LOG_DIR, 10, 7);

    cleanup_test_directory();
    printf("=== Test 2 Complete ===\n\n");
    return 0;
}

static int test_all_retained(void) {
    printf("=== Test 3: All Files Retained ===\n");

    cleanup_test_directory();
    if (create_test_directory() != 0) {
        return 1;
    }

    create_test_file("recent1.log", "Log 1\n", 0);
    create_test_file("recent2.log", "Log 2\n", 1);
    create_test_file("recent3.log", "Log 3\n", 2);

    generate_retention_report(TEST_LOG_DIR, 10, 30);

    cleanup_test_directory();
    printf("=== Test 3 Complete ===\n\n");
    return 0;
}

static int test_all_pruned(void) {
    printf("=== Test 4: All Files Pruned ===\n");

    cleanup_test_directory();
    if (create_test_directory() != 0) {
        return 1;
    }

    create_test_file("old1.log", "Log 1\n", 60);
    create_test_file("old2.log", "Log 2\n", 65);
    create_test_file("old3.log", "Log 3\n", 70);

    generate_retention_report(TEST_LOG_DIR, 2, 7);

    cleanup_test_directory();
    printf("=== Test 4 Complete ===\n\n");
    return 0;
}

static int test_max_files_only(void) {
    printf("=== Test 5: Max Files Only ===\n");

    cleanup_test_directory();
    if (create_test_directory() != 0) {
        return 1;
    }

    create_test_file("log1.log", "Log 1\n", 0);
    create_test_file("log2.log", "Log 2\n", 1);
    create_test_file("log3.log", "Log 3\n", 2);
    create_test_file("log4.log", "Log 4\n", 3);

    generate_retention_report(TEST_LOG_DIR, 2, -1);

    cleanup_test_directory();
    printf("=== Test 5 Complete ===\n\n");
    return 0;
}

static int test_max_age_only(void) {
    printf("=== Test 6: Max Age Only ===\n");

    cleanup_test_directory();
    if (create_test_directory() != 0) {
        return 1;
    }

    create_test_file("new1.log", "Log 1\n", 0);
    create_test_file("new2.log", "Log 2\n", 1);
    create_test_file("old1.log", "Log 3\n", 15);
    create_test_file("old2.log", "Log 4\n", 20);

    generate_retention_report(TEST_LOG_DIR, 0, 10);

    cleanup_test_directory();
    printf("=== Test 6 Complete ===\n\n");
    return 0;
}

int main(void) {
    printf("Retention Report Test Suite\n");
    printf("============================\n\n");

    int failed = 0;

    if (test_basic_retention_report() != 0) {
        printf("FAILED: test_basic_retention_report\n");
        failed++;
    }

    if (test_empty_directory() != 0) {
        printf("FAILED: test_empty_directory\n");
        failed++;
    }

    if (test_all_retained() != 0) {
        printf("FAILED: test_all_retained\n");
        failed++;
    }

    if (test_all_pruned() != 0) {
        printf("FAILED: test_all_pruned\n");
        failed++;
    }

    if (test_max_files_only() != 0) {
        printf("FAILED: test_max_files_only\n");
        failed++;
    }

    if (test_max_age_only() != 0) {
        printf("FAILED: test_max_age_only\n");
        failed++;
    }

    printf("============================\n");
    printf("Tests completed: %d failed\n", failed);

    return failed == 0 ? 0 : 1;
}