#include <sys/ioctl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define C_BLACK 16
#define C_WHITE 15
#define delay 1000000 / 30

uint16_t w = 0;
uint16_t h = 0;
struct winsize win;

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

void init() {
    init_tty(true);
    esc("?25l"); // hide cursor
    // Emit code for key events (as specified in Kitty Keyboard Protocol)
    // https://sw.kovidgoyal.net/kitty/keyboard-protocol/
    esc(">10;u"); // 08 (all keys as code) + 02 (key events)
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
    esc("<u"); // disable key events (Kitty Keyboard Protocol)

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

    bool running = true;
    int t = 0;
    set_bg(C_BLACK);

    char ex_code[] = {0x1b, '[', '2', '7', 'u'};
    size_t ex_len = sizeof(ex_code)/sizeof(char);

    while(running){
        // Input
        if (kbhit()) {
            set_fg(((t / h) % 14) + 1);
            cursor_to(0, t % h);

            char c[80]={0};
            read(0, &c, sizeof(c));
            if (memcmp(c, ex_code, ex_len) == 0) {
                running = false;
            }
            for (unsigned long i = 0; i < sizeof(c); i++) {
                if (c[i] == 0) {
                    printf(".");
                    continue;
                }
                if (c[i] == 0x1b) {
                    printf("^");
                    continue;
                }
                if (c[i] < 40)
                    printf("0x%d", c[i]);
                else
                    printf("%c", c[i]);
            }
            printf("<");
            t++;

        }

        fflush(stdout);
        usleep(delay);
    };
    done(0);
    return 0;
}
