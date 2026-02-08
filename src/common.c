#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- StringArray ---- */

void sa_init(StringArray *sa) {
    sa->items = NULL;
    sa->count = 0;
    sa->capacity = 0;
}

void sa_push(StringArray *sa, const char *str) {
    if (sa->count >= sa->capacity) {
        sa->capacity = sa->capacity ? sa->capacity * 2 : 4;
        sa->items = realloc(sa->items, sa->capacity * sizeof(char *));
    }
    sa->items[sa->count++] = strdup(str);
}

bool sa_contains(const StringArray *sa, const char *str) {
    for (int i = 0; i < sa->count; i++) {
        if (strcmp(sa->items[i], str) == 0) return true;
    }
    return false;
}

int sa_find(const StringArray *sa, const char *str) {
    for (int i = 0; i < sa->count; i++) {
        if (strcmp(sa->items[i], str) == 0) return i;
    }
    return -1;
}

void sa_remove_at(StringArray *sa, int index) {
    if (index < 0 || index >= sa->count) return;
    free(sa->items[index]);
    for (int i = index; i < sa->count - 1; i++) {
        sa->items[i] = sa->items[i + 1];
    }
    sa->count--;
}

void sa_clear(StringArray *sa) {
    for (int i = 0; i < sa->count; i++) free(sa->items[i]);
    sa->count = 0;
}

void sa_free(StringArray *sa) {
    for (int i = 0; i < sa->count; i++) free(sa->items[i]);
    free(sa->items);
    sa->items = NULL;
    sa->count = 0;
    sa->capacity = 0;
}

/* ---- TagArray ---- */

void ta_init(TagArray *ta) {
    ta->items = NULL;
    ta->count = 0;
    ta->capacity = 0;
}

static void ents_tag_free(EntsTag *tag) {
    free(tag->name);
    sa_free(&tag->children);
    sa_free(&tag->ancestry);
    sa_free(&tag->files);
    free(tag->alias);
}

EntsTag *ta_push(TagArray *ta) {
    if (ta->count >= ta->capacity) {
        ta->capacity = ta->capacity ? ta->capacity * 2 : 8;
        ta->items = realloc(ta->items, ta->capacity * sizeof(EntsTag));
    }
    EntsTag *t = &ta->items[ta->count++];
    memset(t, 0, sizeof(EntsTag));
    sa_init(&t->children);
    sa_init(&t->ancestry);
    sa_init(&t->files);
    t->show = true;
    t->has_files = false;
    t->name = NULL;
    t->alias = NULL;
    return t;
}

void ta_free(TagArray *ta) {
    for (int i = 0; i < ta->count; i++) ents_tag_free(&ta->items[i]);
    free(ta->items);
    ta->items = NULL;
    ta->count = 0;
    ta->capacity = 0;
}

/* ---- FileDataArray ---- */

void fda_init(FileDataArray *fda) {
    fda->items = NULL;
    fda->count = 0;
    fda->capacity = 0;
}

FileData *fda_push(FileDataArray *fda) {
    if (fda->count >= fda->capacity) {
        fda->capacity = fda->capacity ? fda->capacity * 2 : 4;
        fda->items = realloc(fda->items, fda->capacity * sizeof(FileData));
    }
    FileData *f = &fda->items[fda->count++];
    memset(f, 0, sizeof(FileData));
    f->last_known_name = NULL;
    return f;
}

void fda_free(FileDataArray *fda) {
    for (int i = 0; i < fda->count; i++) free(fda->items[i].last_known_name);
    free(fda->items);
    fda->items = NULL;
    fda->count = 0;
    fda->capacity = 0;
}

/* ---- AliasMap ---- */

void am_init(AliasMap *am) {
    am->items = NULL;
    am->count = 0;
    am->capacity = 0;
}

