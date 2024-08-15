#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

void esc(char* str) {
    printf("\e[%s", str);
}

char w = 0;
char h = 0;

struct winsize win;
int stdoutid = STDOUT_FILENO;
unsigned long twinsz = TIOCGWINSZ;


void init() {
    esc("48;5;0m"); // bg black
    esc("2J"); // clear screen
    esc("?25l"); // hide cursor
}

void done(int signum) {
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

    char x = 0;
    char y = 0;

    int delay = 1000000 / 30;
    char t = 0;
    while(1){
        t++;

        x++;
        if (x > w) x = 0;
        printf("\e[%d;%dH", y, x);
        printf("\e[48;5;%dm ", t); // bg
        fflush(stdout);
        usleep(delay);
    };
    return 0;
}
