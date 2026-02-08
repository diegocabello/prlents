#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    int indent;        /* indentation level (0, 1, 2, ...) */
    TagType tag_type;
    char *name;
    char *alias;       /* NULL if none */
} ParsedTag;

static void parsed_tag_free(ParsedTag *pt) {
    free(pt->name);
    free(pt->alias);
}

/* Parse a single line of an ENTS file. Returns 0 on success, -1 on skip/empty, -2 on error. */
static int parse_line(const char *line, ParsedTag *out) {
    const char *p = line;

    /* count leading spaces */
    int spaces = 0;
    while (*p == ' ') { spaces++; p++; }

    /* skip empty / whitespace-only lines */
    if (*p == '\0' || *p == '\n' || *p == '\r') return -1;

    if (spaces % 4 != 0) {
        fprintf(stderr, "Invalid indent: %d spaces\n", spaces);
        return -2;
    }
    out->indent = spaces / 4;

    /* tag type */
    if (*p == '-') {
        out->tag_type = TAG_TYPE_DEFAULT;
    } else if (*p == '+') {
        out->tag_type = TAG_TYPE_DUD;
    } else {
        return -2;
    }
    p++;

    /* require at least one space after tag type */
    if (*p != ' ') return -2;
    while (*p == ' ') p++;

    /* parse tag name - until '(', ':', '\n', '\r', or end */
    char name_buf[1024];
    int ni = 0;
    while (*p && *p != '(' && *p != ':' && *p != '\n' && *p != '\r') {
        /* escaped characters */
        if (*p == '\\' && (p[1] == '(' || p[1] == ')' || p[1] == ':')) {
            if (ni < (int)sizeof(name_buf) - 1) name_buf[ni++] = p[1];
            p += 2;
            continue;
        }
        if (ni < (int)sizeof(name_buf) - 1) name_buf[ni++] = *p;
        p++;
    }
    name_buf[ni] = '\0';

    /* trim trailing whitespace from name */
    while (ni > 0 && (name_buf[ni-1] == ' ' || name_buf[ni-1] == '\t')) {
        name_buf[--ni] = '\0';
    }

    if (ni == 0) return -2;
    out->name = strdup(name_buf);

    /* skip spaces */
    while (*p == ' ') p++;

    /* optional alias in () */
    out->alias = NULL;
    if (*p == '(') {
        p++;
        char alias_buf[256];
        int ai = 0;
        while (*p && *p != ')') {
            if (ai < (int)sizeof(alias_buf) - 1) alias_buf[ai++] = *p;
            p++;
        }
        alias_buf[ai] = '\0';
        if (*p == ')') p++;

        /* trim alias */
        char *as = alias_buf;
        while (*as == ' ') as++;
        int alen = strlen(as);
        while (alen > 0 && as[alen-1] == ' ') as[--alen] = '\0';
        if (alen > 0) out->alias = strdup(as);
    }

    return 0;
}

static void build_hierarchy(ParsedTag *parsed, int count, TagsFile *tf) {
    /* stack of indices into tf->tags for tracking parent chain */
    int stack[256];
    int stack_depth = 0;

    for (int i = 0; i < count; i++) {
        ParsedTag *pt = &parsed[i];

        /* register alias */
        if (pt->alias) {
            am_put(&tf->aliases, pt->alias, pt->name);
        }

        /* truncate stack to current indent level */
        if (pt->indent < stack_depth) stack_depth = pt->indent;

        /* build ancestry */
        StringArray ancestry;
        sa_init(&ancestry);
        for (int s = 0; s < stack_depth; s++) {
            sa_push(&ancestry, tf->tags.items[stack[s]].name);
        }

        /* create tag */
        EntsTag *tag = ta_push(&tf->tags);
        tag->name = strdup(pt->name);
        tag->tag_type = pt->tag_type;
        tag->show = true;
        tag->has_files = false;
        tag->alias = pt->alias ? strdup(pt->alias) : NULL;
        tag->ancestry = ancestry;

        /* add as child of parent */
        if (stack_depth > 0) {
            sa_push(&tf->tags.items[stack[stack_depth - 1]].children, pt->name);
        }

        /* push onto stack */
        int tag_index = tf->tags.count - 1;
        if (stack_depth < 256) {
            stack[stack_depth] = tag_index;
            stack_depth = pt->indent + 1;
        }
    }
}

int parse_ents(const char *file_path, TagsFile *tf) {
    tags_file_init(tf);

    FILE *fp = fopen(file_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: could not open %s\n", file_path);
        return -1;
    }

    /* read entire file */
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *content = malloc(fsize + 1);
    fread(content, 1, fsize, fp);
    content[fsize] = '\0';
    fclose(fp);

    /* parse line by line */
    ParsedTag *parsed = NULL;
    int parsed_count = 0;
    int parsed_cap = 0;
    int line_num = 0;

    char *line = strtok(content, "\n");
    while (line) {
        line_num++;
        ParsedTag pt;
        memset(&pt, 0, sizeof(pt));
        int rc = parse_line(line, &pt);

        if (rc == 0) {
            if (parsed_count >= parsed_cap) {
                parsed_cap = parsed_cap ? parsed_cap * 2 : 16;
                parsed = realloc(parsed, parsed_cap * sizeof(ParsedTag));
            }
            parsed[parsed_count++] = pt;
        } else if (rc == -2) {
            fprintf(stderr, "Failed to parse at line %d: %s\n", line_num, line);
            /* clean up */
            for (int i = 0; i < parsed_count; i++) parsed_tag_free(&parsed[i]);
            free(parsed);
            free(content);
            return -1;
        }
        /* rc == -1: empty line, skip */

        line = strtok(NULL, "\n");
    }

    printf("Parsed %d tags\n", parsed_count);

    build_hierarchy(parsed, parsed_count, tf);

    /* clean up parsed tags (strings were strdup'd into tf) */
    for (int i = 0; i < parsed_count; i++) parsed_tag_free(&parsed[i]);
    free(parsed);
    free(content);

    return 0;
}
