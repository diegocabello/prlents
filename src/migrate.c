#include "migrate.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void parse_string_array(cJSON *arr, StringArray *sa) {
    if (!arr || !cJSON_IsArray(arr)) return;
    cJSON *item;
    cJSON_ArrayForEach(item, arr) {
        if (cJSON_IsString(item)) sa_push(sa, item->valuestring);
    }
}

int read_tags_from_json(const char *path, TagsFile *tf) {
    tags_file_init(tf);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Error: could not open %s\n", path);
        return -1;
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
        fprintf(stderr, "Error parsing JSON in %s\n", path);
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
            if (cJSON_IsString(aitem))
                am_put(&tf->aliases, aitem->string, aitem->valuestring);
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
