#include "migrate.h"
#include "cJSON.h"
#include "dtob.h"
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

/*
 * Old format (magic "13032026" or "28042026"):
 *   code 0 = generic OPEN, 1 = CLOSE_ARR, 2 = CLOSE_KV, 3 = CLOSE_TYPES
 *   trit data may have odd byte length (ONE_NIBBLE_PAD)
 *
 * New format (magic "01052026"):
 *   code 0 = OPEN_TYPES, 1 = OPEN_ARR, 2 = OPEN_KV, 3 = generic CLOSE
 *   trit data always even byte length (word-aligned, THREE_NIBBLE_PAD)
 *   types header BEFORE root container (not nested inside)
 *
 * Migration: remap codes, re-pad trit data, restructure types position.
 */

/* from libdtob (internal but linked) */
extern size_t trit_encode_padded(const uint8_t *bytes, size_t byte_len,
                                 uint8_t **out_buf);
extern size_t trit_decode_padded(const uint8_t *buf, size_t buf_len,
                                 uint8_t **out_bytes);

#define OLD_OPEN        0
#define OLD_CLOSE_ARR   1
#define OLD_CLOSE_KV    2
#define OLD_CLOSE_TYPES 3

/* Does this old-format code carry a data payload? */
static int old_code_has_data(uint16_t code) {
    return code >= 4; /* codes 0-3 are structural, 4+ carry trit data */
}

