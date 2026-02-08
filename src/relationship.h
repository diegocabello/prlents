#ifndef RELATIONSHIP_H
#define RELATIONSHIP_H

#include "common.h"

typedef enum {
    OP_UNKNOWN,
    OP_ADD,
    OP_REMOVE,
} Operation;

Operation operation_from_str(const char *s);
bool is_visible_tag(const EntsTag *tag);

int assign_bidir_file_tag_rel(const char *file_name, const char *tag,
                              Operation op, TagsFile *tf, bool force);

/* Returns a StringArray of filenames. Caller must sa_free the result. */
int filter_command(TagsFile *tf, const char **tags, int tag_count,
                   bool explicit_mode, StringArray *result);

int represent_inspect(TagsFile *tf, const char **files, int file_count, bool quiet);

#endif
