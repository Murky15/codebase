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
    String8 ggb_xml_raw = {0};
    
    struct zip_t *ggb_zip = zip_open(argv[1], 0, 'r');
    int xml_found = zip_entry_open(ggb_zip, "geogebra.xml");
    if (xml_found == 0)  {
        zip_entry_read(ggb_zip, &(void*)ggb_xml_raw.str, &ggb_xml_raw.len);
        printf("%.*s", str8_expand(ggb_xml_raw));
        
        struct xml_document *ggb_xml = xml_parse_document(ggb_xml_raw.str, ggb_xml_raw.len);
        //struct xml_node *root = xml_document_root(ggb_xml);
        //fprintf(stderr, "%s\n", xml_easy_content(root));
        
        
        xml_document_free(ggb_xml, false);
        zip_entry_close(ggb_zip);
    }
    zip_close(ggb_zip);
    
    
    return 0;
}