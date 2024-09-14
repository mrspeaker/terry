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
#define SCR_TH 12

#define px_per_tile 4
#define PIX_W SCR_TW * px_per_tile
#define PIX_H SCR_TH * px_per_tile

#define C_BLACK 16
#define C_WHITE 15
#define C_DARKBLUE 17
#define C_DARKGREEN 22
#define C_MAROON 53
#define C_DARKGREY 237
#define C_MIDGREY 245
#define C_LIGHTGREY 250

#define delay 1000000 / 30

uint16_t scr_w = 0;
uint16_t scr_h = 0;
struct winsize win;

uint8_t pixels[PIX_H][PIX_W] = {0};

#define TILE_COLS ((SCR_TW * 2)+1)
#define TILE_ROWS ((SCR_TH * 2)+1)
uint8_t tiles[TILE_ROWS][TILE_COLS] = {0};
bool tiles_ticked[TILE_ROWS][TILE_COLS] = {false};

uint8_t player_x = 0;
uint8_t player_y = 0;
bool player_right = true;
uint8_t cam_x = 0;
uint8_t cam_t = 0;

typedef struct { int8_t x; int8_t y; } dir;
typedef struct { uint8_t x; uint8_t y; } point;

typedef struct {
    bool round;
    bool explodable;
    bool consumable;
} tile_deets;

