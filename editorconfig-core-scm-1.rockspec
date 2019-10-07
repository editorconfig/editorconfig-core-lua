package = "editorconfig-core"
-- version = @VERSION@-@PKGREL@
version = "scm-1"
source = {
--    url = "https://github.com/editorconfig/editorconfig-core-lua/archive/v@VERSION@.tar.gz",
--    dir = "editorconfig-core-lua-@VERSION@",
    url = "git://github.com/editorconfig/editorconfig-core-lua.git",
}
description = {
    summary = "EditorConfig support for the Lua language",
    detailed = "EditorConfig makes it easy to maintain the correct \z
coding style when switching between different text editors and \z
betweendifferent projects. The EditorConfig project maintains a file \z
format and plugins for various text editors which allow this file \z
format to be read and used by those editors. EditorConfig Lua Core \z
provides the same functionality as the Editorconfig C Core library.",
    homepage = "http://editorconfig.org",
    license = "BSD",
}
dependencies = {
    "lua >= 5.2",
}
build = {
    type = "cmake",
    variables = {
        CMAKE_BUILD_TYPE = "Release",
        CMAKE_INSTALL_PREFIX = "$(PREFIX)",
        ECL_LIBDIR = "$(LIBDIR)",
        ENABLE_TESTS = "Off",
    },
}
