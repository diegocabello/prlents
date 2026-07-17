#ifndef MIGRATE_H
#define MIGRATE_H

#include "common.h"

/* Read a tags.json file into a TagsFile struct. Returns 0 on success, -1 on error. */
int read_tags_from_json(const char *path, TagsFile *tf);

/* Migrate old-magic dtob file to new format. Returns 0 on success, -1 on error. */
int migrate_dtob_v2(const char *path);

#endif
