#ifndef MIGRATE_H
#define MIGRATE_H

#include "common.h"

/* Read a tags.json file into a TagsFile struct. Returns 0 on success, -1 on error. */
int read_tags_from_json(const char *path, TagsFile *tf);

#endif
