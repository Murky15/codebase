/*
10/6/24 - Gianni Bernardi & Giuseppe Arena
Simple quiz based around string matching
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

const char *actions[] = {"sleep", "leave", "stop drop and roll", "call 911", "splash water on fire", "roast marshmellows"};
const int actions_count = (sizeof(actions)/sizeof(actions[0]));

int
main (void) {
    puts("You wake up to the smell of smoke and realize that your house is burning!\n");
    puts("You only have time for 1 action before your house explodes.");
    puts("You can: ");
    for (int i = 0; i < actions_count; ++i) {
        printf("%s, ", actions[i]);
    }
    puts("\n");
    
    rsize_t max_size = 50;
    char *buff = malloc(max_size);
    bool match = false;
    while (!match) {
        char *input = gets_s(buff, max_size);
        for (int i = 0; i < actions_count; ++i) {
            if (strcmp(buff, actions[i]) == 0) {
                match = true;
                switch (i) {
                    case 0: puts("Unfortunately this was not a dream and the fire consumed your house, good luck explaining this one to your insurance company."); break;
                    case 1: puts("You made it out just in time to see your house collapse. Guess you're back to living with your parents."); break;
                    case 2: puts("You aren't on fire dummy, your house is. You get tangled up in your bedsheets and trapped in your burning house."); break;
                    case 3: puts("The fire department is able to put the fire out just in time. Cause of fire? you left the oven on."); break;
                    case 4: puts("Turns out it was actually an electrical fire and you just made it worse."); break;
                    case 5: puts("You take a moment to relax and enjoy the little things in life before your final moments. If this is how it ends, you wouldn't go out any other way."); break;
                }
                puts("\n");
                break;
            }
        }
        if (!match)
            puts("Huh\n");
    }
    puts("Thanks for playing!");
    free(buff);
    
    return 0;
}