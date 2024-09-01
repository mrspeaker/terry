#include <sys/ioctl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "ansi_keys.h"

#define COLS 40
#define ROWS 40

#define C_BLACK 16
#define C_WHITE 15
#define delay 1000000 / 30

uint16_t w = 0;
uint16_t h = 0;
struct winsize win;

uint8_t grid[ROWS][COLS] = {0};

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

// only on terminals with 24bit color support
void set_bg_rgb(uint8_t r, uint8_t g, uint8_t b) {
    printf("\e[48;2;%d;%d;%dm", r, g, b);
}

void cls() {
    set_bg(C_BLACK);
    set_fg(C_WHITE);
    esc("2J"); // clear screen
}

// Use the half-block char to make squarer, double res pixels
void print_half_block() {
    printf("\u2580"); // Upper Half Block "▀"
}

void init_grid() {
    for (uint8_t j = 0; j < ROWS; j++) {
        for (uint8_t i = 0; i < COLS; i++) {
            grid[j][i] = rand() % 20;
        }
    }
}

void init() {
    init_ansi_keys(true);
    esc("?25l"); // hide cursor
    init_grid();
}

void render_grid() {
    for (uint8_t j = 0; j < ROWS - 2; j+=2) {
        cursor_to(w / 2 - 20, h / 2 - 8 + j / 2);
        for (uint8_t i = 0; i < COLS; i++) {
            uint8_t top = grid[j][i];
            uint8_t bottom = grid[j + 1][i];

            // Filter out a bunch o colors. (Gradient is indexes 16-51)
            if (top < 16 || top > 51) top = C_BLACK;
            if (bottom < 16 || bottom  > 51) bottom = C_BLACK;

            set_fg(top);
            set_bg(bottom);

            if (top == bottom) {
                printf(" ");
                continue;
            }
            print_half_block();
        }
    }
}

uint8_t get_cell(uint8_t x, uint8_t y) {
    if (y >= ROWS || y < 0) return 0;
    if (x >= COLS || x < 0) return 0;
    return grid[y][x];
}

void update_grid() {
    for (uint8_t j = 0; j < ROWS; j++) {
        for (uint8_t i = 0; i < COLS; i++) {
        }
    }
}

void bg_fill() {
    set_bg(C_BLACK);
    for (int j = 0; j <= h; j++) {
        for (int i = 0; i <= w; i++) {
            cursor_to(i, j);
            if (rand() % 30 == 0) {
                // Star
                set_fg((rand() % 20) + 232);
                printf(".");
            } else {
                // Empty
                printf(" ");
            }
        }
    }
}

void done(int signum) {
    esc("?25h"); // show cursor
    esc("0m"); // reset fg/bg
    init_ansi_keys(false);

    cursor_to(0, 0);
    exit(signum);
};

void resize() {
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);
    w = win.ws_col;
    h = win.ws_row;
    bg_fill();
}

int main() {
    srand(time(0));

    signal(SIGINT, done);
    signal(SIGWINCH, resize);

    cls();
    init();
    resize();

    ansi_keys *keys = make_ansi_keys();

    int16_t x = w / 2;
    int16_t y = h / 2;
    int8_t dx = 0;
    int8_t dy = 0;

    uint32_t t = 0;

    bool running = true;
    while(running){
        dx = 0; // stop moving
        dy = 0;

        update_ansi_keys(keys);
        if (key_pressed('q', keys)) {
            running = false;
        }
        if (key_down('w', keys)) {
            dy = -1;
        }
        if (key_down('s', keys)) {
            dy = 1;
        }
        if (key_down('a', keys)) {
            dx = -1;
        }
        if (key_down('d', keys)) {
            dx = 1;
        }
        if (dx != 0) dy = 0;


        // Update
        t++;
        update_grid();

        // Update cursor
        x += dx;
        if (x < 0) x = w;
        if (x > w) x = 0;

        y += dy;
        if (y < 0) y = h;
        if (y > h) y = 0;

        // Render
        render_grid();

        cursor_to(x, y);
        set_bg((t % (255 - 51))+51);
        set_fg(((t + 1) % (255 - 51))+51);
        print_half_block();

        fflush(stdout);
        usleep(delay);
    };
    done(0);
    return 0;
}
