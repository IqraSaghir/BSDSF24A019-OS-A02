#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int long_format = 0;

/* Convert file permission bits into an rwx string. */
static void format_permissions(mode_t mode, char text[11])
{
    if (S_ISREG(mode))      text[0] = '-';
    else if (S_ISDIR(mode)) text[0] = 'd';
    else if (S_ISLNK(mode)) text[0] = 'l';
    else if (S_ISCHR(mode)) text[0] = 'c';
    else if (S_ISBLK(mode)) text[0] = 'b';
    else if (S_ISFIFO(mode)) text[0] = 'p';
    else if (S_ISSOCK(mode)) text[0] = 's';
    else                    text[0] = '?';

    text[1] = (mode & S_IRUSR) ? 'r' : '-';
    text[2] = (mode & S_IWUSR) ? 'w' : '-';
    text[3] = (mode & S_IXUSR) ? 'x' : '-';

    text[4] = (mode & S_IRGRP) ? 'r' : '-';
    text[5] = (mode & S_IWGRP) ? 'w' : '-';
    text[6] = (mode & S_IXGRP) ? 'x' : '-';

    text[7] = (mode & S_IROTH) ? 'r' : '-';
    text[8] = (mode & S_IWOTH) ? 'w' : '-';
    text[9] = (mode & S_IXOTH) ? 'x' : '-';

    /* Handle the special permission bits. */
    if (mode & S_ISUID)
        text[3] = (mode & S_IXUSR) ? 's' : 'S';

    if (mode & S_ISGID)
        text[6] = (mode & S_IXGRP) ? 's' : 'S';

    if (mode & S_ISVTX)
        text[9] = (mode & S_IXOTH) ? 't' : 'T';

    text[10] = '\0';
}

/* Build a complete path for an entry inside a directory. */
static char *join_path(const char *directory, const char *name)
{
    size_t dlen = strlen(directory);
    size_t nlen = strlen(name);
    int slash_needed = (dlen > 0 && directory[dlen - 1] != '/');

    size_t length = dlen + (size_t)slash_needed + nlen + 1;
    char *full_path = malloc(length);

    if (full_path == NULL)
        return NULL;

    snprintf(full_path, length, "%s%s%s",
             directory, slash_needed ? "/" : "", name);

    return full_path;
}

/* Print metadata for one file or directory entry. */
static void print_long_entry(const char *path, const char *display_name)
{
    struct stat info;

    /* lstat() obtains information about the link itself, if this is a link. */
    if (lstat(path, &info) != 0) {
        perror(path);
        return;
    }

    char permissions[11];
    char owner[128];
    char group[128];
    char date_text[32];

    format_permissions(info.st_mode, permissions);

    struct passwd *user_info = getpwuid(info.st_uid);
    struct group *group_info = getgrgid(info.st_gid);

    if (user_info != NULL)
        snprintf(owner, sizeof(owner), "%s", user_info->pw_name);
    else
        snprintf(owner, sizeof(owner), "%lu",
                 (unsigned long)info.st_uid);

    if (group_info != NULL)
        snprintf(group, sizeof(group), "%s", group_info->gr_name);
    else
        snprintf(group, sizeof(group), "%lu",
                 (unsigned long)info.st_gid);

    /* ctime() returns a date string; select month, day and time. */
    char *ctime_text = ctime(&info.st_mtime);

    if (ctime_text != NULL)
        snprintf(date_text, sizeof(date_text), "%.12s", ctime_text + 4);
    else
        snprintf(date_text, sizeof(date_text), "unknown time");

    printf("%s %2lu %-8s %-8s %8lld %s %s",
           permissions,
           (unsigned long)info.st_nlink,
           owner,
           group,
           (long long)info.st_size,
           date_text,
           display_name);

    /* Display the target of a symbolic link. */
    if (S_ISLNK(info.st_mode)) {
        char target[PATH_MAX + 1];
        ssize_t length = readlink(path, target, sizeof(target) - 1);

        if (length >= 0) {
            target[length] = '\0';
            printf(" -> %s", target);
        }
    }

    putchar('\n');
}

/* List a directory, or print one file if a file path was supplied. */
static void do_ls(const char *directory)
{
    struct stat info;

    /* stat() follows links when determining whether this path is a directory. */
    if (stat(directory, &info) == 0 && !S_ISDIR(info.st_mode)) {
        if (long_format)
            print_long_entry(directory, directory);
        else
            printf("%s\n", directory);

        return;
    }

    DIR *dp = opendir(directory);

    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory : %s\n", directory);
        return;
    }

    /*
     * In long mode, calculate the total number of 1 KiB blocks
     * before printing the entries.
     */
    if (long_format) {
        long long total_blocks = 0;
        struct dirent *entry;

        while (1) {
            errno = 0;
            entry = readdir(dp);

            if (entry == NULL) {
                if (errno != 0)
                    perror("readdir failed");
                break;
            }

            /* Like ordinary ls, skip hidden entries by default. */
            if (entry->d_name[0] == '.')
                continue;

            char *full_path = join_path(directory, entry->d_name);

            if (full_path == NULL) {
                fprintf(stderr, "Out of memory while reading directory\n");
                continue;
            }

            if (lstat(full_path, &info) == 0)
                total_blocks += (long long)info.st_blocks;

            free(full_path);
        }

        rewinddir(dp);
        printf("total %lld\n", (total_blocks + 1) / 2);
    }

    /* Print the actual directory entries. */
    while (1) {
        errno = 0;
        struct dirent *entry = readdir(dp);

        if (entry == NULL) {
            if (errno != 0)
                perror("readdir failed");
            break;
        }

        if (entry->d_name[0] == '.')
            continue;

        if (long_format) {
            char *full_path = join_path(directory, entry->d_name);

            if (full_path == NULL) {
                fprintf(stderr, "Out of memory while reading directory\n");
                continue;
            }

            print_long_entry(full_path, entry->d_name);
            free(full_path);
        } else {
            /* Preserve the original simple listing behavior. */
            printf("%s\n", entry->d_name);
        }
    }

    closedir(dp);
}

int main(int argc, char *argv[])
{
    int option;

    /* Parse the -l option. */
    while ((option = getopt(argc, argv, "l")) != -1) {
        switch (option) {
            case 'l':
                long_format = 1;
                break;

            default:
                fprintf(stderr,
                        "Usage: %s [-l] [file/directory ...]\n",
                        argv[0]);
                return EXIT_FAILURE;
        }
    }

    int operand_count = argc - optind;

    /* With no path supplied, list the current directory. */
    if (operand_count == 0) {
        do_ls(".");
        return EXIT_SUCCESS;
    }

    for (int i = optind; i < argc; i++) {
        if (!long_format) {
            /* Keep the starter program's heading in simple mode. */
            printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i]);
            puts("");
        } else {
            /* Multiple paths get separate headings in long mode. */
            if (operand_count > 1)
                printf("%s:\n", argv[i]);

            do_ls(argv[i]);

            if (operand_count > 1 && i < argc - 1)
                putchar('\n');
        }
    }

    return EXIT_SUCCESS;
}
