#include <sys/ioctl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define COLS 40
#define ROWS 40

#define C_BLACK 16
#define C_WHITE 15
#define delay 1000000 / 30

uint16_t w = 0; // screen width and height (in characters)
uint16_t h = 0;
struct winsize win;

uint8_t grid[ROWS][COLS] = {0};

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
    printf("\e2J"); // clear screen
}

void init_grid() {
    for (uint8_t j = 0; j < ROWS; j++) {
        for (uint8_t i = 0; i < COLS; i++) {
            grid[j][i] = rand() % 20;
        }
    }
}

void init() {
    printf("\e[?25l"); // hide cursor
    init_grid();
}

void render_grid() {
    // TODO: instead of default green -> blue -> black, make it go:
    //  white -> yellow -> red -> blue -> black for real fire-y look.

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
            // make squarer, double res pixels
            printf("\u2580"); // Upper Half Block "▀"
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
            // Average 4 pixels to do the fire effect
            //   *    <- for each pixel,
            //  123      avg the pixels underneath.
            //   4
            grid[j][i] =
                (get_cell(i - 1, j + 1) +
                get_cell(i, j + 1) +
                get_cell(i + 1, j + 1) +
                get_cell(i, j + 2)) / 4;
        }
    }

    // Add new "embers" to bottom row
    for (uint8_t i = 0; i < COLS; i++) {
        grid[ROWS - 1][i] = rand() % 65;
    }
}

void render_text(int t) {
    set_bg(C_BLACK);
    uint32_t col = t / 5;
    set_fg(col);
    cursor_to(w/2-7, h/2+2);
    printf("Mr\e[CSpeaker\e[C2024");
}

// Starry starry background
void bg_fill() {
    set_bg(C_BLACK);
    for (int j = 0; j <= h; j++) {
        for (int i = 0; i <= w; i++) {
            cursor_to(i, j);
            if (rand() % 50 == 0) {
                // Star
                set_fg((rand() % 20) + 232);
                printf(".");
            } else {
                // Empty
                printf(" ");
            }
        }
    }

    // Bottom row
    set_fg(21);
    cursor_to(w / 2 - 19, h / 2 + 11);
    printf("▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔");

}

void done(int signum) {
    printf("\e[?25h"); // show cursor
    printf("\e[0m"); // reset fg/bg

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

    uint32_t t = 0;

    while(1){
        // Update
        t++;
        update_grid();

        // Render
        render_grid();
        render_text(t);

        fflush(stdout);
        usleep(delay);
    };
    done(0);
    return 0;
}
