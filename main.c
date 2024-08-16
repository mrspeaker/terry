#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>

void esc(char* str) {
    printf("\e[%s", str);
}

char w = 0;
char h = 0;

struct winsize win;
int stdoutid = STDOUT_FILENO;
unsigned long twinsz = TIOCGWINSZ;

void nonblock(int enable) {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate); // save current state
    if (enable) {
        ttystate.c_lflag &= ~ICANON; // cannonical
        ttystate.c_cc[VMIN] = 1; // minimum of number input read.
    }  else {
        ttystate.c_lflag |= ICANON;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

void init() {
    system("stty -echo"); //don't echo
    nonblock(1);
    esc("48;5;0m"); // bg black
    esc("2J"); // clear screen
    esc("?25l"); // hide cursor
}

void done(int signum) {
    esc("?25h"); // show cursor
    esc("37m"); // fg to white
    esc("48;5;0m"); // bg to black

    system("stty echo"); //turn on screen echo again
    nonblock(0);

    printf("\nbye\n");

    exit(signum);
};

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
