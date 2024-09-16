# Getting Started With LLVM

> NOTE: At the time of writing, the latest LLVM version is 20.0.0. If your version is different problems may arise!

This directory serves to be a friendlier introduction to the LLVM compiler infrastructure.
It will cover:
1. [How to install and build the LLVM API and tools](#installing-and-building-llvm)
2. [What LLVM intermediate representation is and how we can view LLVM IR output from the compiler](#intro-to-llvm-ir)
3. [How to handwrite a trivial function in LLVM IR and call it from C++ code](#handwritten-ir)
4. [How to generate a trivial function using the LLVM API and call it from C++ code](#generated-ir)

## Installing and Building LLVM

### Installing
There are a variety of ways one can install the LLVM toolchain on their system.
If you are interested in **actually using** the LLVM API and toolchain then **do not** download any pre-packaged releases published on the llvm-project github.
These distributions only include binaries for common tools like clang, rather than the full API.
Instead, we are going to clone the repository like so:
`git clone --depth 1 https://github.com/llvm/llvm-project.git llvm-source`
> This `--depth 1` denotes a [shallow clone](git-scm.com/docs/git-clone#Documentation/git-clone.txt---depthltdepthgt), its purpose is to save storage and speed up checkout time.
> I am also naming the folder `llvm-source` instead of `llvm-project` to make it easier to understand what this folder actually is

To save time for future updates we will also ignore the `user` branch.
`git config --add remote.origin.fetch '^refs/heads/users/*'
git config --add remote.origin.fetch '^refs/heads/revert-*'`

What we just installed is the source for the complete LLVM toolchain.
This includes the compiler infrastructure tools and API, as well as the source for all the other LLVM projects we know and love (like Clang or LLD).\
Now for the tricky part...

### Building
> **Prerequisites**
> * cmake >= 3.20.0
> * A valid cmake [generator](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html) (more on this later)
> * A C++ compiler (MSVC on Windows, g++ on Linux, etc)

Now that we have the LLVM source installed on our system, we need to make another directory to store our outputted binaries.
I like keeping this directory seperate from the source dir to avoid confusion when I inevitably add these tools to my `PATH` variable.
`mkdir llvm-build`
> The pre-built Windows installer for the LLVM toolchain that you would have downloaded if you went to the "releases" page installs the LLVM binaries to `C:\Program Files\LLVM` by default.
> I **highly** recommend you to not make your build directory here because it requires admin permissions to access, and the path has whitespace in the name.
> Both of these things become a monsterous pain when trying to interface with LLVM on the command line.

Now let's cd into the source and get to work.
`cd llvm-source`

Next we need to choose a cmake generator to compile the code, to view a list run `cmake -G` with no arguments.
I recommend choosing a build system that supports parallel building. If you are unsure which to pick, use `Ninja`.
Let's go over some other options
* `-DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld"` - Marks additional subprojects for compilation other than base llvm. In this case,
my project is now set to build: llvm, clang, clant-tools-extra, and lld.
* `-DCMAKE_INSTALL_PREFIX="W:\llvm-build"` - Tells cmake where we want to install our binaries to. Set this to the absolute path of the `llvm-build` folder we created earlier.
* `-DCMAKE_BUILD_TYPE=Release` - Sets optimization level for builds; release mode is best suited for users of LLVM and Clang. **Debug mode is used for developers of the LLVM project**.

These are really all the options we need to care about; all together our command is:
`cmake -S llvm -B build -G Ninja -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld" -DCMAKE_INSTALL_PREFIX="W:\llvm-build" -DCMAKE_BUILD_TYPE=Release`

If you made it this far without errors, congrats, that was the first hardest part.
The second hardest is compiling:
`cmake --build build -j 18`
> The `-j` option we pass here is the number of parallel jobs we want to run during compilation. I recommend your CPU thread count + 2 (This is the default on Ninja)

Feel free to go grab a coffee or something now. This will take a while.
After this completes run:
`cmake --install build`

If you encountered any errors along the way please check the official documentation at: <https://llvm.org/docs/GettingStarted.html>

## Intro to LLVM IR

## Handwritten IR

## Generated IR
