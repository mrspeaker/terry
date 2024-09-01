#ifndef ANSI_KEYS_H
#define ANSI_KEYS_H

#include <termios.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>

#include "./ansi_parse.h"

#define MAX_KEYS 10
#define BUF_SIZE 80

typedef struct {
    int key_code;
    bool is_down;
    bool pressed;
} key_ev;

typedef struct {
    key_ev *keys;
    size_t size;
    char *buf;
    size_t buf_size;
} ansi_keys;

// Set/restore non-canonical and no-echo in tty and set keyboar protocol
void init_ansi_keys(bool enable) {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty); // current state
    if (enable) {
        tty.c_lflag &= ~ICANON; // non-canonical (raw)
        tty.c_lflag &= ~ECHO;   // no echo
        tty.c_cc[VMIN] = 1;     // min bytes needed to return from `read`.

        // Emit code for key events (as specified in Kitty Keyboard Protocol)
        // https://sw.kovidgoyal.net/kitty/keyboard-protocol/
        printf("\e[>10;u"); // 08 (all keys as code) + 02 (key events)
    }  else {
        tty.c_lflag |= ICANON;
        tty.c_lflag |= ECHO;
        printf("\e[<u"); // disable key events (Kitty Keyboard Protocol)
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

// Simulates kbhit
//
// select() allows a program to monitor multiple file descriptors,
// waiting until one or more of the file descriptors become "ready".
// Non blocking.
bool kbhit() {
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0; // wait 0 time
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds) != 0;
}

ansi_keys *make_ansi_keys() {
    ansi_keys *k = (ansi_keys*) malloc(sizeof(ansi_keys));
    k->size = MAX_KEYS;
    k->buf_size = BUF_SIZE;
    k->keys = (key_ev*) malloc(sizeof(key_ev) * k->size);
    k->buf = (char *) malloc(sizeof(char) * k->buf_size);

    for (size_t i = 0; i < k->size; i++) {
        key_ev key = { .key_code = 0 };
        k->keys[i] = key;
    }

    return k;
}

void free_ansi_keys(ansi_keys* kb) {
    if (kb != NULL) {
        free(kb->keys);
        free(kb->buf);
        free(kb);
    }
}

key_ev *find_ansi_key(int key_code, ansi_keys *keys) {
    for (size_t i = 0; i < keys->size; i++) {
        if (keys->keys[i].key_code == key_code) {
            return &keys->keys[i];
        }
    }
    return NULL;
}

void set_ansi_key(int key_code, int key_event, ansi_keys *keys) {
    key_ev *k = find_ansi_key(key_code, keys);
    if (!k) {
        k = find_ansi_key(0, keys); // find free key
        if (k == NULL) return;
        k->key_code = key_code;
    }
    if (key_event == 3) {
        k->key_code = 0;
        k->is_down = false;
        k->pressed = false;
    } else {
        k->is_down = true;
        k->pressed = key_event == 1;
    }
}

bool key_pressed(int key_code, ansi_keys *keys) {
    key_ev *k = find_ansi_key(key_code, keys);
    return k != NULL && k->pressed;
}

bool key_down(int key_code, ansi_keys *keys) {
    key_ev *k = find_ansi_key(key_code, keys);
    return k != NULL && k->is_down;
}

/// Parse ansi data into Key info
void parse_ansi_seq(size_t size, ansi_keys *keys) {
    ansi_state st = ansi_init();
    for (size_t i = 0; i < size; i++) {
        ansi_res res = ansi_step(&st, keys->buf[i]);
        if (res.done) {
            set_ansi_key(res.key_code, res.key_event, keys);
        }
    }
}

/// Update key state from stdin ansi sequences, into a buffer and parse as keys
size_t update_ansi_keys(ansi_keys *keys) {
    if (!kbhit()) {
        return 0; // No key pressed
    }

    memset(keys->buf, 0, keys->buf_size); // clear buffer
    size_t n = read(STDIN_FILENO, keys->buf, keys->buf_size); // read bytes
    parse_ansi_seq(n, keys); // parse it
    return n;
}

bool check_ansi_keys_enabled(ansi_keys *keys) {
    printf("\e[?u"); // query if flags were set
    fflush(stdout);
    usleep(1000000 / 30); // wait a sec

    if (!kbhit()) {
        return false; // No response
    }
    size_t n = read(STDIN_FILENO, keys->buf, keys->buf_size); // read bytes
    ansi_state st = ansi_init();
    for (size_t i = 0; i < n; i++) {
        ansi_res res = ansi_step(&st, keys->buf[i]);
        if (res.done && res.is_query) {
            return true;
        }
    }
    return false;
}

#endif // ANSI_KEYS_H
