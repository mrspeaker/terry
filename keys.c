#include <sys/ioctl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "./ansi_keys.h"

#define C_BLACK 16
#define C_WHITE 15
#define delay 1000000 / 30

uint16_t w = 0;
uint16_t h = 0;
struct winsize win;

void esc(char* str) {
    printf("\e[%s", str);
}

void cursor_to(uint16_t x, uint16_t y) {
    printf("\e[%d;%dH", y, x);
}

void set_bg(uint8_t col) {
    printf("\e[48;5;%dm", col);
}

void set_fg(uint8_t col) {
    printf("\e[38;5;%dm", col);
}

void cls() {
    set_bg(C_BLACK);
    set_fg(C_WHITE);
    esc("2J"); // clear screen
}

void init() {
    init_tty(true);
    esc("?25l"); // hide cursor
}

void done(int signum) {
    init_tty(false);
    esc("?25h"); // show cursor
    esc("0m"); // reset fg/bg

    cursor_to(0, 0);
    exit(signum);
};

void resize() {
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);
    w = win.ws_col;
    h = win.ws_row;
}

/// debug print buffer full line
void print_chars(ansi_keys* keys) {
    for (size_t i = 0; i < keys->buf_size; i++) {
        char c = keys->buf[i];
        if (c == 0) {
            printf("_");
            continue;
        }
        if (c == 0x1b) {
            printf("^");
            continue;
        }
        if (isprint(c))
            printf("%c", c);
        else
            printf("<%d>", c);
    }
    printf("<");
}

/// debug print is_down keys
void print_held_keys(ansi_keys* keys) {
    int down = 0;
    cursor_to(w - keys->size, 0);
    printf("           ");
    for (size_t i = 0; i < keys->size; i++) {
        if (keys->keys[i].key_code > 0) {
            cursor_to(w - down++, 0);
            printf("%c", keys->keys[i].key_code);
        }
    }
}

int main() {
    signal(SIGINT, done);
    signal(SIGWINCH, resize);

    cls();
    init();
    resize();

    ansi_keys *keys = make_keys();

    bool running = true;
    int t = 0;
    set_bg(C_BLACK);

    while(running){
        // Input
        if (update_keys_from_ansi_seq(keys)) {
            set_fg(((t++ / h) % 14) + 1);
            cursor_to(0, t % h);
            print_chars(keys);
        }

        print_held_keys(keys);

        // Use key info... q to quit
        if (is_pressed('q', keys)) {
            running = false;
        }
        // Check up arrow
        if (is_down(ansi_special('A'), keys)) {
            running = false;
        }

        fflush(stdout);
        usleep(delay);
    };

    free_keys(keys);
    done(0);
    return 0;
}
