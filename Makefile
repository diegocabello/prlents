CC = cc
PKG_CONFIG_PATH ?= $(HOME)/.local/lib/pkgconfig
DTOB_CFLAGS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags dtob)
DTOB_LIBS   = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs dtob)
CFLAGS = -Wall -Wextra -O2 -std=c11 $(DTOB_CFLAGS)
SRCDIR = src
LIBDIR = lib
OBJDIR = build

SRCS = $(SRCDIR)/main.c \
       $(SRCDIR)/common.c \
       $(SRCDIR)/parser.c \
       $(SRCDIR)/relationship.c \
       $(SRCDIR)/handle_file.c \
       $(SRCDIR)/merge_tags.c \
       $(SRCDIR)/eval_shell.c \
       $(SRCDIR)/migrate.c \
       $(SRCDIR)/tags_api.c

OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
LIB_OBJS = $(OBJDIR)/cjson/cJSON.o
TARGET = ents

# Objects for libprlents.a (no main)
LIB_SRCS = $(SRCDIR)/common.c \
           $(SRCDIR)/handle_file.c \
           $(SRCDIR)/tags_api.c
LIB_PRL_OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(LIB_SRCS))

all: $(TARGET) libprlents.a

$(TARGET): $(OBJS) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIB_OBJS) $(DTOB_LIBS)

libprlents.a: $(LIB_PRL_OBJS)
	ar rcs $@ $(LIB_PRL_OBJS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -I$(LIBDIR)/cjson -c $< -o $@

$(OBJDIR)/cjson/cJSON.o: $(LIBDIR)/cjson/cJSON.c | $(OBJDIR)/cjson
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/cjson:
	mkdir -p $(OBJDIR)/cjson

clean:
	rm -rf $(OBJDIR) $(TARGET) libprlents.a

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

.PHONY: all clean install
