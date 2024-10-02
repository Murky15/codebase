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
There are a variety of ways one can install the LLVM toolchain on their system.\
If you are interested in **actually using** the LLVM API and toolchain then **do not** download any pre-packaged releases published on the llvm-project github.\
These distributions only include binaries for common tools like clang, rather than the full API.\
Instead, we are going to clone the repository like so:\
`git clone --depth 1 https://github.com/llvm/llvm-project.git llvm-source`
> This `--depth 1` denotes a [shallow clone](git-scm.com/docs/git-clone#Documentation/git-clone.txt---depthltdepthgt), its purpose is to save storage and speed up checkout time.\
> I am also naming the folder `llvm-source` instead of `llvm-project` to make it easier to understand what this folder actually is

To save time for future updates we will also ignore the `user` branch.\
`git config --add remote.origin.fetch '^refs/heads/users/*'
git config --add remote.origin.fetch '^refs/heads/revert-*'`

What we just installed is the source for the complete LLVM toolchain.\
This includes the compiler infrastructure tools and API, as well as the source for all the other LLVM projects we know and love (like Clang or LLD).\
Now for the tricky part...\

### Building
> **Prerequisites**
> * cmake >= 3.20.0
> * A valid cmake [generator](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html) (more on this later)
> * A C++ compiler (MSVC on Windows, g++ on Linux, etc)

Now that we have the LLVM source installed on our system, we need to make another directory to store our outputted binaries.\
I like keeping this directory seperate from the source dir to avoid confusion when I inevitably add these tools to my `PATH` variable.\
`mkdir llvm-build`
> The pre-built Windows installer for the LLVM toolchain that you would have downloaded if you went to the "releases" page installs the LLVM binaries to `C:\Program Files\LLVM` by default.\
> I **highly** recommend you to not make your build directory here because it requires admin permissions to access, and the path has whitespace in the name.\
> Both of these things become a monsterous pain when trying to interface with LLVM on the command line.\

Now let's cd into the source and get to work.\
`cd llvm-source`

Next we need to choose a cmake generator to compile the code, to view a list run `cmake -G` with no arguments.\
I recommend choosing a build system that supports parallel building. If you are unsure which to pick, use `Ninja`.\
Let's go over some other options\
* `-DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld"` - Marks additional subprojects for compilation other than base llvm. In this case,\
my project is now set to build: llvm, clang, clant-tools-extra, and lld.\
* `-DCMAKE_INSTALL_PREFIX="W:\llvm-build"` - Tells cmake where we want to install our binaries to. Set this to the absolute path of the `llvm-build` folder we created earlier.\
* `-DCMAKE_BUILD_TYPE=Release` - Sets optimization level for builds; release mode is best suited for users of LLVM and Clang. **Debug mode is used for developers of the LLVM project**.\

These are really all the options we need to care about; all together our command is:\
`cmake -S llvm -B build -G Ninja -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld" -DCMAKE_INSTALL_PREFIX="W:\llvm-build" -DCMAKE_BUILD_TYPE=Release`

If you made it this far without errors, congrats, that was the first hardest part.\
The second hardest is compiling:\
`cmake --build build -j 18`
> The `-j` option we pass here is the number of parallel jobs we want to run during compilation. I recommend your CPU thread count + 2 (This is the default on Ninja)\

Feel free to go grab a coffee or something now. This will take a while.\
After this completes run:\
`cmake --install build`
to install our compiled files into `llvm-build`

To verify that everything installed correctly, let's go into our `llvm-build` directory and try running a command.\
`cd W:\llvm-build\bin`
`llvm-config --help`
If you see output, congratulations! You've successfuly installed LLVM on your system. Feel free to add this directory to your `PATH` now.\
If you encountered any errors along the way please check the official documentation at: <https://llvm.org/docs/GettingStarted.html>\

## Intro to LLVM IR

### Background

Now that we've successfuly built LLVM, we can take a deep dive into what LLVM *actually is*.\
First let's recap the code generation pipeline. We'll use C to make things simple.\
We have our **source file** which contains the code we write. The compiler's ultimate job is to take our **.c** file(s) and output compiled [object file(s)](https://en.wikipedia.org/wiki/Object_file).\
In short, object files contain the machine code (bytecode), metadata, relocation instructions, and other program symbols generated by the compiler per **translation unit** (source file).\
Most programs are constructed from a variety of object files, it is the responsibility of the [linker](https://en.wikipedia.org/wiki/Linker_(computing)) to "patch" these symbols together\
along with any system libraries they might refer to to create a complete, executable program or application.\

But how do we **actually get** from point A to point B? In other words, after the compiler [front end](https://en.wikipedia.org/wiki/Compiler#Front_end) generates an internal map of the C code (known as an Abstract Syntax Tree),\
how do we transform this arbitrary data structure into machine code?\

This is where LLVM comes in. After the C compiler parses the textual code, it transcribes it into the **Intermediate Representation** langauge developed by LLVM.\
The goal of Intermediate Representation (or IR) is to serve as a language & platform independent\
"middle man" that arbitrary languages can transcribe into to facilitate easy transformation operations (optimizations, etc.) and generation of object code,\
effectively serving as the [middle and back end of the compiler](https://en.wikipedia.org/wiki/Compiler#Middle_end).\
> This is exactly what the Clang C compiler does! Assuming you built\
> Clang in the previous step you can actually see the LLVM IR code Clang generates for a particular **source file**\
> `clang some_file.c -emit-llvm`

### The Basics
LLVM IR can be represented in 2 main formats\
1. Machine-readable Bitcode files (.bc)\
2. Human-readable IR files (.ll)\

The vast and volatile landscape of LLVM IR is far outside the scope of this simple tutorial,\
to gain a comprehensive understanding for the basics of how LLVM IR is structured\
and how to handwrite your own .ll files I **strongly** recommend watching [this talk](https://www.youtube.com/watch?v=m8G_S5LwlTo) before continuing.\

LLVM additionally exposes a C++ API for building **in-memory** representations of LLVM IR for modifying these constructs in code.\
This is helpful when, for example, you are creating your own compiler for a new language.\

### The Goal
Now that we have a basic understanding of LLVM, what it is, what it does, and why it's important; we can build our LLVM "Hello World" example program.\
To satisfy our "Hello World" condition we will print the first 14 numbers of the fibonacci sequence by calling into 2 different fibonacci functions,\
 one generated by LLVM IR in handwritten .ll form and one using the LLVM API.\

First let's open `main.cpp`. This is a simple C++ program which calls an externally defined function, `fib` in a loop 14 times and prints each result.\
Now open up `fibonacci.cpp`. This file defines a `fib` function in C++ to serve as a "control" to ensure everything is working properly while also acting as a model for our IR code.\
To get started, lets compile and link these two C++ files and verify the result.\
`clang main.cpp fibonacci.cpp -o control.exe`
`control.exe`
`> 0 1 1 2 3 5 8 13 21 34 55 89 144 233`
> Remember to *really* think about what the compiler is doing here. We have `main.cpp` and `fibonacci.cpp`.\
> When we ask the compiler to compile both these files, it generates `main.obj` and `fibonacci.obj`.\
> Then the **linker** patches these two obj files together to satisfy the external dependency on `fib` referenced in `main.cpp`

## Handwritten IR
It's time to finally get our hands dirty and dig into some real IR. Let's open up `handwritten_fibonacci.ll`.\
> If you can split-screen `handwritten_fibonacci.ll` and `fibonacci.cpp`, it will help you gain a better conceptual understanding of\
> what the IR is doing because it is just a transcribed version of `fibonacci.cpp`
This is handwritten IR that I wrote to replicate `fibonacci.cpp` in IR.\
> Pro tip: You can use `clang -emit-llvm fibonacci.cpp` to see how the compiler would've generated IR for the `fib` function.\
Reading IR is very similar to platform-independent assembly. The file is commented to explain basic syntactical and semantic behavior,\
but for a comprehensive list of all instructions and their options you should look [here](https://llvm.org/docs/LangRef.html)\

### Compiling
Let's recall how we compiled our first "control" program. We fed both source files to the compiler which transformed them into object files which inevitably got linked together.\
In principle, what we want to do now is no different except now we are trying to transform our llvm IR into an object file directly, instead of starting with a c source file.\
To do this we must utilize a tool we installed when we built LLVM known as `llc`, the llvm system compiler.\
First let's ensure we properly installed the tool.\
`llc -help`
If you do not see output, revisit [the installing stage](#installing-and-building-llvm)\
Otherwise, we can compile this IR into an object file by running:\
`llc handwritten_fibonacci.ll -filetype=obj`
Then we can tell Clang to compile our `main.cpp` source and link it with `handwritten_fibonacci.obj` like so\
`clang main.cpp handwritten_fibonacci.obj -o handwritten.exe`
`handwritten.exe`
`> 0 1 1 2 3 5 8 13 21 34 55 89 144 233`

## Generated IR
This is without a doubt the hardest part of this whole ordeal.\
Soon you will witness for yourself why everyone thinks LLVM is such a pain to work with.\
Remember how petite `handwritten_fibonacci.ll` was? Now open generate_fibonacci.cpp.\
The generated output of this program is set to mimic `handwritten_fibonacci.ll`.\
Don't click off this page just yet, it's not as scary as it looks.\
This file does 3 things:\
	1. Initializes the LLVM API
	2. Defines the `fib` function in IR using the LLVM API
	3. Outputs a fully functional object file we can use to compile our `main` program.

Let's go through each section in more depth. First we see the **wall** of include files LLVM requires. The LLVM codebase\
is far from well organized with various header files including each other anyway, and there is no rhyme or reason as to what should be included because of how volatile the\
project is; but the general rule of thumb is to just include whatever classes you use. The [doxygen](https://llvm.org/doxygen/) is a somewhat helpful point of reference for this.\

### Initialization

All of the relevant documentation for how I made this program can be found in the [LLVM programmers manual](https://llvm.org/docs/ProgrammersManual.html#helpful-hints-for-common-operations).\
This link takes you directly to the actual useful section dealing the API and bypasses all of the *modern C++* cruft for you. You're welcome.\
> **BEWARE** the programming manual is *not* up to date so remember to periodically check back to the doxygen\
Scrolling down to main, we do a quick version check and then jump right in to initializing LLVM.\
The `LLVMContext` manages all internal references and data for the API and should be created on a per-thread basis to help with multithreading.\
The `Module` effectively represents a source file or **translation unit** in C/.ll code. Next we get a handle to a basic 32-bit signed integer type because it is the only one which our function uses.\

### Function declaration & definition

After, we declare the fib function in IR by decribing the function signature in `FunctionType` and creating an instance of the signature in a `Function`.\
We store both pieces of data in a struct, `Function_Pair` because we will need to reference both when *calling* the function later.\
Defining the function and populating it with instructions revolves around the use of the `IRBuilder` convinience class that makes inserting new IR instructions more pleasant.\
We first create the 3 basic blocks `entry`, `x_le_1`, and `x_gt_1` (as seen in `handwritten_fibonacci.ll`) and then go through, pointing our IRBuilder at which block we want to edit and then inserting instructions.\
> Hopefully you can see how arbitrary programs can be created outside the context of this predetirmined example such as\
> creating new `BasicBlock`s on the fly, or inserting instructions based on program operators, etc.\

### Output to object file

After this step the program takes a little break to print the contents of our `Module` which includes our newly defined `fib` function.\
Finally, we tell our program to transform our `Module` into an **object file**. The way LLVM performs this task is not very well documented, very complicated, and very much outside the scope of this tutorial.\
In short, what we are doing here is initializing all output types so that this works without hassle on your machine no matter what hardware/os you are running.\
Then we select our target, open an iostream to our new `fibonacci.obj` file, and run our output pass through the file stream.\
The majority of the work being done here is performed by the *legacy* LLVM pass manager.\
> Don't ask me why, but the *new* LLVM pass manager is responsible for performing optimization passes and whatnot\
> while the **sole** responsibility of the *legacy* pass manager today is to handle the output pass. For more information, look at the links inside `generate_fibonacci.cpp`

### Compiling
If actually programming against the LLVM API is the hardest part, this is definitely the scariest. If you have a genie lying around somewhere, this would be a good time to use a wish.\ 
Run the `llvm-config` tool from your terminal. This is your new best friend.\
It will give your compiler context about where everything is installed on your system.\
> **Note that the compiler this tool expects is the same compiler you used to build LLVM in the first step.**\
You're probably going to need to play with these flags, but what works for me on Windows 11, MSVC (version 19.38.33134) is:\
`llvm-config --cxxflags --system-libs --libs all`. Next we pass this to our compiler **Remember: this part is specific to you!**\
`cl -MD $llvm_info generate_fibonacci.cpp -Fegen_fib.exe`
> For those who don't know, the -MD flag specifies that the C Runtime Library (CRT) should be linked statically, because this is what LLVM expects.\
Assuming everything compiled without error, let's run `gen_fib.exe` to get our `fibonacci.obj` file.\
> You should now see the IR dumb of our `Module` in the terminal. Check back with `handwritten_fibonacci.ll`, is it identical? What is different? What is the same?\
Now for the moment of truth:\
`clang main.cpp fibonacci.obj -o api_test.exe`
`api_test.exe`
`> 0 1 1 2 3 5 8 13 21 34 55 89 144 233`

This sure has been a journey, hasn't it? Now you're ready to tame the beast on your own. Give yourself a pat on the back. This was definitely far from trivial. Good luck soldier.