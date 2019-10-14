
require("pl")
ec = require("editorconfig")

local progname = arg[0]

local function print_usage(ret)
    print(ec._VERSION)
    print("Usage: " .. progname .. " [OPTIONS] FILEPATH1 [FILEPATH2 FILEPATH3 ...]")
    -- print("FILEPATH can be a hyphen (-) if you want to path(s) to be read from stdin.")
    print()
    print("-f                 Specify conf filename other than \".editorconfig\".")
    print("-b                 Specify version (used by devs to test compatibility).")
    print("-h OR --help       Print this help message.")
    print("-v OR --version    Display version information.")
    os.exit(ret)
end

flags, params = app.parse_args(arg, {f = true, b = true})

if flags.h or flags.help then print_usage(0) end
if flags.v or flags.version then print(ec._VERSION); os.exit(0) end
if #params == 0 then print_usage(1) end

local function run_editorconfig(path, show_header)
    if show_header then
        utils.printf("[%s]\n", path)
    end
    local props, names = ec.parse(path, flags.f, flags.b)
    for _, k in ipairs(names) do
        utils.printf("%s=%s\n", k, props[k])
    end
end

local show_header = not (#params == 1)
for _, path in ipairs(params) do
    run_editorconfig(path, show_header)
end
