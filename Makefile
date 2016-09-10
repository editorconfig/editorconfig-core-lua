
LEC_VERSION = 0.1

prefix = /usr/local
libdir = $(prefix)/lib
lualibdir = $(libdir)/lua/5.3

# Lua 5.3
lua53 = lua
LUA_CFLAGS =
LUA_LIBS = -l$(lua53) -lm

# EditorConfig
EC_CFLAGS =
EC_LIBS = -leditorconfig

CC = gcc
#WERROR = -Werror
CFLAGS_VERSION = -DLEC_VERSION_STR=\"$(LEC_VERSION)\"
CFLAGS = $(CFLAGS_VERSION) -O2 -g -Wall -Wextra $(WERROR) -fPIC $(LUA_CFLAGS) $(EC_CFLAGS)
LDFLAGS =
LIBS = $(LUA_LIBS) $(EC_LIBS)

OBJS = editorconfig_lua.o
LEC_SO = editorconfig_core.so

all: $(LEC_SO)

$(LEC_SO): $(OBJS)
	$(CC) -shared -o $@ $^ $(LIBS)

editorconfig_lua.o: editorconfig_lua.c

.PHONY: install dist clean distclean very-clean

install:
	mkdir -p $(DESTDIR)$(lualibdir)
	cp $(LEC_SO) $(DESTDIR)$(lualibdir)

TARNAME = editorconfig-core-lua-$(LEC_VERSION)

dist:
	mkdir $(TARNAME); \
	cp $(OBJS:.o=.c) LICENSE Makefile $(TARNAME) && \
	tar czf $(TARNAME).tar.gz $(TARNAME); \
	rm -rf $(TARNAME)

clean:
	rm -f $(OBJS)

distclean: clean
	rm -f $(LEC_SO)

very-clean: distclean
	rm -f $(TARNAME).tar.gz
