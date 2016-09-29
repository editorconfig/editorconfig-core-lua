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
#include <assert.h>
#include <editorconfig/editorconfig.h>
#include "lua.h"
#include "lauxlib.h"

#ifndef LEC_VERSION
#error "LEC_VERSION is not defined."
#endif

#define strequal(s1, s2) \
    (strcmp((s1), (s2)) == 0)

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
push_property(lua_State *L, const char *name, const char *value)
{
    const struct lec_property *prop = NULL;

    lua_pushstring(L, name);
    for (int i = 0; properties[i].name != NULL; i++) {
        if (strequal(name, properties[i].name)) {
            prop = &properties[i];
            break;
        }
    }
    if (prop == NULL) {
        // Unknown property
        lua_pushstring(L, value);
        return E_OK;
    }
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
    if ((prop->push_func != NULL) &&
                (prop->push_func(L, value) == E_OK)) {
        return E_OK;
    }
    lua_pop(L, 1);
    return E_ERROR;
}

static int parse_error(lua_State *L, int err_num, const char *err_file);

struct lec_handle {
    editorconfig_handle eh;
    lua_Integer count;
};

/* Receives 3 arguments, two optional */
static void
push_udata_handle(lua_State *L)
{
    const char *file_full_path;
    const char *conf_file_name;
    const char *version_to_set;
    int major = -1, minor = -1, patch = -1;
    struct lec_handle *h;
    int err_num;

    file_full_path = luaL_checkstring(L, 1);
    conf_file_name = luaL_opt(L, luaL_checkstring, 2, NULL);
    version_to_set = luaL_opt(L, luaL_checkstring, 3, NULL);
    h = lua_newuserdata(L, sizeof(struct lec_handle));
    luaL_getmetatable(L, "EditorConfig.iter");
    lua_setmetatable(L, -2);
    h->count = 0;
    h->eh = editorconfig_handle_init();
    if (h->eh == NULL) {
        luaL_error(L, "not enough memory to create handle");
        assert(0);
    }
    if (conf_file_name != NULL) {
        editorconfig_handle_set_conf_file_name(h->eh, conf_file_name);
    }
    if (version_to_set != NULL) {
        sscanf(version_to_set, "%d.%d.%d", &major, &minor, &patch);
        editorconfig_handle_set_version(h->eh, major, minor, patch);
    }
    err_num = editorconfig_parse(file_full_path, h->eh);
    if (err_num != 0) {
        parse_error(L, err_num, editorconfig_handle_get_err_file(h->eh));
        assert(0);
    }
}

/* One mandatory argument (file_full_path) */
/* One optional argument (conf_file_name) */
/* One optional argument (version_to_set) */
/* Returns two tables: { keys = values }, { keys } */
static int
lec_parse(lua_State *L)
{
    struct lec_handle *h;
    int name_value_count;
    const char *name, *raw_value;

    push_udata_handle(L);
    // clear stack with userdata handle only
    lua_replace(L, 1);
    lua_settop(L, 1);
    h = lua_touserdata(L, -1);
    assert(h != NULL);
    name_value_count = editorconfig_handle_get_name_value_count(h->eh);
    lua_createtable(L, 0, name_value_count);
    lua_createtable(L, name_value_count, 0);
    for (int i = 0; i < name_value_count; i++) {
        name = raw_value = NULL;
        editorconfig_handle_get_name_value(h->eh, i, &name, &raw_value);
        if (push_property(L, name, raw_value) != E_OK) {
            continue; // Skip
        }
        lua_setfield(L, -3, name);
        lua_pushinteger(L, h->count + 1);
        lua_pushstring(L, name);
        lua_settable(L, -3);
        h->count += 1;
    }

    return 2;
}

/*
 * Don't use "editorconfig_get_error_msg".
 * Error codes: https://github.com/editorconfig/editorconfig-core-c/blob/master/src/lib/editorconfig.c
 */
static int
parse_error(lua_State *L, int err_num, const char *err_file)
{
    const char *err_msg;

    if (err_num == 0) {
        assert(0);
        return luaL_error(L, "no error occurred");
    }
    if (err_num > 0) {
        /* EditorConfig parsing error, 'err_num' is the line number */
        return luaL_error(L, "'%s' at line %d: failed to parse file",
                        err_file ? err_file : "<null>", err_num);
    }
    switch (err_num) {
        case EDITORCONFIG_PARSE_NOT_FULL_PATH:
            err_msg = "input file must be a full path name";
            break;
        case EDITORCONFIG_PARSE_MEMORY_ERROR:
            err_msg = "memory error";
            break;
        case EDITORCONFIG_PARSE_VERSION_TOO_NEW:
            err_msg = "required version is greater than the current version";
            break;
        default:
            err_msg = "unknown error";
            break;
    }
    return luaL_error(L, "%s", err_msg);
}

static int lec_iter(lua_State *L);

static int
lec_iter_open(lua_State *L)
{
    push_udata_handle(L);
    lua_pushcclosure(L, lec_iter, 1);
    return 1;
}

static int
lec_iter(lua_State *L)
{
    struct lec_handle *h = lua_touserdata(L, lua_upvalueindex(1));
    while (1) {
        if (h->count >= editorconfig_handle_get_name_value_count(h->eh)) {
            return 0; // stop iteration
        }
        const char *name = NULL, *raw_value = NULL;
        editorconfig_handle_get_name_value(h->eh, h->count, &name, &raw_value);
        h->count += 1;
        err_t err = push_property(L, name, raw_value);
        if (err == E_OK) {
            return 2;
        }
    }
}

static int
lec_iter_gc(lua_State *L)
{
    struct lec_handle *h = lua_touserdata(L, -1);
    if (h != NULL)
        (void)editorconfig_handle_destroy(h->eh);
    return 0;
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
    {"open", lec_iter_open},
    {NULL, NULL}
};

int luaopen_editorconfig_core (lua_State *L) {
    luaL_newmetatable(L, "EditorConfig.token");
    lua_pushcfunction(L, token_tostring);
    lua_setfield(L, -2, "__tostring");
    luaL_newmetatable(L, "EditorConfig.iter");
    lua_pushcfunction(L, lec_iter_gc);
    lua_setfield(L, -2, "__gc");
    luaL_newlib(L, editorconfig_core);
    add_version(L);
    add_tokens(L);
    // insert T into registry
    lua_getfield(L, -1, "T");
    lua_setfield(L, LUA_REGISTRYINDEX, "EditorConfig.T");
    return 1;
}
