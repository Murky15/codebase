// tcc -I..\ main.c third_party/xml/src/xml.c third_party/zip/src/zip.c -run -- "Text Selector.ggb"

//- @note: Headers
#include <stdio.h>
#include "third_party/xml/src/xml.h"
#include "third_party/zip/src/zip.h"
#include <base/include.h>
#include <os/include.h>

// @note: Source
#include <base/include.c>
#include <os/include.c>

int
main (int argc, char **argv) {
    if (argc != 2) {
        puts("Please enter the name of the GeoGebra file you wish to parse!\n");
        return 1;
    }
    
    // @note: Open .ggb archive & extract xml
    String8 ggb_xml = {0};
    
    struct zip_t *ggb_zip = zip_open(argv[1], 0, 'r');
    int xml_found  = zip_entry_open(ggb_zip, "geogebra.xml");
    if (xml_found == 0)  {
        zip_entry_read(ggb_zip, &(void*)ggb_xml.str, &ggb_xml.len);
        zip_entry_close(ggb_zip);
    }
    zip_close(ggb_zip);
    
    
    return 0;
}