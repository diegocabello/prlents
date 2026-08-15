#include "merge_tags.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int merge_tags(const TagsFile *new_tf) {
    /* Load existing binary */
    TagsFile existing;
    int rc = read_tags_bin(&existing);
    if (rc != 0) {
        tags_file_free(&existing);
        return -1;
    }

    /* If no existing tags (file didn't exist), just save new_tf directly */
    if (existing.tags.count == 0 && existing.files.count == 0) {
        tags_file_free(&existing);
        return save_tags_bin(new_tf);
    }

    /* Build merged result */
    TagsFile merged;
    tags_file_init(&merged);

    /* files: keep existing */
    for (int i = 0; i < existing.files.count; i++) {
        FileData *fd = fda_push(&merged.files);
        fd->last_known_name = existing.files.items[i].last_known_name
            ? strdup(existing.files.items[i].last_known_name) : NULL;
        fd->file_inode = existing.files.items[i].file_inode;
        fd->parent_dir_inode = existing.files.items[i].parent_dir_inode;
    }

    /* aliases: merge both, new takes priority */
    for (int i = 0; i < existing.aliases.count; i++)
        am_put(&merged.aliases, existing.aliases.items[i].key, existing.aliases.items[i].value);
    for (int i = 0; i < new_tf->aliases.count; i++)
        am_put(&merged.aliases, new_tf->aliases.items[i].key, new_tf->aliases.items[i].value);

    /* tags: process new tags first */
    for (int i = 0; i < new_tf->tags.count; i++) {
        const EntsTag *nt = &new_tf->tags.items[i];
        EntsTag *mt = ta_push(&merged.tags);
        mt->name = strdup(nt->name);
        mt->tag_type = nt->tag_type;
        mt->show = true;
        mt->alias = nt->alias ? strdup(nt->alias) : NULL;

        /* copy children and ancestry from new */
        for (int c = 0; c < nt->children.count; c++)
            sa_push(&mt->children, nt->children.items[c]);
        for (int a = 0; a < nt->ancestry.count; a++)
            sa_push(&mt->ancestry, nt->ancestry.items[a]);

        /* look for this tag in existing to keep file associations */
        for (int e = 0; e < existing.tags.count; e++) {
            if (existing.tags.items[e].name &&
                ents_name_equal(existing.tags.items[e].name, nt->name)) {
                if (existing.tags.items[e].has_files) {
                    mt->has_files = true;
                    for (int f = 0; f < existing.tags.items[e].files.count; f++)
                        sa_push(&mt->files, existing.tags.items[e].files.items[f]);
                }
                break;
            }
        }
    }

    /* existing tags not in new: mark as hidden */
    for (int e = 0; e < existing.tags.count; e++) {
        const EntsTag *et = &existing.tags.items[e];
        if (!et->name) continue;

        bool found = false;
        for (int n = 0; n < new_tf->tags.count; n++) {
            if (new_tf->tags.items[n].name &&
                ents_name_equal(new_tf->tags.items[n].name, et->name)) {
                found = true;
                break;
            }
        }

        if (!found) {
            EntsTag *mt = ta_push(&merged.tags);
            mt->name = strdup(et->name);
            mt->tag_type = et->tag_type;
            mt->show = false;
            mt->alias = et->alias ? strdup(et->alias) : NULL;
            for (int c = 0; c < et->children.count; c++)
                sa_push(&mt->children, et->children.items[c]);
            for (int a = 0; a < et->ancestry.count; a++)
                sa_push(&mt->ancestry, et->ancestry.items[a]);
            if (et->has_files) {
                mt->has_files = true;
                for (int f = 0; f < et->files.count; f++)
                    sa_push(&mt->files, et->files.items[f]);
            }
        }
    }

    int result = save_tags_bin(&merged);
    tags_file_free(&existing);
    tags_file_free(&merged);
    return result;
}
