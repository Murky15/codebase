#!/bin/bash
# Yes windows DOES have a builtin bash interpereter. Shocking, right?

# Default config
accepted_compilers=(msvc clang tcc)
accepted_compile_modes=(debug release)
accepted_assemblers=(nasm ml64)
additional_build_options=()

compiler="${accepted_compilers[0]}"
compile_mode="${accepted_compile_modes[0]}"
assembler="${accepted_assemblers[0]}"

build_targets=()

# Unpack command line arguments
for arg in $@; do
  parsed=0

  # Get compiler
  for c in "${accepted_compilers[@]}"; do
    if [[ "$arg" == "$c" ]]; then
      compiler="$c"
      parsed=1
    fi
  done

  # Get compile mode
  for m in "${accepted_compile_modes[@]}"; do
    if [[ "$arg" == "$m" ]]; then
      compile_mode="$m"
      parsed=1
    fi
  done

  # Get assembler
  for a in "${accepted_assemblers[@]}"; do
    if [[ "$arg" == "$a" ]]; then
      assembler="$a"
      parsed=1
    fi
  done

  if [[ "$parsed" != 1 ]]; then
    # Treat option as build target
    build_targets+=("$arg")
  fi
  parsed=0
done

# Print current configuration
echo "Compiler: $compiler"
echo "Compile mode: $compile_mode"
echo "Assembler: $assembler"

# Setup options
if [[ "$compiler" != "msvc" ]]; then
  gcc_like=1
fi

msvc_common="-I../code/ -I../local/ -nologo -FC -Z7 -wd4005"
msvc_debug="cl -Od -DBUILD_DEBUG=1 $msvc_common"
msvc_release="cl -O2 -DBUILD_DEBUG=0 $msvc_common"
msvc_link="-link"
msvc_out="-Fe"

gcc_like_common="-I../code/ -I../local/ -Wall -Wno-macro-redefined -Wno-unused-function -Wno-unused-variable"
gcc_like_debug="$compiler -g -O0 -DBUILD_DEBUG=1$ $gcc_like_common"
gcc_like_release="$compiler -g -O2 -DBUILD_DEBUG=0$ $gcc_like_common"
gcc_like_link=""
gcc_like_out="-o "

# Compile lines
if [[ "$compiler" == "msvc" ]]; then
  compile="msvc_$compile_mode"
  link="msvc_link"
  out="msvc_out"
elif [[ "$gcc_like" == 1 ]]; then
  compile="gcc_like_$compile_mode"
  link="gcc_like_link"
  out="gcc_like_out"
fi

# @hack
if [[ "$OSTYPE" == "msys" ]]; then
  exe_extension=".exe"
fi

# Prep build dirs
mkdir -p build local

# @todo: Metagen

# Build targets
# There are two ways we can do this, pull a Ryan Fleury and go down each project one by one and build it, or make a small
# assumption about directory structure and save some lines of code. I will try the latter and revert to the former if it is better
pushd build >> /dev/null
for target in "${build_targets[@]}"; do
  eval "${!compile} ${!out}$target$exe_extension ../code/$target/main.c ${!link}"
done
popd >> /dev/null