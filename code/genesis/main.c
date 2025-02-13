#define ENABLE_ASSERT 1

//- @note: Headers
#include <stdlib.h>
#include <stdio.h>
#include <zip/src/zip.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <base/include.h>
#include <os/include.h>

// @note: Source
#include <base/include.c>
#include <os/include.c>

#define loop_element_children(c,s) for((c)=(c)->s;(c)!=NULL;(c)=(c)->next)

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
                xmlNodePtr cursor, backup;
                cursor = xmlDocGetRootElement(doc);
                assert(cursor); // Document cannot be NULL
            
                loop_element_children(cursor,xmlChildrenNode) {
                  //printf("%s\n", cursor->name);
                  
                  /*
                  Should grab 
                  Kernel: angleUnit
                  
                  Everything else we need is in the "construction" tag
                  Commands hold element outputs, elements have a type and coords
                  
                  Can elements be created without commands in Geogebra and the XML?
                  */
                  if (!xmlStrcmp(cursor->name, (const xmlChar*)"construction")) {
                    printf("Parsing project: %s\n", xmlGetProp(cursor, "title"));
                    // @todo: Pull this for loop out into macro
                    loop_element_children(cursor,xmlChildrenNode) {
                      if (!xmlStrcmp(cursor->name, (const xmlChar*)"command")) {
                        printf("Command: %s, ", xmlGetProp(cursor, "name"));
                        
                        backup = cursor;
                        char *input = 0, *output = 0;
                        loop_element_children(cursor,xmlChildrenNode) {
                          if (!xmlStrcmp(cursor->name, (const xmlChar*)"input")) {
                            //input = xmlGetProp(cursor, "a0");
                            xmlAttr *prop = cursor->properties;
                            while (prop) {
                              xmlChar *value = xmlNodeListGetString(doc, prop->children, true);
                              printf("idk: %s: %s\n", prop->name, value);
                              xmlFree(value);
                              prop = prop->next;
                            }  
                          } else if (!xmlStrcmp(cursor->name, (const xmlChar*)"output")) {
                            //output = xmlGetProp(cursor, "a0");
                          } else {
                            continue;
                          }
                        }
                        //printf("input(s): %s, output: %s\n", input, output);
                        cursor = backup;         
                      }
                    }                    
                  }
                }
            
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