void am_put(AliasMap *am, const char *key, const char *value) {
    /* overwrite if exists */
    for (int i = 0; i < am->count; i++) {
        if (strcmp(am->items[i].key, key) == 0) {
            free(am->items[i].value);
            am->items[i].value = strdup(value);
            return;
        }
    }
    if (am->count >= am->capacity) {
        am->capacity = am->capacity ? am->capacity * 2 : 4;
        am->items = realloc(am->items, am->capacity * sizeof(AliasEntry));
    }
    am->items[am->count].key = strdup(key);
    am->items[am->count].value = strdup(value);
    am->count++;
}

const char *am_get(const AliasMap *am, const char *key) {
    for (int i = 0; i < am->count; i++) {
        if (strcmp(am->items[i].key, key) == 0) return am->items[i].value;
    }
    return NULL;
}

bool am_contains(const AliasMap *am, const char *key) {
    return am_get(am, key) != NULL;
}

void am_free(AliasMap *am) {
    for (int i = 0; i < am->count; i++) {
        free(am->items[i].key);
        free(am->items[i].value);
    }
    free(am->items);
    am->items = NULL;
    am->count = 0;
    am->capacity = 0;
}

/* ---- TagsFile ---- */

void tags_file_init(TagsFile *tf) {
    fda_init(&tf->files);
    am_init(&tf->aliases);
    ta_init(&tf->tags);
}

void tags_file_free(TagsFile *tf) {
    fda_free(&tf->files);
    am_free(&tf->aliases);
    ta_free(&tf->tags);
}

/* ---- JSON helpers ---- */

const char *tag_type_str(TagType t) {
    return t == TAG_TYPE_DUD ? "dud" : "default";
}

TagType tag_type_from_str(const char *s) {
    if (s && strcmp(s, "dud") == 0) return TAG_TYPE_DUD;
    return TAG_TYPE_DEFAULT;
}

static void parse_string_array(cJSON *arr, StringArray *sa) {
    if (!arr || !cJSON_IsArray(arr)) return;
    cJSON *item;
    cJSON_ArrayForEach(item, arr) {
        if (cJSON_IsString(item)) sa_push(sa, item->valuestring);
    }
}

int read_tags_from_json(TagsFile *tf) {
    tags_file_init(tf);

    FILE *fp = fopen("tags.json", "r");
    if (!fp) {
        printf("Error: tags.json not found. Run 'prlents process tags.ents' to create it.\n");
        return 0; /* not fatal, just empty */
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    fread(buf, 1, len, fp);
    buf[len] = '\0';
    fclose(fp);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        fprintf(stderr, "Error parsing tags.json\n");
        return -1;
    }

    /* files */
    cJSON *files_arr = cJSON_GetObjectItem(root, "files");
    if (files_arr && cJSON_IsArray(files_arr)) {
        cJSON *fitem;
        cJSON_ArrayForEach(fitem, files_arr) {
            FileData *fd = fda_push(&tf->files);
            cJSON *lkn = cJSON_GetObjectItem(fitem, "last_known_name");
            if (lkn && cJSON_IsString(lkn)) fd->last_known_name = strdup(lkn->valuestring);
            cJSON *fi = cJSON_GetObjectItem(fitem, "file_inode");
            if (fi) fd->file_inode = (uint64_t)fi->valuedouble;
            cJSON *pdi = cJSON_GetObjectItem(fitem, "parent_dir_inode");
            if (pdi) fd->parent_dir_inode = (uint64_t)pdi->valuedouble;
        }
    }

    /* aliases */
    cJSON *aliases_obj = cJSON_GetObjectItem(root, "aliases");
    if (aliases_obj && cJSON_IsObject(aliases_obj)) {
        cJSON *aitem;
        cJSON_ArrayForEach(aitem, aliases_obj) {
            if (cJSON_IsString(aitem)) {
                am_put(&tf->aliases, aitem->string, aitem->valuestring);
            }
        }
    }

    /* tags */
    cJSON *tags_arr = cJSON_GetObjectItem(root, "tags");
    if (tags_arr && cJSON_IsArray(tags_arr)) {
        cJSON *titem;
        cJSON_ArrayForEach(titem, tags_arr) {
            EntsTag *tag = ta_push(&tf->tags);

            cJSON *name = cJSON_GetObjectItem(titem, "name");
            if (name && cJSON_IsString(name)) tag->name = strdup(name->valuestring);

            cJSON *type = cJSON_GetObjectItem(titem, "type");
            if (type && cJSON_IsString(type)) tag->tag_type = tag_type_from_str(type->valuestring);

            parse_string_array(cJSON_GetObjectItem(titem, "children"), &tag->children);
            parse_string_array(cJSON_GetObjectItem(titem, "ancestry"), &tag->ancestry);

            cJSON *show = cJSON_GetObjectItem(titem, "show");
            if (show) tag->show = cJSON_IsTrue(show);

            cJSON *tag_files = cJSON_GetObjectItem(titem, "files");
            if (tag_files && cJSON_IsArray(tag_files)) {
                tag->has_files = true;
                parse_string_array(tag_files, &tag->files);
            } else {
                tag->has_files = false;
            }
        }
    }

    cJSON_Delete(root);
    return 0;
}

