namespace eval genesis {
    proc build {{&pinfo} {&tconfig} args} {
        upvar ${&pinfo} pinfo
            upvar ${&tconfig} tconfig
    }
}