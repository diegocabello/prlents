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

/* ---- Helpers ---- */

const char *tag_type_str(TagType t) {
    return t == TAG_TYPE_DUD ? "dud" : "default";
}

TagType tag_type_from_str(const char *s) {
    if (s && strcmp(s, "dud") == 0) return TAG_TYPE_DUD;
    return TAG_TYPE_DEFAULT;
}

/* ---- DTOB format ---- */

#include "dtob.h"

#define TAGS_DTOB_FILE "tags.dtob"

#define REL_MODE_SPARSE_POS 0
#define REL_MODE_MATRIX     1
#define REL_MODE_SPARSE_NEG 2

/* Build a DtobValue string array from StringArray */
static DtobValue *sa_to_dtob(const StringArray *sa) {
    DtobValue *arr = dtob_array();
    for (int i = 0; i < sa->count; i++)
        dtob_array_push(arr, dtob_string(sa->items[i], strlen(sa->items[i])));
    return arr;
}

/* Extract a string from a DtobValue (caller must free) */
static char *dtob_val_to_str(const DtobValue *v) {
    if (!v || v->type != DTOB_STRING || !v->data) return strdup("");
    char *s = malloc(v->data_len + 1);
    memcpy(s, v->data, v->data_len);
    s[v->data_len] = '\0';
    return s;
}

/* Extract uint64 from a DtobValue INT */
static uint64_t dtob_val_to_u64(const DtobValue *v) {
    if (!v || v->type != DTOB_INT || !v->data) return 0;
    uint64_t val = 0;
    for (size_t i = 0; i < v->data_len; i++)
        val = (val << 8) | v->data[i];
    return val;
}

/* Find a KV pair by key string */
static DtobValue *dtob_kv_get(const DtobValue *kv, const char *key) {
    if (!kv || kv->type != DTOB_KV_SET) return NULL;
    size_t klen = strlen(key);
    for (size_t i = 0; i < kv->num_pairs; i++) {
        if (kv->pairs[i].key_len == klen &&
            memcmp(kv->pairs[i].key, key, klen) == 0)
            return kv->pairs[i].value;
    }
    return NULL;
}

/* Read StringArray from a dtob array of strings */
static void dtob_arr_to_sa(const DtobValue *arr, StringArray *sa) {
    if (!arr || arr->type != DTOB_ARRAY) return;
    for (size_t i = 0; i < arr->num_elements; i++) {
        char *s = dtob_val_to_str(arr->elements[i]);
        sa_push(sa, s);
        free(s);
    }
}

/* ---- Relationship helpers ---- */

typedef struct { uint16_t tag_idx; uint16_t file_idx; } RelPair;

static void build_rel_pairs(const TagsFile *tf, RelPair **out_pairs,
                            int *out_count) {
    RelPair *pairs = NULL;
    int count = 0, cap = 0;

    for (int t = 0; t < tf->tags.count; t++) {
        const EntsTag *tag = &tf->tags.items[t];
        if (!tag->has_files) continue;
        for (int f = 0; f < tag->files.count; f++) {
            uint64_t inode = strtoull(tag->files.items[f], NULL, 10);
            for (int fi = 0; fi < tf->files.count; fi++) {
                if (tf->files.items[fi].file_inode == inode) {
                    if (count >= cap) {
                        cap = cap ? cap * 2 : 16;
                        pairs = realloc(pairs, cap * sizeof(RelPair));
                    }
                    pairs[count].tag_idx = (uint16_t)t;
                    pairs[count].file_idx = (uint16_t)fi;
                    count++;
                    break;
                }
            }
        }
    }
    *out_pairs = pairs;
    *out_count = count;
}

static int select_rel_mode(int num_tags, int num_files, int pair_count) {
    uint32_t total_cells = (uint32_t)num_tags * (uint32_t)num_files;
    if (total_cells == 0) return REL_MODE_SPARSE_POS;
    uint32_t cost_pos = 4 * (uint32_t)pair_count;
    uint32_t cost_mat = (total_cells + 7) / 8;
    uint32_t cost_neg = 4 * (total_cells - (uint32_t)pair_count);
    if (cost_pos <= cost_mat && cost_pos <= cost_neg) return REL_MODE_SPARSE_POS;
    if (cost_mat <= cost_neg) return REL_MODE_MATRIX;
    return REL_MODE_SPARSE_NEG;
}