typedef enum {
    TILE_EMPTY,
    TILE_BEDROCK,
    TILE_PLAYER,
    TILE_ROCK,
    TILE_ROCK_FALLING,
    TILE_SANDSTONE,
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
    [TILE_SANDSTONE] = { true, false, true },
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

const uint8_t pal[] = {
    [0] = C_BLACK,
    [1] = C_WHITE,
    [2] = C_DARKBLUE,
    [3] = C_MAROON,
    [4] = C_DARKGREEN,
    [5] = C_DARKGREY,
    [6] = C_MIDGREY,
    [7] = C_LIGHTGREY,
    [8] = 51,
    [9] = 43
};

const uint8_t tile_gfx[][16] = {
    [TILE_ROCK] = {
        4,7,7,4,
        6,6,6,7,
        5,6,6,6,
        4,5,5,4
    },
    [TILE_SANDSTONE] = {
        4,6,7,4,
        6,5,6,7,
        5,6,5,6,
        5,5,6,4,
    },
    [TILE_DIAMOND] = {
        4,8,8,4,
        9,8,8,8,
        9,9,8,8,
        4,9,9,4,
    },
};

void done(int signum);

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

void init_pixels() {
    for (uint8_t j = 0; j < PIX_H; j++) {
        for (uint8_t i = 0; i < PIX_W; i++) {
            pixels[j][i] = 0;
        }
    }
}

void init() {
    init_ansi_keys(true);
    esc("?25l"); // hide cursor
    init_pixels();
}

void render_pixels() {
    for (uint8_t j = 0; j < PIX_H - 1; j+=2) {
        cursor_to(scr_w / 2 - (PIX_W / 2), scr_h / 2 - (PIX_H / 4) + j / 2 + 1);
        for (uint8_t i = 0; i < PIX_W; i++) {
            uint8_t top = pixels[j][i];
            uint8_t bottom = pixels[j + 1][i];
            set_fg(top);
            set_bg(bottom);
            print_half_block();
        }
    }
}

uint8_t get_cell(uint8_t x, uint8_t y) {
    if (y >= PIX_H || y < 0) return 0;
    if (x >= PIX_W || x < 0) return 0;
    return pixels[y][x];
}

bool is_firefly(tile_type t) {
    return t== TILE_FIREFLY_U ||
        t == TILE_FIREFLY_D ||
        t == TILE_FIREFLY_L ||
        t == TILE_FIREFLY_R;
}

uint8_t get_tile(uint8_t x, uint8_t y) {
    if (y >= TILE_ROWS || y < 0) return TILE_BEDROCK;
    if (x >= TILE_COLS || x < 0) return TILE_BEDROCK;
    return tiles[y][x];
}

void set_tile(uint8_t x, uint8_t y, tile_type t) {
    if (y > TILE_ROWS || y < 0) return;
    if (x > TILE_COLS || x < 0) return;
    tiles[y][x] = t;
    tiles_ticked[y][x] = true;

    if (t == TILE_PLAYER) {
        player_x = x;
        player_y = y;
    }
}

void update_pixels(bool flash) {
    uint8_t y1 = min(TILE_ROWS - SCR_TH, max(0, player_y - (SCR_TH / 2)));
    uint8_t y2 = y1 + SCR_TH;

    uint8_t x1 = min(TILE_COLS - SCR_TW, max(0, player_x - (SCR_TW / 2)));
    uint8_t x2 = x1 + SCR_TW;

    for (uint8_t y = y1; y < y2; y++) {
        for (uint8_t x = x1; x < x2; x++) {
            tile_type t = get_tile(x, y);
            for (uint8_t j = 0; j < px_per_tile; j++) {
                for (uint8_t i = 0; i < px_per_tile; i++) {
                    uint8_t *cur = &(pixels[(y - y1) * px_per_tile + j][(x - x1) * px_per_tile + i]);
                    if (flash) {
                        *cur = 48 + (rand() % 3);
                        continue;
                    }

                    switch (t) {
                    case TILE_EMPTY: *cur = C_BLACK; break;
                    case TILE_ROCK:
                    case TILE_ROCK_FALLING:
                        *cur = pal[tile_gfx[TILE_ROCK][j * 4 + i]]; break;
                        break;
                    case TILE_BEDROCK:
                        *cur = i && !j ? 236: 235;
                        break;
                    case TILE_DIAMOND:
                    case TILE_DIAMOND_FALLING:
                        *cur = pal[tile_gfx[TILE_DIAMOND][j * 4 + i]]; break;
                        break;
                    case TILE_SAND:
                        *cur = pal[4]; break;
                    case TILE_SANDSTONE:
                        *cur = pal[tile_gfx[TILE_SANDSTONE][j * 4 + i]]; break;
                    case TILE_FIREFLY_U:
                        *cur = 0xc5 + (rand() % 5);
                        if (i == 0 && j == 0) { *cur = 250; }
                        break;
                    case TILE_FIREFLY_D:
                        *cur = 0xc5 + (rand() % 5);
                        if (i == 1 && j == 1) { *cur = 250; }
                        break;
                    case TILE_FIREFLY_L:
                        *cur = 0xc5 + (rand() % 5);
                        if (i == 0 && j == 1) { *cur = 250; }
                        break;
                    case TILE_FIREFLY_R:
                        *cur = 0xc5 + (rand() % 5);
                        if (i == 1 && j == 0) { *cur = 250; }
                        break;
                    case TILE_PLAYER:
                        *cur = 226 + (rand() % 5);
                        if (j == 1) {
                            if (player_right) {
                                if (i == 1 || i == 3) *cur = 0xcd;
                            } else {
                                if (i == 0 || i == 2) *cur = 0xcd;
                            }
                        }
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

void reset_level() {
    for (uint8_t y = 0; y < TILE_ROWS; y++) {
        for (uint8_t x = 0; x < TILE_COLS; x++) {
            if (x == 0 || x == TILE_COLS - 1 || y == 0 || y == TILE_ROWS -1) {
                set_tile(x, y, TILE_BEDROCK);
                continue;
            }
            if (x % 5 == 2 && y % 5 == 2) {
                set_tile(x, y, TILE_DIAMOND);
                continue;
            }
            uint16_t r = rand() % 1000;

            if (r < 800) {
                set_tile(x, y, TILE_SAND);
                continue;
            }
            if (r < 900) {
                set_tile(x, y, TILE_ROCK);
                if ((rand() % 10) == 1) {
                    set_tile(x, y, TILE_SANDSTONE);
                }
                continue;
            }
            if (r < 940) {
                set_tile(x, y, TILE_FIREFLY_L);
                continue;
            }
            if (r < 970) {
                set_tile(x, y, TILE_AMOEBA);
                continue;
            }
            set_tile(x, y, TILE_EMPTY);
        }
    }

    // add some horizontal random line segments
    uint8_t num_h = (rand() % 5) + 5;
    for (uint8_t i = 0; i < num_h; i++) {
        uint8_t start = rand() % TILE_COLS;
        uint8_t len = 6;
        uint8_t yo = (rand() % ((TILE_ROWS - 2) / 2)) * 2;
        for (uint8_t j = start; j < start + len; j++) {
            set_tile(j, yo, TILE_BEDROCK);
        }
    }
    // add some vertical random line segments
    uint8_t num_v = (rand() % 5) + 5;
    for (uint8_t i = 0; i < num_v; i++) {
        uint8_t start = rand() % TILE_ROWS;
        uint8_t len = 5;
        uint8_t xo = (rand() % ((TILE_COLS - 1) / 2)) * 2;
        for (uint8_t j = start; j < start + len; j++) {
            set_tile(xo, j, TILE_BEDROCK);
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
            } else if (td.consumable) {
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
        explode(i, j + 1);

    // Roll to the left
    } else if (td_d.round &&
               is_empty(i - 1, j) &&
               is_empty(i - 1, j + 1)) {
        set_tile(i, j, TILE_EMPTY);
        set_tile(i - 1, j, fall); //TILE_ROCK_FALLING);

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

void push_block(uint8_t x, uint8_t y, int8_t dx, int8_t dy, bool dig, tile_type ot) {
    tile_type t = get_tile(x + dx * 2, y + dy * 2);
    if (rand() % 2 == 1) return; // random struggle-to-push
    if (t == TILE_EMPTY || t == TILE_SAND) {
        set_tile(x + dx * 2, y + dy * 2, ot);
        if (dig) {
            set_tile(x + dx, y + dy, TILE_EMPTY);
        } else {
            set_tile(x + dx, y + dy, TILE_PLAYER);
            set_tile(x, y, TILE_EMPTY);
        }
    }
}

bool update_player(uint8_t x, uint8_t y, int8_t dx, int8_t dy, bool dig, uint8_t slot) {
    if (dx > 0) player_right = true;
    if (dx < 0) player_right = false;

    tile_type t = get_tile(x + dx, y + dy);
    if (t == TILE_EMPTY || t == TILE_SAND) {
        if (dig) {
            set_tile(x + dx, y + dy, slot == 0 ? TILE_EMPTY : TILE_ROCK);
        } else {
            set_tile(x, y, TILE_EMPTY);
            set_tile(x + dx, y + dy, TILE_PLAYER);
        }
    } else if (t == TILE_DIAMOND) {
        if (dig) {
            set_tile(x + dx, y + dy, TILE_EMPTY);
        }
        else {
            set_tile(x, y, TILE_EMPTY);
            set_tile(x + dx, y + dy, TILE_PLAYER);
        }
        return true;
    } else if (t == TILE_ROCK && (dx != 0 || dy != 0)) {
        push_block(x, y, dx, dy, dig, TILE_ROCK);
    } else if (t == TILE_SANDSTONE && (dx != 0 || dy != 0)) {
        push_block(x, y, dx, dy, dig, TILE_SANDSTONE);
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

void update_amoeba(uint8_t x, uint8_t y) {
    // if touching firefly - explode
    if (is_firefly(get_tile(x, y - 1)) ||
        is_firefly(get_tile(x, y + 1)) ||
        is_firefly(get_tile(x - 1, y)) ||
        is_firefly(get_tile(x + 1, y))) {
        explode(x, y);
        set_tile(x, y, TILE_DIAMOND);
    }
}

void reset_ticked() {
    memset(tiles_ticked, false, TILE_COLS * TILE_ROWS);
}

bool tick_tiles(int8_t dx, int8_t dy, bool dig, uint8_t slot) {
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
                if (update_player(i, j, dx, dy, dig, slot)) {
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
            case TILE_AMOEBA: update_amoeba(i, j);
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
    bool dig = false;
    int8_t slot = 0;

    uint32_t t = 0;
    int8_t flash = 0;

    reset_level();

    bool running = true;
    while(running){
        dx = 0; // stop moving
        dy = 0;
        dig = false;

        update_ansi_keys(keys);
        if (key_pressed('q', keys)) {
            running = false;
        }
        if (key_down('w', keys) || key_down(ansi_special('A'), keys)) {
            dy = -1;
        }
        if (key_down('s', keys) || key_down(ansi_special('B'), keys)) {
            dy = 1;
        }
        if (key_down('a', keys) || key_down(ansi_special('D'), keys)) {
            dx = -1;
        }
        if (key_down('d', keys) || key_down(ansi_special('C'), keys)) {
            dx = 1;
        }
        if (key_down(' ', keys)) {
            dig = true;
        }
        if (key_pressed('1', keys)) {
            slot = 0;
        }
        if (key_pressed('2', keys)) {
            slot = 1;
        }
        if (key_pressed('r', keys)) {
            key_unpress('r', keys);
            reset_level();
        }
        if (dx != 0) dy = 0;

        // Update
        t++;

        if (t % 4 == 0) {
            if (tick_tiles(dx, dy, dig, slot)) {
                flash = 2;
            }
        }
        update_pixels(flash > 0);
        if (--flash < 0) flash = 0;

        // Render
        render_pixels();

        set_bg(C_BLACK);
        set_fg(C_WHITE);
        cursor_to(scr_w / 2 - (PIX_W / 2), (scr_h / 2) + (PIX_H / 4) + 1);
        printf("move: wsad | r: restart | spc: 0=dig, 1=rock | cur: ");
        printf(slot == 0 ? "dig " : "rock");


        fflush(stdout);
        usleep(delay);
    };
    done(0);
    return 0;
}
