#include "relationship.h"
#include "handle_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

Operation operation_from_str(const char *s) {
    if (strcmp(s, "assign") == 0 || strcmp(s, "add") == 0) return OP_ADD;
    if (strcmp(s, "remove") == 0 || strcmp(s, "rm") == 0) return OP_REMOVE;
    return OP_UNKNOWN;
}

bool is_visible_tag(const EntsTag *tag) {
    return tag->show;
}

/* Find a visible tag by name, return its index or -1 */
static int find_tag_index(const TagsFile *tf, const char *name) {
    for (int i = 0; i < tf->tags.count; i++) {
        if (tf->tags.items[i].name && ents_name_equal(tf->tags.items[i].name, name)
            && is_visible_tag(&tf->tags.items[i])) {
            return i;
        }
    }
    return -1;
}

/* Resolve alias to actual tag name */
static const char *resolve_alias(const TagsFile *tf, const char *tag) {
    const char *actual = am_get(&tf->aliases, tag);
    return actual ? actual : tag;
}

/* Get all tags a file is associated with (by inode string), just names */
static void single_inspect(const TagsFile *tf, const char *inode_str, StringArray *out) {
    sa_init(out);
    for (int i = 0; i < tf->tags.count; i++) {
        const EntsTag *tag = &tf->tags.items[i];
        if (!is_visible_tag(tag)) continue;
        if (!tag->has_files) continue;
        if (sa_contains(&tag->files, inode_str)) {
            sa_push(out, tag->name);
        }
    }
}

/* Get tags with full ancestry paths for display */
static void represent_single_inspect(const TagsFile *tf, const char *inode_str, StringArray *out) {
    sa_init(out);
    for (int i = 0; i < tf->tags.count; i++) {
        const EntsTag *tag = &tf->tags.items[i];
        if (!is_visible_tag(tag)) continue;
        if (!tag->has_files) continue;
        if (sa_contains(&tag->files, inode_str)) {
            if (tag->ancestry.count > 0) {
                /* build full path */
                size_t total = 0;
                for (int a = 0; a < tag->ancestry.count; a++)
                    total += strlen(tag->ancestry.items[a]) + 1;
                total += strlen(tag->name) + 1;

                char *path = malloc(total);
                path[0] = '\0';
                for (int a = 0; a < tag->ancestry.count; a++) {
                    strcat(path, tag->ancestry.items[a]);
                    strcat(path, "/");
                }
                strcat(path, tag->name);
                sa_push(out, path);
                free(path);
            } else {
                sa_push(out, tag->name);
            }
        }
    }
}

/* Recursively collect all descendant tag names (default tags only go into default_set) */
static void collect_tags_recursive(const EntsTag *tag, const TagsFile *tf,
                                   StringArray *all_set, StringArray *default_set) {
    if (!sa_contains(all_set, tag->name)) sa_push(all_set, tag->name);
    if (tag->tag_type == TAG_TYPE_DEFAULT) {
        if (!sa_contains(default_set, tag->name)) sa_push(default_set, tag->name);
    }

    for (int i = 0; i < tag->children.count; i++) {
        int ci = find_tag_index(tf, tag->children.items[i]);
        if (ci >= 0) {
            collect_tags_recursive(&tf->tags.items[ci], tf, all_set, default_set);
        }
    }
}

static int collect_tags_recursively(const char *tag_name, const TagsFile *tf,
                                    StringArray *all_set, StringArray *default_set) {
    const char *display = resolve_alias(tf, tag_name);
    int idx = find_tag_index(tf, display);
    if (idx < 0) {
        fprintf(stderr, "tag '%s' is not in tags\n", tag_name);
        return -1;
    }
    sa_init(all_set);
    sa_init(default_set);
    collect_tags_recursive(&tf->tags.items[idx], tf, all_set, default_set);
    return 0;
}

