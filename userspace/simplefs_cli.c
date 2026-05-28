// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>

#include <dirent.h>

#include <time.h>
#include <sys/random.h>

#include <sys/ioctl.h>
#include <sys/stat.h>

#include "../simplefs.h"

#define SIMPLEFS_PATH_MAX 512

struct command {
    const char *name;
    int (*handler)(int argc, char **argv);
    const char *usage;
};

static void usage(const char *prog);

static int cmd_demo(int argc __attribute__((unused)), char **argv);
static int cmd_zero(int argc __attribute__((unused)), char **argv);
static int cmd_erase(int argc __attribute__((unused)), char **argv);
static int cmd_meta(int argc __attribute__((unused)), char **argv);
static int cmd_mapping(int argc __attribute__((unused)), char **argv);
static int cmd_verify(int argc __attribute__((unused)), char **argv);

static struct command commands[] = {
    {
        .name = "demo",
        .handler = cmd_demo,
        .usage = "demo <mountpoint>",
    },
    {
        .name = "zero",
        .handler = cmd_zero,
        .usage = "zero <mountpoint>",
    },
    {
        .name = "erase",
        .handler = cmd_erase,
        .usage = "erase <mountpoint>",
    },
    {
        .name = "meta",
        .handler = cmd_meta,
        .usage = "meta <mountpoint>",
    },
    {
        .name = "mapping",
        .handler = cmd_mapping,
        .usage = "mapping <mountpoint> <filename>",
    },
    {
        .name = "verify",
        .handler = cmd_verify,
        .usage = "verify <mountpoint>",
    },
};

static bool simplefs_is_file_name(const char *name)
{
    return strncmp(name, "file_", 5) == 0;
}

static int validate_mountpoint(const char *mnt)
{
    struct stat st;

    if (stat(mnt, &st) < 0) {
        fprintf(stderr,
                "stat(%s) failed: %s\n",
                mnt,
                strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr,
                "%s is not a directory\n",
                mnt);
        return -1;
    }

    return 0;
}

static void usage(const char *prog)
{
    size_t i;

    fprintf(stderr, "Usage:\n");

    for (i = 0;
         i < sizeof(commands) / sizeof(commands[0]);
         i++) {

        fprintf(stderr,
                "  %s %s\n",
                prog,
                commands[i].usage);
    }
}

static int open_any_file(
    const char *mnt,
    char *fullpath,
    size_t size)
{
    DIR *dir;

    struct dirent *de;

    int fd = -1;

    dir = opendir(mnt);

    if (!dir) {
        fprintf(stderr,
                "opendir(%s) failed: %s\n",
                mnt,
                strerror(errno));
        return -1;
    }

    while ((de = readdir(dir))) {

        if (!simplefs_is_file_name(de->d_name))
            continue;

        if (snprintf(fullpath,
                     size,
                     "%s/%s",
                     mnt,
                     de->d_name)
            >= (int)size)
            continue;

        fd = open(fullpath, O_RDWR);

        if (fd >= 0)
            break;
    }

    closedir(dir);

    return fd;
}

static int simplefs_ioctl_call(
    const char *mnt,
    unsigned long request,
    void *arg)
{
    char path[SIMPLEFS_PATH_MAX];

    int fd;

    int ret = -1;

    fd = open_any_file(
        mnt,
        path,
        sizeof(path)
    );

    if (fd < 0) {
        fprintf(stderr,
                "failed to open simplefs file\n");
        return -1;
    }

    if (ioctl(fd, request, arg) < 0) {

        fprintf(stderr,
                "ioctl(%lu) failed: %s\n",
                request,
                strerror(errno));

        goto out;
    }

    ret = 0;

out:

    close(fd);

    return ret;
}

static int random_u64(uint64_t *v)
{
    ssize_t ret;

    ret = getrandom(
        v,
        sizeof(*v),
        0
    );

    if (ret != sizeof(*v)) {

        fprintf(stderr,
                "getrandom failed: %s\n",
                strerror(errno));

        return -1;
    }

    return 0;
}

