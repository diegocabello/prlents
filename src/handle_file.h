#ifndef HANDLE_FILE_H
#define HANDLE_FILE_H

#include "common.h"
#include <stdint.h>

/* Returns the file's inode, adding it to tf->files if needed. Returns 0 on error. */
uint64_t handle_file(const char *file_path, TagsFile *tf);

/* Walk CWD tree to find a filename for a given inode. Caller must free result. Returns NULL if not found. */
char *find_filename_by_inode(uint64_t target_inode);

#endif
