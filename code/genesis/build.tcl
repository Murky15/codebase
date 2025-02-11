namespace eval genesis {
    proc build {{&pinfo} {&tconfig} args} {
        upvar ${&pinfo} pinfo;
        upvar ${&tconfig} tconfig;
        
# this is ugly, project dir should be passed to the build scripts by butler;
        set {project dir} {./code/genesis/}
        if {$tconfig(compiler) == "tcc"} {
            set {compiler opts} "-Icode -I${project dir}/third_party ${project dir}main.c ${project dir}third_party/zip/src/zip.c ${project dir}third_party/libxml2d.dll -o build/genesis.exe";
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