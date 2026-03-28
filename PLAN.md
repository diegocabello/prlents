# PRLENTS C Rewrite Plan

## Overview
Rewrite prlents from Rust to C, in-place within the existing repo. Keep the same binary name (`prlents`), same CLI interface, same JSON format (`tags.json`). Use cJSON (vendored) for JSON handling.

## File Structure (after rewrite)
```
prlents/
├── CLAUDE.md
├── Makefile
├── README.md
├── documentation/          (unchanged)
├── src/
│   ├── main.c              — arg parsing, command dispatch
│   ├── common.h            — shared types (TagType, EntsTag, TagsFile, FileData)
│   ├── common.c            — read_tags_from_json, save_tags_to_json, path utilities
│   ├── parser.h            — parse_ents() declaration
│   ├── parser.c            — ENTS file parser (hand-rolled, no nom equivalent needed)
│   ├── relationship.h      — filter, inspect, assign declarations
│   ├── relationship.c      — filter_command, assign_bidir_file_tag_rel, inspect
│   ├── handle_file.h       — handle_file, find_filename_by_inode declarations
│   ├── handle_file.c       — inode lookup, directory walking (nftw/opendir+readdir)
│   ├── merge_tags.h        — merge_tags declaration
│   ├── merge_tags.c        — merge logic for re-processing .ents files
│   ├── eval_shell.h        — print_shell_functions declaration
│   ├── eval_shell.c        — bash/zsh shell function printer
│   ├── cJSON.h             — vendored cJSON header
│   └── cJSON.c             — vendored cJSON source
├── Cargo.toml              (keep for reference, won't be used)
└── target/                 (can ignore)
```

## Implementation Order

### 1. Setup — Makefile + cJSON vendor
- Create `Makefile` with `CC=cc`, `-Wall -Wextra`, link `-lm`
- Vendor cJSON.c and cJSON.h into `src/`

### 2. `common.h` / `common.c` — Core types and JSON I/O
Port from `common.rs`:
- `enum TagType { TAG_DUD, TAG_DEFAULT }`
- `struct EntsTag` with: name, tag_type, children (string array), ancestry (string array), show (bool), files (string array), alias
- `struct FileData` with: last_known_name, file_inode, parent_dir_inode
- `struct TagsFile` with: files array, aliases hashmap, tags array
- `read_tags_from_json()` — read `tags.json` via cJSON, populate TagsFile
- `save_tags_to_json()` — serialize TagsFile back to JSON via cJSON
- Simple dynamic array helpers (since C has no Vec)
- Simple string hashmap for aliases (can be a flat array of key-value pairs, the alias count is small)

### 3. `parser.c` — ENTS file parser
Port from `parser.rs`:
- Read file line by line
- For each line: count leading spaces (must be multiple of 4), parse tag type (`-` or `+`), parse tag name (up to `(` or newline), parse optional alias in parens
- Build hierarchy by tracking indent levels with a stack
- Produce a `TagsFile` with tags and aliases populated
- No need for nom — the grammar is simple enough for manual line-by-line parsing

### 4. `handle_file.c` — File identification and inode lookup
Port from `handle_file.rs`:
- `get_file_identifier()` — use `stat()` to get `st_ino` on macOS/Linux
- `handle_file()` — check if file exists in TagsFile by name or inode, register new files
- `find_filename_by_inode()` — recursive directory walk using `nftw()` or `opendir()`/`readdir()` to find file by inode
- `find_file_with_inodes()` — locate file by path or by walking directories

### 5. `relationship.c` — Core tagging and filtering logic
Port from `relationship.rs`:
- `enum Operation { OP_ADD, OP_REMOVE, OP_UNKNOWN }`
- `assign_bidir_file_tag_rel()` — the main assign/remove logic with exclusion rule enforcement
- `collect_tags_recursively()` — walk children to collect all descendant tags (uses string sets)
- `filter_command()` — collect files from matching tags, resolve inodes to filenames
- `single_inspect()` / `represent_inspect()` — show tags for files
- String set operations (intersection, contains) — simple array-based since tag counts are small

### 6. `merge_tags.c` — Tag merging on re-process
Port from `merge_tags.rs`:
- Read existing `tags.json`, compare with newly parsed tags
- Preserve file assignments, update tag structure, mark removed tags as `show: false`

### 7. `eval_shell.c` — Shell function printer
Port from `eval_shell.rs`:
- Detect shell from `$SHELL` env var
- Print bash or zsh helper functions

### 8. `main.c` — Entry point and command dispatch
Port from `main.rs`:
- Parse CLI args manually (check for `--eval-shell`, `-e`, `-f`, `-q`, then positional command + args)
- Dispatch to: process/parse, filter/fil/union, intersect/int, inspect/insp, ttf/tagtofiles, ftt/filetotags
- Same exact CLI interface as the Rust version

## Key Differences from Rust
- **Memory management**: manual malloc/free, careful cleanup on all exit paths
- **No nom**: the ENTS grammar is simple — line-by-line parsing with string ops
- **No serde**: cJSON handles JSON serialization/deserialization
- **No jwalk**: use POSIX `nftw()` for recursive directory walking (single-threaded, but fast enough)
- **No argh**: manual arg parsing (the arg structure is simple)
- **String sets**: small array-based sets with linear search (tag/file counts are small in practice)

## Build
```
make        # builds prlents
make clean  # removes build artifacts
```
