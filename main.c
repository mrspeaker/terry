#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

void esc(char* str) {
    printf("\x1b[%s", str);
    fflush(stdout);
}

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


int main() {
    signal(SIGINT, done);
    init();

    int delay = 1000000 / 1;
    char t = 0;
    while(1){
        t++;
        esc("48;5;");
        printf("5m "); // bg
        fflush(stdout);
        usleep(delay);
    };
    return 0;
}
