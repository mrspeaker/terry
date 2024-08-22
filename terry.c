#include <sys/ioctl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define COLS 40
#define ROWS 40

#define C_BLACK 16
#define C_WHITE 15
#define delay 1000000 / 30

uint16_t w = 0;
uint16_t h = 0;
struct winsize win;

uint8_t grid[ROWS][COLS] = {0};

// Set/restore non-canonical and no-echo in tty
void init_tty(bool enable) {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty); // current state
    if (enable) {
        tty.c_lflag &= ~ICANON; // non-canonical (raw)
        tty.c_lflag &= ~ECHO;   // no echo
        tty.c_cc[VMIN] = 1;     // min bytes needed to return from `read`.
    }  else {
        tty.c_lflag |= ICANON;
        tty.c_lflag |= ECHO;
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
    init_tty(true);
    esc("?25l"); // hide cursor
    esc(">2;1u");
    init_grid();
}

#define FIRE_PAL 15

uint8_t pal[FIRE_PAL] = {
    16, 17, 18, 19, 54, 89, 90, 124,
    160, 196, 208, 215,216, 223, 231};

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
    for (uint8_t i = 0; i < COLS; i++) {
        grid[ROWS - 1][i] = rand() % 65;
    }
}

void render_text(int t) {
    set_bg(C_BLACK);
    uint32_t col = t / 5;

    set_fg(col);
    cursor_to(w/2-7, h/2+2);
    printf("Mr");

    set_fg(col - 1);
    cursor_to(w/2-4, h/2+2);
    printf("Speaker");

    set_fg(col - 2);
    cursor_to(w/2+4, h/2+2);
    printf("2024");
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
    init_tty(false);
    esc("?25h"); // show cursor
    esc("0m"); // reset fg/bg

    esc("<1u");

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

    int16_t x = w / 2;
    int16_t y = h / 2;
    int8_t dx = 0;
    int8_t dy = -1;

    uint32_t t = 0;
    char key;

    bool running = true;
    while(running){
        // Input
        if (kbhit()) {
            dx = 0; // stop moving
            dy = 0;
            key = fgetc(stdin);

            if (key == 'q') running = false; // quit

            if (key == 'w') dy = -1;
            if (key == 's') dy = 1;
            if (key == 'a') dx = -1;
            if (key == 'd') dx = 1;

            // Check for escape code sequences
            if (key == '\x1b') {
                key = fgetc(stdin); // '['
                key = fgetc(stdin);
                if (key == 27) running = false; // esc
                if (key == 'A') dy = -1; // up arrow
                if (key == 'B') dy = 1; // down arrow
                if (key == 'C') dx = 1; // right arrow
                if (key == 'D') dx = -1; // left arrow
                cursor_to(0, 0);
                set_bg(C_BLACK);
                set_fg(C_WHITE);
                printf("%d", key);
            }
                cursor_to(0, 0);
                set_bg(C_BLACK);
                set_fg(C_WHITE);
                while((key = fgetc(stdin)) != 'u') {
                    if (key < 30)
                        printf("0x%d ", key);
                    else
                        printf("%c", key);


                }
                printf("                        ");

        }

        // Update
        t++;
        update_grid();

        // Cursor direction
        uint32_t r = rand() % 100;
        if (r < 4) {
            dx = 0;
            dy = 0;
            r = rand() % 4;
            if (r == 0) dx = -1;
            if (r == 1) dx = 1;
            if (r == 2) dy = -1;
            if (r == 3) dy = 1;
        }

        // Update cursor
        x += dx;
        if (x < 0) x = w;
        if (x > w) x = 0;

        y += dy;
        if (y < 0) y = h;
        if (y > h) y = 0;

        // Render
        render_grid();
        render_text(t);

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
