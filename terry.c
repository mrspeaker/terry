#include <sys/ioctl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "ansi_keys.h"

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define SCR_TW 20
#define SCR_TH 16
#define COLS SCR_TW * 4
#define ROWS SCR_TH * 4

#define C_BLACK 16
#define C_WHITE 15
#define delay 1000000 / 30

uint16_t scr_w = 0;
uint16_t scr_h = 0;
struct winsize win;

uint8_t grid[ROWS][COLS] = {0};

#define TILE_COLS SCR_TW * 2
#define TILE_ROWS SCR_TH * 2
uint8_t tiles[TILE_ROWS][TILE_COLS] = {0};
bool tiles_ticked[TILE_ROWS][TILE_COLS] = {false};

uint8_t player_x = 0;
uint8_t player_y = 0;
uint8_t cam_x = 0;
uint8_t cam_t = 0;

typedef struct { int8_t x; int8_t y; } dir;
typedef struct { uint8_t x; uint8_t y; } point;

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
    TILE_DIAMOND,
    TILE_DIAMOND_FALLING,
    TILE_EXP_1,
    TILE_EXP_2,
    TILE_EXP_3,
    TILE_EXP_4,
    TILE_FIREFLY_U,
    TILE_FIREFLY_D,
    TILE_FIREFLY_L,
    TILE_FIREFLY_R,
    TILE_AMOEBA,
    TILE__LEN
} tile_type;

const tile_deets tiledefs[TILE__LEN] = {
    [TILE_EMPTY] = { false, false, true },
    [TILE_BEDROCK] = { true, false, false },
    [TILE_PLAYER] = { false, true, true },
    [TILE_ROCK] = { true, false, true },
    [TILE_ROCK_FALLING] = { false, false, true },
    [TILE_SAND] = { true, false, true },
    [TILE_DIAMOND] = { true, false, true },
    [TILE_DIAMOND_FALLING] = { false, false, true },
    [TILE_EXP_1] = { false, false, false },
    [TILE_EXP_2] = { false, false, false },
    [TILE_EXP_3] = { false, false, false },
    [TILE_EXP_4] = { false, false, false },
    [TILE_FIREFLY_U] = { false, true, true },
    [TILE_FIREFLY_D] = { false, true, true },
    [TILE_FIREFLY_L] = { false, true, true },
    [TILE_FIREFLY_R] = { false, true, true },
    [TILE_AMOEBA] = { false, false, false },
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
}

void init() {
    init_ansi_keys(true);
    esc("?25l"); // hide cursor
    init_grid();
}

