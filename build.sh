#!/bin/bash

# Constants
codebase_root_dir="W:"

# Parse command line
for arg in "$@"; do declare "$arg"=1; done

# @todo: Yadda yadda yadda add whatever option handling we want here
if [[ "$release_mode" == "" ]]; then
  debug_mode=1
  echo "[Debug mode]"
else
  echo "[Release mode]"
fi

debug_defines="-DENABLE_ASSERT=1"
if [[ "$msvc" == 1 ]]; then
  echo "[MSVC compile]"
  debug="-Od -Zi -WX $debug_defines"
  release="-O2"
  common="cl -nologo -FC -J -I$codebase_root_dir/code -EHa- -GR- -W3 -wd4146 -wd4005 -wd4101"
  add_lib=""
  link=""
  out="-Fe"
else
  echo "[Clang compile]"
  debug="-O0 -g -Werror -pedantic -Wall -Wno-missing-braces -Wno-newline-eof -Wno-keyword-macro -Wno-macro-redefined -Wno-braced-scalar-init -Wno-unused-function $debug_defines"
  release="-O2 -w"
  common="clang -I$codebase_root_dir/code"
  add_lib="-l"
  link="-Xlinker"
  out="-o"
fi

jai_compile="jai -debugger -output_path $codebase_root_dir/build -quiet" # Most things will be handled by metaprogram

# @todo: Set per-build settings like 'only-compile', 'assemble', etc

# Setup requested config
if [[ "$debug_mode" == 1 ]]; then
  compile="$common $debug"
else
  compile="$common $release"
fi
link="$link"
out="$out"

# Prep build dirs
mkdir -p build

# @todo: C metagen

# Build
pushd build >> /dev/null
built=0
if [[ "$dumb" == 1 ]]; then built=1 && eval "$compile $codebase_root_dir"/code/dumb/main.c "$add_lib User32.lib $add_lib Gdi32.lib $out"dumb.exe || exit 1; fi

if [[ "$llvm_example" == 1 ]]; then built=1 && eval "$compile $codebase_root_dir"/code/llvm_example/main.cpp "$link $out"llvm_example.exe || exit 1; fi

if [[ "$built" == 0 ]]; then echo "Unrecognized target!"; fi
popd >> /dev/null