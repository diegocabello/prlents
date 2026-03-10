#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TAG_TYPE_DEFAULT,
    TAG_TYPE_DUD,
} TagType;

/* Dynamic string array */
typedef struct {
    char **items;
    int count;
    int capacity;
} StringArray;

typedef struct {
    char *name;
    TagType tag_type;
    StringArray children;
    StringArray ancestry;
    bool show;
    StringArray files;
    bool has_files;   /* false = JSON null, true = array (possibly empty) */
    /* Parser-only fields (not serialized) */
    char *alias;
} EntsTag;

typedef struct {
    EntsTag *items;
    int count;
    int capacity;
} TagArray;

typedef struct {
    char *last_known_name;
    uint64_t file_inode;
    uint64_t parent_dir_inode;
} FileData;

typedef struct {
    FileData *items;
    int count;
    int capacity;
} FileDataArray;

typedef struct {
    char *key;
    char *value;
} AliasEntry;

typedef struct {
    AliasEntry *items;
    int count;
    int capacity;
} AliasMap;

typedef struct {
    FileDataArray files;
    AliasMap aliases;
    TagArray tags;
} TagsFile;

/* StringArray */
void sa_init(StringArray *sa);
void sa_push(StringArray *sa, const char *str);
bool sa_contains(const StringArray *sa, const char *str);
int  sa_find(const StringArray *sa, const char *str);
void sa_remove_at(StringArray *sa, int index);
void sa_free(StringArray *sa);
void sa_clear(StringArray *sa);

/* TagArray */
void ta_init(TagArray *ta);
EntsTag *ta_push(TagArray *ta);
void ta_free(TagArray *ta);

/* FileDataArray */
void fda_init(FileDataArray *fda);
FileData *fda_push(FileDataArray *fda);
void fda_free(FileDataArray *fda);

/* AliasMap */
void am_init(AliasMap *am);
void am_put(AliasMap *am, const char *key, const char *value);
const char *am_get(const AliasMap *am, const char *key);
bool am_contains(const AliasMap *am, const char *key);
void am_free(AliasMap *am);

/* TagsFile */
void tags_file_init(TagsFile *tf);
void tags_file_free(TagsFile *tf);
int  read_tags_bin(TagsFile *tf);
int  save_tags_bin(const TagsFile *tf);

/* Utility */
const char *tag_type_str(TagType t);
TagType tag_type_from_str(const char *s);

#endif
