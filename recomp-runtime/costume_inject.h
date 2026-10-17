#ifndef DOA3_RECOMP_COSTUME_INJECT_H
#define DOA3_RECOMP_COSTUME_INJECT_H

#include "d3d_presenter.h"

#include <stdbool.h>
#include <stdint.h>

/* DOAXBV swimsuits on DOA3's women (docs/doaxbv-swimsuits.md). Each female
   fighter, told apart by her skeleton, can wear her DOAXBV body in any of her
   swimsuits from the neck down while keeping her own DOA3 head, hair and
   animation. Suits come from the recipe pack (RECOMP_COSTUME_PACK) and are
   built from the player's DOAXBV disc (RECOMP_DOAXBV_DISC) when chosen; the
   players switch them live (pad: hold Back with the shoulder buttons and
   triggers; keyboard: 1 / 2). Everyone starts in her DOA3 costume.

   For development, RECOMP_COSTUME_DIR loads converted assets (.dxc, .dxr)
   from a folder and RECOMP_COSTUME_INJECT_P1 / _P2 pin one to a player;
   RECOMP_COSTUME_SUIT=0 replaces the whole fighter, head included. */

#ifdef __cplusplus
extern "C" {
#endif

bool recomp_costume_inject_enabled(void);

/* Called for each world-transformed draw with its WORLD0, view and
   projection. Returns true when the draw belongs to a replaced fighter and
   must not be submitted; the replacement is submitted on that fighter's
   first draw of the present, using the draw as the render-state template. */
bool recomp_costume_inject_draw(uint32_t present, uint32_t vertex_data, const float world[16],
    const float view[16], const float projection[16],
    const RecompD3dPresenterDrawCommand *template_draw,
    const float *template_material /* D3DMATERIAL8, 17 floats, or NULL */);

#ifdef __cplusplus
}
#endif

#endif
