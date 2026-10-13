#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
    size_t max_len;
} NameList;

static int long_format = 0;
static int horizontal_format = 0;
static int recursive_format = 0;

/* Compare two filename strings for alphabetical sorting. */
static int compare_names(const void *a, const void *b)
{
    const char *left = *(char * const *)a;
    const char *right = *(char * const *)b;

    return strcmp(left, right);
}

static void free_names(NameList *list)
{
    for (size_t i = 0; i < list->count; i++)
        free(list->items[i]);

    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    list->max_len = 0;
}

static int add_name(NameList *list, const char *name)
{
    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity ?
                              list->capacity * 2 : 16;

        char **new_items = realloc(
            list->items, new_capacity * sizeof(*new_items));

        if (new_items == NULL)
            return -1;

        list->items = new_items;
        list->capacity = new_capacity;
    }

    char *copy = strdup(name);

    if (copy == NULL)
        return -1;

    list->items[list->count++] = copy;

    size_t length = strlen(name);

    if (length > list->max_len)
        list->max_len = length;

    return 0;
}

static char *join_path(const char *directory, const char *name)
{
    size_t dlen = strlen(directory);
    size_t nlen = strlen(name);
    int slash = dlen > 0 && directory[dlen - 1] != '/';

    size_t length = dlen + (size_t)slash + nlen + 1;
    char *path = malloc(length);

    if (path == NULL)
        return NULL;

    snprintf(path, length, "%s%s%s",
             directory, slash ? "/" : "", name);

    return path;
}


static int ends_with(const char *name, const char *suffix)
{
    size_t n = strlen(name);
    size_t s = strlen(suffix);
    return n >= s && strcmp(name + n - s, suffix) == 0;
}

static const char *file_color(const char *name, mode_t mode)
{
    if (S_ISLNK(mode))
        return "\033[0;35m";  /* Pink/magenta */

    if (S_ISDIR(mode))
        return "\033[0;34m";  /* Blue */

    if (S_ISFIFO(mode) || S_ISCHR(mode) ||
        S_ISBLK(mode) || S_ISSOCK(mode))
        return "\033[7m";     /* Reverse video */

    if (ends_with(name, ".tar") ||
        ends_with(name, ".gz") ||
        ends_with(name, ".zip"))
        return "\033[0;31m";  /* Red */

    if (mode & (S_IXUSR | S_IXGRP | S_IXOTH))
        return "\033[0;32m";  /* Green */

    return "";
}

static void print_colored_name(const char *path, const char *name)
{
    struct stat info;

    if (lstat(path, &info) != 0) {
        printf("%s", name);
        return;
    }

    const char *color = file_color(name, info.st_mode);

    if (*color)
        printf("%s%s\033[0m", color, name);
    else
        printf("%s", name);
}

static void print_padded_name(const char *path, const char *name,
                              size_t width, int pad)
{
    print_colored_name(path, name);

    if (pad) {
        size_t length = strlen(name);
        while (length < width) {
            putchar(' ');
            length++;
        }
    }
}

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

    if (mode & S_ISUID)
        text[3] = (mode & S_IXUSR) ? 's' : 'S';

    if (mode & S_ISGID)
        text[6] = (mode & S_IXGRP) ? 's' : 'S';

    if (mode & S_ISVTX)
        text[9] = (mode & S_IXOTH) ? 't' : 'T';

    text[10] = '\0';
}


static void print_long_entry(const char *path, const char *name)
{
    struct stat info;

    if (lstat(path, &info) != 0) {
        perror(path);
        return;
    }

    char permissions[11];
    char date_text[32];

    format_permissions(info.st_mode, permissions);

    struct passwd *user = getpwuid(info.st_uid);
    struct group *group = getgrgid(info.st_gid);
    char *time_text = ctime(&info.st_mtime);

    if (time_text)
        snprintf(date_text, sizeof(date_text), "%.12s", time_text + 4);
    else
        snprintf(date_text, sizeof(date_text), "unknown time");

    printf("%s %2lu %-8s %-8s %8lld %s ",
           permissions,
           (unsigned long)info.st_nlink,
           user ? user->pw_name : "?",
           group ? group->gr_name : "?",
           (long long)info.st_size,
           date_text);

    print_colored_name(path, name);

    if (S_ISLNK(info.st_mode)) {
        char target[PATH_MAX + 1];
        ssize_t n = readlink(path, target, sizeof(target) - 1);

        if (n >= 0) {
            target[n] = '\0';
            printf(" -> %s", target);
        }
    }

    putchar('\n');
}


static unsigned int terminal_width(void)
{
    struct winsize window;

    if (isatty(STDOUT_FILENO) &&
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0 &&
        window.ws_col > 0)
        return window.ws_col;

    return 80;
}

/* Default mode: print down each column, then across. */

