#ifndef MERGE_TAGS_H
#define MERGE_TAGS_H

#include "common.h"

/* Merge new_tf into the existing tags.bin, preserving file associations. */
int merge_tags(const TagsFile *new_tf);

#endif
