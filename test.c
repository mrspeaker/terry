#include <stdio.h>
#include "./ansi_parse.h"

int main() {
    ansi_state s = ansi_init();
    ansi_step(&s, '\x1b');
    ansi_step(&s, '[');
    ansi_step(&s, '2');
    ansi_step(&s, '3');
    ansi_step(&s, '4');
    ansi_step(&s, ';');
    ansi_step(&s, '1');
    ansi_step(&s, ':');
    ansi_step(&s, '3');
    ansi_res r = ansi_step(&s, 'u');
    printf("%d, %d %d %d %d.\n", s.state, r.done, r.key_code, r.modifier, r.key_event);
    return 0;
}
