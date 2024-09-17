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
#define C_YELLOW 226

#define delay 1000000 / 30

uint16_t scr_w = 0;
uint16_t scr_h = 0;
struct winsize win;

uint8_t pixels[PIX_H][PIX_W] = {0};

#define TILE_COLS ((SCR_TW * 2)+1)
#define TILE_ROWS ((SCR_TH * 2)+1)

uint8_t cam_x = 0;
uint8_t cam_t = 0;

typedef struct { int8_t x; int8_t y; } dir;
typedef struct { uint8_t x; uint8_t y; } point;

typedef struct {
    uint8_t x;
    uint8_t y;
    int8_t dx;
    int8_t dy;
    dir  dir;
    int8_t lives;
    uint8_t slot;
    bool got_diamond;
    bool moved;
    bool dig;
} player_state;

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
    TILE_BALLOON,
    TILE_BALLOON_RISING,
    TILE_EXP,
    TILE_FIREFLY,
    TILE_AMOEBA,
    TILE__LEN
} tile_type;

typedef struct {
    bool round;
    bool explodable;
    bool consumable;
} tile_deets;

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
    [TILE_BALLOON] = { true, false, true },
    [TILE_BALLOON_RISING] = { false, false, true },
    [TILE_EXP] = { false, false, false },
    [TILE_FIREFLY] = { false, true, true },
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
    [9] = 43,
    [10] = C_YELLOW
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
    [TILE_BALLOON] = {
        4,3,3,4,
        3,3,3,3,
        5,3,3,5,
        4,3,3,4,
    },
};

typedef enum {
    TD_TICKS,
    TD_DIR
} tile_data_type;

typedef union {
    dir dir;
    int ticks;
} tile_data;

typedef struct {
    tile_data_type type;
    tile_data data;
} tagged_tile_data;

typedef struct {
    tile_type type;
    tagged_tile_data tile_data;
} tile;

tile bedrocked = {.type=TILE_BEDROCK, .tile_data.type=TD_TICKS};

tile tiles[TILE_ROWS][TILE_COLS] = {0};
bool tiles_ticked[TILE_ROWS][TILE_COLS] = {false};

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

tile *get_tile(uint8_t x, uint8_t y) {
    if (y >= TILE_ROWS || y < 0) return &bedrocked;
    if (x >= TILE_COLS || x < 0) return &bedrocked;
    return &tiles[y][x];
}

bool set_tile(uint8_t x, uint8_t y, tile_type t) {
    if (y > TILE_ROWS || y < 0) return false;
    if (x > TILE_COLS || x < 0) return false;
    tiles_ticked[y][x] = true;

    tiles[y][x].type = t;
    tiles[y][x].tile_data.type = TD_TICKS;
    tiles[y][x].tile_data.data.ticks = 0;

    return true;
}

void set_tile_and_data_dir(uint8_t x, uint8_t y, tile_type t, dir d) {
    if (set_tile(x, y, t)) {
        tiles[y][x].tile_data.type = TD_DIR;
        tiles[y][x].tile_data.data.dir.x = d.x;
        tiles[y][x].tile_data.data.dir.y = d.y;
    }
}

void set_tile_and_data_ticks(uint8_t x, uint8_t y, tile_type t, int ticks) {
    if (set_tile(x, y, t)) {
        tiles[y][x].tile_data.data.ticks = ticks;
    }
}


void move_tile(uint8_t x, uint8_t y, dir d, tile_type t) {
    set_tile(x, y, TILE_EMPTY);
    set_tile(x + d.x, y + d.y, t);
}

void move_tile_dir(uint8_t x, uint8_t y, dir d, tile_type t) {
    set_tile(x, y, TILE_EMPTY);
    set_tile_and_data_dir(x + d.x, y + d.y, t, d);
}