/* Populate tag->files from relationship pairs */
static void apply_rel(TagsFile *tf, uint16_t ti, uint16_t fi) {
    if (ti >= (uint16_t)tf->tags.count || fi >= (uint16_t)tf->files.count) return;
    EntsTag *tag = &tf->tags.items[ti];
    if (!tag->has_files) { tag->has_files = true; sa_init(&tag->files); }
    char inode_str[32];
    snprintf(inode_str, sizeof(inode_str), "%llu",
             (unsigned long long)tf->files.items[fi].file_inode);
    if (!sa_contains(&tag->files, inode_str))
        sa_push(&tag->files, inode_str);
}

/* ---- DTOB save ---- */

int save_tags_bin(const TagsFile *tf) {
    DtobValue *root = dtob_kvset();

    /* aliases */
    DtobValue *aliases_kv = dtob_kvset();
    for (int i = 0; i < tf->aliases.count; i++)
        dtob_kvset_put(aliases_kv, tf->aliases.items[i].key,
                       dtob_string(tf->aliases.items[i].value,
                                   strlen(tf->aliases.items[i].value)));
    dtob_kvset_put(root, "aliases", aliases_kv);

    /* tags */
    DtobValue *tags_arr = dtob_array();
    for (int i = 0; i < tf->tags.count; i++) {
        const EntsTag *tag = &tf->tags.items[i];
        DtobValue *t = dtob_kvset();
        dtob_kvset_put(t, "n", dtob_string(tag->name ? tag->name : "",
                                            tag->name ? strlen(tag->name) : 0));
        /* tag_type as custom typed value (bare element, no key) */
        uint8_t tcode = (uint8_t)(DTOB_CUSTOM_MIN + tag->tag_type);
        DtobValue *tv = dtob_custom(tcode, NULL, 0);
        t->elements = realloc(t->elements, (t->num_elements + 1) * sizeof(DtobValue *));
        t->elements[t->num_elements++] = tv;
        dtob_kvset_put(t, "s", tag->show ? dtob_true() : dtob_false());
        dtob_kvset_put(t, "c", sa_to_dtob(&tag->children));
        dtob_kvset_put(t, "a", sa_to_dtob(&tag->ancestry));
        dtob_array_push(tags_arr, t);
    }
    dtob_kvset_put(root, "tags", tags_arr);

    /* files */
    DtobValue *files_arr = dtob_array();
    for (int i = 0; i < tf->files.count; i++) {
        const FileData *fd = &tf->files.items[i];
        DtobValue *f = dtob_kvset();
        dtob_kvset_put(f, "n", dtob_string(fd->last_known_name ? fd->last_known_name : "",
                                            fd->last_known_name ? strlen(fd->last_known_name) : 0));
        dtob_kvset_put(f, "i", dtob_uint(fd->file_inode));
        dtob_kvset_put(f, "p", dtob_uint(fd->parent_dir_inode));
        dtob_array_push(files_arr, f);
    }
    dtob_kvset_put(root, "files", files_arr);

    /* relationships — native DTOB fields */
    RelPair *pairs;
    int pair_count;
    build_rel_pairs(tf, &pairs, &pair_count);
    int mode = select_rel_mode(tf->tags.count, tf->files.count, pair_count);

    const char *mode_str = mode == REL_MODE_SPARSE_POS ? "pos" :
                           mode == REL_MODE_MATRIX ? "matrix" : "neg";
    dtob_kvset_put(root, "rel_mode", dtob_string(mode_str, strlen(mode_str)));

    if (mode == REL_MODE_SPARSE_POS || mode == REL_MODE_SPARSE_NEG) {
        /* build raw binary blob: each pair = uint16 LE tag_idx + uint16 LE file_idx */
        int raw_count = 0;
        uint8_t *buf = NULL;

        if (mode == REL_MODE_SPARSE_POS) {
            buf = malloc((size_t)pair_count * 4);
            for (int i = 0; i < pair_count; i++) {
                buf[i*4+0] = (uint8_t)(pairs[i].tag_idx & 0xFF);
                buf[i*4+1] = (uint8_t)(pairs[i].tag_idx >> 8);
                buf[i*4+2] = (uint8_t)(pairs[i].file_idx & 0xFF);
                buf[i*4+3] = (uint8_t)(pairs[i].file_idx >> 8);
            }
            raw_count = pair_count;
        } else {
            /* negative: emit all absent pairs */
            int nt = tf->tags.count, nf = tf->files.count;
            size_t cap = 64;
            buf = malloc(cap * 4);
            for (int t = 0; t < nt; t++) {
                for (int f = 0; f < nf; f++) {
                    bool found = false;
                    for (int p = 0; p < pair_count; p++) {
                        if (pairs[p].tag_idx == t && pairs[p].file_idx == f) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        if ((size_t)raw_count >= cap) {
                            cap *= 2;
                            buf = realloc(buf, cap * 4);
                        }
                        buf[raw_count*4+0] = (uint8_t)(t & 0xFF);
                        buf[raw_count*4+1] = (uint8_t)((t >> 8) & 0xFF);
                        buf[raw_count*4+2] = (uint8_t)(f & 0xFF);
                        buf[raw_count*4+3] = (uint8_t)((f >> 8) & 0xFF);
                        raw_count++;
                    }
                }
            }
        }
        dtob_kvset_put(root, "rel_pairs", dtob_raw(buf, (size_t)raw_count * 4));
        free(buf);
    } else {
        /* matrix mode: bit matrix as raw, with dimensions */
        int nt = tf->tags.count, nf = tf->files.count;
        uint32_t total = (uint32_t)nt * (uint32_t)nf;
        uint32_t mat_bytes = (total + 7) / 8;
        uint8_t *matrix = calloc(mat_bytes, 1);
        for (int i = 0; i < pair_count; i++) {
            uint32_t bit = (uint32_t)pairs[i].tag_idx * (uint32_t)nf + pairs[i].file_idx;
            matrix[bit / 8] |= (0x80 >> (bit % 8));
        }
        dtob_kvset_put(root, "rel_rows", dtob_uint((uint64_t)nt));
        dtob_kvset_put(root, "rel_cols", dtob_uint((uint64_t)nf));
        dtob_kvset_put(root, "rel_matrix", dtob_raw(matrix, mat_bytes));
        free(matrix);
    }

    free(pairs);

    /* encode with types header */
    DtobTypesHeader th;
    dtob_types_init(&th);
    dtob_types_add(&th, DTOB_CUSTOM_MIN + TAG_TYPE_DEFAULT, "default");
    dtob_types_add(&th, DTOB_CUSTOM_MIN + TAG_TYPE_DUD, "dud");

    size_t enc_len;
    uint8_t *enc = dtob_encode_with_types(root, &th, &enc_len);
    dtob_free(root);

    if (!enc) {
        fprintf(stderr, "Error: DTOB encoding failed\n");
        return -1;
    }

    FILE *fp = fopen(TAGS_DTOB_FILE, "wb");
    if (!fp) {
        fprintf(stderr, "Error: could not write %s\n", TAGS_DTOB_FILE);
        free(enc);
        return -1;
    }
    fwrite(enc, 1, enc_len, fp);
    fclose(fp);
    free(enc);
    return 0;
}

