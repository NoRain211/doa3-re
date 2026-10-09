#include "native_fiber.h"
#include "runtime.h"
#include "stop_report.h"

void *recomp_native_fiber_current(void)
{
    void *fiber = IsThreadAFiber() ? GetCurrentFiber()
        : ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
    if (fiber == NULL) recomp_stop(2, "fiber:convert-failed");
    return fiber;
}

void *recomp_native_fiber_create(LPFIBER_START_ROUTINE entry, void *parameter)
{
    return CreateFiberEx(0u, 0x100000u, FIBER_FLAG_FLOAT_SWITCH, entry, parameter);
}

void recomp_native_fiber_switch(void *target)
{
    /* Each native continuation owns its diagnostic call stack, including
       switches between game fibers within a scheduled guest thread. */
    RecompDispatchContext saved;
    recomp_dispatch_context_save(&saved);
    recomp_dispatch_context_clear();
    SwitchToFiber(target);
    recomp_dispatch_context_restore(&saved);
}
