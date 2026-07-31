# ents

`ents` is a local, hierarchical tagging system for files. You describe the tag
tree in a small indentation-based `.ents` file, compile it into a `tags.dtob`
database, and then assign tags to files, query unions or intersections, and
inspect a file's tags from the command line.

Files are identified by inode rather than only by path. If a known file moves,
`ents` can search below the current directory for the inode and update its last
known path without losing its tag relationships.

This repository is the C implementation of the older `prlents` prototype. The
current Makefile builds an executable named `ents`; a few messages and the
generated shell helpers still use the legacy command name `prlents`.

## Build

Requirements:

- a C11 compiler and `make`
- `pkg-config`
- the [DTOB](https://gitlab.com/diegocabello/dtob) library

DTOB must be installed with a `dtob.pc` pkg-config file. The Makefile checks
`~/.local/lib/pkgconfig` by default:

```sh
make
```

To use another DTOB installation:

```sh
PKG_CONFIG_PATH=/path/to/lib/pkgconfig make
```

The build produces:

- `ents`, the CLI;
- `libprlents.a`, a small embedding library containing the database, file
  tracking, and tag API code.

`make clean` removes the build directory, executable, and static library.
`make install` copies `ents` into `/usr/local/bin`.

## Quick start

Create `tags.ents`:

```text
+ work
    - active (a)
    - archived
+ media
    - books
    - music
```

Compile it into the database in the current directory:

```sh
./ents process tags.ents
```

Assign a file to a tag, inspect it, and query the tag:

```sh
./ents ftt add ./notes.txt active
./ents inspect ./notes.txt
./ents filter active
```

All normal commands read `./tags.dtob`. Run `ents` from the project or
collection directory whose database you want to use. File recovery searches
recursively from that same working directory.

## The `.ents` format

Each non-empty line defines one tag:

```text
<type> <name> [(alias)]
```

- `-` creates a default, assignable tag.
- `+` creates a dud tag: an organizational container that cannot be assigned
  directly to a file.
- Four spaces represent one nesting level. Tabs and partial indentation are not
  accepted.
- An optional name in parentheses is an alias.
- `\(`, `\)`, and `\:` escape those characters in a tag name.

For example:

```text
+ projects
    + client work (clients)
        - active
        - archived
    - personal (p)
```

The parser derives each tag's children and full ancestry from indentation.
Aliases such as `clients` and `p` can be used anywhere a tag name is accepted.

Default tags are exclusive along a single ancestor/descendant chain. A file
cannot simultaneously have both `projects/client work/active` and an assignable
ancestor or descendant of that tag. `--force` resolves such a conflict by
removing the conflicting ancestor or descendant assignment first. Sibling tags
can coexist.

## Commands

Flags may appear anywhere in the command line:

- `-e`, `--explicit`: query exactly the named tag instead of including its
  assignable descendants.
- `-f`, `--force`: replace conflicting ancestor/descendant assignments.
- `-q`, `--quiet`: suppress untagged files in `inspect` output.
- `--eval-shell`: print Bash or Zsh helper functions and exit.

### Build or update the database

```sh
./ents process [tags.ents]
./ents parse [tags.ents]
```

`parse` is an alias of `process`. The default input is `tags.ents`. On the first
run, the command creates `tags.dtob`. On later runs it merges the new hierarchy
with the existing database:

- existing file records and relationships are retained;
- the new hierarchy and tag types take precedence;
- aliases from the new file take precedence;
- tags removed from the `.ents` file are kept but marked hidden, preserving
  their stored relationships.

### Assign or remove relationships

There are two equivalent orientations.

Tag to files:

```sh
./ents ttf add <tag> <file>...
./ents ttf assign <tag> <file>...
./ents ttf remove <tag> <file>...
./ents ttf rm <tag> <file>...
```

File to tags:

```sh
./ents ftt add <file> <tag>...
./ents ftt remove <file> <tag>...
```

Long command names `tagtofiles` and `filetotags` are also accepted. Dud tags
cannot receive direct file assignments. Forced `ftt` assignment currently only
supports one tag at a time.

When a path is first seen, `ents` records its inode, parent-directory inode, and
relative path. If the path later stops existing, queries search the current
directory tree for the same inode and repair the stored path when found.

### Query files

Union (all matching files, sorted and deduplicated):

```sh
./ents filter <tag>...
./ents fil <tag>...
./ents union <tag>...
./ents un <tag>...
```

Without `--explicit`, each named tag expands recursively and contributes files
from all default tags below it. This makes a dud tag useful as a queryable
container even though files cannot be assigned directly to it.

Intersection:

```sh
./ents intersection <tag>...
./ents intersect <tag>...
./ents int <tag>...
```

Each tag is expanded using the same rules as `filter`, then only filenames
present in every result set are printed.

### Inspect files

```sh
./ents inspect <file>...
./ents insp <file>...
```

This prints each visible tag assigned to a file as its full ancestry path. With
multiple files, output is grouped under file headers. `--quiet` omits files that
have no tags.

## DTOB storage

`tags.dtob` is the authoritative database. JSON is supported only as a legacy
migration input; routine reads and writes use DTOB directly.

### Why DTOB is used

DTOB gives `ents` a compact binary representation with a self-describing custom
types header. `ents` registers semantic types for tag names, booleans, tag
kinds, inodes, parent inodes, and file records instead of treating the database
as an untyped byte blob. The current DTOB file magic is `01052026`, with the
types section stored before the root value.

The root DTOB value is a key/value set with this logical shape:

```text
aliases:  { alias: canonical_tag, ... }
tags:     [ [name, show, tag_type, children, ancestry], ... ]
files:    [ file(name, inode, parent_inode), ... ]
rel_mode: "pos" | "matrix" | "neg"
rel_pairs or rel_matrix: encoded tag/file relationships
```

The custom DTOB schema includes:

- `name`, a raw string-like value;
- `show`, an enum of custom `true` and `false` nullable members;
- `tag_type`, an enum of custom `default` and `dud` members;
- `inode` and `parent`, unsigned 64-bit values;
- `file`, a struct containing `name`, `inode`, and `parent`.

The DTOB header travels with the database, so decoding uses the schema embedded
in the file. `read_tags_bin` decodes the DTOB tree and reconstructs the C
`TagsFile`, including its aliases, hierarchy, file registry, and relationships.
`save_tags_bin` performs the reverse conversion and writes a complete new
database.

### Adaptive relationship encoding

Tag/file relationships usually dominate the changing part of the database.
Before writing, `ents` estimates three DTOB payload sizes and chooses the
smallest representation:

- `pos`: a sparse list of relationships that exist;
- `matrix`: a dense bit matrix with one bit per tag/file cell;
- `neg`: a sparse list of relationships that do not exist.

Sparse entries are four bytes each: a little-endian 16-bit tag index followed
by a little-endian 16-bit file index. The matrix uses rows for tags and columns
for files. The selected representation is recorded in `rel_mode`, and the DTOB
reader expands any of the three forms into the same in-memory relationships.

### Fast relationship-only writes

An assignment that changes only relationships does not need to re-encode the
aliases, hierarchy, or file metadata. `fast_patch_relations` uses DTOB's chunk
encoder to build a new relationship tail, locates the trit-encoded `rel_mode`
key within the final 256 KiB of `tags.dtob`, overwrites from that boundary, and
truncates the file at the new DTOB root close marker.

If the boundary cannot be found or encoded, it safely falls back to a complete
`save_tags_bin` rewrite. A newly discovered or relocated file marks metadata as
dirty and also triggers the full rewrite path.

## Migration

Migrate the legacy JSON database into the current DTOB schema:

```sh
./ents migrate [tags.json]
```

The default input is `tags.json`; output is always `./tags.dtob`.

Convert an older DTOB encoding in place:

```sh
cp tags.dtob tags.dtob.bak
./ents migrate2 [tags.dtob]
```

`migrate2` accepts the old `13032026` and `28042026` magics. It remaps the old
container open/close codes, decodes and re-encodes trit payloads with the new
word-aligned padding, moves the DTOB types section before the root container,
and writes the result with the `01052026` magic. It edits the named file in
place, so make a backup first. Running it on an already-current file is a no-op.

## Shell helpers

`--eval-shell` emits Bash or Zsh functions:

- `ct <tag>` stores the current tag in `~/.entsfs`;
- `fil [tag]` selects a tag and runs an intersection query, or queries the
  current tag when called with no argument;
- `tag <file>...` assigns files to the current tag;
- the prompt displays the selected tag.

The generated functions currently invoke `prlents`, the project's legacy
binary name. Make the current executable available under that name before
sourcing them:

```sh
ln -sf "$(pwd)/ents" "$HOME/.local/bin/prlents"
source <(prlents --eval-shell)
```

Add the `source` line to `.bashrc` or `.zshrc` if you want the helpers in every
shell.

## Embedding API

`libprlents.a` exposes the database and tag-selection API declared in
`src/common.h` and `src/tags_api.h`. It can:

- load and save `tags.dtob`;
- look up tags by name or alias;
- return direct, child, or immediate-parent tag files;
- update the selected files for a tag while leaving relationships outside the
  supplied file set unchanged.

Returned filename arrays from the getter functions are owned by the caller.
Consumers must also link DTOB and `libm` as reported by `pkg-config --libs
dtob`.

## Source layout

```text
src/main.c          argument parsing and command dispatch
src/parser.c        indentation-based .ents parser
src/common.c        core containers and DTOB serialization
src/relationship.c relationship rules, queries, and inspection
src/handle_file.c   inode-based file registration and relocation
src/merge_tags.c    hierarchy refresh while preserving assignments
src/migrate.c       JSON and old-DTOB migrations
src/eval_shell.c    Bash/Zsh helper generation
src/tags_api.c      embedding-facing tag/file API
lib/cjson/          vendored parser used for JSON migration
```
