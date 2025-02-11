#define ENABLE_ASSERT 1

//- @note: Headers
#include <stdio.h>
#include <zip/src/zip.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
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
    if (ggb_zip) {
        int xml_found = zip_entry_open(ggb_zip, "geogebra.xml");
        if (xml_found == 0)  {
            zip_entry_read(ggb_zip, &(void*)ggb_xml_raw.str, &ggb_xml_raw.len);
            //printf("%.*s", str8_expand(ggb_xml_raw));
            xmlDocPtr doc = xmlReadMemory(ggb_xml_raw.str, ggb_xml_raw.len, "noname.xml", NULL, 0);
            if (doc) {
                xmlNodePtr cursor = xmlDocGetRootElement(doc);
                assert(cursor);
            
                
            
                xmlFreeDoc(doc);
            } else {
                fprintf(stderr, "Failed to parse GeoGebra XML!\n");
            }
            zip_entry_close(ggb_zip);
        } 
        zip_close(ggb_zip);
    } else {
        fprintf(stderr, "Unable to find GeoGebra ZIP!\n");
    } 
    
    return 0;
}