static void print_columns(const NameList *list, const char *directory)
{
    if (list->count == 0)
        return;

    size_t cell_width = list->max_len + 2;
    size_t columns = terminal_width() / cell_width;

    if (columns < 1)
        columns = 1;
    if (columns > list->count)
        columns = list->count;

    size_t rows = (list->count + columns - 1) / columns;
    columns = (list->count + rows - 1) / rows;

    for (size_t row = 0; row < rows; row++) {
        for (size_t col = 0; col < columns; col++) {
            size_t index = row + col * rows;

            if (index >= list->count)
                break;

            int has_next = col + 1 < columns &&
                           row + (col + 1) * rows < list->count;

            char *path = join_path(directory, list->items[index]);

            if (path) {
                print_padded_name(path, list->items[index],
                                  cell_width, has_next);
                free(path);
            } else {
                printf("%s", list->items[index]);
            }
        }

        putchar('\n');
    }
}



static void print_horizontal(const NameList *list, const char *directory)
{
    if (list->count == 0)
        return;

    size_t cell_width = list->max_len + 2;
    size_t columns = terminal_width() / cell_width;

    if (columns < 1)
        columns = 1;
    if (columns > list->count)
        columns = list->count;

    for (size_t i = 0; i < list->count; i++) {
        int end_of_row = ((i + 1) % columns == 0) ||
                         (i + 1 == list->count);

        char *path = join_path(directory, list->items[i]);

        if (path) {
            print_padded_name(path, list->items[i],
                              cell_width, !end_of_row);
            free(path);
        } else {
            printf("%s", list->items[i]);
        }

        if (end_of_row)
            putchar('\n');
    }
}


static void do_ls(const char *directory)
{
    struct stat info;

    if (stat(directory, &info) == 0 && !S_ISDIR(info.st_mode)) {
        if (long_format)
            print_long_entry(directory, directory);
        else {
            print_colored_name(directory, directory);
            putchar('\n');
        }

        return;
    }

    DIR *dp = opendir(directory);

    if (dp == NULL) {
        perror(directory);
        return;
    }

    NameList names = {0};
    int read_error = 0;

    while (1) {
        errno = 0;
        struct dirent *entry = readdir(dp);

        if (entry == NULL) {
            if (errno != 0)
                read_error = errno;
            break;
        }

        if (entry->d_name[0] == '.')
            continue;

        if (add_name(&names, entry->d_name) != 0) {
            fprintf(stderr, "Not enough memory to list directory\n");
            free_names(&names);
            closedir(dp);
            return;
        }
    }

    closedir(dp);

    if (read_error != 0) {
        errno = read_error;
        perror("readdir");
    }

    /* Sort all collected filenames before displaying them. */
    qsort(names.items, names.count, sizeof(names.items[0]), compare_names);

    if (long_format) {
        long long total_blocks = 0;

        for (size_t i = 0; i < names.count; i++) {
            char *path = join_path(directory, names.items[i]);

            if (path == NULL)
                continue;

            if (lstat(path, &info) == 0)
                total_blocks += (long long)info.st_blocks;

            free(path);
        }

        printf("total %lld\n", (total_blocks + 1) / 2);

        for (size_t i = 0; i < names.count; i++) {
            char *path = join_path(directory, names.items[i]);

            if (path == NULL) {
                fprintf(stderr, "Not enough memory for path\n");
                continue;
            }

            print_long_entry(path, names.items[i]);
            free(path);
        }
    } else if (horizontal_format) {
        print_horizontal(&names, directory);
    } else {
        print_columns(&names, directory);
    }


    if (recursive_format) {
        for (size_t i = 0; i < names.count; i++) {
            char *child_path = join_path(directory, names.items[i]);

            if (child_path == NULL) {
                fprintf(stderr, "Not enough memory for child path\n");
                continue;
            }

            struct stat child_info;

            if (lstat(child_path, &child_info) == 0 &&
                S_ISDIR(child_info.st_mode)) {
                printf("\n%s:\n", child_path);
                do_ls(child_path);
            }

            free(child_path);
        }
    }

    free_names(&names);
}

int main(int argc, char *argv[])
{
    int option;

    while ((option = getopt(argc, argv, "lxR")) != -1) {
        switch (option) {
            case 'l':
                long_format = 1;
                horizontal_format = 0;
                break;

            case 'x':
                horizontal_format = 1;
                long_format = 0;
                break;

            case 'R':
                recursive_format = 1;
                break;

            default:
                fprintf(stderr, "Usage: %s [-l] [-x] [-R] [file/directory ...]\n",
                        argv[0]);
                return EXIT_FAILURE;
        }
    }

    int count = argc - optind;

    if (count == 0) {
        if (recursive_format)
            printf(".:\n");
        do_ls(".");
        return EXIT_SUCCESS;
    }

    for (int i = optind; i < argc; i++) {
        if (count > 1 || recursive_format)
            printf("%s:\n", argv[i]);
        else if (!long_format)
            printf("Directory listing of %s:\n", argv[i]);

        do_ls(argv[i]);

        if (count > 1 && i < argc - 1)
            putchar('\n');
    }

    return EXIT_SUCCESS;
}