static cJSON *string_array_to_json(const StringArray *sa) {
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < sa->count; i++) {
        cJSON_AddItemToArray(arr, cJSON_CreateString(sa->items[i]));
    }
    return arr;
}

int save_tags_to_json(const TagsFile *tf) {
    cJSON *root = cJSON_CreateObject();

    /* files */
    cJSON *files_arr = cJSON_CreateArray();
    for (int i = 0; i < tf->files.count; i++) {
        const FileData *fd = &tf->files.items[i];
        cJSON *fobj = cJSON_CreateObject();
        cJSON_AddStringToObject(fobj, "last_known_name", fd->last_known_name ? fd->last_known_name : "");
        cJSON_AddNumberToObject(fobj, "file_inode", (double)fd->file_inode);
        cJSON_AddNumberToObject(fobj, "parent_dir_inode", (double)fd->parent_dir_inode);
        cJSON_AddItemToArray(files_arr, fobj);
    }
    cJSON_AddItemToObject(root, "files", files_arr);

    /* aliases */
    cJSON *aliases_obj = cJSON_CreateObject();
    for (int i = 0; i < tf->aliases.count; i++) {
        cJSON_AddStringToObject(aliases_obj, tf->aliases.items[i].key, tf->aliases.items[i].value);
    }
    cJSON_AddItemToObject(root, "aliases", aliases_obj);

    /* tags */
    cJSON *tags_arr = cJSON_CreateArray();
    for (int i = 0; i < tf->tags.count; i++) {
        const EntsTag *tag = &tf->tags.items[i];
        cJSON *tobj = cJSON_CreateObject();
        cJSON_AddStringToObject(tobj, "name", tag->name ? tag->name : "");
        cJSON_AddStringToObject(tobj, "type", tag_type_str(tag->tag_type));
        cJSON_AddItemToObject(tobj, "children", string_array_to_json(&tag->children));
        cJSON_AddItemToObject(tobj, "ancestry", string_array_to_json(&tag->ancestry));
        if (tag->show)
            cJSON_AddTrueToObject(tobj, "show");
        else
            cJSON_AddFalseToObject(tobj, "show");

        if (tag->has_files) {
            cJSON_AddItemToObject(tobj, "files", string_array_to_json(&tag->files));
        } else {
            cJSON_AddNullToObject(tobj, "files");
        }
        cJSON_AddItemToArray(tags_arr, tobj);
    }
    cJSON_AddItemToObject(root, "tags", tags_arr);

    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);

    FILE *fp = fopen("tags.json", "w");
    if (!fp) {
        fprintf(stderr, "Error: could not write tags.json\n");
        free(json_str);
        return -1;
    }
    fputs(json_str, fp);
    fputc('\n', fp);
    fclose(fp);
    free(json_str);
    return 0;
}
