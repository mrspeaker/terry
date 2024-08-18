#include <sys/ioctl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>

#define C_BLACK 0
#define C_WHITE 15
#define delay 1000000 / 30

#define COLS 40
#define ROWS 40

unsigned char w = 0;
unsigned char h = 0;
struct winsize win;

unsigned char grid[ROWS][COLS] = {0};

void esc(char* str) {
    printf("\e[%s", str);
}

// Set/restore non-canonical and no-echo in tty
void init_tty(int enable) {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate); // current state
    if (enable) {
        // non-canonical mode:
        //   https://www.gnu.org/software/libc/manual/html_node/Noncanonical-Input.html
        ttystate.c_lflag &= ~ICANON; // non-cannonical
        ttystate.c_lflag &= ~ECHO;   // no echo
        ttystate.c_cc[VMIN] = 1;     // min bytes needed to return from `read`.
    }  else {
        ttystate.c_lflag |= ICANON;
        ttystate.c_lflag |= ECHO;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

// Simulates kbhit
//
// select() allows a program to monitor multiple file descriptors,
// waiting until one or more of the file descriptors become "ready".
// Non blocking.
int kbhit() {
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0; // wait 0 time
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}

void cursor_to(char x, char y) {
    printf("\e[%d;%dH", y, x);
}

void set_bg(unsigned char col) {
    printf("\e[48;5;%dm", col);
}

// only on terminals with 24bit color support
void set_bg_rgb(unsigned char r, unsigned char g, unsigned char b) {
    printf("\e[48;2;%d;%d;%dm", r, g, b);
}

void set_fg(unsigned char col) {
    printf("\e[38;5;%dm", col);
}

// Use the half-block char to make squarer, double res pixels
void two_px_test() {
    printf("▀"); // ▄
}

void cls() {
    set_bg(C_BLACK);
    set_fg(C_WHITE);
    esc("2J"); // clear screen
}

void init_grid() {
    for (unsigned char j = 0; j < ROWS; j++) {
        for (unsigned char i = 0; i < COLS; i++) {
            grid[j][i] = (i+j) % 20;// (rand() % 20);
        }
    }
}

void init() {
    init_tty(1);
    esc("?25l"); // hide cursor

    init_grid();
}

void render_grid() {
    set_fg(2);
    for (unsigned char j = 0; j < ROWS; j+=2) {
        for (unsigned char i = 0; i < COLS; i++) {
            unsigned char top = grid[j][i] + 232;
            unsigned char bottom = grid[j + 1][i] + 232;

            cursor_to(i, j/2);

            set_fg(top);
            set_bg(bottom);

            if (top == bottom) {
                printf(" ");
                continue;
            }
            printf("▀"); // printf("▄");
        }
    }
}

void update_grid() {
    for (unsigned char j = 0; j < ROWS; j++) {
        for (unsigned char i = 0; i < COLS; i++) {
            grid[j][i] = (grid[j][i] + 1) % 20;
        }
    }
}

void bg_fill() {
    for (int j = 0; j <= h; j++) {
        for (int i = 0; i <= w; i++) {
            cursor_to(i, j);
            set_bg((j / 1) % 10 + 232);
            printf(" ");
        }
    }

    render_grid();

    cursor_to(w / 2 - 10, h / 2);
    set_bg(0);
    set_fg(250);
    printf("hello, W A S D");
}

void done(int signum) {
    init_tty(0);
    esc("?25h"); // show cursor
    esc("0m"); // reset fg/bg

    printf("\nbye\n");
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

    char x = w / 2;
    char y = h / 2;
    char dx = 0;
    char dy = 0;

    unsigned int t = 0;

    char ch;
    int running = 1;

    while(running){
        // Input
        if (kbhit()) {
            dx = 0; // stop moving
            dy = 0;
            ch = fgetc(stdin);

            if (ch == 'q') running = 0; // quit

            if (ch == 'w') dy = -1;
            if (ch == 's') dy = 1;
            if (ch == 'a') dx = -1;
            if (ch == 'd') dx = 1;
        }

        // Update
        t++;
        update_grid();

        x += dx;
        if (x < 0) x = w;
        if (x > w) x = 0;

        y += dy;
        if (y < 0) y = h;
        if (y > h) y = 0;

        // Render
        render_grid();

        cursor_to(x, y);
        set_bg(t % 255);
        set_fg((t + 1) % 255);
        two_px_test();

        fflush(stdout);
        usleep(delay);
    };
    done(0);
    return 0;
}
