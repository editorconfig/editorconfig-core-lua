# EditorConfig Lua Core

EditorConfig Lua Core provides the same functionality as the
[EditorConfig C Core][1] library.

## EditorConfig Project

EditorConfig makes it easy to maintain the correct coding style when switching
between different text editors and between different projects. The EditorConfig
project maintains a file format and plugins for various text editors which
allow this file format to be read and used by those editors. For information
on the file format and supported text editors, see the [EditorConfig][2]
website.

## Installation

#### Build using LuaRocks

```
luarocks make --pack-binary-rock
```

Then install the binary rock file, e.g:

```
luarocks --local install editorconfig-core-scm-1.linux-x86_64.rock
```

#### Build using CMake

```
mkdir build
cd build
cmake ..
make
make test   # optional
```

Then copy the `editorconfig.so` binary module to somewhere in
your `LUA_CPATH`.

To be able to run the tests you may have to update the git submodule
with `git submodule update --init`.

## Usage

The `parse` module function returns a name/value property table. Typical usage
by plugins would be:

```lua
ec = require("editorconfig")

for name, value in pairs(ec.parse("/full/path/to/file")) do
    configure_property[name](value)
end
```

Sometimes it is useful to have the same stable order for each `parse`
invocation that the EditorConfig C Core library provides. For that the property
names are available as an array in a second return value:

```lua
prop, names = ec.parse("/full/path/to/file")
print(#names .. " properties:")
for idx, name in ipairs(names) do
  print(string.format("%s: %s=%s", idx, name, prop[name]))
end
```

Note also the use of the length operator to retrieve the EditorConfig
property count for a given file.

#### Note on API stability

Version 0.3.0 introduced major backward incompatibilities.

* The `open`function was removed.
* Every EditorConfig value has the Lua type `string`.
* Lua module renamed to `editorconfig`. 

Please update accordingly.

[1]: https://github.com/editorconfig/editorconfig-core-c
[2]: https://editorconfig.org
