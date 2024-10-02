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
There are a variety of ways one can install the LLVM toolchain on their system.<br />
If you are interested in **actually using** the LLVM API and toolchain then **do not** download any pre-packaged releases published on the llvm-project github.<br />
These distributions only include binaries for common tools like clang, rather than the full API.<br />
Instead, we are going to clone the repository like so:<br />
`git clone --depth 1 https://github.com/llvm/llvm-project.git llvm-source`<br />
> This `--depth 1` denotes a [shallow clone](git-scm.com/docs/git-clone#Documentation/git-clone.txt---depthltdepthgt), its purpose is to save storage and speed up checkout time.<br />
> I am also naming the folder `llvm-source` instead of `llvm-project` to make it easier to understand what this folder actually is<br />

To save time for future updates we will also ignore the `user` branch.<br />
`git config --add remote.origin.fetch '^refs/heads/users/*'<br />
git config --add remote.origin.fetch '^refs/heads/revert-*'`<br />

What we just installed is the source for the complete LLVM toolchain.<br />
This includes the compiler infrastructure tools and API, as well as the source for all the other LLVM projects we know and love (like Clang or LLD).<br />
Now for the tricky part...<br />

### Building
> **Prerequisites**
> * cmake >= 3.20.0
> * A valid cmake [generator](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html) (more on this later)
> * A C++ compiler (MSVC on Windows, g++ on Linux, etc)

Now that we have the LLVM source installed on our system, we need to make another directory to store our outputted binaries.<br />
I like keeping this directory seperate from the source dir to avoid confusion when I inevitably add these tools to my `PATH` variable.<br />
`mkdir llvm-build`<br />
> The pre-built Windows installer for the LLVM toolchain that you would have downloaded if you went to the "releases" page installs the LLVM binaries to `C:<br />Program Files<br />LLVM` by default.<br />
> I **highly** recommend you to not make your build directory here because it requires admin permissions to access, and the path has whitespace in the name.<br />
> Both of these things become a monsterous pain when trying to interface with LLVM on the command line.<br />

Now let's cd into the source and get to work.<br />
`cd llvm-source`<br />

Next we need to choose a cmake generator to compile the code, to view a list run `cmake -G` with no arguments.<br />
I recommend choosing a build system that supports parallel building. If you are unsure which to pick, use `Ninja`.<br />
Let's go over some other options<br />
* `-DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld"` - Marks additional subprojects for compilation other than base llvm. In this case,<br />
my project is now set to build: llvm, clang, clant-tools-extra, and lld.<br />
* `-DCMAKE_INSTALL_PREFIX="W:<br />llvm-build"` - Tells cmake where we want to install our binaries to. Set this to the absolute path of the `llvm-build` folder we created earlier.<br />
* `-DCMAKE_BUILD_TYPE=Release` - Sets optimization level for builds; release mode is best suited for users of LLVM and Clang. **Debug mode is used for developers of the LLVM project**.<br />

These are really all the options we need to care about; all together our command is:<br />
`cmake -S llvm -B build -G Ninja -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld" -DCMAKE_INSTALL_PREFIX="W:<br />llvm-build" -DCMAKE_BUILD_TYPE=Release`<br />

If you made it this far without errors, congrats, that was the first hardest part.<br />
The second hardest is compiling:<br />
`cmake --build build -j 18`<br />
> The `-j` option we pass here is the number of parallel jobs we want to run during compilation. I recommend your CPU thread count + 2 (This is the default on Ninja)<br />

Feel free to go grab a coffee or something now. This will take a while.<br />
After this completes run:<br />
`cmake --install build`<br />
to install our compiled files into `llvm-build`<br />

To verify that everything installed correctly, let's go into our `llvm-build` directory and try running a command.<br />
`cd W:<br />llvm-build<br />bin`<br />
`llvm-config --help`<br />
If you see output, congratulations! You've successfuly installed LLVM on your system. Feel free to add this directory to your `PATH` now.<br />
If you encountered any errors along the way please check the official documentation at: <https://llvm.org/docs/GettingStarted.html><br />

## Intro to LLVM IR

### Background

Now that we've successfuly built LLVM, we can take a deep dive into what LLVM *actually is*.<br />
First let's recap the code generation pipeline. We'll use C to make things simple.<br />
We have our **source file** which contains the code we write. The compiler's ultimate job is to take our **.c** file(s) and output compiled [object file(s)](https://en.wikipedia.org/wiki/Object_file).<br />
In short, object files contain the machine code (bytecode), metadata, relocation instructions, and other program symbols generated by the compiler per **translation unit** (source file).<br />
Most programs are constructed from a variety of object files, it is the responsibility of the [linker](https://en.wikipedia.org/wiki/Linker_(computing)) to "patch" these symbols together<br />
along with any system libraries they might refer to to create a complete, executable program or application.<br />

But how do we **actually get** from point A to point B? In other words, after the compiler [front end](https://en.wikipedia.org/wiki/Compiler#Front_end) generates an internal map of the C code (known as an Abstract Syntax Tree),<br />
how do we transform this arbitrary data structure into machine code?<br />

This is where LLVM comes in. After the C compiler parses the textual code, it transcribes it into the **Intermediate Representation** langauge developed by LLVM.<br />
The goal of Intermediate Representation (or IR) is to serve as a language & platform independent<br />
"middle man" that arbitrary languages can transcribe into to facilitate easy transformation operations (optimizations, etc.) and generation of object code,<br />
effectively serving as the [middle and back end of the compiler](https://en.wikipedia.org/wiki/Compiler#Middle_end).<br />
> This is exactly what the Clang C compiler does! Assuming you built<br />
> Clang in the previous step you can actually see the LLVM IR code Clang generates for a particular **source file**<br />
> `clang some_file.c -emit-llvm`<br />

### The Basics
LLVM IR can be represented in 2 main formats<br />
1. Machine-readable Bitcode files (.bc)<br />
2. Human-readable IR files (.ll)<br />

The vast and volatile landscape of LLVM IR is far outside the scope of this simple tutorial,<br />
to gain a comprehensive understanding for the basics of how LLVM IR is structured<br />
and how to handwrite your own .ll files I **strongly** recommend watching [this talk](https://www.youtube.com/watch?v=m8G_S5LwlTo) before continuing.<br />

LLVM additionally exposes a C++ API for building **in-memory** representations of LLVM IR for modifying these constructs in code.<br />
This is helpful when, for example, you are creating your own compiler for a new language.<br />

### The Goal
Now that we have a basic understanding of LLVM, what it is, what it does, and why it's important; we can build our LLVM "Hello World" example program.<br />
To satisfy our "Hello World" condition we will print the first 14 numbers of the fibonacci sequence by calling into 2 different fibonacci functions,<br />
 one generated by LLVM IR in handwritten .ll form and one using the LLVM API.<br />

First let's open `main.cpp`. This is a simple C++ program which calls an externally defined function, `fib` in a loop 14 times and prints each result.<br />
Now open up `fibonacci.cpp`. This file defines a `fib` function in C++ to serve as a "control" to ensure everything is working properly while also acting as a model for our IR code.<br />
To get started, lets compile and link these two C++ files and verify the result.<br />
`clang main.cpp fibonacci.cpp -o control.exe`<br />
`control.exe`<br />
`> 0 1 1 2 3 5 8 13 21 34 55 89 144 233`<br />
> Remember to *really* think about what the compiler is doing here. We have `main.cpp` and `fibonacci.cpp`.<br />
> When we ask the compiler to compile both these files, it generates `main.obj` and `fibonacci.obj`.<br />
> Then the **linker** patches these two obj files together to satisfy the external dependency on `fib` referenced in `main.cpp`<br />

## Handwritten IR
It's time to finally get our hands dirty and dig into some real IR. Let's open up `handwritten_fibonacci.ll`.<br />
> If you can split-screen `handwritten_fibonacci.ll` and `fibonacci.cpp`, it will help you gain a better conceptual understanding of<br />
> what the IR is doing because it is just a transcribed version of `fibonacci.cpp`<br />
This is handwritten IR that I wrote to replicate `fibonacci.cpp` in IR.<br />
> Pro tip: You can use `clang -emit-llvm fibonacci.cpp` to see how the compiler would've generated IR for the `fib` function.<br />
Reading IR is very similar to platform-independent assembly. The file is commented to explain basic syntactical and semantic behavior,<br />
but for a comprehensive list of all instructions and their options you should look [here](https://llvm.org/docs/LangRef.html)<br />

### Compiling
Let's recall how we compiled our first "control" program. We fed both source files to the compiler which transformed them into object files which inevitably got linked together.<br />
In principle, what we want to do now is no different except now we are trying to transform our llvm IR into an object file directly, instead of starting with a c source file.<br />
To do this we must utilize a tool we installed when we built LLVM known as `llc`, the llvm system compiler.<br />
First let's ensure we properly installed the tool.<br />
`llc -help`<br />
If you do not see output, revisit [the installing stage](#installing-and-building-llvm)<br />
Otherwise, we can compile this IR into an object file by running:<br />
`llc handwritten_fibonacci.ll -filetype=obj`<br />
Then we can tell Clang to compile our `main.cpp` source and link it with `handwritten_fibonacci.obj` like so<br />
`clang main.cpp handwritten_fibonacci.obj -o handwritten.exe`<br />
`handwritten.exe`<br />
`> 0 1 1 2 3 5 8 13 21 34 55 89 144 233`<br />

## Generated IR
This is without a doubt the hardest part of this whole ordeal.<br />
Soon you will witness for yourself why everyone thinks LLVM is such a pain to work with.<br />
Remember how petite `handwritten_fibonacci.ll` was? Now open generate_fibonacci.cpp.<br />
The generated output of this program is set to mimic `handwritten_fibonacci.ll`.<br />
Don't click off this page just yet, it's not as scary as it looks.<br />
This file does 3 things:<br />
	1. Initializes the LLVM API
	2. Defines the `fib` function in IR using the LLVM API
	3. Outputs a fully functional object file we can use to compile our `main` program.

Let's go through each section in more depth. First we see the **wall** of include files LLVM requires. The LLVM codebase<br />
is far from well organized with various header files including each other anyway, and there is no rhyme or reason as to what should be included because of how volatile the<br />
project is; but the general rule of thumb is to just include whatever classes you use. The [doxygen](https://llvm.org/doxygen/) is a somewhat helpful point of reference for this.<br />

### Initialization

All of the relevant documentation for how I made this program can be found in the [LLVM programmers manual](https://llvm.org/docs/ProgrammersManual.html#helpful-hints-for-common-operations).<br />
This link takes you directly to the actual useful section dealing the API and bypasses all of the *modern C++* cruft for you. You're welcome.<br />
> **BEWARE** the programming manual is *not* up to date so remember to periodically check back to the doxygen<br />
Scrolling down to main, we do a quick version check and then jump right in to initializing LLVM.<br />
The `LLVMContext` manages all internal references and data for the API and should be created on a per-thread basis to help with multithreading.<br />
The `Module` effectively represents a source file or **translation unit** in C/.ll code. Next we get a handle to a basic 32-bit signed integer type because it is the only one which our function uses.<br />

### Function declaration & definition

After, we declare the fib function in IR by decribing the function signature in `FunctionType` and creating an instance of the signature in a `Function`.<br />
We store both pieces of data in a struct, `Function_Pair` because we will need to reference both when *calling* the function later.<br />
Defining the function and populating it with instructions revolves around the use of the `IRBuilder` convinience class that makes inserting new IR instructions more pleasant.<br />
We first create the 3 basic blocks `entry`, `x_le_1`, and `x_gt_1` (as seen in `handwritten_fibonacci.ll`) and then go through, pointing our IRBuilder at which block we want to edit and then inserting instructions.<br />
> Hopefully you can see how arbitrary programs can be created outside the context of this predetirmined example such as<br />
> creating new `BasicBlock`s on the fly, or inserting instructions based on program operators, etc.<br />

### Output to object file

After this step the program takes a little break to print the contents of our `Module` which includes our newly defined `fib` function.<br />
Finally, we tell our program to transform our `Module` into an **object file**. The way LLVM performs this task is not very well documented, very complicated, and very much outside the scope of this tutorial.<br />
In short, what we are doing here is initializing all output types so that this works without hassle on your machine no matter what hardware/os you are running.<br />
Then we select our target, open an iostream to our new `fibonacci.obj` file, and run our output pass through the file stream.<br />
The majority of the work being done here is performed by the *legacy* LLVM pass manager.<br />
> Don't ask me why, but the *new* LLVM pass manager is responsible for performing optimization passes and whatnot<br />
> while the **sole** responsibility of the *legacy* pass manager today is to handle the output pass. For more information, look at the links inside `generate_fibonacci.cpp`<br />

### Compiling
If actually programming against the LLVM API is the hardest part, this is definitely the scariest. If you have a genie lying around somewhere, this would be a good time to use a wish.<br /> 
Run the `llvm-config` tool from your terminal. This is your new best friend.<br />
It will give your compiler context about where everything is installed on your system.<br />
> **Note that the compiler this tool expects is the same compiler you used to build LLVM in the first step.**<br />
You're probably going to need to play with these flags, but what works for me on Windows 11, MSVC (version 19.38.33134) is:<br />
`llvm-config --cxxflags --system-libs --libs all`. Next we pass this to our compiler **Remember: this part is specific to you!**<br />
`cl -MD $llvm_info generate_fibonacci.cpp -Fegen_fib.exe`<br />
> For those who don't know, the -MD flag specifies that the C Runtime Library (CRT) should be linked statically, because this is what LLVM expects.<br />
Assuming everything compiled without error, let's run `gen_fib.exe` to get our `fibonacci.obj` file.<br />
> You should now see the IR dumb of our `Module` in the terminal. Check back with `handwritten_fibonacci.ll`, is it identical? What is different? What is the same?<br />
Now for the moment of truth:<br />
`clang main.cpp fibonacci.obj -o api_test.exe`<br />
`api_test.exe`<br />
`> 0 1 1 2 3 5 8 13 21 34 55 89 144 233`<br />

This sure has been a journey, hasn't it? Now you're ready to tame the beast on your own. Give yourself a pat on the back. This was definitely far from trivial. Good luck soldier.