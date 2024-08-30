#ifndef ANSI_PARSE_H
#define ANSI_PARSE_H

#include <ctype.h>
#include <stdbool.h>

typedef struct {
    enum {
        ANSI_ESC,
        ANSI_BRACKET,
        ANSI_BYTE,
        ANSI_MODIFIER,
        ANSI_EVENT,
    } state;
    int key_code;
    int key_modifier;
    int key_event;
} ansi_state;

typedef struct {
    bool done;
    int key_code;
    int modifier;
    int key_event;
} ansi_res;

int ansi_special(char key);
ansi_state ansi_init(void);
ansi_res ansi_step(ansi_state *state, char c);

#endif // ANSI_PARSE_H
