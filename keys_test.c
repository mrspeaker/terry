#include <sys/ioctl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include "./ansi_parse.h"
#include "./keys.h"

#define C_BLACK 16
#define C_WHITE 15
#define delay 1000000 / 30
#define MAX_KEYS 10

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

void cls() {
    set_bg(C_BLACK);
    set_fg(C_WHITE);
    esc("2J"); // clear screen
}

void init() {
    init_tty(true);
    esc("?25l"); // hide cursor
    // Emit code for key events (as specified in Kitty Keyboard Protocol)
    // https://sw.kovidgoyal.net/kitty/keyboard-protocol/
    esc(">10;u"); // 08 (all keys as code) + 02 (key events)
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

    bool running = true;
    int t = 0;
    set_bg(C_BLACK);

    key_ev keys[MAX_KEYS];
    init_keys(keys, MAX_KEYS);

    while(running){
        // Input
        if (kbhit()) {
            set_fg(((t / h) % 14) + 1);

            char c[80]={0};
            size_t n = read(0, &c, sizeof(c));
            if (n < sizeof(c) && n > 0) {
                // parse the input
                int num_codes = 0;
                ansi_state st = ansi_init();
                for (size_t i = 0; i < n; i++) {
                    ansi_res res = ansi_step(&st, c[i]);
                    if (res.done) {
                        cursor_to(w - 8, ++num_codes);
                        if (isprint(res.key_code)) {
                            printf("%c %d %d \n", res.key_code, res.modifier, res.key_event);
                        } else {
                            printf("%d %d %d \n", res.key_code, res.modifier, res.key_event);
                        }

                        set_key(res.key_code, res.key_event, keys, MAX_KEYS);
                    }
                }
                cursor_to(w - 8, ++num_codes);
                printf("....... ");
            } else {
                // missed some input... just bail on all (keyup everything!)
            }

            cursor_to(0, t % h);
            print_chars(c, sizeof(c));
            t++;

        }


        if (is_pressed('q', keys, MAX_KEYS)) {
            running = false;
        }
        if (is_down('z', keys, MAX_KEYS)) {
            running = false;
        }

        fflush(stdout);
        usleep(delay);
    };
    done(0);
    return 0;
}