int migrate_dtob_v2(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "Error: could not open %s\n", path); return -1; }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsize < 10) { fclose(fp); fprintf(stderr, "Error: file too small\n"); return -1; }

    uint8_t *buf = malloc(fsize);
    fread(buf, 1, fsize, fp);
    fclose(fp);

    if (memcmp(buf, "01052026", 8) == 0) {
        printf("%s is already in the new format\n", path);
        free(buf); return 0;
    }
    if (memcmp(buf, "13032026", 8) != 0 && memcmp(buf, "28042026", 8) != 0) {
        fprintf(stderr, "Error: %s has unrecognized magic '%.8s'\n", path, buf);
        free(buf); return -1;
    }

    /*
     * Pass 1: Walk old file byte-by-byte, build open_type[] map.
     * Old format is NOT word-aligned (odd-length trit data shifts offsets).
     */
    size_t stack_cap = 256;
    size_t *open_stack = malloc(stack_cap * sizeof(size_t));
    /* open_type_map stores: index=position in OLD buf, value=new open code+1 */
    uint8_t *open_type_map = calloc(fsize, 1);
    int depth = 0;

    size_t pos = 8;
    while (pos + 1 < (size_t)fsize) {
        if (!DTOB_IS_CTRL(buf[pos])) { pos++; continue; }
        uint16_t code = ((buf[pos] & 0x1F) << 8) | buf[pos + 1];

        if (code == OLD_OPEN) {
            if (depth >= (int)stack_cap) {
                stack_cap *= 2;
                open_stack = realloc(open_stack, stack_cap * sizeof(size_t));
            }
            open_stack[depth++] = pos;
            pos += 2;
        } else if (code >= OLD_CLOSE_ARR && code <= OLD_CLOSE_TYPES) {
            uint8_t new_open;
            if (code == OLD_CLOSE_ARR)   new_open = 1; /* OPEN_ARR */
            else if (code == OLD_CLOSE_KV) new_open = 2; /* OPEN_KV */
            else                           new_open = 0; /* OPEN_TYPES */

            if (depth > 0)
                open_type_map[open_stack[--depth]] = new_open + 1;
            pos += 2;
        } else {
            pos += 2;
            if (old_code_has_data(code))
                while (pos < (size_t)fsize && !DTOB_IS_CTRL(buf[pos])) pos++;
        }
    }

    /*
     * Pass 2: Rebuild buffer with remapped codes AND re-padded trit data.
     * Output buffer may be larger (odd→even padding adds bytes).
     */
    size_t out_cap = fsize * 2;
    uint8_t *out = malloc(out_cap);
    size_t wp = 0;

    /* write new magic */
    memcpy(out, "01052026", 8);
    wp = 8;

    pos = 8;
    while (pos + 1 < (size_t)fsize) {
        if (!DTOB_IS_CTRL(buf[pos])) { pos++; continue; }
        uint16_t code = ((buf[pos] & 0x1F) << 8) | buf[pos + 1];

        /* determine new code */
        uint16_t new_code;
        if (code == OLD_OPEN) {
            new_code = open_type_map[pos] > 0 ? (open_type_map[pos] - 1) : 2;
        } else if (code >= OLD_CLOSE_ARR && code <= OLD_CLOSE_TYPES) {
            new_code = 3; /* generic CLOSE */
        } else {
            new_code = code; /* data codes unchanged */
        }

        /* write control word */
        if (wp + 2 > out_cap) { out_cap *= 2; out = realloc(out, out_cap); }
        out[wp]     = 0xC0 | ((new_code >> 8) & 0x3F);
        out[wp + 1] = new_code & 0xFF;
        wp += 2;
        pos += 2;

        /* if data-bearing, re-encode trit data */
        if (old_code_has_data(code)) {
            size_t data_start = pos;
            while (pos < (size_t)fsize && !DTOB_IS_CTRL(buf[pos])) pos++;
            size_t data_len = pos - data_start;

            if (data_len > 0) {
                /* decode old trit data */
                uint8_t *decoded = NULL;
                size_t dec_len = trit_decode_padded(buf + data_start, data_len, &decoded);

                if (!decoded || dec_len == 0) {
                    fprintf(stderr, "Error: trit decode failed at offset %zu\n", data_start);
                    free(out); free(buf); free(open_stack); free(open_type_map);
                    return -1;
                }

                /* re-encode with new padding (always even length) */
                uint8_t *reencoded = NULL;
                size_t reenc_len = trit_encode_padded(decoded, dec_len, &reencoded);
                free(decoded);

                if (!reencoded) {
                    fprintf(stderr, "Error: trit re-encode failed at offset %zu\n", data_start);
                    free(out); free(buf); free(open_stack); free(open_type_map);
                    return -1;
                }

                /* write re-encoded data */
                while (wp + reenc_len > out_cap) { out_cap *= 2; out = realloc(out, out_cap); }
                memcpy(out + wp, reencoded, reenc_len);
                wp += reenc_len;
                free(reencoded);
            }
        }
    }

    free(open_stack);
    free(open_type_map);
    free(buf);

    /* pass 2 done — out buffer is word-aligned with re-padded trit data */

    /*
     * Pass 3: Structural fix — move types section before root.
     * Old: OPEN_KV(root) → OPEN_TYPES → types... → CLOSE → data... → CLOSE
     * New: OPEN_TYPES → types... → CLOSE → OPEN_KV(root) → data... → CLOSE
     */
    /* Pass 3 scanner: word-aligned (2-byte steps).
     * After pass 2, all control words are at even offsets and trit data
     * has even byte length. We MUST NOT scan byte-by-byte because trit
     * data can contain bytes in 0xC0-0xDF range (e.g. middle byte of
     * a 3-byte group when enc_tbl values are large). */
    size_t types_start = 0, types_end = 0;
    int found_types = 0;
    pos = 8;
    while (pos + 1 < wp) {
        uint16_t code = ((out[pos] & 0x1F) << 8) | out[pos + 1];
        if (code == 0 && !found_types) { /* OPEN_TYPES */
            types_start = pos;
            int td = 1;
            size_t tp = pos + 2;
            while (tp + 1 < wp && td > 0) {
                uint16_t tc = ((out[tp] & 0x1F) << 8) | out[tp + 1];
                if (tc <= 2) td++;
                else if (tc == 3) td--;
                tp += 2;
                /* skip trit data (even number of bytes) */
                if (tc >= 4)
                    while (tp + 1 < wp && !DTOB_IS_CTRL(out[tp])) tp += 2;
            }
            types_end = tp;
            found_types = 1;
            break;
        }
        pos += 2;
        if (code >= 4)
            while (pos + 1 < wp && !DTOB_IS_CTRL(out[pos])) pos += 2;
    }

    if (!found_types) {
        fprintf(stderr, "Error: could not find OPEN_TYPES in translated output\n");
        free(out); return -1;
    }

    /* reassemble: magic + types + root_open + data_after_types + root_close */
    size_t types_len = types_end - types_start;
    uint8_t *final = malloc(wp);
    size_t fp2 = 0;
    memcpy(final + fp2, out, 8); fp2 += 8;                                       /* magic */
    memcpy(final + fp2, out + types_start, types_len); fp2 += types_len;          /* types */
    memcpy(final + fp2, out + 8, types_start - 8); fp2 += (types_start - 8);     /* root OPEN */
    memcpy(final + fp2, out + types_end, wp - types_end); fp2 += (wp - types_end); /* data + root CLOSE */
    free(out);

    /* write result */
    fp = fopen(path, "wb");
    if (!fp) { free(final); return -1; }
    fwrite(final, 1, fp2, fp);
    fclose(fp);
    free(final);

    printf("Migrated %s to new dtob format\n", path);
    return 0;
}
