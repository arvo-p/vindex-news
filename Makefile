CC      := gcc
CXX     := g++
CFLAGS  := -Wall -O2 -Iclient
CXXFLAGS:= -Wall -O2 -Iserver

BUILD_DIR       := build
CLIENT_OBJ_DIR  := $(BUILD_DIR)/client
SERVER_OBJ_DIR  := $(BUILD_DIR)/server

CLIENT_SRCS := \
	client/main.c \
	client/cmd.c \
	client/colorscheme.c \
	client/loadfiles.c \
	client/nav.c \
	client/tabs.c \
	client/ui.c

SERVER_SRCS := \
	server/vindexServer.cpp \
	server/files_path.cpp \
	server/blacklist.cpp \
	server/groups.cpp \
	server/feed.cpp \
	server/utils.cpp

CLIENT_OBJS := $(patsubst client/%.c,$(CLIENT_OBJ_DIR)/%.o,$(CLIENT_SRCS))
SERVER_OBJS := $(patsubst server/%.cpp,$(SERVER_OBJ_DIR)/%.o,$(SERVER_SRCS))

CLIENT_BIN := client/vindex
SERVER_BIN := server/vindexServer

PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin
UNAME_S := $(shell uname -s)

.PHONY: all client server install uninstall clean

all: client server

client: $(CLIENT_BIN)

server: $(SERVER_BIN)

install: all
ifneq ($(UNAME_S),Linux)
	$(error make install is only supported on Linux)
endif
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 755 $(CLIENT_BIN) "$(DESTDIR)$(BINDIR)/vindex"
	install -m 755 $(SERVER_BIN) "$(DESTDIR)$(BINDIR)/vindexServer"

uninstall:
ifneq ($(UNAME_S),Linux)
	$(error make uninstall is only supported on Linux)
endif
	rm -f "$(DESTDIR)$(BINDIR)/vindex" "$(DESTDIR)$(BINDIR)/vindexServer"

$(CLIENT_BIN): $(CLIENT_OBJS)
	$(CC) $(CLIENT_OBJS) -o $@ -lpthread

$(SERVER_BIN): $(SERVER_OBJS)
	$(CXX) $(SERVER_OBJS) -o $@ -lcurl

$(CLIENT_OBJ_DIR)/%.o: client/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(SERVER_OBJ_DIR)/%.o: server/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(CLIENT_BIN) $(SERVER_BIN)
