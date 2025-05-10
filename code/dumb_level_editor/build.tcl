namespace eval dumb_level_editor {
    proc build {{&pinfo} {&tconfig} args} {
        upvar ${&pinfo} pinfo;
        upvar ${&tconfig} tconfig;
        
# this is ugly, project dir should be passed to the build scripts by butler;
        set {project dir} {./code/dumb_level_editor/}
        if {$tconfig(compiler) == "tcc"} {
            set {compiler opts} "-Icode -I${project dir}/raylib/include ${project dir}level_editor.c ${project dir}raylib/lib/raylib.dll -o build/dumb_level_editor.exe";
        } else {
            puts "This compiler is not supported";
            return 1;
        }
        
        
		try {
    		set results [exec {*}"$tconfig(compiler) ${compiler opts}"]
                set status 0
		} trap CHILDSTATUS {results options} {
			puts $results
                set status [lindex [dict get $options -errorcode] 2]
		}
        
        
        return $status
    }
}