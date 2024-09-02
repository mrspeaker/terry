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

uint16_t scr_w = 0;
uint16_t scr_h = 0;
struct winsize win;

uint8_t grid[ROWS][COLS] = {0};

#define TILE_COLS COLS / 2
#define TILE_ROWS ROWS / 2
uint8_t tiles[TILE_ROWS][TILE_COLS] = {0};

typedef struct {
    bool round;
    bool explodable;
    bool digable;
} tile_deets;

typedef enum {
    TILE_EMPTY,
    TILE_BEDROCK,
    TILE_PLAYER,
    TILE_ROCK,
    TILE_ROCK_FALLING,
    TILE_SAND,
    TILE__LEN
} tile_type;

const tile_deets tiledefs[TILE__LEN] = {
    [TILE_EMPTY] = { false, false, true },
    [TILE_BEDROCK] = { true, false, false },
    [TILE_PLAYER] = { false, true, true },
    [TILE_ROCK] = { true, false, true },
    [TILE_ROCK_FALLING] = { false, false, true },
    [TILE_SAND] = { true, false, true },
};

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
            grid[j][i] = 0;
        }
    }
    for (uint8_t j = 0; j < TILE_ROWS; j++) {
        for (uint8_t i = 0; i < TILE_COLS; i++) {
            if (rand() % 10 <= 5) {
                tiles[j][i] = TILE_SAND;
                continue;
            }
            if (rand() % 10 < 5) {
                tiles[j][i] = TILE_ROCK;
                continue;
            }
            tiles[j][i] = TILE_EMPTY;
        }
    }
}

void init() {
    init_ansi_keys(true);
    esc("?25l"); // hide cursor
    init_grid();
}

void render_grid() {
    for (uint8_t j = 0; j < ROWS - 1; j+=2) {
        cursor_to(scr_w / 2 - 20, scr_h / 2 - 8 + j / 2);
        for (uint8_t i = 0; i < COLS; i++) {
            uint8_t top = grid[j][i];
            uint8_t bottom = grid[j + 1][i];

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

void update_grid(int8_t x, int8_t y, int8_t col) {
    for (uint8_t j = 0; j < ROWS; j++) {
        for (uint8_t i = 0; i < COLS; i++) {
            grid[j][i] = tiles[j / 2][i / 2];
            if (i == x && j == y) {
                grid[j][i] = col;
            }
        }
    }
}

uint8_t get_tile(uint8_t x, uint8_t y) {
    if (y >= TILE_ROWS || y < 0) return TILE_BEDROCK;
    if (x >= TILE_COLS || x < 0) return TILE_BEDROCK;
    return tiles[y][x];
}

void set_tile(uint8_t x, uint8_t y, tile_type t) {
    if (y >= TILE_ROWS || y < 0) return;
    if (x >= TILE_COLS || x < 0) return;
    tiles[y][x] = t;
}

bool is_empty(uint8_t x, uint8_t y) {
    return get_tile(x, y) == TILE_EMPTY;
}
bool is_round(uint8_t x, uint8_t y) {
    return tiledefs[get_tile(x, y)].round;
}

void tick_grid() {
    for (int8_t j = TILE_ROWS-1; j >= 0; j--) {
        for (uint8_t i = 0; i < TILE_COLS; i++) {
            uint8_t t = get_tile(i, j);

            if (t == TILE_EMPTY || t == TILE_BEDROCK || t == TILE_SAND) continue;

            switch (t) {

            case TILE_ROCK:
                if (is_empty(i, j + 1)) {
                    set_tile(i, j, TILE_ROCK_FALLING);
                // Roll to the left
                } else if (is_round(i, j + 1) &&
                           is_empty(i - 1, j) &&
                           is_empty(i - 1, j + 1)) {
                    set_tile(i, j, TILE_EMPTY);
                    set_tile(i - 1, j, TILE_ROCK_FALLING);
                // Roll to the right
                } else if (is_round(i, j + 1) &&
                           is_empty(i + 1, j) &&
                           is_empty(i + 1, j + 1)) {
                    set_tile(i, j, TILE_EMPTY);
                    set_tile(i + 1, j, TILE_ROCK_FALLING);
                }
                break;

            case TILE_ROCK_FALLING:
                // Straight down
                if (is_empty(i, j + 1)) {
                    set_tile(i, j, TILE_EMPTY);
                    set_tile(i, j + 1, TILE_ROCK_FALLING);
                // Roll to the left
                } else if (is_round(i, j + 1) &&
                           is_empty(i - 1, j) &&
                           is_empty(i - 1, j + 1)) {
                    set_tile(i, j, TILE_EMPTY);
                    set_tile(i - 1, j, TILE_ROCK_FALLING);
                // Roll to the right
                } else if (is_round(i, j + 1) &&
                           is_empty(i + 1, j) &&
                           is_empty(i + 1, j + 1)) {
                    set_tile(i, j, TILE_EMPTY);
                    set_tile(i + 1, j, TILE_ROCK_FALLING);
                } else {
                    set_tile(i, j, TILE_ROCK);
                }
                break;
            }
        }
    }
}

void bg_fill() {
    set_bg(C_BLACK);
    for (int j = 0; j <= scr_h; j++) {
        for (int i = 0; i <= scr_w; i++) {
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
    scr_w = win.ws_col;
    scr_h = win.ws_row;
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

    int16_t x = ROWS / 2;
    int16_t y = COLS / 2;
    int8_t dx = 0;
    int8_t dy = 0;
    int8_t col = 40;

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
        if (key_pressed(' ', keys)) {
            col = ((col + 1) % 10) + 30;
        }
        if (dx != 0) dy = 0;


        // Update
        t++;

        // Update cursor
        
        x += dx;
        if (x < 0) x = COLS - 1;
        if (x > COLS - 1) x = 0;

        y += dy;
        if (y < 0) y = ROWS - 1;
        if (y > ROWS - 1) y = 0;

        if (t % 8 == 0) {
            tick_grid();
            uint8_t tile = get_tile(x/2, y/2);
            if (tile == TILE_SAND) {
                tiles[y/2][x/2] = TILE_EMPTY;
            }
        }
        update_grid(x, y, col);



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
