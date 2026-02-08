#include "merge_tags.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int merge_tags(const char *new_json_content, const char *output_file) {
    /* Parse the new tags */
    cJSON *new_root = cJSON_Parse(new_json_content);
    if (!new_root) {
        fprintf(stderr, "Error parsing new tags JSON\n");
        return -1;
    }

    /* Check if output file exists */
    FILE *fp = fopen(output_file, "r");
    if (!fp) {
        /* doesn't exist, just write the new content with show=true on all tags */
        cJSON *tags = cJSON_GetObjectItem(new_root, "tags");
        if (tags && cJSON_IsArray(tags)) {
            cJSON *t;
            cJSON_ArrayForEach(t, tags) {
                cJSON *show = cJSON_GetObjectItem(t, "show");
                if (show) cJSON_ReplaceItemInObject(t, "show", cJSON_CreateTrue());
                else cJSON_AddTrueToObject(t, "show");
            }
        }

        /* add empty files array if missing */
        if (!cJSON_GetObjectItem(new_root, "files")) {
            cJSON_AddItemToObject(new_root, "files", cJSON_CreateArray());
        }

        char *out = cJSON_Print(new_root);
        FILE *wfp = fopen("tags.json", "w");
        if (wfp) {
            fputs(out, wfp);
            fputc('\n', wfp);
            fclose(wfp);
        }
        free(out);
        cJSON_Delete(new_root);
        return 0;
    }

    /* Read existing file */
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *existing_buf = malloc(len + 1);
    fread(existing_buf, 1, len, fp);
    existing_buf[len] = '\0';
    fclose(fp);

    cJSON *existing_root = cJSON_Parse(existing_buf);
    free(existing_buf);
    if (!existing_root) {
        fprintf(stderr, "Error parsing existing tags.json\n");
        cJSON_Delete(new_root);
        return -1;
    }

    cJSON *existing_tags = cJSON_GetObjectItem(existing_root, "tags");
    cJSON *new_tags = cJSON_GetObjectItem(new_root, "tags");

    /* Build merged tags array */
    cJSON *merged_tags = cJSON_CreateArray();

    /* Process new tags - update from existing if present */
    if (new_tags && cJSON_IsArray(new_tags)) {
        cJSON *nt;
        cJSON_ArrayForEach(nt, new_tags) {
            cJSON *nt_name = cJSON_GetObjectItem(nt, "name");
            if (!nt_name) continue;

            /* look for this tag in existing */
            cJSON *found_existing = NULL;
            if (existing_tags && cJSON_IsArray(existing_tags)) {
                cJSON *et;
                cJSON_ArrayForEach(et, existing_tags) {
                    cJSON *et_name = cJSON_GetObjectItem(et, "name");
                    if (et_name && strcmp(et_name->valuestring, nt_name->valuestring) == 0) {
                        found_existing = et;
                        break;
                    }
                }
            }

            cJSON *merged = cJSON_Duplicate(nt, 1);

            if (found_existing) {
                /* keep existing files */
                cJSON *ex_files = cJSON_GetObjectItem(found_existing, "files");
                if (ex_files) {
                    cJSON_DeleteItemFromObject(merged, "files");
                    cJSON_AddItemToObject(merged, "files", cJSON_Duplicate(ex_files, 1));
                }
            }

            /* set show = true */
            cJSON *show = cJSON_GetObjectItem(merged, "show");
            if (show) cJSON_ReplaceItemInObject(merged, "show", cJSON_CreateTrue());
            else cJSON_AddTrueToObject(merged, "show");

            cJSON_AddItemToArray(merged_tags, merged);
        }
    }

    /* Process existing tags not in new - mark as hidden */
    if (existing_tags && cJSON_IsArray(existing_tags)) {
        cJSON *et;
        cJSON_ArrayForEach(et, existing_tags) {
            cJSON *et_name = cJSON_GetObjectItem(et, "name");
            if (!et_name) continue;

            /* check if already in merged */
            int found = 0;
            cJSON *mt;
            cJSON_ArrayForEach(mt, merged_tags) {
                cJSON *mt_name = cJSON_GetObjectItem(mt, "name");
                if (mt_name && strcmp(mt_name->valuestring, et_name->valuestring) == 0) {
                    found = 1;
                    break;
                }
            }

            if (!found) {
                cJSON *hidden = cJSON_Duplicate(et, 1);
                cJSON *show = cJSON_GetObjectItem(hidden, "show");
                if (show) cJSON_ReplaceItemInObject(hidden, "show", cJSON_CreateFalse());
                else cJSON_AddFalseToObject(hidden, "show");
                cJSON_AddItemToArray(merged_tags, hidden);
            }
        }
    }

    /* Build merged result */
    cJSON *merged_root = cJSON_CreateObject();

    /* files - keep existing */
    cJSON *existing_files = cJSON_GetObjectItem(existing_root, "files");
    if (existing_files) {
        cJSON_AddItemToObject(merged_root, "files", cJSON_Duplicate(existing_files, 1));
    } else {
        cJSON_AddItemToObject(merged_root, "files", cJSON_CreateArray());
    }

    /* aliases - merge both, new takes priority */
    cJSON *merged_aliases = cJSON_CreateObject();
    cJSON *existing_aliases = cJSON_GetObjectItem(existing_root, "aliases");
    if (existing_aliases && cJSON_IsObject(existing_aliases)) {
        cJSON *a;
        cJSON_ArrayForEach(a, existing_aliases) {
            if (cJSON_IsString(a)) {
                cJSON_AddStringToObject(merged_aliases, a->string, a->valuestring);
            }
        }
    }
    cJSON *new_aliases = cJSON_GetObjectItem(new_root, "aliases");
    if (new_aliases && cJSON_IsObject(new_aliases)) {
        cJSON *a;
        cJSON_ArrayForEach(a, new_aliases) {
            if (cJSON_IsString(a)) {
                cJSON_DeleteItemFromObject(merged_aliases, a->string);
                cJSON_AddStringToObject(merged_aliases, a->string, a->valuestring);
            }
        }
    }
    cJSON_AddItemToObject(merged_root, "aliases", merged_aliases);

    /* tags */
    cJSON_AddItemToObject(merged_root, "tags", merged_tags);

    /* write */
    char *out = cJSON_Print(merged_root);
    FILE *wfp = fopen("tags.json", "w");
    if (wfp) {
        fputs(out, wfp);
        fputc('\n', wfp);
        fclose(wfp);
    }
    free(out);

    cJSON_Delete(new_root);
    cJSON_Delete(existing_root);
    cJSON_Delete(merged_root);

    return 0;
}
