CC = cc
CFLAGS = -Wall -Wextra -O2 -std=c11
SRCDIR = src
LIBDIR = lib
OBJDIR = build

DTOB_DIR = ../dtob/impl/c
DTOB_LIB = $(DTOB_DIR)/libdtob.a
DTOB_INC = $(DTOB_DIR)/lib

SRCS = $(SRCDIR)/main.c \
       $(SRCDIR)/common.c \
       $(SRCDIR)/parser.c \
       $(SRCDIR)/relationship.c \
       $(SRCDIR)/handle_file.c \
       $(SRCDIR)/merge_tags.c \
       $(SRCDIR)/eval_shell.c \
       $(SRCDIR)/migrate.c

OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
LIB_OBJS = $(OBJDIR)/cjson/cJSON.o
TARGET = prlents

all: $(TARGET)

$(TARGET): $(OBJS) $(LIB_OBJS) $(DTOB_LIB)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIB_OBJS) $(DTOB_LIB) -lm

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
	rm -rf $(OBJDIR) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

.PHONY: all clean install
