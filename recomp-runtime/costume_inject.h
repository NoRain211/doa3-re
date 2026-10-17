#ifndef DOA3_RECOMP_COSTUME_INJECT_H
#define DOA3_RECOMP_COSTUME_INJECT_H

#include "d3d_presenter.h"

#include <stdbool.h>
#include <stdint.h>

/* Experimental cross-game costume: RECOMP_COSTUME_INJECT=<asset> draws a
   converted DOAX character on DOA3 fighters in place of their own model.
   RECOMP_COSTUME_FIGHTERS=<mask> picks fighters (bit 0 = first, default 3).
   The asset is built offline from a DOAX mesh dump and stays private. */

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
