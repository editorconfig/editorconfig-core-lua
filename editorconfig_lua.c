/*
 * Copyright (c) 2016 João Valverde <joao.valverde@tecnico.ulisboa.pt>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <editorconfig/editorconfig.h>
#include "lua.h"
#include "lauxlib.h"

#ifndef LEC_VERSION_STR
#define LEC_VERSION_STR     "0.1"
#endif

#define INDENT_STYLE_TAB    "tab@indent_style"
#define INDENT_STYLE_SPACE  "space@indent_style"
#define INDENT_SIZE_TAB     "tab@indent_size"
#define END_OF_LINE_LF      "lf@end_of_line"
#define END_OF_LINE_CRLF    "crlf@end_of_line"
#define END_OF_LINE_CR      "cr@end_of_line"
#define CHARSET_LATIN1      "latin1@charset"
#define CHARSET_UTF_8       "utf-8@charset"
#define CHARSET_UTF_8_BOM   "utf-8-bom@charset"
#define CHARSET_UTF_16BE    "utf-16be@charset"
#define CHARSET_UTF_16LE    "utf-16le@charset"
#define MAXLINELEN_OFF      "off@max_line_length"

#define MSGBUFSIZ   64
#define E_OK        0
#define E_ERROR     -1

#define strequal(s1, s2)    (strcmp((s1), (s2)) == 0)


static char *_strncpy0(char *dest, const char *src, size_t n) {
    memset(dest, 0, n);
    return strncpy(dest, src, n);
}

struct lec_tokens {
    const char *name, *value;
};

static struct lec_tokens tokens[] = {
    { "INDENT_STYLE_TAB",   INDENT_STYLE_TAB    },
    { "INDENT_STYLE_SPACE", INDENT_STYLE_SPACE  },
    { "INDENT_SIZE_TAB",    INDENT_SIZE_TAB     },
    { "END_OF_LINE_LF",     END_OF_LINE_LF      },
    { "END_OF_LINE_CRLF",   END_OF_LINE_CRLF    },
    { "END_OF_LINE_CR",     END_OF_LINE_CR      },
    { "CHARSET_LATIN1",     CHARSET_LATIN1      },
    { "CHARSET_UTF_8",      CHARSET_UTF_8       },
    { "CHARSET_UTF_8_BOM",  CHARSET_UTF_8_BOM   },
    { "CHARSET_UTF_16BE",   CHARSET_UTF_16BE    },
    { "CHARSET_UTF_16LE",   CHARSET_UTF_16LE    },
    { "MAXLINELEN_OFF",     MAXLINELEN_OFF      },
    { NULL, NULL }
};

static void add_globals(lua_State *L) {
    int i;

    for (i = 0; tokens[i].name != NULL; i++) {
        lua_pushstring(L, tokens[i].value);
        lua_setfield(L, -2, tokens[i].name);
    }
}

static void add_version(lua_State *L) {
    int major = 0, minor = 0, patch = 0;
    const char *fmt;

    editorconfig_get_version(&major, &minor, &patch);
    fmt = "Lua EditorConfig v" LEC_VERSION_STR " [C Core Version %d.%d.%d]";
    lua_pushfstring(L, fmt, major, minor, patch);
    lua_setfield(L, -2, "_VERSION");
}

/*
 * Don't use "editorconfig_get_error_msg".
 * Error codes: https://github.com/editorconfig/editorconfig-core-c/blob/master/src/lib/editorconfig.c
 */
static void parse_error_msg(int err_num, char *buf, size_t bufsiz) {
    const char *msg;

    if (err_num == 0)
        msg = "no error occurred";
    else if(err_num > 0)
        msg = "failed to parse file";
    else if (err_num == EDITORCONFIG_PARSE_NOT_FULL_PATH)
        msg = "input file must be a full path name";
    else if (err_num == EDITORCONFIG_PARSE_MEMORY_ERROR)
        msg = "memory error";
    else if (err_num == EDITORCONFIG_PARSE_VERSION_TOO_NEW)
        msg = "required version is greater than the current version";
    else
        msg = "unknown error";

    _strncpy0(buf, msg, bufsiz);
}

static void parse_error_err_file(editorconfig_handle eh, char *buf, size_t bufsiz) {
    const char *err_file;

    err_file = editorconfig_handle_get_err_file(eh);
    if (err_file == NULL)
        err_file = "<null>";

    _strncpy0(buf, err_file, bufsiz);
}

static int parse_error(lua_State *L, editorconfig_handle eh, int err_num) {
    char err_msg[MSGBUFSIZ], err_file[MSGBUFSIZ];

    parse_error_msg(err_num, err_msg, sizeof(err_msg));
    parse_error_err_file(eh, err_file, sizeof(err_file));
    (void)editorconfig_handle_destroy(eh);

    if (err_num == 0) {
        return luaL_error(L, "error code is zero, probably a bug");
    }

    if (err_num < 0) {
        return luaL_error(L, "%s", err_msg);
    }

    /* EditorConfig parsing error, 'err_num' is the line number */
    return luaL_error(L, "'%s' at line %d: %s", err_file, err_num, err_msg);
}

