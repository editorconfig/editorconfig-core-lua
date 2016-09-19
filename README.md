# EditorConfig Lua Core

EditorConfig Lua Core provides the same functionality as the [EditorConfig C Core](https://github.com/editorconfig/editorconfig-core-c) library.

## EditorConfig Project

EditorConfig makes it easy to maintain the correct coding style when switching between different text editors and between different projects. The EditorConfig project maintains a file format and plugins for various text editors which allow this file format to be read and used by those editors. For information on the file format and supported text editors, see the [EditorConfig website](http://editorconfig.org>).

## Installation

Build/install using LuaRocks (recommended):

```sh
luarocks make
```

Build/install using CMake:

```sh
mkdir cmbuild
cd cmbuild
cmake ..
make
make test   #optional
```

Then copy the `editorconfig_core.so` binary module to somewhere in your `LUA_CPATH`.

## Usage

The `editorconfig_core.parse` module function returns a key/value property table:

```lua
ec = require("editorconfig_core")
p = ec.parse("/full/path/to/file")
for k, v in pairs(p) do print(string.format("%s=%s", k, v)) end
```

Sometimes it is useful to have the same stable order for each `parse()` invocation that the EditorConfig C Core library provides. For that the property keys are available as an array in a second return value:

```lua
p, k = ec.parse("/full/path/to/file")
print(#k .. " properties:")
for i, v in ipairs(k) do print(string.format("%s: %s=%s", i, v, p[v])) end
```

Note the use of the length operator to retrieve the EditorConfig property count for a given file.
