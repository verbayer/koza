#ifndef CAPS_H
#define CAPS_H
#include <sys/capability.h>
#include <sys/prctl.h>
#include <stdio.h>
#include <errno.h>

int caps_drop_all(void);

int caps_set(const char *caps_str);

/*
  caps_str formati: "capability=eip"
  Ornek: "cap_net_bind_service=eip cap_chown=eip"
  caps_drop_all() sonrasi cagrilmali, permitted set bos olmali
  e: effective, i: inheritable, p: permitted
 */

int caps_drop_bounding(cap_t allowed_caps);
int caps_set_ambient_from_string(const char *caps_str);

#endif
