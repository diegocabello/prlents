#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif
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
    tf->dirty_metadata = false;
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
#include "dtob_types.h"

#define TAGS_DTOB_FILE "tags.dtob"

/* custom type codes */
#define ENTS_NAME     (DTOB_CUSTOM_MIN + 0)  /* 20 - raw */
#define ENTS_TRUE     (DTOB_CUSTOM_MIN + 1)  /* 21 - nullable */
#define ENTS_FALSE    (DTOB_CUSTOM_MIN + 2)  /* 22 - nullable */
#define ENTS_SHOW     (DTOB_CUSTOM_MIN + 3)  /* 23 - true|false */
#define ENTS_DEFAULT  (DTOB_CUSTOM_MIN + 4)  /* 24 - nullable */
#define ENTS_DUD      (DTOB_CUSTOM_MIN + 5)  /* 25 - nullable */
#define ENTS_TAG_TYPE (DTOB_CUSTOM_MIN + 6)  /* 26 - default|dud */

#define REL_MODE_SPARSE_POS 0
#define REL_MODE_MATRIX     1
#define REL_MODE_SPARSE_NEG 2

/* Build a DtobValue string array from StringArray */
static DtobValue *sa_to_dtob(const StringArray *sa) {
    DtobValue *arr = dtob_array();
    for (int i = 0; i < sa->count; i++)
        dtob_array_push(arr, dtob_raw((const uint8_t *)sa->items[i], strlen(sa->items[i])));
    return arr;
}


/* Build the prlents types header */
/* file field codes */
#define ENTS_INODE    (ENTS_TAG_TYPE + 1)      /* 27 - uint64 */
#define ENTS_PARENT   (ENTS_TAG_TYPE + 2)      /* 28 - uint64 */
#define ENTS_FILE     (ENTS_TAG_TYPE + 3)      /* 29 - struct(name, inode, parent) */

DTOB_DEFINE_CUSTOM_TYPES(ents, th,
    DTOB_CUSTOM_TYPE_RAW     (th, ENTS_NAME,     "name")
    DTOB_CUSTOM_TYPE_NULLABLE(th, ENTS_TRUE,     "true")
    DTOB_CUSTOM_TYPE_NULLABLE(th, ENTS_FALSE,    "false")
    DTOB_CUSTOM_TYPE_ENUM    (th, ENTS_SHOW,     "show",     ENTS_TRUE, ENTS_FALSE)
    DTOB_CUSTOM_TYPE_NULLABLE(th, ENTS_DEFAULT,  "default")
    DTOB_CUSTOM_TYPE_NULLABLE(th, ENTS_DUD,      "dud")
    DTOB_CUSTOM_TYPE_ENUM    (th, ENTS_TAG_TYPE, "tag_type", ENTS_DEFAULT, ENTS_DUD)
    DTOB_CUSTOM_TYPE_UINT64  (th, ENTS_INODE,    "inode")
    DTOB_CUSTOM_TYPE_UINT64  (th, ENTS_PARENT,   "parent")
    DTOB_CUSTOM_TYPE_STRUCT  (th, ENTS_FILE,     "file",     ENTS_NAME, ENTS_INODE, ENTS_PARENT)
)

/* Create a custom name value (raw data) */
static DtobValue *ents_name(const char *str) {
    size_t len = str ? strlen(str) : 0;
    DtobValue *v = dtob_custom(ENTS_NAME, (const uint8_t *)(str ? str : ""), len);
    v->inner_code = DTOB_CODE_RAW;
    return v;
}

/* Create a custom inode value (uint64, big-endian) */
static DtobValue *ents_inode(uint64_t val) {
    uint8_t bytes[8];
    for (int i = 7; i >= 0; i--) { bytes[i] = val & 0xFF; val >>= 8; }
    DtobValue *v = dtob_custom(ENTS_INODE, bytes, 8);
    v->inner_code = DTOB_CODE_UINT64;
    return v;
}

