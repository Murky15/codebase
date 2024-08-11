#!/bin/bash

# Constants
codebase_root_dir="W:"

# Parse command line
for arg in "$@"; do declare "$arg"=1; done

# @todo: Yadda yadda yadda add whatever option handling we want here
if [[ "$release" == 0 ]]; then
  debug=1
  echo "[Debug mode]"
else
  echo "[Release mode]"
fi

# Right now let's just worry about supporting MSVC
echo "[MSVC compile]"
cl_debug="-Od -Zi -WX"
cl_release="-O2"
cl_common="-nologo -std:c11 -FC -J -I"$codebase_root_dir"/code -C -EHa- -GR- -W4" # -C might be helpful for our metaprogram
cl_link=""
cl_out="-Fe"

jai_compile="jai -debugger -output_path $codebase_root_dir/build -quiet" # Most things will be handled by metaprogram

# @todo: Set per-build settings like 'only-compile', 'assemble', etc

# Setup requested config
if [[ "$debug" == 1 ]]; then
  compile="$cl_debug $cl_common"
else
  compile="$cl_release $cl_common"
fi
link="$cl_link"
out="$cl_out"

# Prep build dirs
mkdir -p build

# @todo: C metagen

# Build
pushd build >> /dev/null
built=0
if [[ "$debaser" == 1 ]]; then built=1 && eval "$jai_compile $codebase_root_dir"/code/debaser/debaser.jai || exit 1; fi

if [[ "$built" == 0 ]]; then echo "Unrecognized target!"; fi
popd >> /dev/null