static int cmd_demo(int argc __attribute__((unused)), char **argv)
{
    const char *mnt = argv[2];

    DIR *dir;

    struct dirent *de;

    int ok = 0;
    int fail = 0;

    if (validate_mountpoint(mnt))
        return 1;

    dir = opendir(mnt);

    if (!dir) {
        perror("opendir");
        return 1;
    }

    while ((de = readdir(dir))) {

        char path[SIMPLEFS_PATH_MAX];

        int fd;

        uint64_t wr;
        uint64_t rd = 0;

        if (!simplefs_is_file_name(de->d_name))
            continue;

        if (snprintf(path,
                     sizeof(path),
                     "%s/%s",
                     mnt,
                     de->d_name)
            >= (int)sizeof(path))
            continue;

        fd = open(path, O_RDWR);

        if (fd < 0) {

            fprintf(stderr,
                    "open(%s) failed: %s\n",
                    path,
                    strerror(errno));

            fail++;
            continue;
        }

        if (random_u64(&wr)) {
            close(fd);
            fail++;
            continue;
        }

        if (pwrite(fd,
                   &wr,
                   sizeof(wr),
                   0)
            != sizeof(wr)) {

            fprintf(stderr,
                    "pwrite(%s) failed: %s\n",
                    path,
                    strerror(errno));

            close(fd);

            fail++;
            continue;
        }

        fsync(fd);

        if (pread(fd,
                  &rd,
                  sizeof(rd),
                  0)
            != sizeof(rd)) {

            fprintf(stderr,
                    "pread(%s) failed: %s\n",
                    path,
                    strerror(errno));

            close(fd);

            fail++;
            continue;
        }

        close(fd);

        if (wr == rd) {

            printf("[OK]   %s  0x%016llx\n",
                   de->d_name,
                   (unsigned long long)wr);

            ok++;

        } else {

            printf("[FAIL] %s  "
                   "wrote=0x%016llx "
                   "read=0x%016llx\n",
                   de->d_name,
                   (unsigned long long)wr,
                   (unsigned long long)rd);

            fail++;
        }
    }

    closedir(dir);

    printf("\nTotal: ok=%d fail=%d\n",
           ok,
           fail);

    return fail ? 1 : 0;
}

static int cmd_zero(int argc __attribute__((unused)), char **argv)
{
    const char *mnt = argv[2];

    if (simplefs_ioctl_call(
            mnt,
            SIMPLEFS_IOC_ZERO_ALL,
            NULL))
        return 1;

    printf("All files zeroed.\n");

    return 0;
}

static int cmd_erase(int argc __attribute__((unused)), char **argv)
{
    const char *mnt = argv[2];

    if (simplefs_ioctl_call(
            mnt,
            SIMPLEFS_IOC_ERASE_FS,
            NULL))
        return 1;

    printf("Filesystem erased.\n");

    return 0;
}

static int cmd_meta(int argc __attribute__((unused)), char **argv)
{
    const char *mnt = argv[2];

    struct simplefs_meta_list hdr;

    struct simplefs_file_meta *arr = NULL;

    unsigned int max = 1024;

    unsigned int i;

    int ret = 1;

    arr = calloc(max, sizeof(*arr));

    if (!arr) {

        fprintf(stderr,
                "calloc failed\n");

        return 1;
    }

    memset(&hdr, 0, sizeof(hdr));

    hdr.max_count = max;

    hdr.entries_ptr =
        (uint64_t)(uintptr_t)arr;

    if (simplefs_ioctl_call(
            mnt,
            SIMPLEFS_IOC_GET_META,
            &hdr))
        goto out;

    printf("%-20s %-12s %-12s %s\n",
           "NAME",
           "OFFSET",
           "SIZE",
           "CRC32");

    printf("--------------------------------------------------\n");

    for (i = 0; i < hdr.count; i++) {

        printf("%-20s %-12llu %-12llu 0x%08x\n",
               arr[i].name,
               (unsigned long long)
               arr[i].offset_sector,
               (unsigned long long)
               arr[i].size_bytes,
               arr[i].content_hash);
    }

    ret = 0;

out:

    free(arr);

    return ret;
}

static int cmd_meta(int argc __attribute__((unused)), char **argv)
{
    const char *mnt = argv[2];

    struct simplefs_meta_list hdr;

    struct simplefs_file_meta *arr = NULL;

    unsigned int i;

    int ret = 1;

    memset(&hdr, 0, sizeof(hdr));

    if (simplefs_ioctl_call(
            mnt,
            SIMPLEFS_IOC_GET_META,
            &hdr))
        return 1;

    arr = calloc(hdr.count, sizeof(*arr));

    if (!arr) {

        fprintf(stderr,
                "calloc failed\n");

        return 1;
    }

    hdr.max_count =
        hdr.count;

    hdr.entries_ptr =
        (uint64_t)(uintptr_t)arr;

    if (simplefs_ioctl_call(
            mnt,
            SIMPLEFS_IOC_GET_META,
            &hdr))
        goto out;

    printf("%-20s %-12s %-12s %s\n",
           "NAME",
           "OFFSET",
           "SIZE",
           "CRC32");

    printf("--------------------------------------------------\n");

    for (i = 0; i < hdr.count; i++) {

        printf("%-20s %-12llu %-12llu 0x%08x\n",
               arr[i].name,
               (unsigned long long)
               arr[i].offset_sector,
               (unsigned long long)
               arr[i].size_bytes,
               arr[i].content_hash);
    }

    ret = 0;

    out:

        free(arr);

    return ret;
}

static int cmd_verify(int argc __attribute__((unused)), char **argv)
{
    return cmd_meta(argc, argv);
}

int main(int argc, char **argv)
{
    size_t i;

    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    for (i = 0;
         i < sizeof(commands) / sizeof(commands[0]);
         i++) {

        if (!strcmp(argv[1],
                    commands[i].name)) {

            return commands[i].handler(
                argc,
                argv
            );
        }
    }

    usage(argv[0]);

    return 1;
}