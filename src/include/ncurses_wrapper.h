//
// Created by Eliomar Alejandro Rodriguez Ferrer on 22/12/25.
//

#ifndef LLMROUTING_NCURSES_WRAPPER_H
#define LLMROUTING_NCURSES_WRAPPER_H

#include <ncurses.h>
#include <string.h>
#include <ctype.h>

int
print_smart(
    WINDOW *win,
    int start_y,
    int start_x,
    const char *text
);

#endif //LLMROUTING_NCURSES_WRAPPER_H