#ifndef TAGS_API_H
#define TAGS_API_H

#include "common.h"

/*
 * Glass-facing API for reading/writing tag-file relationships in tags.dtob.
 * Uses prlents' native save_tags_bin which writes the proper types header
 * and automatically selects the best relationship encoding.
 */

/* Find a tag by name, return its index or -1 */
int prl_find_tag(const TagsFile *tf, const char *tag_name);

/* Get filenames (not inodes) for a tag. Caller frees the returned array and strings. */
char **prl_get_tag_files(const TagsFile *tf, const char *tag_name, int *out_count);

/* Get filenames tagged by any child of the given tag. Caller frees. */
char **prl_get_child_tag_files(const TagsFile *tf, const char *tag_name, int *out_count);

/*
 * Update which files are associated with a tag.
 * all_files/num_all: all filenames presented to the user.
 * selected_files/num_selected: filenames the user selected.
 * Only modifies relationships for files in all_files.
 * Files not in all_files keep their existing relationship.
 */
/* Get filenames tagged by the immediate parent of the given tag. Caller frees. */
char **prl_get_parent_tag_files(const TagsFile *tf, const char *tag_name, int *out_count);

int prl_set_tag_files(TagsFile *tf, const char *tag_name,
                      const char **all_files, int num_all,
                      const char **selected_files, int num_selected);

#endif