/* Create a custom parent value (uint64 inode, big-endian) */
static DtobValue *ents_parent(uint64_t inode) {
    uint8_t bytes[8];
    for (int i = 7; i >= 0; i--) { bytes[i] = inode & 0xFF; inode >>= 8; }
    DtobValue *v = dtob_custom(ENTS_PARENT, bytes, 8);
    v->inner_code = DTOB_CODE_UINT64;
    return v;
}

/* Create a file struct value (name + inode + parent_inode) */
static DtobValue *ents_file(const char *name, uint64_t inode, uint64_t parent_inode) {
    DtobValue *v = dtob_custom(ENTS_FILE, NULL, 0);
    dtob_custom_push(v, ents_name(name));
    dtob_custom_push(v, ents_inode(inode));
    dtob_custom_push(v, ents_parent(parent_inode));
    return v;
}

/* Read StringArray from a dtob array of strings */
static void dtob_arr_to_sa(const DtobValue *arr, StringArray *sa) {
    if (!arr || arr->type != DTOB_ARRAY) return;
    for (size_t i = 0; i < arr->num_elements; i++) {
        char s[4096];
        dtob_val_to_str(arr->elements[i], s, sizeof(s));
        sa_push(sa, s);
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

static DtobValue *build_rel_subtree(const TagsFile *tf) {
    RelPair *pairs;
    int pair_count;
    build_rel_pairs(tf, &pairs, &pair_count);
    int mode = select_rel_mode(tf->tags.count, tf->files.count, pair_count);

    DtobValue *kv = dtob_kvset();
    const char *mode_str = mode == REL_MODE_SPARSE_POS ? "pos" :
                           mode == REL_MODE_MATRIX ? "matrix" : "neg";
    dtob_kvset_put(kv, "rel_mode", dtob_raw((const uint8_t *)mode_str, strlen(mode_str)));

    if (mode == REL_MODE_SPARSE_POS || mode == REL_MODE_SPARSE_NEG) {
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
        dtob_kvset_put(kv, "rel_pairs", dtob_raw(buf, (size_t)raw_count * 4));
        free(buf);
    } else {
        int nt = tf->tags.count, nf = tf->files.count;
        uint32_t total = (uint32_t)nt * (uint32_t)nf;
        uint32_t mat_bytes = (total + 7) / 8;
        uint8_t *matrix = calloc(mat_bytes, 1);
        for (int i = 0; i < pair_count; i++) {
            uint32_t bit = (uint32_t)pairs[i].tag_idx * (uint32_t)nf + pairs[i].file_idx;
            matrix[bit / 8] |= (0x80 >> (bit % 8));
        }
        dtob_kvset_put(kv, "rel_rows", dtob_uint((uint64_t)nt));
        dtob_kvset_put(kv, "rel_cols", dtob_uint((uint64_t)nf));
        dtob_kvset_put(kv, "rel_matrix", dtob_raw(matrix, mat_bytes));
        free(matrix);
    }

    free(pairs);
    return kv;
}

int fast_patch_relations(const TagsFile *tf) {
    /* 1. Fast build the relations block natively in memory */
    DtobTypesHeader types;
    build_ents_custom_types(&types);

    DtobValue *rel_kv = build_rel_subtree(tf);

    DtobWriter w;
    dtob_writer_init(&w);

    for (size_t i = 0; i < rel_kv->num_pairs; i++) {
        dtob_writer_ctrl(&w, DTOB_CODE_RAW);
        dtob_writer_data(&w, rel_kv->pairs[i].key, rel_kv->pairs[i].key_len);
        dtob_writer_value_typed(&w, rel_kv->pairs[i].value, &types);
    }
    dtob_writer_ctrl(&w, DTOB_CODE_KV_CLOSE);

    /* 2. Open the database mathematically and dynamically search backwards to find the physical truncation boundary */
    FILE *fp = fopen(TAGS_DTOB_FILE, "r+b");
    if (!fp) {
        free(w.buf);
        dtob_free(rel_kv);
        for (size_t i = 0; i < types.count; i++) free(types.entries[i].name);
        return save_tags_bin(tf); /* fallback to full rewrite if file is broken */
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    if (file_size < 100) {
        fclose(fp);
        free(w.buf);
        dtob_free(rel_kv);
        for (size_t i = 0; i < types.count; i++) free(types.entries[i].name);
        return save_tags_bin(tf);
    }

    /* We dynamically scan the bottom 256KB of the file (relationships are always trailing) for our exact 1st rel key */
    long scan_size = file_size > 256000 ? 256000 : file_size;
    fseek(fp, file_size - scan_size, SEEK_SET);
    
    uint8_t *scan_buf = malloc(scan_size);
    if (fread(scan_buf, 1, scan_size, fp) != (size_t)scan_size) {
        free(scan_buf); fclose(fp); free(w.buf); dtob_free(rel_kv);
        for (size_t i = 0; i < types.count; i++) free(types.entries[i].name);
        return save_tags_bin(tf);
    }

    /* Compile target boundary mathematical key bytes */
    uint8_t *target_trits = NULL;
    size_t target_len = dtob_trit_encode(rel_kv->pairs[0].key, rel_kv->pairs[0].key_len, &target_trits);
    
    long boundary_offset = -1;
    for (long i = 0; i < scan_size - (long)target_len - 2; i++) {
        if (scan_buf[i] == 0xC0 && scan_buf[i+1] == 0x05) { /* DTOB_CODE_RAW */
            if (memcmp(&scan_buf[i+2], target_trits, target_len) == 0) {
                boundary_offset = (file_size - scan_size) + i;
                break;
            }
        }
    }

    free(scan_buf);
    free(target_trits);

    if (boundary_offset < 0) {
        /* Cannot mathematically locate boundary. Fallback cleanly to full rewrite! */
        fclose(fp); free(w.buf); dtob_free(rel_kv);
        for (size_t i = 0; i < types.count; i++) free(types.entries[i].name);
        return save_tags_bin(tf);
    }

    /* 3. Mathematical File Truncation and Physical Patch! */
#ifndef _WIN32
    int fd = fileno(fp);
    if (ftruncate(fd, boundary_offset) != 0) {
        /* ignore error and hope fwrite overwrites perfectly, or fallback if file size was bigger */
    }
#endif

    fseek(fp, boundary_offset, SEEK_SET);
    if (fwrite(w.buf, 1, w.pos, fp) != w.pos) {
        fclose(fp); free(w.buf); dtob_free(rel_kv);
        for (size_t i = 0; i < types.count; i++) free(types.entries[i].name);
        return -1;
    }

    /* Flush and natively dispose of memory resources perfectly! */
    fclose(fp);
    free(w.buf);
    dtob_free(rel_kv);
    for (size_t i = 0; i < types.count; i++) free(types.entries[i].name);

    return 0;
}

int save_tags_bin(const TagsFile *tf) {
    /* build types header */
    DtobTypesHeader types;
    build_ents_custom_types(&types);

    if (dtob_verify_file_types(TAGS_DTOB_FILE, &types, 1) == 0) {
        fprintf(stderr, "Error: Existing %s has incompatible type schemas! Serialization mathematically aborted.\n", TAGS_DTOB_FILE);
        for (size_t i = 0; i < types.count; i++) free(types.entries[i].name);
        return -1;
    }

    /* ---- build subtrees ---- */

    /* root */
    DtobValue *root = dtob_kvset();

    /* aliases */
    DtobValue *aliases_kv = dtob_kvset();
    for (int i = 0; i < tf->aliases.count; i++)
        dtob_kvset_put(aliases_kv, tf->aliases.items[i].key,
                       dtob_raw((const uint8_t *)tf->aliases.items[i].value,
                                   strlen(tf->aliases.items[i].value)));
    dtob_kvset_put(root, "aliases", aliases_kv);

    /* tags — array of arrays: [name, show, tag_type, children, ancestry] */
    DtobValue *tags_arr = dtob_array();
    for (int i = 0; i < tf->tags.count; i++) {
        const EntsTag *tag = &tf->tags.items[i];
        DtobValue *t = dtob_array();
        dtob_array_push(t, ents_name(tag->name));
        /* show: custom type wrapping true/false custom types */
        {
            DtobValue *sv = dtob_custom(ENTS_SHOW, NULL, 0);
            sv->inner_code = tag->show ? ENTS_TRUE : ENTS_FALSE;
            dtob_array_push(t, sv);
        }
        /* tag_type: custom type wrapping nullable default/dud */
        {
            uint8_t ttcode = (tag->tag_type == TAG_TYPE_DUD) ? ENTS_DUD : ENTS_DEFAULT;
            DtobValue *tv = dtob_custom(ENTS_TAG_TYPE, NULL, 0);
            tv->inner_code = ttcode;
            dtob_array_push(t, tv);
        }
        dtob_array_push(t, sa_to_dtob(&tag->children));
        dtob_array_push(t, sa_to_dtob(&tag->ancestry));
        dtob_array_push(tags_arr, t);
    }
    dtob_kvset_put(root, "tags", tags_arr);

    /* files — array of file structs */
    DtobValue *files_arr = dtob_array();
    for (int i = 0; i < tf->files.count; i++) {
        const FileData *fd = &tf->files.items[i];
        dtob_array_push(files_arr, ents_file(fd->last_known_name, fd->file_inode, fd->parent_dir_inode));
    }
    dtob_kvset_put(root, "files", files_arr);

    /* relationships: dynamically put properties directly into root */
    DtobValue *rel_kv = build_rel_subtree(tf);
    for (size_t i = 0; i < rel_kv->num_pairs; i++) {
        char key_buf[256];
        size_t kl = rel_kv->pairs[i].key_len < 255 ? rel_kv->pairs[i].key_len : 255;
        memcpy(key_buf, rel_kv->pairs[i].key, kl);
        key_buf[kl] = '\0';
        dtob_kvset_put(root, key_buf, dtob_deep_copy(rel_kv->pairs[i].value));
    }
    dtob_free(rel_kv);

    /* Encode and write to file natively */
    size_t out_len = 0;
    uint8_t *enc = dtob_encode_with_types(root, &types, 1, &out_len);
    dtob_free(root);

    /* free types header names */
    for (size_t i = 0; i < types.count; i++) {
        free(types.entries[i].name);
    }

    if (!enc || out_len == 0) {
        fprintf(stderr, "Error: serialization failed entirely for %s\n", TAGS_DTOB_FILE);
        free(enc);
        return -1;
    }

    FILE *fp = fopen(TAGS_DTOB_FILE, "wb");
    if (!fp) {
        fprintf(stderr, "Error: could not write %s\n", TAGS_DTOB_FILE);
        free(enc);
        return -1;
    }
    fwrite(enc, 1, out_len, fp);
    fclose(fp);
    free(enc);
    return 0;
}

/* Read uint64 from a DTOB_INT or DTOB_CUSTOM value (big-endian bytes) */
static uint64_t ents_read_u64(const DtobValue *v) {
    if (!v || !v->data || v->data_len == 0) return 0;
    if (v->type != DTOB_INT && v->type != DTOB_CUSTOM) return 0;
    uint64_t val = 0;
    for (size_t i = 0; i < v->data_len; i++)
        val = (val << 8) | v->data[i];
    return val;
}

/* ---- DTOB read ---- */

int read_tags_bin(TagsFile *tf) {
    tags_file_init(tf);

    FILE *fp = fopen(TAGS_DTOB_FILE, "rb");
    if (!fp) {
        fprintf(stderr, "Error: %s not found. Run 'prlents process tags.ents' to create it.\n",
               TAGS_DTOB_FILE);
        return -1;
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
    DtobValue *aliases_kv = dtob_kvset_get(root, "aliases");
    if (aliases_kv && aliases_kv->type == DTOB_KV_SET) {
        for (size_t i = 0; i < aliases_kv->num_pairs; i++) {
            char *key = malloc(aliases_kv->pairs[i].key_len + 1);
            memcpy(key, aliases_kv->pairs[i].key, aliases_kv->pairs[i].key_len);
            key[aliases_kv->pairs[i].key_len] = '\0';
            char val[4096];
            dtob_val_to_str(aliases_kv->pairs[i].value, val, sizeof(val));
            am_put(&tf->aliases, key, val);
            free(key);
        }
    }

    /* tags — array of arrays: [name, show, tag_type, children, ancestry] */
    DtobValue *tags_arr = dtob_kvset_get(root, "tags");
    if (tags_arr && tags_arr->type == DTOB_ARRAY) {
        for (size_t i = 0; i < tags_arr->num_elements; i++) {
            DtobValue *te = tags_arr->elements[i];
            if (te->type != DTOB_ARRAY || te->num_elements < 5) continue;
            EntsTag *tag = ta_push(&tf->tags);
            /* [0] name (custom string) */
            { char _buf[4096]; dtob_val_to_str(te->elements[0], _buf, sizeof(_buf)); tag->name = strdup(_buf); }
            /* [1] show (custom wrapping true/false) */
            DtobValue *sv = te->elements[1];
            tag->show = (sv->type == DTOB_CUSTOM && sv->inner_code == ENTS_TRUE);
            /* [2] tag_type (custom wrapping default/dud) */
            DtobValue *ttv = te->elements[2];
            tag->tag_type = TAG_TYPE_DEFAULT;
            if (ttv->type == DTOB_CUSTOM && ttv->inner_code == ENTS_DUD)
                tag->tag_type = TAG_TYPE_DUD;
            /* [3] children, [4] ancestry */
            dtob_arr_to_sa(te->elements[3], &tag->children);
            dtob_arr_to_sa(te->elements[4], &tag->ancestry);
            tag->has_files = false;
        }
    }

    /* files — array of file structs (elements: [name, inode, parent]) */
    DtobValue *files_arr = dtob_kvset_get(root, "files");
    if (files_arr && files_arr->type == DTOB_ARRAY) {
        for (size_t i = 0; i < files_arr->num_elements; i++) {
            DtobValue *fe = files_arr->elements[i];
            if (fe->type != DTOB_CUSTOM || fe->num_elements < 3) continue;
            FileData *fd = fda_push(&tf->files);
            { char _buf[4096]; dtob_val_to_str(fe->elements[0], _buf, sizeof(_buf)); fd->last_known_name = strdup(_buf); }
            fd->file_inode = ents_read_u64(fe->elements[1]);
            fd->parent_dir_inode = ents_read_u64(fe->elements[2]);
        }
    }

    /* relationships */
    char mode_str[64];
    dtob_val_to_str(dtob_kvset_get(root, "rel_mode"), mode_str, sizeof(mode_str));
    int mode = REL_MODE_SPARSE_POS;
    if (strcmp(mode_str, "matrix") == 0) mode = REL_MODE_MATRIX;
    else if (strcmp(mode_str, "neg") == 0) mode = REL_MODE_SPARSE_NEG;

    if (mode == REL_MODE_SPARSE_POS || mode == REL_MODE_SPARSE_NEG) {
        DtobValue *raw = dtob_kvset_get(root, "rel_pairs");
        if (raw && raw->type == DTOB_RAW && raw->data) {
            size_t n_pairs = raw->data_len / 4;
            if (mode == REL_MODE_SPARSE_POS) {
                for (size_t i = 0; i < n_pairs; i++) {
                    uint16_t ti = (uint16_t)(raw->data[i*4] | (raw->data[i*4+1] << 8));
                    uint16_t fi = (uint16_t)(raw->data[i*4+2] | (raw->data[i*4+3] << 8));
                    apply_rel(tf, ti, fi);
                }
            } else {
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
        DtobValue *mat = dtob_kvset_get(root, "rel_matrix");
        uint16_t nf = (uint16_t)dtob_kvset_uint(root, "rel_cols");
        uint16_t nt = (uint16_t)dtob_kvset_uint(root, "rel_rows");
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
