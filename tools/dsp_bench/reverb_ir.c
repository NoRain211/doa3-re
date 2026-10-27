/* Impulse response of the runtime's I3DL2 reverb for one listener. */
#include "dsound_reverb.h"
#include <stdio.h>
#include <stdlib.h>
static RecompReverb reverb;
int main(int argc, char **argv)
{
    if (argc != 15) { fprintf(stderr, "usage: reverb_ir room roomhf rolloff decay hfratio refl refldelay reverb revdelay diffusion density hfref amp out.f32\n"); return 1; }
    RecompI3dl2Listener p = {atoi(argv[1]), atoi(argv[2]), (float)atof(argv[3]), (float)atof(argv[4]), (float)atof(argv[5]),
        atoi(argv[6]), (float)atof(argv[7]), atoi(argv[8]), (float)atof(argv[9]), (float)atof(argv[10]), (float)atof(argv[11]), (float)atof(argv[12])};
    recomp_reverb_set(&reverb, &p);
    enum { N = 96000 };
    static float in[N], out[2 * N];
    in[0] = (float)atof(argv[13]);
    recomp_reverb_render(&reverb, in, N, out);
    FILE *f = fopen(argv[14], "wb"); fwrite(out, sizeof out, 1, f); fclose(f);
    return 0;
}


