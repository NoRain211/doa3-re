#ifndef RECOMP_NATIVE_FIBER_H
#define RECOMP_NATIVE_FIBER_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void *recomp_native_fiber_current(void);
void *recomp_native_fiber_create(LPFIBER_START_ROUTINE entry, void *parameter);
void recomp_native_fiber_switch(void *target);

#endif