int assign_bidir_file_tag_rel(const char *file_name, const char *tag,
                              Operation op, TagsFile *tf, bool force) {
    uint64_t file_inode = handle_file(file_name, tf);
    if (file_inode == 0) return -1;

    char inode_str[32];
    snprintf(inode_str, sizeof(inode_str), "%llu", (unsigned long long)file_inode);

    const char *display_tag = resolve_alias(tf, tag);
    int tag_idx = find_tag_index(tf, display_tag);
    if (tag_idx < 0) {
        printf("tag or alias does not exist: %s\n", tag);
        return 0;
    }

    char unassign_msg[512] = "";

    switch (op) {
    case OP_ADD: {
        EntsTag *t = &tf->tags.items[tag_idx];

        if (t->tag_type == TAG_TYPE_DUD) {
            printf("cannot assign dud tag to files: \t%s\n", display_tag);
            return 0;
        }

        /* Default tag: check exclusion rules */
        StringArray already_assigned;
        single_inspect(tf, inode_str, &already_assigned);

        StringArray all_set, children_set;
        collect_tags_recursively(tag, tf, &all_set, &children_set);

        /* Check ancestry conflicts */
        for (int a = 0; a < t->ancestry.count; a++) {
            if (sa_contains(&already_assigned, t->ancestry.items[a])) {
                int anc_idx = find_tag_index(tf, t->ancestry.items[a]);
                if (anc_idx >= 0 && tf->tags.items[anc_idx].tag_type == TAG_TYPE_DEFAULT) {
                    if (!force) {
                        printf("cannot assign default tag %s to file %s due to it having been assigned ancestor default tag %s\n",
                               tag, file_name, t->ancestry.items[a]);
                        sa_free(&already_assigned);
                        sa_free(&all_set);
                        sa_free(&children_set);
                        return 0;
                    } else {
                        assign_bidir_file_tag_rel(file_name, t->ancestry.items[a], OP_REMOVE, tf, false);
                        snprintf(unassign_msg, sizeof(unassign_msg),
                                 "and forcefully unassigned ancestor default tag %s", t->ancestry.items[a]);
                        /* re-find tag_idx since tf may have changed */
                        tag_idx = find_tag_index(tf, display_tag);
                    }
                }
            }
        }

        /* Check children conflicts */
        StringArray conflicts;
        sa_init(&conflicts);
        for (int i = 0; i < already_assigned.count; i++) {
            if (sa_contains(&children_set, already_assigned.items[i])) {
                sa_push(&conflicts, already_assigned.items[i]);
            }
        }

        if (conflicts.count > 0) {
            if (!force) {
                if (conflicts.count == 1 && ents_name_equal(conflicts.items[0], display_tag)) {
                    printf("pre-exist file, tag: \t%s \t%s\n", file_name, display_tag);
                    sa_free(&already_assigned);
                    sa_free(&all_set);
                    sa_free(&children_set);
                    sa_free(&conflicts);
                    return 0;
                }
                /* build conflict string */
                char conflict_str[1024] = "";
                for (int i = 0; i < conflicts.count; i++) {
                    if (i > 0) strcat(conflict_str, ", ");
                    strcat(conflict_str, conflicts.items[i]);
                }
                printf("cannot assign default tag %s to file %s due to children %s\n",
                       tag, file_name, conflict_str);
                sa_free(&already_assigned);
                sa_free(&all_set);
                sa_free(&children_set);
                sa_free(&conflicts);
                return 0;
            } else {
                for (int i = 0; i < conflicts.count; i++) {
                    assign_bidir_file_tag_rel(file_name, conflicts.items[i], OP_REMOVE, tf, false);
                }
                /* build message */
                char conflict_str[1024] = "";
                for (int i = 0; i < conflicts.count; i++) {
                    if (i > 0) strcat(conflict_str, ", ");
                    strcat(conflict_str, conflicts.items[i]);
                }
                snprintf(unassign_msg, sizeof(unassign_msg),
                         "and forcefully unassigned %s%s",
                         conflicts.count == 1 ? "child tag " : "children tags ",
                         conflict_str);
                /* re-find tag_idx */
                tag_idx = find_tag_index(tf, display_tag);
            }
        }

        sa_free(&already_assigned);
        sa_free(&all_set);
        sa_free(&children_set);
        sa_free(&conflicts);

        /* Add file to tag */
        t = &tf->tags.items[tag_idx];
        if (!t->has_files) {
            t->has_files = true;
            sa_init(&t->files);
        }
        if (!sa_contains(&t->files, inode_str)) {
            sa_push(&t->files, inode_str);
            printf("assigned file, tag: \t%s \t%s %s\n", file_name, display_tag, unassign_msg);
        } else {
            printf("pre-exist file, tag: \t%s \t%s\n", file_name, display_tag);
        }
        break;
    }

    case OP_REMOVE: {
        EntsTag *t = &tf->tags.items[tag_idx];
        if (t->has_files) {
            int pos = sa_find(&t->files, inode_str);
            if (pos >= 0) {
                sa_remove_at(&t->files, pos);
                printf("removed  file, tag: \t%s \t%s\n", file_name, tag);
            } else {
                printf("there is no correlation between file '%s' and tag '%s'\n", file_name, display_tag);
            }
        } else {
            printf("there is no correlation between file '%s' and tag '%s'\n", file_name, display_tag);
        }
        break;
    }

    case OP_UNKNOWN:
        printf("invalid operation\n");
        break;
    }

    return 0;
}

