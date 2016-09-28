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

#ifndef LEC_VERSION
#error "LEC_VERSION is not defined."
#endif

#define MSGBUFSIZ   64

#define strequal(s1, s2) \
    (strcmp((s1), (s2)) == 0)

#define strncpy0(s1, s2, n) \
    strncpy(memset((s1), 0, (n)), (s2), (n))

#define E_OK         1
#define E_NULL       0
#define E_ERROR     -1

typedef int err_t;

typedef err_t (push_ec_value_fn)(lua_State *L, const char *value);

enum ec_token {
    LEC_T_NONE = -1,
    LEC_T_INDENT_STYLE_TAB,
    LEC_T_INDENT_STYLE_SPACE,
    LEC_T_INDENT_SIZE_TAB,
    LEC_T_END_OF_LINE_LF,
    LEC_T_END_OF_LINE_CRLF,
    LEC_T_END_OF_LINE_CR,
    LEC_T_CHARSET_LATIN1,
    LEC_T_CHARSET_UTF_8,
    LEC_T_CHARSET_UTF_16BE,
    LEC_T_CHARSET_UTF_16LE,
    LEC_T_MAX_LINE_LENGTH_OFF,
    _T_NUM_TOKENS
};

struct lec_token {
    const char *symbol;
    const char *value;
};

struct lec_property {
    const char *name;
    enum ec_token *tokens;
    push_ec_value_fn *push_func;
};

static int
token_tostring(lua_State *L)
{
    const struct lec_token *t;
    t = lua_touserdata(L, -1);
    lua_pushstring(L, t->value);
    return 1;
}

