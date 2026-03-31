#include "handle_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>

static uint64_t get_inode(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (uint64_t)st.st_ino;
}

static char *make_relative(const char *path) {
    /* if it starts with ./ already, just dup it */
    if (path[0] == '.' && path[1] == '/') return strdup(path);

    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) return strdup(path);

    size_t cwdlen = strlen(cwd);
    if (strncmp(path, cwd, cwdlen) == 0 && path[cwdlen] == '/') {
        /* strip CWD prefix, return relative */
        return strdup(path + cwdlen + 1);
    }
    return strdup(path);
}

static char *portable_path(const char *path) {
    char *p = strdup(path);
    for (int i = 0; p[i]; i++) {
        if (p[i] == '\\') p[i] = '/';
    }
    return p;
}

/* Recursive directory walk to find file by name */
static int walk_find_by_name(const char *dir, const char *target_name,
                             uint64_t *out_inode, uint64_t *out_parent_inode,
                             char *out_path, size_t out_path_size) {
    DIR *d = opendir(dir);
    if (!d) return 0;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);

        struct stat st;
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (walk_find_by_name(full, target_name, out_inode, out_parent_inode, out_path, out_path_size))
            {
                closedir(d);
                return 1;
            }
        } else if (strcasecmp(ent->d_name, target_name) == 0) {
            *out_inode = (uint64_t)st.st_ino;
            struct stat pst;
            if (stat(dir, &pst) == 0) *out_parent_inode = (uint64_t)pst.st_ino;
            else *out_parent_inode = 0;
            snprintf(out_path, out_path_size, "%s", full);
            closedir(d);
            return 1;
        }
    }
    closedir(d);
    return 0;
}

uint64_t handle_file(const char *file_path, TagsFile *tf) {
    char *norm = portable_path(file_path);

    /* check if already known by name */
    for (int i = 0; i < tf->files.count; i++) {
        if (strcmp(tf->files.items[i].last_known_name, norm) == 0) {
            free(norm);
            return tf->files.items[i].file_inode;
        }
    }

    struct stat st;
    if (stat(file_path, &st) == 0) {
        uint64_t file_ino = (uint64_t)st.st_ino;

        /* get parent dir inode */
        uint64_t parent_ino = 0;
        const char *last_slash = strrchr(file_path, '/');
        if (last_slash && last_slash != file_path) {
            char parent[PATH_MAX];
            size_t plen = last_slash - file_path;
            memcpy(parent, file_path, plen);
            parent[plen] = '\0';
            parent_ino = get_inode(parent);
        } else {
            parent_ino = get_inode(".");
        }

        /* check if inode already known (file was renamed) */
        for (int i = 0; i < tf->files.count; i++) {
            if (tf->files.items[i].file_inode == file_ino) {
                free(tf->files.items[i].last_known_name);
                tf->files.items[i].last_known_name = make_relative(norm);
                tf->files.items[i].parent_dir_inode = parent_ino;
                free(norm);
                tf->dirty_metadata = true;
                return file_ino;
            }
        }

        /* new file */
        FileData *fd = fda_push(&tf->files);
        fd->last_known_name = make_relative(norm);
        fd->file_inode = file_ino;
        fd->parent_dir_inode = parent_ino;
        free(norm);
        tf->dirty_metadata = true;
        return file_ino;
    }

    /* file not found at path - walk directory tree */
    const char *basename = strrchr(file_path, '/');
    basename = basename ? basename + 1 : file_path;

    uint64_t found_inode = 0, found_parent = 0;
    char found_path[PATH_MAX];
    if (walk_find_by_name(".", basename, &found_inode, &found_parent, found_path, sizeof(found_path))) {
        /* check if inode already known */
        for (int i = 0; i < tf->files.count; i++) {
            if (tf->files.items[i].file_inode == found_inode) {
                free(tf->files.items[i].last_known_name);
                tf->files.items[i].last_known_name = portable_path(found_path);
                tf->files.items[i].parent_dir_inode = found_parent;
                free(norm);
                tf->dirty_metadata = true;
                return found_inode;
            }
        }

        FileData *fd = fda_push(&tf->files);
        fd->last_known_name = portable_path(found_path);
        fd->file_inode = found_inode;
        fd->parent_dir_inode = found_parent;
        free(norm);
        tf->dirty_metadata = true;
        return found_inode;
    }

    fprintf(stderr, "File '%s' not found in any directory\n", file_path);
    free(norm);
    return 0;
}

char *find_filename_by_inode(uint64_t target_inode) {
    /* walk directory tree from CWD */
    DIR *stack_dirs[128];
    char stack_paths[128][PATH_MAX];
    int depth = 0;

    stack_dirs[0] = opendir(".");
    if (!stack_dirs[0]) return NULL;
    snprintf(stack_paths[0], PATH_MAX, ".");
    depth = 1;

    while (depth > 0) {
        struct dirent *ent = readdir(stack_dirs[depth - 1]);
        if (!ent) {
            closedir(stack_dirs[depth - 1]);
            depth--;
            continue;
        }
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", stack_paths[depth - 1], ent->d_name);

        struct stat st;
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (depth < 128) {
                stack_dirs[depth] = opendir(full);
                if (stack_dirs[depth]) {
                    snprintf(stack_paths[depth], PATH_MAX, "%s", full);
                    depth++;
                }
            }
        } else {
            if ((uint64_t)st.st_ino == target_inode) {
                /* close all open dirs */
                for (int i = 0; i < depth; i++) closedir(stack_dirs[i]);
                return portable_path(full);
            }
        }
    }

    return NULL;
}
