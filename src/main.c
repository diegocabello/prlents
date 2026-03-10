#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "common.h"
#include "parser.h"
#include "relationship.h"
#include "eval_shell.h"
#include "merge_tags.h"
#include "migrate.h"

int main(int argc, char **argv) {
    /* check for --eval-shell before anything else */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--eval-shell") == 0) {
            print_shell_functions();
            return 0;
        }
    }

    if (argc < 2) {
        printf("Usage: prlents <ttf|ftt|fil|int|insp|process>\n");
        return 0;
    }

    /* parse flags */
    bool flag_explicit = false;
    bool flag_force = false;
    bool flag_quiet = false;

    /* collect positional args (skip flags) */
    const char *positional[256];
    int pos_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--explicit") == 0) {
            flag_explicit = true;
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force") == 0) {
            flag_force = true;
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            flag_quiet = true;
        } else {
            if (pos_count < 256) positional[pos_count++] = argv[i];
        }
    }

    if (pos_count < 1) {
        printf("Usage: prlents <ttf|ftt|fil|int|insp|process>\n");
        return 0;
    }

    const char *command = positional[0];
    const char **args = &positional[1];
    int arg_count = pos_count - 1;

    /* ---- process ---- */
    if (strcmp(command, "process") == 0 || strcmp(command, "parse") == 0) {
        const char *file_path = arg_count > 0 ? args[0] : "tags.ents";

        TagsFile tf;
        if (parse_ents(file_path, &tf) != 0) return 1;

        /* check if tags.dtob exists */
        FILE *check = fopen("tags.dtob", "r");
        if (!check) {
            /* no existing file, save directly */
            if (save_tags_bin(&tf) != 0) {
                tags_file_free(&tf);
                return 1;
            }
        } else {
            fclose(check);
            if (merge_tags(&tf) != 0) {
                tags_file_free(&tf);
                return 1;
            }
        }

        printf("Successfully parsed %s and saved to tags.dtob\n", file_path);
        tags_file_free(&tf);
        return 0;
    }

    /* ---- migrate ---- */
    if (strcmp(command, "migrate") == 0) {
        const char *json_path = arg_count > 0 ? args[0] : "tags.json";

        TagsFile tf;
        if (read_tags_from_json(json_path, &tf) != 0) return 1;

        printf("Read %d tags, %d files, %d aliases from %s\n",
               tf.tags.count, tf.files.count, tf.aliases.count, json_path);

        if (save_tags_bin(&tf) != 0) {
            tags_file_free(&tf);
            return 1;
        }

        printf("Migrated to tags.dtob\n");
        tags_file_free(&tf);
        return 0;
    }

    /* ---- commands that need tags.dtob ---- */
    TagsFile tf;
    if (read_tags_bin(&tf) != 0) return 1;

    /* ---- filter / union ---- */
    if (strcmp(command, "filter") == 0 || strcmp(command, "fil") == 0 ||
        strcmp(command, "union") == 0 || strcmp(command, "un") == 0) {

        StringArray result;
        if (filter_command(&tf, args, arg_count, flag_explicit, &result) == 0) {
            for (int i = 0; i < result.count; i++)
                printf("%s\n", result.items[i]);
            sa_free(&result);
        }

    /* ---- intersection ---- */
    } else if (strcmp(command, "intersection") == 0 || strcmp(command, "intersect") == 0 ||
               strcmp(command, "int") == 0) {

        if (arg_count < 1) {
            fprintf(stderr, "need at least one tag for intersection\n");
            tags_file_free(&tf);
            return 1;
        }

        if (arg_count == 1) {
            StringArray result;
            if (filter_command(&tf, args, 1, flag_explicit, &result) == 0) {
                for (int i = 0; i < result.count; i++)
                    printf("%s\n", result.items[i]);
                sa_free(&result);
            }
        } else {
            /* intersect: get filter for first tag, then intersect with each subsequent */
            StringArray first;
            if (filter_command(&tf, &args[0], 1, flag_explicit, &first) != 0) {
                tags_file_free(&tf);
                return 1;
            }

            for (int t = 1; t < arg_count; t++) {
                StringArray other;
                if (filter_command(&tf, &args[t], 1, flag_explicit, &other) != 0) {
                    sa_free(&first);
                    tags_file_free(&tf);
                    return 1;
                }

                /* keep only items in both */
                StringArray intersected;
                sa_init(&intersected);
                for (int i = 0; i < first.count; i++) {
                    if (sa_contains(&other, first.items[i]))
                        sa_push(&intersected, first.items[i]);
                }

                sa_free(&first);
                sa_free(&other);
                first = intersected;
            }

            for (int i = 0; i < first.count; i++)
                printf("%s\n", first.items[i]);
            sa_free(&first);
        }

    /* ---- inspect ---- */
    } else if (strcmp(command, "inspect") == 0 || strcmp(command, "insp") == 0) {
        represent_inspect(&tf, args, arg_count, flag_quiet);

    /* ---- ttf / ftt ---- */
    } else if (strcmp(command, "tagtofiles") == 0 || strcmp(command, "ttf") == 0 ||
               strcmp(command, "filetotags") == 0 || strcmp(command, "ftt") == 0) {

        if (arg_count < 2) {
            printf("not enough options\n");
            tags_file_free(&tf);
            return 0;
        }

        Operation op = operation_from_str(args[0]);
        if (op == OP_UNKNOWN) {
            printf("invalid operation: %s\n", args[0]);
            tags_file_free(&tf);
            return 0;
        }

        const char *monad = args[1];
        const char **extra = &args[2];
        int extra_count = arg_count - 2;

        if (strcmp(command, "tagtofiles") == 0 || strcmp(command, "ttf") == 0) {
            /* resolve tag */
            const char *display_tag = am_get(&tf.aliases, monad);
            if (!display_tag) display_tag = monad;

            int tidx = -1;
            for (int i = 0; i < tf.tags.count; i++) {
                if (tf.tags.items[i].name && strcmp(tf.tags.items[i].name, display_tag) == 0
                    && is_visible_tag(&tf.tags.items[i])) {
                    tidx = i;
                    break;
                }
            }

            if (tidx < 0) {
                printf("tag does not exist: %s\n", monad);
            } else if (tf.tags.items[tidx].tag_type == TAG_TYPE_DUD) {
                printf("cannot assign dud tag to files: \t%s\n", monad);
            } else {
                for (int i = 0; i < extra_count; i++)
                    assign_bidir_file_tag_rel(extra[i], monad, op, &tf, flag_force);
                save_tags_bin(&tf);
            }

        } else {
            /* ftt */
            struct stat st;
            if (stat(monad, &st) != 0) {
                printf("file does not exist: %s\n", monad);
                tags_file_free(&tf);
                return 0;
            }

            if (flag_force && extra_count > 1) {
                printf("logic for multiple force tags hasn't been implemented yet\n");
                tags_file_free(&tf);
                return 0;
            }

            for (int i = 0; i < extra_count; i++)
                assign_bidir_file_tag_rel(monad, extra[i], op, &tf, flag_force);
            save_tags_bin(&tf);
        }

    } else {
        printf("invalid command: %s\n", command);
    }

    tags_file_free(&tf);
    return 0;
}
