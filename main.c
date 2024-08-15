#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>

void esc(char* str) {
    printf("\e[%s", str);
}

char w = 0;
char h = 0;

struct winsize win;
int stdoutid = STDOUT_FILENO;
unsigned long twinsz = TIOCGWINSZ;

void init() {
    system("stty -echo"); //don't echo
    system("stty -icanon");
    esc("48;5;0m"); // bg black
    esc("2J"); // clear screen
    esc("?25l"); // hide cursor

    esc("> 1 u");
}

void done(int signum) {
    esc("?25h"); // show cursor
    esc("37m"); // fg to white
    esc("48;5;0m"); // bg to black

    esc("< u");
    esc("?1003l");
    system("stty echo"); //turn on screen echo again
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

    /*int fd = open("/dev/input/event0", O_RDONLY);
    if (fd == -1) {
        perror("Error opening input device");
        return 1;
    }
    close(fd);*/

    resize();

    char x = 0;
    char y = 0;

    int delay = 1000000 / 30;
    char t = 0;

    esc("?1003h");

    char buffer[16] = " ";
    int flags = fcntl(stdin->_fileno, F_GETFL, 0);
    fcntl(stdin->_fileno, F_SETFL, flags | O_NONBLOCK);

    while(1){
        t++;

        x++;
        if (x > w) x = 0;

        //ioctl(stdoutid, KDGETKEYCODE, &kb);

        /*        ret = read(0, &ch, 1);
        if ( !ret ) {
            tcsetattr(0, TCSANOW, &restore);
            fprintf(stderr, "getpos: error reading response!\n");
            return 1;
        }
        buf[i] = ch;
        printf("buf[%d]: \t%c \t%d\n", i, ch, ch);*/

        read(fileno(stdin), buffer, 16);
        fprintf(stdout, "%s", buffer);

        printf("\e[%d;%dH", y, x);
        printf("\e[48;5;%dm ", t); // bg
        fflush(stdout);
        usleep(delay);
    };
    return 0;
}
