CC = cc
CFLAGS = -Wall -Wextra -O2 -std=c11
SRCDIR = src
LIBDIR = lib
OBJDIR = build

SRCS = $(SRCDIR)/main.c \
       $(SRCDIR)/common.c \
       $(SRCDIR)/parser.c \
       $(SRCDIR)/relationship.c \
       $(SRCDIR)/handle_file.c \
       $(SRCDIR)/merge_tags.c \
       $(SRCDIR)/eval_shell.c

LIB_SRCS = $(LIBDIR)/cjson/cJSON.c

OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
LIB_OBJS = $(OBJDIR)/cjson/cJSON.o
TARGET = prlents

all: $(TARGET)

$(TARGET): $(OBJS) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -I$(LIBDIR)/cjson -c $< -o $@

$(OBJDIR)/cjson/cJSON.o: $(LIBDIR)/cjson/cJSON.c | $(OBJDIR)/cjson
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/cjson:
	mkdir -p $(OBJDIR)/cjson

clean:
	rm -rf $(OBJDIR) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

.PHONY: all clean install
