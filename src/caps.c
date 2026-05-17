#define _GNU_SOURCE

#include <sys/capability.h>
#include <sys/prctl.h>
#include <stdio.h>
#include <errno.h>
#include "../include/caps.h"


int caps_drop_all(void){

    cap_t empty = cap_init();
    if (!empty) {
        perror("caps_drop_all: cap_init");
        return -1;
    }
    if (cap_set_proc(empty) == -1) {
        perror("caps_drop_all: cap_set_proc");
        cap_free(empty);
        return -1;
    }
    cap_free(empty);
    return 0;

}

int caps_set(const char *caps_str){

    cap_t caps = cap_from_text(caps_str);
    if (!caps) {
        perror("caps_set: cap_from_text");
        return -1;
    }
    if (cap_set_proc(caps) == -1) {
        perror("caps_set: cap_set_proc");
        cap_free(caps);
        return -1;
    }
    cap_free(caps);
    return 0;

}

int caps_set_ambient_from_string(const char *caps_str) {
    cap_t caps = cap_from_text(caps_str);
    if (!caps) {
        perror("caps_set_ambient_from_string: cap_from_text");
        return -1;
    }

    for (int cap = 0; cap <= CAP_LAST_CAP; cap++) {
        cap_flag_value_t val;
        if (cap_get_flag(caps, cap, CAP_INHERITABLE, &val) == 0) {
            if (val == CAP_SET) {
                if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, cap, 0, 0) == -1) {
                    perror("caps_set_ambient_from_string: prctl");
                    cap_free(caps);
                    return -1;
                }
            }
        }
    }

    cap_free(caps);
    return 0;
}


int caps_drop_bounding(void){

    for (int cap = 0; cap <= CAP_LAST_CAP; cap++) {
        if (prctl(PR_CAPBSET_DROP, cap, 0, 0, 0) == -1) {
            if (errno == EINVAL) continue; // bu cap desteklenmiyorsa geç
            perror("caps_drop_bounding: prctl");
            return -1;
        }
    }
    return 0;

}


