#include <sys/ioctl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#define C_BLACK 0
#define C_WHITE 7
#define delay 1000000 / 30

unsigned char w = 0;
unsigned char h = 0;
struct winsize win;

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

// seems like it works in alacritty, but not in mac term.
void set_bg_rgb(unsigned char r, unsigned char g, unsigned char b) {
    printf("\e[48;2;%d;%d;%dm", r, g, b);
}

void set_fg(unsigned char col) {
    printf("\e[38;5;%dm", col);
}

void cls() {
    set_bg(C_BLACK);
    set_fg(C_WHITE);
    esc("2J"); // clear screen
}

void init() {
    init_tty(1);
    esc("?25l"); // hide cursor
}

void bg_fill() {
    for (int j = 0; j <= h; j++) {
        for (int i = 0; i <= w; i++) {
            cursor_to(i, j);
            set_bg((j + i) % 10 + 232);
            printf(" ");
        }
    }
    cursor_to(w / 2 - 10, h / 2);
    printf("hello, W A S D");
}

void done(int signum) {
    init_tty(0);
    esc("?25h"); // show cursor

    set_bg(C_BLACK);
    set_fg(C_WHITE);

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
    signal(SIGINT, done);
    signal(SIGWINCH, resize);

    resize();
    init();

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

        x += dx;
        if (x < 0) x = w;
        if (x > w) x = 0;

        y += dy;
        if (y < 0) y = h;
        if (y > h) y = 0;

        // Render
        cursor_to(x, y);
        set_bg(t % 255);
        printf(" ");

        fflush(stdout);
        usleep(delay);
    };
    done(0);
    return 0;
}