int filter_command(TagsFile *tf, const char **tags, int tag_count,
                   bool explicit_mode, StringArray *result) {
    sa_init(result);
    StringArray all_tag_names;
    sa_init(&all_tag_names);

    for (int i = 0; i < tag_count; i++) {
        if (!explicit_mode) {
            StringArray all_set, default_set;
            if (collect_tags_recursively(tags[i], tf, &all_set, &default_set) < 0) {
                sa_free(&all_tag_names);
                return -1;
            }
            for (int j = 0; j < default_set.count; j++) {
                if (!sa_contains(&all_tag_names, default_set.items[j]))
                    sa_push(&all_tag_names, default_set.items[j]);
            }
            sa_free(&all_set);
            sa_free(&default_set);
        } else {
            const char *resolved = resolve_alias(tf, tags[i]);
            if (!sa_contains(&all_tag_names, resolved))
                sa_push(&all_tag_names, resolved);
        }
    }

    /* Collect unique inodes from matching tags */
    StringArray unique_inodes;
    sa_init(&unique_inodes);

    for (int i = 0; i < all_tag_names.count; i++) {
        int idx = find_tag_index(tf, all_tag_names.items[i]);
        if (idx >= 0 && tf->tags.items[idx].has_files) {
            for (int f = 0; f < tf->tags.items[idx].files.count; f++) {
                const char *inode = tf->tags.items[idx].files.items[f];
                if (!sa_contains(&unique_inodes, inode))
                    sa_push(&unique_inodes, inode);
            }
        }
    }

    sa_free(&all_tag_names);

    /* Convert inodes to filenames */
    bool needs_save = false;

    for (int i = 0; i < unique_inodes.count; i++) {
        uint64_t inode = strtoull(unique_inodes.items[i], NULL, 10);

        /* find in files registry */
        int found_pos = -1;
        for (int f = 0; f < tf->files.count; f++) {
            if (tf->files.items[f].file_inode == inode) {
                found_pos = f;
                break;
            }
        }

        if (found_pos >= 0) {
            const char *lkn = tf->files.items[found_pos].last_known_name;
            /* check if file still exists at last known path */
            struct stat st;
            if (stat(lkn, &st) == 0) {
                sa_push(result, lkn);
            } else {
                /* try to find by inode */
                char *current_path = find_filename_by_inode(inode);
                if (current_path) {
                    if (strcmp(tf->files.items[found_pos].last_known_name, current_path) != 0) {
                        free(tf->files.items[found_pos].last_known_name);
                        tf->files.items[found_pos].last_known_name = strdup(current_path);
                        needs_save = true;
                    }
                    sa_push(result, current_path);
                    free(current_path);
                } else {
                    fprintf(stderr, "Warning: File with inode %llu not found in filesystem\n",
                            (unsigned long long)inode);
                }
            }
        }
    }

    sa_free(&unique_inodes);

    /* sort results */
    if (result->count > 1) {
        for (int i = 0; i < result->count - 1; i++) {
            for (int j = i + 1; j < result->count; j++) {
                if (strcmp(result->items[i], result->items[j]) > 0) {
                    char *tmp = result->items[i];
                    result->items[i] = result->items[j];
                    result->items[j] = tmp;
                }
            }
        }
    }

    if (needs_save) save_tags_bin(tf);

    return 0;
}

int represent_inspect(TagsFile *tf, const char **files, int file_count, bool quiet) {
    int multi = file_count > 1;
    const char *tab = multi ? "\t" : "";

    for (int i = 0; i < file_count; i++) {
        uint64_t file_inode = handle_file(files[i], tf);
        if (file_inode == 0) continue;

        char inode_str[32];
        snprintf(inode_str, sizeof(inode_str), "%llu", (unsigned long long)file_inode);

        StringArray elements;
        represent_single_inspect(tf, inode_str, &elements);

        if (elements.count == 0 && quiet) {
            sa_free(&elements);
            continue;
        }

        if (multi) {
            int flen = strlen(files[i]);
            int header_len = flen + 5;
            if (header_len < 20) header_len = 20;
            int padding = header_len - flen;

            printf("\n=====%s", files[i]);
            for (int p = 0; p < padding; p++) putchar('=');
            putchar('\n');
        }

        for (int e = 0; e < elements.count; e++) {
            printf("%s%s\n", tab, elements.items[e]);
        }

        sa_free(&elements);
    }

    return 0;
}