/* ---- DTOB read ---- */

int read_tags_bin(TagsFile *tf) {
    tags_file_init(tf);

    FILE *fp = fopen(TAGS_DTOB_FILE, "rb");
    if (!fp) {
        printf("Error: %s not found. Run 'prlents process tags.ents' to create it.\n",
               TAGS_DTOB_FILE);
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    uint8_t *buf = malloc(fsize);
    fread(buf, 1, fsize, fp);
    fclose(fp);

    DtobValue *root = dtob_decode(buf, fsize);
    free(buf);
    if (!root) {
        fprintf(stderr, "Error: failed to decode %s\n", TAGS_DTOB_FILE);
        return -1;
    }

    if (root->type != DTOB_KV_SET) {
        fprintf(stderr, "Error: %s root is not a KV set\n", TAGS_DTOB_FILE);
        dtob_free(root);
        return -1;
    }

    /* aliases */
    DtobValue *aliases_kv = dtob_kv_get(root, "aliases");
    if (aliases_kv && aliases_kv->type == DTOB_KV_SET) {
        for (size_t i = 0; i < aliases_kv->num_pairs; i++) {
            char *key = malloc(aliases_kv->pairs[i].key_len + 1);
            memcpy(key, aliases_kv->pairs[i].key, aliases_kv->pairs[i].key_len);
            key[aliases_kv->pairs[i].key_len] = '\0';
            char *val = dtob_val_to_str(aliases_kv->pairs[i].value);
            am_put(&tf->aliases, key, val);
            free(key);
            free(val);
        }
    }

    /* tags */
    DtobValue *tags_arr = dtob_kv_get(root, "tags");
    if (tags_arr && tags_arr->type == DTOB_ARRAY) {
        for (size_t i = 0; i < tags_arr->num_elements; i++) {
            DtobValue *te = tags_arr->elements[i];
            if (te->type != DTOB_KV_SET) continue;
            EntsTag *tag = ta_push(&tf->tags);
            tag->name = dtob_val_to_str(dtob_kv_get(te, "n"));
            /* tag_type from custom typed element (bare, no key) */
            tag->tag_type = TAG_TYPE_DEFAULT;
            for (size_t ei = 0; ei < te->num_elements; ei++) {
                if (te->elements[ei]->type == DTOB_CUSTOM) {
                    tag->tag_type = (TagType)(te->elements[ei]->custom_code - DTOB_CUSTOM_MIN);
                    break;
                }
            }
            DtobValue *sv = dtob_kv_get(te, "s");
            tag->show = (sv && sv->type == DTOB_TRUE);
            dtob_arr_to_sa(dtob_kv_get(te, "c"), &tag->children);
            dtob_arr_to_sa(dtob_kv_get(te, "a"), &tag->ancestry);
            tag->has_files = false;
        }
    }

    /* files */
    DtobValue *files_arr = dtob_kv_get(root, "files");
    if (files_arr && files_arr->type == DTOB_ARRAY) {
        for (size_t i = 0; i < files_arr->num_elements; i++) {
            DtobValue *fe = files_arr->elements[i];
            if (fe->type != DTOB_KV_SET) continue;
            FileData *fd = fda_push(&tf->files);
            fd->last_known_name = dtob_val_to_str(dtob_kv_get(fe, "n"));
            fd->file_inode = dtob_val_to_u64(dtob_kv_get(fe, "i"));
            fd->parent_dir_inode = dtob_val_to_u64(dtob_kv_get(fe, "p"));
        }
    }

    /* relationships */
    char *mode_str = dtob_val_to_str(dtob_kv_get(root, "rel_mode"));
    int mode = REL_MODE_SPARSE_POS;
    if (strcmp(mode_str, "matrix") == 0) mode = REL_MODE_MATRIX;
    else if (strcmp(mode_str, "neg") == 0) mode = REL_MODE_SPARSE_NEG;
    free(mode_str);

    if (mode == REL_MODE_SPARSE_POS || mode == REL_MODE_SPARSE_NEG) {
        DtobValue *raw = dtob_kv_get(root, "rel_pairs");
        if (raw && raw->type == DTOB_RAW && raw->data) {
            size_t n_pairs = raw->data_len / 4;
            if (mode == REL_MODE_SPARSE_POS) {
                for (size_t i = 0; i < n_pairs; i++) {
                    uint16_t ti = (uint16_t)(raw->data[i*4] | (raw->data[i*4+1] << 8));
                    uint16_t fi = (uint16_t)(raw->data[i*4+2] | (raw->data[i*4+3] << 8));
                    apply_rel(tf, ti, fi);
                }
            } else {
                /* negative: all pairs exist EXCEPT these */
                int nt = tf->tags.count, nf = tf->files.count;
                uint32_t *neg_set = malloc(n_pairs * sizeof(uint32_t));
                for (size_t i = 0; i < n_pairs; i++) {
                    uint16_t ti = (uint16_t)(raw->data[i*4] | (raw->data[i*4+1] << 8));
                    uint16_t fi = (uint16_t)(raw->data[i*4+2] | (raw->data[i*4+3] << 8));
                    neg_set[i] = ((uint32_t)ti << 16) | fi;
                }
                for (int t = 0; t < nt; t++) {
                    for (int f = 0; f < nf; f++) {
                        uint32_t key = ((uint32_t)t << 16) | f;
                        bool negated = false;
                        for (size_t n = 0; n < n_pairs; n++) {
                            if (neg_set[n] == key) { negated = true; break; }
                        }
                        if (!negated) apply_rel(tf, (uint16_t)t, (uint16_t)f);
                    }
                }
                free(neg_set);
            }
        }
    } else {
        /* matrix mode */
        DtobValue *mat = dtob_kv_get(root, "rel_matrix");
        uint16_t nf = (uint16_t)dtob_val_to_u64(dtob_kv_get(root, "rel_cols"));
        uint16_t nt = (uint16_t)dtob_val_to_u64(dtob_kv_get(root, "rel_rows"));
        if (mat && mat->type == DTOB_RAW && mat->data && nf > 0) {
            for (int t = 0; t < nt && t < tf->tags.count; t++) {
                for (int f = 0; f < nf && f < tf->files.count; f++) {
                    uint32_t bit = (uint32_t)t * (uint32_t)nf + f;
                    if (bit / 8 < mat->data_len &&
                        (mat->data[bit / 8] & (0x80 >> (bit % 8))))
                        apply_rel(tf, (uint16_t)t, (uint16_t)f);
                }
            }
        }
    }

    dtob_free(root);
    return 0;
}