void update_pixels(player_state *s, bool flash) {
    uint8_t y1 = min(TILE_ROWS - SCR_TH, max(0, s->y - (SCR_TH / 2)));
    uint8_t y2 = y1 + SCR_TH;

    uint8_t x1 = min(TILE_COLS - SCR_TW, max(0, s->x - (SCR_TW / 2)));
    uint8_t x2 = x1 + SCR_TW;

    for (uint8_t y = y1; y < y2; y++) {
        for (uint8_t x = x1; x < x2; x++) {
            tile *t = get_tile(x, y);
            for (uint8_t j = 0; j < px_per_tile; j++) {
                for (uint8_t i = 0; i < px_per_tile; i++) {
                    uint8_t *cur = &(pixels[(y - y1) * px_per_tile + j][(x - x1) * px_per_tile + i]);
                    if (flash) {
                        *cur = 48 + (rand() % 3);
                        continue;
                    }

                    switch (t->type) {
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
                    case TILE_BALLOON:
                    case TILE_BALLOON_RISING:
                        *cur = pal[tile_gfx[TILE_BALLOON][j * 4 + i]]; break;
                    case TILE_FIREFLY:
                        *cur = 0xc5 + (rand() % 5);
                        if (i == 0 && j == 0) {
                            if (t->tile_data.data.dir.x < 0) *cur = C_YELLOW;
                            if (t->tile_data.data.dir.x > 0) *cur = C_LIGHTGREY;
                            if (t->tile_data.data.dir.y < 0) *cur = C_BLACK;
                            if (t->tile_data.data.dir.y > 0) *cur = C_MAROON;
                        }

                        break;
                    case TILE_PLAYER:
                        *cur = 226 + (rand() % 5);
                        if (j == 1) {
                            if (s->dir.x < 0) {
                                if (i == 1 || i == 3) *cur = 0xcd;
                            } else {
                                if (i == 0 || i == 2) *cur = 0xcd;
                            }
                        }
                        break;
                    case TILE_AMOEBA:
                        *cur = 17 + (rand() % 50);
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

void reset_level(uint8_t player_x, uint8_t player_y) {
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
                if ((rand() % 10) == 1) {
                    set_tile(x, y, TILE_BALLOON);
                }
                continue;
            }
            if (r < 940) {
                set_tile_and_data_dir(x, y, TILE_FIREFLY, (dir){1,0});
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

    set_tile(player_x, player_y, TILE_PLAYER);
}

bool is_empty(uint8_t x, uint8_t y) {
    return get_tile(x, y)->type == TILE_EMPTY;
}
bool is_round(uint8_t x, uint8_t y) {
    return tiledefs[get_tile(x, y)->type].round;
}

void explode(uint8_t x, uint8_t y) {
    tile_type t = get_tile(x, y)->type;
    set_tile_and_data_ticks(x, y, TILE_EXP, 0);

    for (int8_t i = -1; i <= 1; i++) {
        for (int8_t j = -1; j <= 1; j++) {
            t = get_tile(x + i, y + j)->type;
            tile_deets td = tiledefs[t];
            if (td.explodable) {
                explode(x + i, y + j);
            } else if (td.consumable) {
                //set_tile(x + i, y + j, TILE_EXP_1);
                set_tile_and_data_ticks(x + i, y + j, TILE_EXP, 0);
            }
        }
    }
}

void update_tile_fallable(uint8_t i, uint8_t j, tile_type t) {
    tile_type dn = get_tile(i, j + 1)->type;
    tile_deets td_dn = tiledefs[dn];

    if (dn == TILE_EMPTY) {
        set_tile(i, j, t);
        // Roll to the left
    } else if (td_dn.round &&
               is_empty(i - 1, j) &&
               is_empty(i - 1, j + 1)) {
        set_tile(i, j, TILE_EMPTY);
        set_tile(i - 1, j, t);
        // Roll to the right
    } else if (td_dn.round &&
               is_empty(i + 1, j) &&
               is_empty(i + 1, j + 1)) {
        set_tile(i, j, TILE_EMPTY);
        set_tile(i + 1, j, t);
    }
}

void update_tile_shootable(uint8_t i, uint8_t j, tile *tile) {
    if (tile->tile_data.type == TD_DIR) {
        dir d = tile->tile_data.data.dir;
        tile_type t = get_tile(i + d.x, j + d.y)->type;
        if (t == TILE_EMPTY || t == TILE_SAND) {
            move_tile_dir(i, j, d, tile->type);
        }
    }
}

void update_tile_falling(uint8_t i, uint8_t j, tile_type rest, tile_type fall) {
    tile_type dn = get_tile(i, j + 1)->type;
    tile_deets td_dn = tiledefs[dn];

    // Straight down
    if (dn == TILE_EMPTY) {
        set_tile(i, j, TILE_EMPTY);
        set_tile(i, j + 1, fall);

    // explode things
    } else if (td_dn.explodable) {
        explode(i, j + 1);

    // Roll to the left
    } else if (td_dn.round &&
               is_empty(i - 1, j) &&
               is_empty(i - 1, j + 1)) {
        set_tile(i, j, TILE_EMPTY);
        set_tile(i - 1, j, fall);

    // Roll to the right
    } else if (td_dn.round &&
               is_empty(i + 1, j) &&
               is_empty(i + 1, j + 1)) {
        set_tile(i, j, TILE_EMPTY);
        set_tile(i + 1, j, fall);
    } else {
        set_tile(i, j, rest);
    }
}

void update_tile_riseable(uint8_t i, uint8_t j, tile_type t) {
    tile_type up = get_tile(i, j - 1)->type;
    tile_deets td_up = tiledefs[up];

    if (up == TILE_EMPTY) {
        set_tile(i, j, t);
        // Roll to the left
    } else if (td_up.round &&
               is_empty(i - 1, j) &&
               is_empty(i - 1, j - 1)) {
        set_tile(i, j, TILE_EMPTY);
        set_tile(i - 1, j, t);
        // Roll to the right
    } else if (td_up.round &&
               is_empty(i + 1, j) &&
               is_empty(i + 1, j - 1)) {
        set_tile(i, j, TILE_EMPTY);
        set_tile(i + 1, j, t);
    }
}

void update_tile_rising(uint8_t i, uint8_t j, tile_type rest, tile_type rise) {
    tile_type up = get_tile(i, j - 1)->type;
    tile_deets td_up = tiledefs[up];

    // Straight up
    if (up == TILE_EMPTY) {
        set_tile(i, j, TILE_EMPTY);
        set_tile(i, j - 1, rise);

    // explode things
    } else if (td_up.explodable) {
        explode(i, j - 1);

    // Rise to the left
    } else if (td_up.round &&
               is_empty(i - 1, j) &&
               is_empty(i - 1, j - 1)) {
        set_tile(i, j, TILE_EMPTY);
        set_tile(i - 1, j, rise);

    // Roll to the right
    } else if (td_up.round &&
               is_empty(i + 1, j) &&
               is_empty(i + 1, j - 1)) {
        set_tile(i, j, TILE_EMPTY);
        set_tile(i + 1, j, rise);
    } else {
        set_tile(i, j, rest);
    }

}

void push_block(uint8_t x, uint8_t y, player_state *s, tile_type ot) {
    int8_t dx = s->dx;
    int8_t dy = s->dy;
    bool dig = s->dig;

    tile_type t = get_tile(x + dx * 2, y + dy * 2)->type;
    if (rand() % 2 == 1) return; // random struggle-to-push
    if (t == TILE_EMPTY || t == TILE_SAND) {
        if (dig) {
            // Shoot it?! - test this: "fires" the tile as far as it can travel
            // instead of just moving one block
            set_tile_and_data_dir(x + dx * 2, y + dy * 2, ot, (dir){dx, dy});
            set_tile(x + dx, y + dy, TILE_EMPTY);
        } else {
            set_tile(x + dx * 2, y + dy * 2, ot);
            set_tile(x + dx, y + dy, TILE_PLAYER);
            s->x = x + dx;
            s->y = y + dy;
            set_tile(x, y, TILE_EMPTY);
        }
    }
}

void update_player(uint8_t x, uint8_t y, player_state *s) {
    int8_t dx = s->dx;
    int8_t dy = s->dy;
    bool dig = s->dig;
    s->moved = false;
    s->got_diamond = false;

    uint8_t old_x = s->x;
    uint8_t old_y = s->y;

    /*if (dx != 0) s->facing_right = true;
      if (dx < 0) s->facing_right = false;*/

    bool pushing = dx != 0 || dy != 0;

    tile_type t = get_tile(x + s->dx, y + s->dy)->type;
    if (t == TILE_EMPTY || t == TILE_SAND) {
        if (dig) {
            set_tile(x + dx, y + dy, s->slot == 0 ? TILE_EMPTY : TILE_ROCK);
        } else {
            set_tile(x, y, TILE_EMPTY);
            set_tile(x + dx, y + dy, TILE_PLAYER);
            s->x = x + dx;
            s->y = y + dy;
        }
    } else if (t == TILE_DIAMOND) {
        if (dig) {
            set_tile(x + dx, y + dy, TILE_EMPTY);
        }
        else {
            set_tile(x, y, TILE_EMPTY);
            set_tile(x + dx, y + dy, TILE_PLAYER);
            s->x = x + dx;
            s->y = y + dy;
        }
        s->got_diamond = true;
    } else if (pushing && (t == TILE_ROCK || t == TILE_SANDSTONE || t == TILE_BALLOON)) {
        push_block(x, y, s, t);
    }
    if (old_x != s->x || old_y != s->y) {
        s->moved = true;
    }
}

dir rotate_left(dir *d) {
    if (d->y == -1) return (dir){ -1, 0 };
    if (d->x == -1) return (dir){ 0, 1 };
    if (d->y == 1) return (dir){ 1, 0 };
    return (dir){ 0, -1 };
}

dir rotate_right(dir *d) {
    if (d->y == -1) return (dir){ 1, 0 };
    if (d->x == 1) return (dir){ 0, 1 };
    if (d->y == 1) return (dir){ -1, 0 };
    return (dir){ 0, -1 };
}

void update_firefly(uint8_t x, uint8_t y, dir *d) {
    // if touching player - explode
    if (get_tile(x, y - 1)->type == TILE_PLAYER ||
        get_tile(x, y + 1)->type == TILE_PLAYER ||
        get_tile(x - 1, y)->type == TILE_PLAYER ||
        get_tile(x + 1, y)->type == TILE_PLAYER) {
        explode(x, y);
        return;
    }
    if (d->x == 0 && d->y ==0) done(39);

    // Try rotate left
    dir rotL = rotate_left(d);
    if (get_tile(x + rotL.x, y + rotL.y)->type == TILE_EMPTY) {
        d->x = rotL.x;
        d->y = rotL.y;
        if (d->x == 0 && d->y ==0) done(33);
        move_tile_dir(x, y, *d, TILE_FIREFLY);
        return;
    }

    // Try go straight
    if (get_tile(x + d->x, y + d->y)->type == TILE_EMPTY) {
        if (d->x == 0 && d->y ==0) done(35);
        move_tile_dir(x, y, *d, TILE_FIREFLY);
        return;
    }
        if (d->x == 0 && d->y ==0) done(37);


    // rotate right
    dir rotR = rotate_right(d);
    d->x = rotR.x;
    d->y = rotR.y;
    set_tile_and_data_dir(x, y, TILE_FIREFLY, *d);
}

void update_amoeba(uint8_t x, uint8_t y) {
    // if touching firefly - explode
    if (get_tile(x, y - 1)->type == TILE_FIREFLY ||
        get_tile(x, y + 1)->type == TILE_FIREFLY ||
        get_tile(x - 1, y)->type == TILE_FIREFLY ||
        get_tile(x + 1, y)->type == TILE_FIREFLY) {
        explode(x, y);
        set_tile(x, y, TILE_DIAMOND);
    }
}

void reset_ticked() {
    memset(tiles_ticked, false, TILE_COLS * TILE_ROWS);
}

bool tick_tiles(player_state *s) {
    bool flash = false;
    reset_ticked();
    for (int8_t j = TILE_ROWS-1; j >= 0; j--) {
        for (uint8_t i = 0; i < TILE_COLS; i++) {
            // Only process each cell once per tick
            if (tiles_ticked[j][i]) continue;

            tile *tile = get_tile(i, j);
            uint8_t t= tile->type;

            if (t == TILE_EMPTY || t == TILE_BEDROCK || t == TILE_SAND) continue;

            switch (t) {
            case TILE_ROCK:
                update_tile_fallable(i, j, TILE_ROCK_FALLING);
                update_tile_shootable(i, j, tile);
                break;
            case TILE_ROCK_FALLING:
                update_tile_falling(i, j, TILE_ROCK, TILE_ROCK_FALLING);
                break;
            case TILE_DIAMOND:
                update_tile_fallable(i, j, TILE_DIAMOND_FALLING);
                break;
            case TILE_DIAMOND_FALLING:
                update_tile_falling(i, j, TILE_DIAMOND, TILE_DIAMOND_FALLING);
                break;
            case TILE_SANDSTONE:
                update_tile_shootable(i, j, tile);
                break;
            case TILE_BALLOON:
                update_tile_riseable(i, j, TILE_BALLOON_RISING);
                break;
            case TILE_BALLOON_RISING:
                update_tile_rising(i, j, TILE_BALLOON, TILE_BALLOON_RISING);
                break;
            case TILE_PLAYER:
                update_player(i, j, s);
                if (s->got_diamond) {
                    s->lives += 16;
                    flash = true;
                }
                if (s->moved) {
                    s->lives -= 1;
                }
                break;
            case TILE_EXP:
                if (tile->tile_data.data.ticks++ > 4) {
                    set_tile(i, j, TILE_EMPTY);
                }
                break;
            case TILE_FIREFLY: update_firefly(i, j, &tile->tile_data.data.dir); break;
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

void reset(player_state *s) {
    s->x = 5;
    s->y = 5;
    s->lives = 16;
    reset_level(s->x, s->y);
}

int main() {
    srand(time(0));

    signal(SIGINT, done);
    signal(SIGWINCH, resize);

    cls();
    init();
    resize();

    ansi_keys *keys = make_ansi_keys();

    uint32_t t = 0;
    int8_t flash = 0;

    player_state s;
    reset(&s);

    bool running = true;
    while(running){
        s.dx = 0; // stop moving
        s.dy = 0;
        s.dig = false;

        update_ansi_keys(keys);
        if (key_pressed('q', keys)) {
            running = false;
        }
        if (key_down('w', keys) || key_down(ansi_special('A'), keys)) {
            s.dy = -1;
        }
        if (key_down('s', keys) || key_down(ansi_special('B'), keys)) {
            s.dy = 1;
        }
        if (key_down('a', keys) || key_down(ansi_special('D'), keys)) {
            s.dx = -1;
        }
        if (key_down('d', keys) || key_down(ansi_special('C'), keys)) {
            s.dx = 1;
        }
        if (key_down(' ', keys)) {
            s.dig = true;
        }
        if (key_pressed('1', keys)) {
            s.slot = 0;
        }
        if (key_pressed('2', keys)) {
            s.slot = 1;
        }
        if (key_pressed('r', keys)) {
            key_unpress('r', keys);
            reset(&s);
        }
        if (s.dx != 0) s.dy = 0;

        // Update
        t++;

        if (t % 4 == 0) {
            if (tick_tiles(&s)) {
                flash = 2;
            }
        }
        update_pixels(&s, flash > 0);
        if (--flash < 0) flash = 0;

        // Render
        render_pixels();

        set_bg(C_BLACK);
        set_fg(C_WHITE);
        cursor_to(scr_w / 2 - (PIX_W / 2), (scr_h / 2) + (PIX_H / 4) + 1);
        printf("energy: %d | ", s.lives);
        printf("move: wsad | r: restart | spc: 0=dig, 1=rock | cur: ");
        printf(s.slot == 0 ? "dig " : "rock");


        fflush(stdout);
        usleep(delay);
    };
    done(0);
    return 0;
}
