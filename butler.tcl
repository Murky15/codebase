package require platform

# This script serves to build my projects and manage my codebase (and feels like a waste of time writing)
# Each project in the codebase has unique build requirements, so why don't we have each project declare their
# own namespace which stores their build proc and metaprogram and whatever, and we call that from here when
# the user types "butler build {project}" and we pass along parameters such as context and compiler and any additional
# options. But maybe we could have some standard "helper routines" in this namespace to help parse like build options
# specified in the source and stuff.
# Maybe i'll even add a gui too :-)

namespace eval butler {
    if 0 {
        Build procedure function signature (in it's project namespace):
                                            proc build {{&pinfo} {&tconfig} {release mode} args} {
                                                ...your code here...
                                            }
                                            
                                            Where
                                            pinfo - Array ref with platform information in pinfo(os) and pinfo(arch)
                                            tconfig - Array ref with information on the requested tconfig(compiler), tconfig(linker), and tconfig(assembler)
                                            release mode - true / false
                                            args - Any project specific build arguments (switches)
                                            *note - Array references must be resolved with upvar, e.g: upvar ${&pinfo} pinfo
                                            
                                            Return: 0 if success, >0 if failure
    }
    
# Supported configs
    set compilers  [list tcc clang msvc gicc]
        set linkers    [list mslink lld ld]
        set assemblers [list masm nasm fasm]
        
# Query platform information
    set {raw platform} [split [platform::generic] -]
        set pinfo(os) [lindex ${raw platform} 0]
        set pinfo(arch) [lindex ${raw platform} 1]
        unset {raw platform}
    
    proc print_help {} {
        variable {help string} {
            Butler - Tool for managing codebase and project builds
                Gianni Bernardi 11/28/24
                
                Synopsis:
            tclsh butler.tcl [ options ... ]
                
                options:
            build [project] [toolchain config] [release] - [project switches]
        }
        puts ${help string}
    }
    
    proc project_builder {args} {
        if {[llength $args] > 0} {
# Supported configs
            global butler::compilers butler::linkers butler::assemblers butler::pinfo
                set project ""
                set compiler ""
                set linker ""
                set assembler ""
                set {release mode} false
                set {found switches} false
                for {set i 0; set arg [lindex $args 0]} {$i < [llength $args]} {set arg [lindex $args $i]} {
                if {$arg == "-"} {lpop args $i; set {found switches} true; break} ; #Project specific switches
                
                variable search {lsearch -inline}
                if {$compiler == "" && [set compiler [{*}$search $compilers $arg]] != ""} {
                    puts "$compiler compile"
                        if {$compiler != "tcc"} {puts "Warning!! Metaprogramming not yet supported for this compiler!"}
                } elseif {$linker == "" && [set linker [{*}$search $linkers $arg]] != ""} {
                    puts "$linker link"
                } elseif {$assembler == "" && [set assembler [{*}$search $assemblers $arg]] != ""} {
                    puts "$assembler set"
                } elseif {$arg == "release"} {
                    puts "Release mode"
                        set {release mode} true
                } else {
# Interpret as project name
                    set project [expr {$project == ""?$arg:[puts "Error! Duplicate project assignment!: $arg"; exit 1]}]
                }
                
                lpop args $i
            }
            if {$compiler == ""}  {puts "No compiler set, assuming [set compiler tcc]"}
            if {$linker == ""}    {puts "No linker set, assuming $compiler default"; set linker "default"}    ;# This is kinda strange, because technically there is no garuntee that we will *actually*
            if {$assembler == ""} {puts "No assembler set, assuming $compiler default"; set assembler "default"} ;# use the 'default' assembler/linker b/c we don't try and determine what that is here, we just pass the empty string to the project build command. @fix?
            if {${release mode} == false} {puts "Debug mode"}
            puts "Building $project..."
                
# Pack parameters
            array set tconfig "
                compiler  $compiler
                linker    $linker
                assembler $assembler
                "
                set switches [expr {${found switches} == true ? $args:[list]}]
                
# Load project build script & execute
            source "./code/$project/build.tcl" ;# Potential issue sourcing project namespace within butler namespace?
            set status [${project}::build pinfo tconfig {release mode} {*}$switches]
                if {$status == 0} {
                puts "Build finished successfully!"
                    exit $status
            } else {
                puts "Build failed!"
                    exit $status
            }
            
        } else {
            print_help
        }
    }
    
# Unpack command line arguments
    if {[llength $::argv] > 0} {
        switch -nocase [lindex $::argv 0] {
            build   { project_builder {*}[lremove $::argv 0] }
            default { print_help }
        }
    } else {
        print_help
    }
}

