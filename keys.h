#include <stdbool.h>
#include <unistd.h>

typedef struct {
    int key_code;
    bool is_down;
    bool pressed;
} key_ev;

void init_keys(key_ev *keys, size_t size) {
    for (size_t i = 0; i < size; i++) {
        key_ev k = { .key_code = 0 };
        keys[i] = k;
    }
}

key_ev *find_key(int key_code, key_ev *keys, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (keys[i].key_code == key_code) {
            return &keys[i];
        }
    }
    return NULL;
}

key_ev *get_key(key_ev *keys, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (keys[i].key_code == 0) {
            return &keys[i];
        }
    }
    return NULL;
}

bool is_pressed(int key_code, key_ev *keys, size_t size) {
    key_ev *k = find_key(key_code, keys, size);
    return k != NULL && k->pressed;
}

bool is_down(int key_code, key_ev *keys, size_t size) {
    key_ev *k = find_key(key_code, keys, size);
    return k != NULL && k->is_down;
}

void set_key(int key_code, int key_event, key_ev *keys, size_t size) {
    key_ev *k = find_key(key_code, keys, size);
    if (!k) {
        k = get_key(keys, size);
        k->key_code = key_code;
    }
    if (key_event == 3) {
        k->key_code = 0;
        k->is_down = false;
        k->pressed = false;
    } else {
        k->is_down = true;
        k->pressed = key_event == 1;
    }
}