static err_t
push_ec_boolean(lua_State *L, const char *value)
{
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

static err_t
push_ec_number(lua_State *L, const char *value)
{
    long number;
    const char *nptr = value;
    char *endptr = NULL;

    number = strtol(nptr, &endptr, 0);
    if (*nptr == '\0' || *endptr != '\0') {
        /* Accept all digits in dec/hex/octal only */
        return E_ERROR;
    }
    if (number <= 0) {
        /* Accept positive integers only */
        return E_ERROR;
    }
    lua_pushinteger(L, (lua_Integer)number);
    return E_OK;
}

static struct lec_token tokens[] = {
    [LEC_T_INDENT_STYLE_TAB]    = { "INDENT_STYLE_TAB",    "tab"      },
    [LEC_T_INDENT_STYLE_SPACE]  = { "INDENT_STYLE_SPACE",  "space"    },
    [LEC_T_INDENT_SIZE_TAB]     = { "INDENT_SIZE_TAB",     "tab"      },
    [LEC_T_END_OF_LINE_LF]      = { "END_OF_LINE_LF",      "lf"       },
    [LEC_T_END_OF_LINE_CRLF]    = { "END_OF_LINE_CRLF",    "crlf"     },
    [LEC_T_END_OF_LINE_CR]      = { "END_OF_LINE_CR",      "cr"       },
    [LEC_T_CHARSET_LATIN1]      = { "CHARSET_LATIN1",      "latin1"   },
    [LEC_T_CHARSET_UTF_8]       = { "CHARSET_UTF_8",       "utf-8"    },
    [LEC_T_CHARSET_UTF_16BE]    = { "CHARSET_UTF_16BE",    "utf-16be" },
    [LEC_T_CHARSET_UTF_16LE]    = { "CHARSET_UTF_16LE",    "utf-16le" },
    [LEC_T_MAX_LINE_LENGTH_OFF] = { "MAX_LINE_LENGTH_OFF", "off"      },
    { NULL, NULL }
};

static enum ec_token indent_style_tokens[] = {
    LEC_T_INDENT_STYLE_TAB,
    LEC_T_INDENT_STYLE_SPACE,
    LEC_T_NONE
};

static enum ec_token indent_size_tokens[] = {
    LEC_T_INDENT_SIZE_TAB,
    LEC_T_NONE
};

static enum ec_token end_of_line_tokens[] = {
    LEC_T_END_OF_LINE_LF,
    LEC_T_END_OF_LINE_CRLF,
    LEC_T_END_OF_LINE_CR,
    LEC_T_NONE
};

static enum ec_token charset_tokens[] = {
    LEC_T_CHARSET_LATIN1,
    LEC_T_CHARSET_UTF_8,
    LEC_T_CHARSET_UTF_16BE,
    LEC_T_CHARSET_UTF_16LE,
    LEC_T_NONE
};

static enum ec_token max_line_length_tokens[] = {
    LEC_T_MAX_LINE_LENGTH_OFF,
    LEC_T_NONE
};

static struct lec_property properties[] = {
    { "indent_style",
        indent_style_tokens, NULL },
    { "indent_size",
        indent_size_tokens, push_ec_number },
    { "tab_width",
        NULL, push_ec_number },
    { "end_of_line",
        end_of_line_tokens, NULL },
    { "charset",
        charset_tokens, NULL },
    { "trim_trailing_whitespace",
        NULL, push_ec_boolean },
    { "insert_final_newline",
        NULL, push_ec_boolean },
    { "max_line_length",
        max_line_length_tokens, push_ec_number },
    { NULL, NULL, NULL }
};

static err_t
push_property(lua_State *L, const struct lec_property *prop, const char *value)
{
    if (prop->tokens != NULL) {
        for (enum ec_token *t = prop->tokens; *t != LEC_T_NONE; t++) {
            if (strequal(tokens[*t].value, value)) {
                lua_getfield(L, LUA_REGISTRYINDEX, "EditorConfig.T");
                lua_getfield(L, -1, tokens[*t].symbol);
                lua_remove(L, -2);
                return E_OK;
            }
        }
    }
    if (prop->push_func != NULL) {
        return prop->push_func(L, value);
    }
    return E_ERROR;
}

/* t1[kn] = vn */
/* t2[n] = kn */
/* Receives two tables on the stack */
static int
add_name_value(lua_State *L, const char *name, const char *value,
                                                    lua_Integer idx)
{
    const struct lec_property *prop = NULL;

    for (int i = 0; properties[i].name != NULL; i++) {
        if (strequal(name, properties[i].name)) {
            prop = &properties[i];
            break;
        }
    }
    if (prop == NULL) {
        // Unknown property
        lua_pushstring(L, value);
    }
    else if (push_property(L, prop, value) != E_OK) {
        return 0; // Skip
    }
    lua_setfield(L, -3, name);
    lua_pushinteger(L, idx);
    lua_pushstring(L, name);
    lua_settable(L, -3);
    return 1;
}

static int parse_error(lua_State *L, editorconfig_handle eh, int err_num);

/* One mandatory argument (file_full_path) */
/* One optional argument (conf_file_name) */
/* One optional argument (version_to_set) */
/* Returns two tables: { keys = values }, { keys } */
static int
lec_parse(lua_State *L)
{
    const char *file_full_path;
    const char *conf_file_name;
    const char *version_to_set;
    int major = -1, minor = -1, patch = -1;
    editorconfig_handle eh;
    int err_num, name_value_count;
    const char *name, *raw_value;
    lua_Integer idx = 1;

    file_full_path = luaL_checkstring(L, 1);
    conf_file_name = luaL_opt(L, luaL_checkstring, 2, NULL);
    version_to_set = luaL_opt(L, luaL_checkstring, 3, NULL);
    if (version_to_set != NULL) {
        sscanf(version_to_set, "%d.%d.%d", &major, &minor, &patch);
    }

    eh = editorconfig_handle_init();
    if (eh == NULL) {
        return luaL_error(L, "not enough memory to create handle");
    }
    if (conf_file_name != NULL) {
        editorconfig_handle_set_conf_file_name(eh, conf_file_name);
    }
    if (version_to_set != NULL) {
        editorconfig_handle_set_version(eh, major, minor, patch);
    }

    err_num = editorconfig_parse(file_full_path, eh);
    if (err_num != 0) {
        return parse_error(L, eh, err_num);
    }
    /* Create the EditorConfig parse tables */
    name_value_count = editorconfig_handle_get_name_value_count(eh);
    lua_createtable(L, 0, name_value_count);
    lua_createtable(L, name_value_count, 0);
    for (int i = 0; i < name_value_count; i++) {
        name = raw_value = NULL;
        editorconfig_handle_get_name_value(eh, i, &name, &raw_value);
        idx += add_name_value(L, name, raw_value, idx);
    }

    (void)editorconfig_handle_destroy(eh);
    return 2;
}

/*
 * Don't use "editorconfig_get_error_msg".
 * Error codes: https://github.com/editorconfig/editorconfig-core-c/blob/master/src/lib/editorconfig.c
 */
static void
parse_error_msg(int err_num, char *buf, size_t bufsiz)
{
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

    strncpy0(buf, msg, bufsiz);
}

static void
parse_error_err_file(editorconfig_handle eh, char *buf, size_t bufsiz)
{
    const char *err_file;

    err_file = editorconfig_handle_get_err_file(eh);
    if (err_file == NULL)
        err_file = "<null>";

    strncpy0(buf, err_file, bufsiz);
}

static int
parse_error(lua_State *L, editorconfig_handle eh, int err_num)
{
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

static void
add_version(lua_State *L)
{
    int major = 0, minor = 0, patch = 0;
    const char *fmt;

    fmt = "EditorConfig Lua Core Version %s";
    lua_pushfstring(L, fmt, LEC_VERSION);
    lua_setfield(L, -2, "_VERSION");
    editorconfig_get_version(&major, &minor, &patch);
    fmt = "EditorConfig C Core Version %d.%d.%d";
    lua_pushfstring(L, fmt, major, minor, patch);
    lua_setfield(L, -2, "_C_VERSION");
}

static void
add_tokens(lua_State *L)
{
    struct lec_token *t;

    lua_createtable(L, 0, _T_NUM_TOKENS);
    for (int i = 0; tokens[i].symbol != NULL; i++) {
        t = lua_newuserdata(L, sizeof(struct lec_token));
        *t = tokens[i];
        luaL_getmetatable(L, "EditorConfig.token");
        lua_setmetatable(L, -2);
        lua_setfield(L, -2, tokens[i].symbol);
    }
    lua_setfield(L, -2, "T");
}

static const struct luaL_Reg editorconfig_core[] = {
    {"parse", lec_parse},
    {NULL, NULL}
};

int luaopen_editorconfig_core (lua_State *L) {
    luaL_newmetatable(L, "EditorConfig.token");
    lua_pushcfunction(L, token_tostring);
    lua_setfield(L, -2, "__tostring");
    luaL_newlib(L, editorconfig_core);
    add_version(L);
    add_tokens(L);
    // insert T into registry
    lua_getfield(L, -1, "T");
    lua_setfield(L, LUA_REGISTRYINDEX, "EditorConfig.T");
    return 1;
}
