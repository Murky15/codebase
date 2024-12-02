namespace eval dumb {
	proc build {{&pinfo} {&tconfig} args} {
		upvar ${&pinfo}   pinfo
		upvar ${&tconfig} tconfig
		
		if {$pinfo(os) != "win32"} {
			puts "dumb - This platform is unsupported" 
			return 1
		}
		
		if {$tconfig(compiler) != "msvc"} {
			puts "dumb - Let's just use msvc for now..."
		}

		# This doesn't show cmd output if compile is successful, but do we really need to see that?
		try {
    		set results [exec {*}{cl -nologo -FC -J -I./code -EHa- -GR- -Od -Zi -WX -W3 -wd4146 -wd4005 -wd4101 -DDEBUG=1 -DENABLE_ASSERT=1 ./code/dumb/main.c User32.lib Gdi32.lib -Fe./build/dumb.exe}]
   	 	set status 0
		} trap CHILDSTATUS {results options} {
			puts $results
    		set status [lindex [dict get $options -errorcode] 2]
		}
	
		return $status
	}
}