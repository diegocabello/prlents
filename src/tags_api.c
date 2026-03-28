#include "tags_api.h"
#include "handle_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* strip leading ./ from a path */
static const char *strip_dot_slash(const char *path) {
    if (path[0] == '.' && path[1] == '/') return path + 2;
    return path;
}

int prl_find_tag(const TagsFile *tf, const char *tag_name) {
    for (int i = 0; i < tf->tags.count; i++) {
        if (strcmp(tf->tags.items[i].name, tag_name) == 0)
            return i;
    }
    /* try resolving as alias */
    const char *real_name = am_get(&tf->aliases, tag_name);
    if (real_name) {
        for (int i = 0; i < tf->tags.count; i++) {
            if (strcmp(tf->tags.items[i].name, real_name) == 0)
                return i;
        }
    }
    return -1;
}

char **prl_get_tag_files(const TagsFile *tf, const char *tag_name, int *out_count) {
    *out_count = 0;
    int ti = prl_find_tag(tf, tag_name);
    if (ti < 0) return NULL;

    const EntsTag *tag = &tf->tags.items[ti];

    /* Dud tags are organizational containers — aggregate from children */
    if (tag->tag_type == TAG_TYPE_DUD)
        return prl_get_child_tag_files(tf, tag_name, out_count);

    if (!tag->has_files || tag->files.count == 0) return NULL;

    /* resolve inodes to filenames */
    char **result = malloc(tag->files.count * sizeof(char *));
    int count = 0;

    for (int i = 0; i < tag->files.count; i++) {
        uint64_t inode = strtoull(tag->files.items[i], NULL, 10);
        /* find filename in tf->files */
        for (int f = 0; f < tf->files.count; f++) {
            if (tf->files.items[f].file_inode == inode) {
                const char *name = strip_dot_slash(tf->files.items[f].last_known_name);
                result[count++] = strdup(name);
                break;
            }
        }
    }

    *out_count = count;
    if (count == 0) { free(result); return NULL; }
    return result;
}

char **prl_get_child_tag_files(const TagsFile *tf, const char *tag_name, int *out_count) {
    *out_count = 0;
    int ti = prl_find_tag(tf, tag_name);
    if (ti < 0) return NULL;

    const EntsTag *parent = &tf->tags.items[ti];
    if (parent->children.count == 0) return NULL;

    /* collect all files from all children (deduplicated) */
    char **result = NULL;
    int count = 0, cap = 0;

    for (int c = 0; c < parent->children.count; c++) {
        int child_count = 0;
        char **child_files = prl_get_tag_files(tf, parent->children.items[c], &child_count);
        if (!child_files) continue;

        for (int f = 0; f < child_count; f++) {
            /* check for duplicate */
            bool dup = false;
            for (int r = 0; r < count; r++) {
                if (strcmp(result[r], child_files[f]) == 0) { dup = true; break; }
            }
            if (!dup) {
                if (count >= cap) {
                    cap = cap ? cap * 2 : 16;
                    result = realloc(result, cap * sizeof(char *));
                }
                result[count++] = strdup(child_files[f]);
            }
            free(child_files[f]);
        }
        free(child_files);
    }

    *out_count = count;
    return result;
}

char **prl_get_parent_tag_files(const TagsFile *tf, const char *tag_name, int *out_count) {
    *out_count = 0;
    int ti = prl_find_tag(tf, tag_name);
    if (ti < 0) return NULL;

    const EntsTag *tag = &tf->tags.items[ti];
    /* the immediate parent is the last element of ancestry */
    if (tag->ancestry.count == 0) return NULL;

    const char *parent_name = tag->ancestry.items[tag->ancestry.count - 1];
    return prl_get_tag_files(tf, parent_name, out_count);
}

int prl_set_tag_files(TagsFile *tf, const char *tag_name,
                      const char **all_files, int num_all,
                      const char **selected_files, int num_selected) {
    int ti = prl_find_tag(tf, tag_name);
    if (ti < 0) {
        fprintf(stderr, "Error: tag '%s' not found\n", tag_name);
        return -1;
    }

    EntsTag *tag = &tf->tags.items[ti];
    if (!tag->has_files) {
        sa_init(&tag->files);
        tag->has_files = true;
    }

    /* for each presented file, remove its inode from the tag
       (we'll add back the selected ones) */
    for (int i = 0; i < num_all; i++) {
        uint64_t inode = 0;
        for (int f = 0; f < tf->files.count; f++) {
            const char *stored = strip_dot_slash(tf->files.items[f].last_known_name);
            if (strcmp(stored, all_files[i]) == 0) {
                inode = tf->files.items[f].file_inode;
                break;
            }
        }
        if (inode == 0) continue;

        char inode_str[32];
        snprintf(inode_str, sizeof(inode_str), "%llu", (unsigned long long)inode);
        int idx = sa_find(&tag->files, inode_str);
        if (idx >= 0) sa_remove_at(&tag->files, idx);
    }

    /* add back only the selected ones */
    for (int i = 0; i < num_selected; i++) {
        uint64_t inode = 0;
        for (int f = 0; f < tf->files.count; f++) {
            const char *stored = strip_dot_slash(tf->files.items[f].last_known_name);
            if (strcmp(stored, selected_files[i]) == 0) {
                inode = tf->files.items[f].file_inode;
                break;
            }
        }

        /* file not known to prlents yet — register it */
        if (inode == 0) {
            inode = handle_file(selected_files[i], tf);
            if (inode == 0) continue;
        }

        char inode_str[32];
        snprintf(inode_str, sizeof(inode_str), "%llu", (unsigned long long)inode);
        if (!sa_contains(&tag->files, inode_str))
            sa_push(&tag->files, inode_str);
    }

    return 0;
}
