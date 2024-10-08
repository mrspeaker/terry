#ifndef ANSI_PARSE_H
#define ANSI_PARSE_H

#include <ctype.h>
#include <stdbool.h>

#define ESC '\x1B'

typedef struct {
    enum {
        ANSI_ESC,
        ANSI_BRACKET,
        ANSI_BYTE,
        ANSI_MODIFIER,
        ANSI_EVENT,
        ANSI_QUERY
    } state;
    int key_code;
    int key_modifier;
    int key_event;
} ansi_state;

typedef struct {
    bool done;
    bool is_query;
    int key_code;
    int modifier;
    int key_event;
} ansi_res;

ansi_state ansi_init(void) {
    ansi_state s = {
        .state = ANSI_ESC
    };
    return s;
}

void ansi_done(ansi_state *state, ansi_res *res) {
    res->key_code = state->key_code;
    res->modifier = state->key_modifier;
    res->key_event = state->key_event;
    res->is_query = state->state == ANSI_QUERY;

    state->key_code = 0;
    state->key_modifier = 0;
    state->key_event = 0;
    state->state = ANSI_ESC;

    res->done = true;
}

int ansi_special(char key) {
    return (0xE0 << 8) | key;
}

ansi_res ansi_step(ansi_state *state, char c) {
    ansi_res r = {
        .done = false
    };

    switch (state->state) {
    case ANSI_ESC:
        if (c == ESC) {
            state->state = ANSI_BRACKET;
        }
        break;

    case ANSI_BRACKET:
        state->state = ANSI_BYTE;
        state->key_code = 0;
        break;

    case ANSI_BYTE:
        if (isdigit(c)) {
            int num = c - '0';
            state->key_code = (state->key_code * 10) + num;
        } else if (c == '?') {
            // query response
            state->key_code = 0;
            state->state = ANSI_QUERY;
        } else if (c >= 'A' && c <= 'D') {
            state->key_code = ansi_special(c);
            state->key_event = 1;
            ansi_done(state, &r);
        } else {
            if (c == ';') {
                state->state = ANSI_MODIFIER;
            } else {
                state->key_event = 1;
                ansi_done(state, &r);
            }
        }
        break;

    case ANSI_MODIFIER:
        // TODO: wrong! modifier can be > 1 digit
        // (because can be ctrl+shift)
        if (isdigit(c)) {
            state->key_modifier = c - '0';
        } else if (c == ':') {
            state->state = ANSI_EVENT;
        } else {
            // err
        }
        break;

    case ANSI_EVENT:
        if (isdigit(c)) {
            state->key_event = c - '0';
        } else if (c >= 'A' && c <= 'D' ) {
            // arrow keys
            state->key_code = ansi_special(c);
            ansi_done(state, &r);
        } else if (c == 'u') {
            ansi_done(state, &r);
        } else {
            // err.
        }
        break;

    case ANSI_QUERY:
        if (isdigit(c)) {
            int num = c - '0';
            state->key_code = (state->key_code * 10) + num;
        } else if (c == 'u') {
            ansi_done(state, &r);
        } else {
            // err
        }
        break;

    default:
        break;
    }

    return r;
}

#endif // ANSI_PARSE_H
