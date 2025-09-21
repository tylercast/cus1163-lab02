#include "proc_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define PROC_DIR "/proc"
#define BUF_SIZE 4096

// ---------- helpers ----------

static int is_all_digits(const char *s) {
    if (!s || !*s) return 0;
    for (const char *p = s; *p; ++p)
        if (!isdigit((unsigned char)*p)) return 0;
    return 1;
}

static int read_file_syscalls(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open"); return -1; }

    char buf[BUF_SIZE];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        if (write(STDOUT_FILENO, buf, (size_t)n) < 0) {
            perror("write");
            close(fd);
            return -1;
        }
    }
    if (n < 0) perror("read");
    if (close(fd) < 0) perror("close");
    return (n < 0) ? -1 : 0;
}

static int read_first_n_lines_syscalls(const char *path, int nlines) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open"); return -1; }

    char buf[BUF_SIZE];
    ssize_t n;
    int lines = 0;

    while ((n = read(fd, buf, sizeof(buf))) > 0 && lines < nlines) {
        for (ssize_t i = 0; i < n && lines < nlines; ++i) {
            if (write(STDOUT_FILENO, &buf[i], 1) < 0) {
                perror("write");
                close(fd);
                return -1;
            }
            if (buf[i] == '\n') lines++;
        }
    }
    if (n < 0) perror("read");
    if (close(fd) < 0) perror("close");
    return (n < 0) ? -1 : 0;
}

static void print_table_header(void) {
    printf("PID      Type                \n");
    printf("---      ----                \n");
}

// ---------- REQUIRED API ----------

int list_process_directories(void) {
    DIR *dir = opendir(PROC_DIR);
    if (!dir) { perror("opendir"); return -1; }

    printf("Process directories in /proc:\n");
    print_table_header();

    struct dirent *ent;
    int count = 0;
    struct stat st;
    char path[PATH_MAX];

    while ((ent = readdir(dir)) != NULL) {
        if (!is_all_digits(ent->d_name)) continue;

        int is_dir = 0;
#ifdef DT_DIR_
