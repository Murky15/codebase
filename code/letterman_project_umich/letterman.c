#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "base/include.h"
#include "getopt_windows.h"

#include "base/include.c"
#include "getopt_windows.c"

/*
  Have a cache of "expanded" words.
*/

typedef u32 Letterman_Capabilities;
enum {
  LETTERMAN_CAN_CHANGE = (1 << 0),
  LETTERMAN_CAN_INSERT = (1 << 1),
  LETTERMAN_CAN_SWAP   = (1 << 2),
};

// typedef struct

typedef struct Dictionary {
  s64 count;

} Dictionary;

int
main (int argc, char **argv) {
  Arena *permanent_arena = arena_alloc();

  // Name, Argument, Flag, Short name
  local_persist struct option command_options[] = {
    {"help", no_argument, 0, 'h'},
    {"queue", no_argument, 0, 'q'},
    {"stack", no_argument, 0, 's'},
    {"begin", required_argument, 0, 'b'},
    {"end", required_argument, 0, 'e'},
    {"output", required_argument, 0, 'o'},
    {"change", no_argument, 0, 'c'},
    {"length", no_argument, 0, 'l'},
    {"swap", no_argument, 0, 'p'},
    {0}
  };
  int option_index;

  int use_queue = -1;
  bool output_words = true;
  String8 begin = {0}, end = {0};
  Letterman_Capabilities flags = 0;
  while (true) {
    int c = getopt_long(argc, argv, "hqsb:e:o:clp", command_options, &option_index);
    if (c == -1) break;

    switch (c) {
      case 'h': {
        printf("My implementation of the umich letterman project that I'm trying to crack as fast as possible.\n");
        exit(0);
      } break;
      case 's': fallthrough
      case 'q': {
        if (use_queue == -1) {
          use_queue = (c == 'q');
        } else {
          fprintf(stderr, "Error: you must specify either stack or queue, but not both!\n");
          exit(1);
        }
      } break;
      case 'b': begin = str8_cstring(optarg); break;
      case 'e': end = str8_cstring(optarg); break;
      case 'o': {
        if (optarg[0] == 'M') {
          output_words = false;
        } else {
          fprintf(stderr, "Error: Unrecognized output format!\n");
          exit(1);
        }
      } break;
      case 'c': flags |= LETTERMAN_CAN_CHANGE; break;
      case 'l': flags |= LETTERMAN_CAN_INSERT; break;
      case 'p': flags |= LETTERMAN_CAN_SWAP; break;
    }
  }

  if (use_queue == -1) {
    fprintf(stderr, "Error: you must specify either stack or queue, but not both!\n");
    exit(1);
  }

  if (begin.str == 0 || end.str == 0) {
    fprintf(stderr, "Error: you must specify both a beginning and end word!\n");
    exit(1);
  }

  if (flags == 0) {
    fprintf(stderr, "Guess letterman is taking the day off today (no capabilities specified).\n");
    exit(1);
  }

  Letterman_Capabilities capability_hazard_mask = LETTERMAN_CAN_CHANGE | LETTERMAN_CAN_SWAP;
  if ((flags & capability_hazard_mask) && begin.len != end.len) {
    fprintf(stderr, "Error: this is an impossible situation!\n");
    exit(1);
  }

  fseek(stdin, 0, SEEK_END);
  size_t dictionary_text_size = ftell(stdin);
  fseek(stdin, 0, SEEK_SET);
  u8 *file_buffer = arena_pushn(permanent_arena, u8, dictionary_text_size);
  fread(file_buffer, 1, dictionary_text_size, stdin);
  String8 dictionary_text = str8(file_buffer, dictionary_text_size);
  String8List dictionary_lines = str8_split(permanent_arena, dictionary_text, 1, "\n");

  return 0;
}