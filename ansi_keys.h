#ifndef ANSI_KEYS_H
#define ANSI_KEYS_H

#include <termios.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "./ansi_parse.h"
#include "./keys.h"

// Set/restore non-canonical and no-echo in tty and set keyboar protocol
void init_tty(bool enable) {
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

/// Parse ansi data into Key info
void parse_input(char *buf, size_t size, key_ev *keys, size_t k_size) {
    ansi_state st = ansi_init();
    for (size_t i = 0; i < size; i++) {
        ansi_res res = ansi_step(&st, buf[i]);
        if (res.done) {
            set_key(res.key_code, res.key_event, keys, k_size);
        }
    }
}

/// Read ansi keyboard data from stdin into a buffer
size_t read_keys(char *buf, size_t size, key_ev *keys, size_t k_size) {
    if (kbhit()) {
        memset(buf, 0, size);
        size_t n = read(STDIN_FILENO, buf, size);
        parse_input(buf, n, keys, k_size);
        return n;
    }
    return 0;
}

#endif // ANSI_KEYS_H
