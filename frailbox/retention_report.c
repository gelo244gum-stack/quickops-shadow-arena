#define _GNU_SOURCE
#include "retention_report.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

#define DEFAULT_MAX_FILES 100
#define DEFAULT_MAX_AGE_DAYS 30

static time_t get_current_time(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        perror("clock_gettime");
        return 0;
    }
    return ts.tv_sec;
}

static int is_hidden_file(const char *filename) {
    return filename[0] == '.';
}

static int compare_files_by_mtime(const void *a, const void *b) {
    const struct LogFileInfo *file_a = (const struct LogFileInfo *)a;
    const struct LogFileInfo *file_b = (const struct LogFileInfo *)b;
    if (file_a->mtime < file_b->mtime)
        return 1;
    if (file_a->mtime > file_b->mtime)
        return -1;
    return 0;
}

void generate_retention_report(const char *log_dir, int max_files, int max_age_days) {
    if (!log_dir || max_files <= 0 || max_age_days < 0) {
        fprintf(stderr, "Invalid parameters: log_dir=%s, max_files=%d, max_age_days=%d\n",
                log_dir ? log_dir : "NULL", max_files, max_age_days);
        return;
    }

    DIR *dir = opendir(log_dir);
    if (!dir) {
        perror("opendir");
        return;
    }

    struct LogFileInfo files[1024];
    int count = 0;
    time_t now = get_current_time();
    int max_age_seconds = max_age_days * 24 * 60 * 60;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (count >= 1024) {
            fprintf(stderr, "Warning: Maximum file limit reached (1024)\n");
            break;
        }

        if (is_hidden_file(entry->d_name))
            continue;

        char full_path[MAX_FILENAME_LEN + 256];
        snprintf(full_path, sizeof(full_path), "%s/%s", log_dir, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) {
            perror("stat");
            continue;
        }

        if (!S_ISREG(st.st_mode))
            continue;

        char ext[16];
        const char *dot = strrchr(entry->d_name, '.');
        if (dot && strlen(dot) < sizeof(ext)) {
            strcpy(ext, dot);
        } else {
            strcpy(ext, "");
        }

        if (strcmp(ext, ".log") != 0)
            continue;

        struct LogFileInfo *info = &files[count];
        strncpy(info->filename, entry->d_name, MAX_FILENAME_LEN - 1);
        info->filename[MAX_FILENAME_LEN - 1] = '\0';
        info->size = st.st_size;
        info->mtime = st.st_mtime;
        info->retained = 1;
        info->reason[0] = '\0';

        count++;
    }

    closedir(dir);

    if (count == 0) {
        fprintf(stdout, "No .log files found in %s\n", log_dir);
        return;
    }

    qsort(files, count, sizeof(struct LogFileInfo), compare_files_by_mtime);

    for (int i = 0; i < count; i++) {
        struct LogFileInfo *info = &files[i];
        time_t age = now - info->mtime;

        if (max_age_days >= 0 && age > (time_t)max_age_seconds) {
            info->retained = 0;
            snprintf(info->reason, MAX_REASON_LEN, "age > %d days (%ld days old)",
                    max_age_days, age / (24 * 60 * 60));
        } else if (max_files > 0 && i >= max_files) {
            info->retained = 0;
            snprintf(info->reason, MAX_REASON_LEN, "beyond max files (%d files, max=%d)",
                    i + 1, max_files);
        } else {
            snprintf(info->reason, MAX_REASON_LEN, "retained");
        }
    }

    print_retention_report(files, count);
}

void print_retention_report(struct LogFileInfo *files, int count) {
    printf("Log File Retention Report\n");
    printf("==========================\n");
    printf("%-30s %10s %12s %15s %20s\n",
           "Filename", "Size", "Mtime", "Retained", "Reason");
    printf("%-30s %10s %12s %15s %20s\n",
           "--------", "----", "------", "--------", "------");

    for (int i = 0; i < count; i++) {
        struct LogFileInfo *info = &files[i];
        char mtime_str[32];
        time_t mtime = info->mtime;
        struct tm *tm_info = localtime(&mtime);
        strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%d %H:%M:%S", tm_info);

        printf("%-30s %10ld %12s %15s %20s\n",
               info->filename,
               info->size,
               mtime_str,
               info->retained ? "YES" : "NO",
               info->reason);
    }

    int retained_count = 0;
    for (int i = 0; i < count; i++) {
        if (files[i].retained)
            retained_count++;
    }

    printf("\nSummary:\n");
    printf("Total .log files: %d\n", count);
    printf("Retained: %d\n", retained_count);
    printf("Pruned: %d\n", count - retained_count);
}