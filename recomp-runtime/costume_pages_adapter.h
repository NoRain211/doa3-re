#ifndef DOA3_COSTUME_PAGES_ADAPTER_H
#define DOA3_COSTUME_PAGES_ADAPTER_H

#include "runtime.h"

/* The hand-written costume functions bound to a (USA) guest address, or NULL. */
RecompFunction recomp_costume_pages_lookup(uint32_t address);

#endif
