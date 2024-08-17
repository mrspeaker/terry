#include <sys/ioctl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

void esc(char* str) {
    printf("\e[%s", str);
}

char w = 0;
char h = 0;

struct winsize win;
int stdoutid = STDOUT_FILENO;
unsigned long twinsz = TIOCGWINSZ;

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

// Simulates kbhit: wait 0 time for input
//
// select() allows a program to monitor multiple file descriptors,
// waiting until one or more of the file descriptors become "ready".
// Non blocking.
int kbhit() {
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}

void init() {
    init_tty(1);
    esc("48;5;0m"); // bg black
    esc("2J"); // clear screen
    esc("?25l"); // hide cursor
}

void done(int signum) {
    init_tty(0);
    esc("?25h"); // show cursor
    esc("37m"); // fg to white
    esc("48;5;0m"); // bg to black

    printf("\nbye\n");
    exit(signum);
};

void resize() {
    init();
    ioctl(stdoutid, twinsz, &win);
    w = win.ws_col;
    h = win.ws_row;
}

int main() {
    signal(SIGINT, done);
    signal(SIGWINCH, resize);

    resize();

    char x = w / 2;
    char y = h  / 2;
    char dx = 0;
    char dy = 0;

    int delay = 1000000 / 30;
    unsigned char t = 0;

    char ch;
    int running = 1;
    while(running){
        // Input
        if (kbhit()) {
            dx = 0;
            dy = 0;
            ch = fgetc(stdin);
            if (ch == 'q') running = 0;
            if (ch == 'w') dy = -1;
            if (ch == 's') dy = 1;
            if (ch == 'd') dx = 1;
            if (ch == 'a') dx = -1;
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
        printf("\e[%d;%dH", y, x);
        printf("\e[48;5;%dm ", t % 255); // bg
        fflush(stdout);
        usleep(delay);
    };
    return 0;
}