void render_grid() {
    for (uint8_t j = 0; j < ROWS - 1; j+=2) {
        cursor_to(scr_w / 2 - (COLS / 2), scr_h / 2 - (ROWS/4) + j / 2 + 1);
        for (uint8_t i = 0; i < COLS; i++) {
            uint8_t top = grid[j][i];
            uint8_t bottom = grid[j + 1][i];

            set_fg(top);
            set_bg(bottom);

            if (top == bottom) {
                if (top == 238) {
                    // sand
                    set_fg(top+1);
                    // ▙ ░ ▒ ▓
                    printf("▚");
                } else {
                    printf(" ");
                }
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

void update_grid(bool flash) {
    uint8_t y1 = min(SCR_TH, max(0, player_y - (SCR_TH / 2)));
    uint8_t y2 = y1 + SCR_TH;

    uint8_t x1 = min(SCR_TW, max(0, player_x - (SCR_TW / 2)));
    uint8_t x2 = x1 + SCR_TW;

    for (uint8_t y = y1; y < y2; y++) {
        for (uint8_t x = x1; x < x2; x++) {
            tile_type t = tiles[y][x];
            for (uint8_t j = 0; j < 4; j++) {
                for (uint8_t i = 0; i < 4; i++) {
                    uint8_t *cur = &(grid[(y - y1) * 4 + j][(x - x1) * 4 + i]);
                    if (flash) {
                        *cur = 51;
                        continue;
                    }

                    switch (t) {
                    case TILE_EMPTY: *cur = C_BLACK; break;
                    case TILE_ROCK:
                    case TILE_ROCK_FALLING:
                        *cur = i && !j ? 245: 244;
                        break;
                    case TILE_DIAMOND:
                    case TILE_DIAMOND_FALLING:
                        *cur = i && !j ? 43: 44 + (rand() % 5);
                        break;
                    case TILE_SAND: *cur = 238; break;
                    case TILE_FIREFLY_U:
                        *cur = 172 + (rand() % 5);
                        if (i == 0 && j == 0) { *cur = 250; }
                        break;
                    case TILE_FIREFLY_D:
                        *cur = 172 + (rand() % 5);
                        if (i == 1 && j == 1) { *cur = 250; }
                        break;
                    case TILE_FIREFLY_L:
                        *cur = 172 + (rand() % 5);
                        if (i == 0 && j == 1) { *cur = 250; }
                        break;
                    case TILE_FIREFLY_R:
                        *cur = 172 + (rand() % 5);
                        if (i == 1 && j == 0) { *cur = 250; }
                        break;
                    case TILE_PLAYER:
                        *cur = 226 + (rand() % 5);
                        break;
                    case TILE_AMOEBA:
                        *cur = 17 + (rand() % 5);
                        break;
                    default:
                        *cur = rand()%(232-196)+197;
                        break;
                    }
                }
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
    tiles_ticked[y][x] = true;

    if (t == TILE_PLAYER) {
        player_x = x;
        player_y = y;
    }
}

void reset_level() {
    for (uint8_t y = 0; y < TILE_ROWS; y++) {
        for (uint8_t x = 0; x < TILE_COLS; x++) {
            if (x % 5 == 2 && y % 5 == 2) {
                set_tile(x, y, TILE_DIAMOND);
                continue;
            }
            uint8_t r = rand() % 100;

            if (r < 80) {
                set_tile(x, y, TILE_SAND);
                continue;
            }
            if (r < 90) {
                set_tile(x, y, TILE_ROCK);
                continue;
            }
            if (r < 94) {
                set_tile(x, y, TILE_FIREFLY_L);
                continue;
            }
            if (r < 99) {
                set_tile(x, y, TILE_AMOEBA);
                continue;
            }
            set_tile(x, y, TILE_EMPTY);
        }
    }
    set_tile(5, 5, TILE_PLAYER);
}

bool is_empty(uint8_t x, uint8_t y) {
    return get_tile(x, y) == TILE_EMPTY;
}
bool is_round(uint8_t x, uint8_t y) {
    return tiledefs[get_tile(x, y)].round;
}

void explode(uint8_t x, uint8_t y) {
    tile_type t = get_tile(x, y);
    set_tile(x, y, TILE_EXP_1);

    for (int8_t i = -1; i <= 1; i++) {
        for (int8_t j = -1; j <= 1; j++) {
            t = get_tile(x + i, y + j);
            tile_deets td = tiledefs[t];
            if (td.explodable) {
                explode(x + i, y + j);
            } else if (td.digable) {
                set_tile(x + i, y + j, TILE_EXP_1);
            }
        }
    }
}

void update_tile_rock(uint8_t i, uint8_t j, tile_type fall) {
    tile_type d = get_tile(i, j + 1);
    tile_deets td_d = tiledefs[d];

    if (d == TILE_EMPTY) {
        set_tile(i, j, fall);//TILE_ROCK_FALLING);
        // Roll to the left
    } else if (td_d.round &&
               is_empty(i - 1, j) &&
               is_empty(i - 1, j + 1)) {
        set_tile(i, j, TILE_EMPTY);
        set_tile(i - 1, j, fall);//TILE_ROCK_FALLING);
        // Roll to the right
    } else if (td_d.round &&
               is_empty(i + 1, j) &&
               is_empty(i + 1, j + 1)) {
        set_tile(i, j, TILE_EMPTY);
        set_tile(i + 1, j, fall);//TILE_ROCK_FALLING);
    }
}

void update_tile_rock_falling(uint8_t i, uint8_t j, tile_type rest, tile_type fall) {
    tile_type d = get_tile(i, j + 1);
    tile_deets td_d = tiledefs[d];

    // Straight down
    if (d == TILE_EMPTY) {
        set_tile(i, j, TILE_EMPTY);
        set_tile(i, j + 1, fall);//TILE_ROCK_FALLING);
      // explode things
    } else if (td_d.explodable) {
        //...
        explode(i, j + 1);

        // Roll to the left
    } else if (td_d.round &&
               is_empty(i - 1, j) &&
               is_empty(i - 1, j + 1)) {
        set_tile(i, j, TILE_EMPTY);
        set_tile(i - 1, j, fall);//TILE_ROCK_FALLING);
        // Roll to the right
    } else if (td_d.round &&
               is_empty(i + 1, j) &&
               is_empty(i + 1, j + 1)) {
        set_tile(i, j, TILE_EMPTY);
        set_tile(i + 1, j, fall);//TILE_ROCK_FALLING);
    } else {
        set_tile(i, j, rest);//TILE_ROCK);
    }

}

void move_tile(uint8_t x, uint8_t y, dir d, tile_type t) {
    set_tile(x, y, TILE_EMPTY);
    set_tile(x + d.x, y + d.y, t);
}

void push_rock(uint8_t x, uint8_t y, int8_t dx) {
    tile_type t = get_tile(x + dx * 2, y);
    if (t == TILE_EMPTY && rand() % 2 == 0) {
        set_tile(x + dx * 2, y, TILE_ROCK);
        set_tile(x + dx, y, TILE_PLAYER);
        set_tile(x, y, TILE_EMPTY);
    }
}

bool update_player(uint8_t x, uint8_t y, int8_t dx, int8_t dy) {
    tile_type t = get_tile(x + dx, y + dy);
    if (t == TILE_EMPTY || t == TILE_SAND) {
        set_tile(x, y, TILE_EMPTY);
        set_tile(x + dx, y + dy, TILE_PLAYER);
    } else if (t == TILE_DIAMOND) {
        set_tile(x, y, TILE_EMPTY);
        set_tile(x + dx, y + dy, TILE_PLAYER);
        return true;
    } else if (dx != 0 && t == TILE_ROCK) {
        push_rock(x, y, dx);
    }
    return false;
}

dir rotate_left(int8_t dx, int8_t dy) {
    if (dy == -1) return (dir){ -1, 0 };
    if (dx == -1) return (dir){ 0, 1 };
    if (dy == 1) return (dir){ 1, 0 };
    return (dir){ 0, -1 };
}

dir rotate_right(int8_t dx, int8_t dy) {
    if (dy == -1) return (dir){ 1, 0 };
    if (dx == 1) return (dir){ 0, 1 };
    if (dy == 1) return (dir){ -1, 0 };
    return (dir){ 0, -1 };
}

tile_type get_firefly(dir d) {
    if (d.y == -1) return TILE_FIREFLY_U;
    if (d.x == 1) return TILE_FIREFLY_R;
    if (d.y == 1) return TILE_FIREFLY_D;
    return TILE_FIREFLY_L;
}

void update_firefly(uint8_t x, uint8_t y, int8_t dx, int8_t dy) {

    dir d = { dx, dy };

    // if touching player - explode
    if (get_tile(x, y - 1) == TILE_PLAYER ||
        get_tile(x, y + 1) == TILE_PLAYER ||
        get_tile(x - 1, y) == TILE_PLAYER ||
        get_tile(x + 1, y) == TILE_PLAYER) {
        explode(x, y);
        return;
    }

    // Try rotate left
    dir rotL = rotate_left(dx, dy);
    if (get_tile(x + rotL.x, y + rotL.y) == TILE_EMPTY) {
        move_tile(x, y, rotL, get_firefly(rotL));
        return;
    }

    // Try go straight
    if (get_tile(x + dx, y + dy) == TILE_EMPTY) {
        move_tile(x, y, d, get_firefly(d));
        return;
    }

    // rotate right
    set_tile(x, y, get_firefly(rotate_right(dx, dy)));
}

void reset_ticked() {
    memset(tiles_ticked, false, TILE_COLS * TILE_ROWS);
}

bool tick_grid(int8_t dx, int8_t dy) {
    bool flash = false;
    reset_ticked();
    for (int8_t j = TILE_ROWS-1; j >= 0; j--) {
        for (uint8_t i = 0; i < TILE_COLS; i++) {
            // Only process each cell once per tick
            if (tiles_ticked[j][i]) continue;

            uint8_t t = get_tile(i, j);
            if (t == TILE_EMPTY || t == TILE_BEDROCK || t == TILE_SAND) continue;

            switch (t) {
            case TILE_ROCK:
                update_tile_rock(i, j, TILE_ROCK_FALLING);
                break;
            case TILE_ROCK_FALLING:
                update_tile_rock_falling(i, j, TILE_ROCK, TILE_ROCK_FALLING);
                break;
            case TILE_DIAMOND:
                update_tile_rock(i, j, TILE_DIAMOND_FALLING);
                break;
            case TILE_DIAMOND_FALLING:
                update_tile_rock_falling(i, j, TILE_DIAMOND, TILE_DIAMOND_FALLING);
                break;
            case TILE_PLAYER:
                if (update_player(i, j, dx, dy)) {
                    flash = true;
                };
                break;
            case TILE_EXP_1: set_tile(i, j, TILE_EXP_2); break;
            case TILE_EXP_2: set_tile(i, j, TILE_EXP_3); break;
            case TILE_EXP_3: set_tile(i, j, TILE_EXP_4); break;
            case TILE_EXP_4: set_tile(i, j, TILE_EMPTY); break;
            case TILE_FIREFLY_U: update_firefly(i, j, 0, -1); break;
            case TILE_FIREFLY_D: update_firefly(i, j, 0, 1); break;
            case TILE_FIREFLY_L: update_firefly(i, j, -1, 0); break;
            case TILE_FIREFLY_R: update_firefly(i, j, 1, 0); break;
            default:
                break;
            }
        }
    }
    return flash;
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

    int8_t dx = 0;
    int8_t dy = 0;

    uint32_t t = 0;
    int8_t flash = 0;

    reset_level();

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
            key_unpress(' ', keys);
            reset_level();
        }
        if (dx != 0) dy = 0;

        // Update
        t++;

        if (t % 5 == 0) {
            if (tick_grid(dx, dy)) {
                flash = 2;
            }
        }
        update_grid(flash > 0);
        if (--flash < 0) flash = 0;

        // Render
        render_grid();

        fflush(stdout);
        usleep(delay);
    };
    done(0);
    return 0;
}
