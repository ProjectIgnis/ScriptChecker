# Project Ignis card script check suite

This is a reimplementation of the previous script checker program (https://github.com/ProjectIgnis/Checker)

This is run as a part of the CI for our [card script collection](https://github.com/ProjectIgnis/CardScripts) and [greenlight script collection](https://github.com/ProjectIgnis/Greenlight). Currently, it only performs a basic syntax check, but more features may be added in the future. 

This also serves as a demo for how to use the [redesigned EDOPro ocgcore C API](https://github.com/edo9300/ygopro-core). 

## Script syntax checker

Usage:
```
script_syntax_check [directories...]
```

All specified directories (default cwd if none provided) are searched for card scripts, including to one subdirectory level. The first script found with the name is always used, so do not specify multiple directories with scripts with the same name.

A basic Lua syntax check is done on scripts on pushes and pull requests. It loads `constant.lua` and `utility.lua` into ocgcore. Then it searches through one subfolder level for files of the form `cX.lua`, where `X` is an integer, loading them into the core as a dummy card with the same passcode. Three-digit passcodes and 151000000 (Action Duel script) are skipped as a workaround.

This catches basic Lua syntax errors like missing `end` statements and runtime errors in `initial_effect` (or a lack of `initial_effect` in a card script).

### Caveats

Does not catch runtime errors in other functions declared within a script unless they are called by `initial_effect` of some other script. This is not a static analyzer and it will not catch incorrect parameters for calls outside of `initial_effect` or any other runtime error.
