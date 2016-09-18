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

#define strequal(s1, s2)    (strcmp((s1), (s2)) == 0)
#define strncpy0(s1, s2, n) strncpy(memset((s1), 0, n), (s2), n)

#define E_OK         1
#define E_NULL       0
#define E_ERROR     -1

typedef int err_t;

typedef err_t (push_ec_value_fn)(lua_State *L, const char *value);

struct lec_property {
    const char *name;
    push_ec_value_fn *push_func;
};

static err_t
push_ec_string(lua_State *L, const char *value)
{
    lua_pushstring(L, value);
    return E_OK;
}

static err_t
push_ec_boolean(lua_State *L, const char *value)
{
    if (strequal(value, "true")) {
        lua_pushboolean(L, 1);
    }
    else if (strequal(value, "false")) {
        lua_pushboolean(L, 0);
    }
    else {
        return E_ERROR;
    }
    return E_OK;
}

static err_t
push_ec_number(lua_State *L, const char *value)
{
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

static err_t
push_ec_indent_size(lua_State *L, const char *value)
{
    if (strequal(value, "tab")) {
        return push_ec_string(L, value);
    }
    return push_ec_number(L, value);
}

static err_t
push_ec_max_line_length(lua_State *L, const char *value)
{
    if (strequal(value, "off")) {
        return push_ec_string(L, value);
    }
    return push_ec_number(L, value);
}

static struct lec_property properties[] = {
    { "indent_style",
        push_ec_string },
    { "indent_size",
        push_ec_indent_size },
    { "tab_width",
        push_ec_number },
    { "end_of_line",
        push_ec_string },
    { "charset",
        push_ec_string },
    { "trim_trailing_whitespace",
        push_ec_boolean },
    { "insert_final_newline",
        push_ec_boolean },
    { "max_line_length",
        push_ec_max_line_length },
    { NULL, NULL }
};

/* t1[kn] = vn */
/* t2[n] = kn */
/* Receives two tables on the stack */
static int
add_name_value(lua_State *L, const char *name, const char *value,
                                                    lua_Integer idx)
{
    push_ec_value_fn *push_func = NULL;

    for (int i = 0; properties[i].name != NULL; i++) {
        if (strequal(name, properties[i].name)) {
            push_func = properties[i].push_func;
            break;
        }
    }
    if (push_func == NULL) {
        // Unknown property
        lua_pushstring(L, value);
    }
    else if (push_func(L, value) != E_OK) {
        return 0; // Skip
    }
    lua_setfield(L, -3, name);
    lua_pushinteger(L, idx);
    lua_pushstring(L, name);
    lua_settable(L, -3);
    return 1;
}

static int parse_error(lua_State *L, editorconfig_handle eh, int err_num);

/* One mandatory argument (full_filename) */
/* One optional argument (conf_file_name) */
/*
 * Return : { keys = values }, { keys }
 */
static int
lec_parse(lua_State *L)
{
    const char *full_filename, *conf_file_name;
    const char *version_to_set;
    int major = -1, minor = -1, patch = -1;
    editorconfig_handle eh;
    int err_num, name_value_count;
    const char *name, *raw_value;
    lua_Integer idx = 1;

    full_filename = luaL_checkstring(L, 1);
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

    err_num = editorconfig_parse(full_filename, eh);
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
    lua_pushfstring(L, fmt,major, minor, patch);
    lua_setfield(L, -2, "_C_VERSION");
}

static const struct luaL_Reg editorconfig_core[] = {
    {"parse", lec_parse},
    {NULL, NULL}
};

int luaopen_editorconfig_core (lua_State *L) {
    luaL_newlib(L, editorconfig_core);
    add_version(L);
    return 1;
}
