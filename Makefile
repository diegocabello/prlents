CC = cc
CFLAGS = -Wall -Wextra -O2 -std=c11
SRCDIR = src
LIBDIR = lib
OBJDIR = build

DTOB_DIR = ../dtob-doc
DTOB_LIB = $(DTOB_DIR)/libdtob.a
DTOB_INC = $(DTOB_DIR)/lib

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
TARGET = prlents

# Objects for libprlents.a (no main)
LIB_SRCS = $(SRCDIR)/common.c \
           $(SRCDIR)/handle_file.c \
           $(SRCDIR)/tags_api.c
LIB_PRL_OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(LIB_SRCS))

all: $(TARGET) libprlents.a

$(TARGET): $(OBJS) $(LIB_OBJS) $(DTOB_LIB)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIB_OBJS) $(DTOB_LIB) -lm

libprlents.a: $(LIB_PRL_OBJS)
	ar rcs $@ $(LIB_PRL_OBJS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -I$(LIBDIR)/cjson -I$(DTOB_INC) -c $< -o $@

$(OBJDIR)/cjson/cJSON.o: $(LIBDIR)/cjson/cJSON.c | $(OBJDIR)/cjson
	$(CC) $(CFLAGS) -c $< -o $@

$(DTOB_LIB):
	$(MAKE) -C $(DTOB_DIR) libdtob.a

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/cjson:
	mkdir -p $(OBJDIR)/cjson

clean:
	rm -rf $(OBJDIR) $(TARGET) libprlents.a

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

.PHONY: all clean install