static int push_ec_boolean(lua_State *L, const char *value) {
    if (strequal(value, "true")) {
        lua_pushboolean(L, 1);
        return E_OK;
    }
    if (strequal(value, "false")) {
        lua_pushboolean(L, 0);
        return E_OK;
    }
    return E_ERROR;
}

static int push_ec_number(lua_State *L, const char *value) {
    long number;
    const char *nptr = value;
    char *endptr = NULL;

    number = strtol(nptr, &endptr, 0);
    if (*nptr == '\0' || *endptr != '\0') {
        /* Accept al digits dec/hex/octal only */
        return E_ERROR;
    }
    if (number <= 0) {
        /* Accept positive integers only */
        return E_ERROR;
    }
    lua_pushinteger(L, (lua_Integer)number);
    return E_OK;
}

/* Push key/value on stack and call settable */
/* Skip known keys with invalid values (according to the EC spec) */
/* First push value, then key, then rotate */
/* No need to pop to clean errors with rotate, keys (names) are always valid */
static int add_ec_name_value(lua_State *L,
                const char *name, const char *value)
{
    /* We ignore unknown values for known keys (but not unknown keys) */
    if (strequal(name, "indent_style")) {
        if (strequal(value, "tab")) {
            lua_pushstring(L, INDENT_STYLE_TAB);
        }
        else if (strequal(value, "space")) {
            lua_pushstring(L, INDENT_STYLE_SPACE);
        }
        else {
            return 0;
        }
    }
    else if (strequal(name, "indent_size")) {
        if (strequal(value, "tab")) {
            lua_pushstring(L, INDENT_SIZE_TAB);
        }
        else if (push_ec_number(L, value) != E_OK) {
            return 0;
        }
    }
    else if (strequal(name, "tab_width")) {
        if (push_ec_number(L, value) != E_OK) {
            return 0;
        }
    }
    else if (strequal(name, "end_of_line")) {
        if (strequal(value, "lf")) {
            lua_pushstring(L, END_OF_LINE_LF);
        }
        else if (strequal(value, "crlf")) {
            lua_pushstring(L, END_OF_LINE_CRLF);
        }
        else if (strequal(value, "cr")) {
            lua_pushstring(L, END_OF_LINE_CR);
        }
        else {
            return 0;
        }
    }
    else if (strequal(name, "charset")) {
        if (strequal(value, "latin1")) {
            lua_pushstring(L, CHARSET_LATIN1);
        }
        else if (strequal(value, "utf-8")) {
            lua_pushstring(L, CHARSET_UTF_8);
        }
        else if (strequal(value, "utf-8-bom")) {
            lua_pushstring(L, CHARSET_UTF_8_BOM);
        }
        else if (strequal(value, "utf-16be")) {
            lua_pushstring(L, CHARSET_UTF_16BE);
        }
        else if (strequal(value, "utf-16le")) {
            lua_pushstring(L, CHARSET_UTF_16LE);
        }
        else {
            return 0;
        }
    }
    else if (strequal(name, "trim_trailing_whitespace")) {
        if(push_ec_boolean(L, value) != E_OK) {
            return 0;
        }
    }
    else if (strequal(name, "insert_final_newline")) {
        if(push_ec_boolean(L, value) != E_OK) {
            return 0;
        }
    }
    else if (strequal(name, "max_line_length")) {
        if (strequal(value, "off")) {
            lua_pushstring(L, MAXLINELEN_OFF);
        }
        else if (push_ec_number(L, value) != E_OK) {
            return 0;
        }
    }
    else {
        /* Unknown key */
        lua_pushstring(L, value);
    }
    lua_pushstring(L, name);
    lua_rotate(L, -2, 1);
    lua_settable(L, -3);
    /* One table entry inserted (for book-keeping) */
    return 1;
}

static void create_ec_table(lua_State *L, editorconfig_handle eh) {
    int name_value_count;
    const char *name, *value;
    int i, count = 0;

    /* Create the EditorConfig parse table */
    name_value_count = editorconfig_handle_get_name_value_count(eh);
    lua_createtable(L, 0, name_value_count);
    for (i = 0; i < name_value_count; i++) {
        name = value = NULL;
        editorconfig_handle_get_name_value(eh, i, &name, &value);
        count += add_ec_name_value(L, name, value);
    }
    lua_pushinteger(L, count);
}

static int lec_parse(lua_State *L) {
    const char *full_filename;
    editorconfig_handle eh;
    int err_num;

    full_filename = luaL_checkstring(L, 1);

    eh = editorconfig_handle_init();
    if (eh == NULL) {
        return luaL_error(L, "not enough memory to create handle");
    }

    err_num = editorconfig_parse(full_filename, eh);
    if (err_num != 0) {
        return parse_error(L, eh, err_num);
    }

    create_ec_table(L, eh);

    (void)editorconfig_handle_destroy(eh);
    return 2;
}

static const struct luaL_Reg editorconfig_core [] = {
    {"parse", lec_parse},
    {NULL, NULL}
};

int luaopen_editorconfig_core (lua_State *L) {
    luaL_newlib(L, editorconfig_core);
    add_globals(L);
    add_version(L);
    return 1;
}
