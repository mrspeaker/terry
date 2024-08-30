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
#define MAX_KEYS 10
#define BUF_SIZE 80

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

typedef struct {
    char code;
    bool is_down;
} key_event;

void print_chars(char *c, int len) {
    for (int i = 0; i < len; i++) {
        if (c[i] == 0) {
            printf("_");
            continue;
        }
        if (c[i] == 0x1b) {
            printf("^");
            continue;
        }
        if (isprint(c[i]))
            printf("%c", c[i]);
        else
            printf("<%d>", c[i]);
    }
    printf("<");
}


int main() {
    signal(SIGINT, done);
    signal(SIGWINCH, resize);

    cls();
    init();
    resize();

    char buf[BUF_SIZE]={0};
    key_ev keys[MAX_KEYS];
    init_keys(keys, MAX_KEYS);

    bool running = true;
    int t = 0;
    set_bg(C_BLACK);

    while(running){
        // Input
        size_t n = read_keys_from_ansi_seq(buf, BUF_SIZE, keys, MAX_KEYS);
        if (n > 0) {
            set_fg(((t++ / h) % 14) + 1);
            cursor_to(0, t % h);
            print_chars(buf, BUF_SIZE);
        }

        // Print held keys
        int down = 0;
        cursor_to(w - MAX_KEYS, 0);
        printf("           ");
        for (int i = 0; i < MAX_KEYS; i++) {
            if (keys[i].key_code > 0) {
                cursor_to(w - down++, 0);
                printf("%c", keys[i].key_code);
            }
        }

        if (is_pressed('q', keys, MAX_KEYS)) {
            running = false;
        }
        if (is_down(ansi_special('A'), keys, MAX_KEYS)) {
            //up arrow
            running = false;
        }

        fflush(stdout);
        usleep(delay);
    };
    done(0);
    return 0;
}
