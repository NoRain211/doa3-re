#include "costume_inject.h"

#include "d3d_draw_model.h"
#include "d3d_frame_adapter.h"
#include "input_host_win32.h"
#include "input_pulse_source.h"
#include "overlay_text.h"
#include "suit_recipe.h"
#include "runtime.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <dirent.h>
#endif

/* DOA3 keeps each fighter's 23 bone matrices (row-vector, 64 bytes each) in
   consecutive banks; a fighter part's WORLD0 is a verbatim copy of one.
   Measured in the attract demo: 0x004785D0 and 0x00478B90. */
/* Slots 2 and 3 are tag partners (third bank referenced by game code); they
   wear their team's (slot & 1) costume. Banks that never match cost nothing.
   Story cutscenes reuse these banks for other characters (Tina's intro puts
   the newspaper reader in slot 0 and Tina in slot 2); suit mode tells the
   women apart by skeleton (female_row), so extras and men keep their models. */
enum { FIGHTERS = 4, TEAMS = 2, DOA3_BONES = 23, DOAX_BONES = 32, MAX_WORLDS = 4 };
static const uint32_t bank_base[FIGHTERS] = {0x004785d0u, 0x00478b90u, 0x00479150u, 0x00479710u};

typedef struct Texture {
    RecompD3dTextureDesc desc;
    uint8_t *bytes, *palette;
    uint32_t byte_count, palette_count;
} Texture;

typedef struct Part {
    uint32_t stride, fvf, primitive, vertex_count, index_count, flags, world_count;
    int32_t texture;
    uint32_t bones[MAX_WORLDS];
    float offsets[MAX_WORLDS][16];
    uint8_t *vertices;
    uint16_t *indices;
    float *skinned; /* vertex_count * 8 floats: position, normal, uv */
    bool has_material;
    float material[17]; /* source D3DMATERIAL8: diffuse, ambient, specular, emissive, power */
    /* Soft part (hair, chest): spring stiffness, damping, gain, offset limit,
       and a per-vertex 0-1 jiggle mask learned from the source's own motion. */
    bool has_alpha_test, alpha_test;
    uint32_t alpha_func, alpha_ref;
    bool soft;
    float soft_params[4];
    float source_rotation[9]; /* bone rotation (rows) when the rest shape was captured */
    float anchor[3];          /* bone-local hair root */
    bool hang;
    float *mask;
    uint32_t file_index; /* order in the asset file (parts are re-sorted at load) */
} Part;

typedef struct Asset {
    uint32_t relative_count;
    uint32_t relative_bone[16], relative_parent[16];
    float relative[16][16];
    uint32_t hierarchy_count;
    uint32_t hierarchy_bone[32], hierarchy_parent[32];
    float hierarchy_offset[32][3];
    Texture *textures;
    uint32_t texture_count;
    Part *parts;
    uint32_t part_count;
    /* Suit mode: where the source head sits on the fighter's, along the
       head's up and forward axes (metres), from doax-<girl>-head.txt. */
    float head_up, head_forward;
} Asset;

static struct {
    bool configured, enabled, keep_head;
    unsigned fighter_mask;
    Asset *fighter_asset[FIGHTERS];
    uint32_t injected_present[FIGHTERS];
    /* Lighting of the fighter's last lit draw: the draw that triggers the
       replacement may be an unlit part (eyes, effects). */
    bool have_lit[FIGHTERS];
    RecompD3dDirectionalLighting lit[FIGHTERS];
    float lit_material[FIGHTERS][17];
    /* Suit mode: the fighter's own face material (from her largest opaque
       face draw lately), worn by the source's skin so the neck seam matches. */
    float skin_material[FIGHTERS][17];
    uint32_t skin_indices[FIGHTERS], skin_present[FIGHTERS];
    /* Per fighter: soft-part spring state (offset, velocity) and the last two
       positions of each bone, for acceleration. */
    float *soft_state[FIGHTERS];
    float history[FIGHTERS][2][DOAX_BONES][3];
    uint32_t history_present[FIGHTERS], history_count[FIGHTERS];
    /* Vertex-data span of each fighter's own draws, by present: props from the
       same loaded model (shoe straps, ribbons) fall inside it. */
    uint32_t span_present[FIGHTERS], span_low[FIGHTERS], span_high[FIGHTERS];
    uint32_t last_low[FIGHTERS], last_high[FIGHTERS];
} inject;

/* DOAX bone-local -> DOA3 bone-local rotation, row vectors (v3 = vX * C).
   Torso, head, hands and feet share axes. Limb rows map the bone direction
   and the knee/elbow bend axis (cross of the two segment directions at the
   most bent sample) between the games; hands and feet were confirmed by
   matching the ankle/wrist relative rotations. */
static const int8_t correction[15][9] = {
    {1,0,0, 0,1,0, 0,0,1}, {1,0,0, 0,1,0, 0,0,1}, {1,0,0, 0,1,0, 0,0,1},
    {1,0,0, 0,1,0, 0,0,1},                      /* 3 left foot */
    {0,0,-1, 0,1,0, 1,0,0},                     /* 4 left forearm */
    {1,0,0, 0,1,0, 0,0,1},                      /* 5 left hand */
    {0,-1,0, 0,0,-1, 1,0,0},                    /* 6 left thigh */
    {0,-1,0, 0,0,1, -1,0,0},                    /* 7 left shin */
    {0,0,1, 0,1,0, -1,0,0},                     /* 8 left upper arm */
    {1,0,0, 0,1,0, 0,0,1},                      /* 9 right foot */
    {0,0,1, 0,1,0, -1,0,0},                     /* 10 right forearm */
    {1,0,0, 0,1,0, 0,0,1},                      /* 11 right hand */
    {0,1,0, 0,0,-1, -1,0,0},                    /* 12 right thigh */
    {0,1,0, 0,0,1, 1,0,0},                      /* 13 right shin */
    {0,0,-1, 0,1,0, 1,0,0},                     /* 14 right upper arm */
};

static void multiply(const float a[16], const float b[16], float out[16])
{
    float r[16];
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j) {
            double s = 0;
            for (unsigned k = 0; k < 4; ++k) s += (double)a[i*4+k] * b[k*4+j];
            r[i*4+j] = (float)s;
        }
    memcpy(out, r, sizeof r);
}

static void invert_affine(const float m[16], float out[16])
{
    /* Rotation may carry scale; invert the 3x3 generally. */
    const float *a = m;
    double det = a[0]*(a[5]*a[10]-a[6]*a[9]) - a[1]*(a[4]*a[10]-a[6]*a[8]) + a[2]*(a[4]*a[9]-a[5]*a[8]);
    double inv = det != 0 ? 1.0/det : 0;
    float r[16] = {0};
    r[0] = (float)((a[5]*a[10]-a[6]*a[9])*inv); r[1] = (float)((a[2]*a[9]-a[1]*a[10])*inv); r[2] = (float)((a[1]*a[6]-a[2]*a[5])*inv);
    r[4] = (float)((a[6]*a[8]-a[4]*a[10])*inv); r[5] = (float)((a[0]*a[10]-a[2]*a[8])*inv); r[6] = (float)((a[2]*a[4]-a[0]*a[6])*inv);
    r[8] = (float)((a[4]*a[9]-a[5]*a[8])*inv); r[9] = (float)((a[1]*a[8]-a[0]*a[9])*inv); r[10] = (float)((a[0]*a[5]-a[1]*a[4])*inv);
    for (unsigned c = 0; c < 3; ++c)
        r[12+c] = -(m[12]*r[c] + m[13]*r[4+c] + m[14]*r[8+c]);
    r[15] = 1;
    memcpy(out, r, sizeof r);
}

/* Mirrors animation_skeleton.c swing(): the rotation taking row a to row b. */
static void swing(const float a[3], const float b[3], float m[16])
{
    memset(m, 0, 64); m[15] = 1;
    double c = (double)a[0]*b[0] + (double)a[1]*b[1] + (double)a[2]*b[2];
    float axis[3] = {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
    if (1.0 + c <= 1e-6) {
        unsigned least = 0;
        for (unsigned i = 1; i < 3; ++i) if (fabsf(a[i]) < fabsf(a[least])) least = i;
        float basis[3] = {0}; basis[least] = 1;
        float x[3] = {a[1]*basis[2]-a[2]*basis[1], a[2]*basis[0]-a[0]*basis[2], a[0]*basis[1]-a[1]*basis[0]};
        float l = sqrtf(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]);
        for (unsigned i = 0; i < 3; ++i) x[i] /= l;
        for (unsigned r = 0; r < 3; ++r)
            for (unsigned k = 0; k < 3; ++k) m[r*4+k] = 2.0f*x[r]*x[k] - (r == k);
        return;
    }
    for (unsigned r = 0; r < 3; ++r)
        for (unsigned k = 0; k < 3; ++k)
            m[r*4+k] = (float)((r == k ? c : 0.0) + (double)axis[r]*axis[k]/(1.0 + c));
    for (unsigned i = 0; i < 3; ++i) {
        unsigned u = (i+1)%3, v = (i+2)%3;
        m[u*4+v] += axis[i]; m[v*4+u] -= axis[i];
    }
}

/* Mirrors animation_skeleton.c blend(). */
static void blend(const float a[16], const float b[16], float w, float out[16])
{
    float at[16] = {0}, rel[16], rot[16] = {0};
    for (unsigned r = 0; r < 3; ++r) for (unsigned c = 0; c < 3; ++c) at[r*4+c] = a[c*4+r];
    at[15] = 1;
    multiply(b, at, rel);
    float axis[3] = {rel[6]-rel[9], rel[8]-rel[2], rel[1]-rel[4]};
    float len = sqrtf(axis[0]*axis[0]+axis[1]*axis[1]+axis[2]*axis[2]);
    if (len > 0) for (unsigned i = 0; i < 3; ++i) axis[i] /= len;
    float cosine = (rel[10]+rel[5]+rel[0]-1)*0.5f;
    double angle = atan2((double)len*0.5, cosine)*w;
    float s = (float)sin(angle), c = (float)cos(angle);
    for (unsigned r = 0; r < 3; ++r)
        for (unsigned k = 0; k < 3; ++k) rot[r*4+k] = (r == k ? c : 0) + (1-c)*axis[r]*axis[k];
    for (unsigned i = 0; i < 3; ++i) {
        unsigned u = (i+1)%3, v = (i+2)%3;
        rot[u*4+v] += s*axis[i]; rot[v*4+u] -= s*axis[i];
    }
    rot[15] = 1;
    float result[16];
    multiply(rot, a, result);
    for (unsigned i = 12; i < 15; ++i) result[i] = (1-w)*a[i] + w*b[i];
    result[15] = 1;
    memcpy(out, result, sizeof result);
}

/* DOAX's 32-bone bank posed by a DOA3 skeleton (see derived_joints). */
static void doax_bank(const Asset *a, const float doa3[DOA3_BONES][16], float b[DOAX_BONES][16])
{
    memset(b, 0, sizeof(float) * DOAX_BONES * 16);
    for (unsigned i = 0; i < 15; ++i) {
        float c[16] = {0};
        for (unsigned r = 0; r < 3; ++r) for (unsigned k = 0; k < 3; ++k) c[r*4+k] = correction[i][r*3+k];
        c[15] = 1;
        multiply(c, doa3[i], b[i]);
    }
    /* Keep DOA3's rotations but rebuild positions from the source's own bone
       offsets, parents first, so joint skins built for the source proportions
       still close. DOAX-only bones take their whole source-frame transform. */
    for (uint32_t h = 0; h < a->hierarchy_count; ++h) {
        const uint32_t bone = a->hierarchy_bone[h], parent = a->hierarchy_parent[h];
        int relative = -1, extra = -1;
        for (uint32_t i = 0; i < a->relative_count; ++i) {
            if (a->relative_bone[i] != bone) continue;
            if (a->relative_parent[i] == parent) relative = (int)i; else extra = (int)i;
        }
        if (relative >= 0) {
            multiply(a->relative[relative], b[parent], b[bone]);
            /* A second relative (the neck to the head): DOA3 has no neck, so
               carry it halfway between the chest and the head it joins. */
            static int neck = -1;
            if (neck < 0) { const char *t = getenv("RECOMP_COSTUME_NECK"); neck = t ? atoi(t) : 50; }
            if (extra >= 0 && a->relative_parent[extra] < 15 && neck > 0) {
                float carried[16], mid[16];
                multiply(a->relative[extra], b[a->relative_parent[extra]], carried);
                blend(b[bone], carried, (float)neck / 100.0f, mid);
                memcpy(mid + 12, b[bone] + 12, 3 * sizeof(float));
                memcpy(b[bone], mid, sizeof mid);
            }
            continue;
        }
        const float *o = a->hierarchy_offset[h], *pm = b[parent];
        for (unsigned c = 0; c < 3; ++c)
            b[bone][12+c] = o[0]*pm[c] + o[1]*pm[4+c] + o[2]*pm[8+c] + pm[12+c];
    }
    for (uint32_t i = 0; i < a->relative_count; ++i) {
        bool placed = false;
        for (uint32_t h = 0; h < a->hierarchy_count; ++h) placed |= a->hierarchy_bone[h] == a->relative_bone[i];
        if (!placed) multiply(a->relative[i], b[a->relative_parent[i]], b[a->relative_bone[i]]);
    }
    static const unsigned links[4][3] = {{26,16,8}, {27,20,14}, {24,5,4}, {25,11,10}};
    for (unsigned i = 0; i < 4; ++i) {
        float s[16];
        swing(b[links[i][1]], b[links[i][2]], s);
        multiply(b[links[i][1]], s, b[links[i][0]]);
        memcpy(b[links[i][0]]+12, b[links[i][2]]+12, 4*sizeof(float));
    }
    blend(b[8], b[26], 0.5f, b[30]);
    blend(b[14], b[27], 0.5f, b[31]);
    blend(b[4], b[24], 0.5f, b[4]);
    blend(b[10], b[25], 0.5f, b[10]);
    memcpy(b[24], b[4], 64); memcpy(b[25], b[10], 64);
    memcpy(b[28], b[24], 64); memcpy(b[29], b[25], 64);
    memcpy(b[28]+12, b[5]+12, 16); memcpy(b[29]+12, b[11]+12, 16);
}

/* Suit mode keeps the fighter's own head: put the source head joint on the
   fighter's, so the source neck piece (blended between neck and head)
   stretches from the body up to the fighter's head. */
static void attach_head(const Asset *a, const float doa3[DOA3_BONES][16], float b[DOAX_BONES][16])
{
    if (getenv("RECOMP_COSTUME_DEBUG")) {
        float g = 0; for (unsigned c = 0; c < 3; ++c) g += (doa3[1][12+c]-b[1][12+c])*(doa3[1][12+c]-b[1][12+c]);
        static unsigned n; if (n++ % 300 == 0) fprintf(stderr, "recomp costume head gap: %.3f\n", sqrtf(g));
    }
    /* The two games put the head joint at different points of the same
       face: relative to its joint, DOAX Kasumi's face sits 6.75 cm lower and
       1.75 cm further back than DOA3 Kasumi's (fitted to 0.7 mm). Seat the
       source head, and so the top of her neck, where her face would cover
       the fighter's. Each girl's offset comes with her suits. */
    float up = a->head_up, forward = a->head_forward;
    const char *t = getenv("RECOMP_COSTUME_HEAD_UP"); if (t) up = (float)atof(t) / 100.0f;
    t = getenv("RECOMP_COSTUME_HEAD_FORWARD"); if (t) forward = (float)atof(t) / 100.0f;
    float natural[3]; memcpy(natural, b[1] + 12, sizeof natural);
    for (unsigned c = 0; c < 3; ++c) b[1][12+c] = doa3[1][12+c] + up * doa3[1][4+c] + forward * doa3[1][8+c];
    /* DOA3's torso can be longer than the source's (Kasumi: about 2 cm),
       which would stretch her neck to reach the head. Raise her upper body
       along the spine by that much instead; the waist skin, already blended
       between hip and chest, takes up the difference. */
    float w[3], rise = 0;
    for (unsigned c = 0; c < 3; ++c) w[c] = b[1][12+c] - natural[c];
    for (unsigned c = 0; c < 3; ++c) rise += w[c] * b[0][4+c];
    static int torso = -1;
    if (torso < 0) { const char *t = getenv("RECOMP_COSTUME_TORSO_FIX"); torso = !(t && *t == '0'); }
    if (getenv("RECOMP_COSTUME_DEBUG")) {
        static unsigned n; if (n++ % 120 == 0) fprintf(stderr, "recomp costume neck stretch along spine: %.3f\n", rise);
    }
    if (!torso) return;
    rise = fminf(fmaxf(rise, 0.0f), 0.04f);
    static const unsigned upper[] = {0, 4, 5, 8, 10, 11, 14, 16, 17, 19, 20, 21, 24, 25, 26, 27, 28, 29, 30, 31};
    float up_axis[3] = {b[0][4], b[0][5], b[0][6]};
    for (size_t i = 0; i < sizeof upper / sizeof upper[0]; ++i)
        for (unsigned c = 0; c < 3; ++c) b[upper[i]][12+c] += rise * up_axis[c];
    for (unsigned c = 0; c < 3; ++c) b[23][12+c] += 0.5f * rise * up_axis[c];
}

/* DOA3 neckwear (Kasumi's chokers, collars) is a separate piece on the head
   bone that lies wholly in a band just under the face. Suit mode hides it:
   the source neck takes its place. Faces and hair reach well above it. In
   fights the head pieces share one vertex buffer, so test only the vertices
   this draw's indices use. */
static bool neckwear(const RecompD3dPresenterDrawCommand *d)
{
    static const void *last_vertices, *last_indices; static uint32_t last_count; static bool last;
    if (d->vertex_bytes == last_vertices && d->index_bytes == last_indices && d->index_count == last_count) return last;
    last_vertices = d->vertex_bytes; last_indices = d->index_bytes; last_count = d->index_count; last = false;
    if (!d->vertex_bytes || !d->index_bytes || d->vertex_stride < 12 || !d->index_count || d->index_count > 3000) return false;
    const uint8_t *v = d->vertex_bytes;
    const uint16_t *idx = d->index_bytes;
    for (uint32_t i = 0; i < d->index_count; ++i) {
        if (idx[i] >= d->vertex_count) return false;
        float p[3];
        memcpy(p, v + (size_t)idx[i] * d->vertex_stride, sizeof p);
        if (p[1] > 0.05f || p[1] < -0.04f || p[0]*p[0] + p[2]*p[2] > 0.09f*0.09f) return false;
    }
    last = true;
    return true;
}

/* An asset image in memory: a .dxc file as read, or one built from a .dxr
   recipe and the player's DOAXBV disc (suit_recipe.c). */
typedef struct Image { uint8_t *p; size_t n, at; } Image;
static bool read_all(Image *f, void *p, size_t n)
{
    if (f->at + n > f->n) return false;
    memcpy(p, f->p + f->at, n); f->at += n;
    return true;
}

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    uint8_t *p = NULL;
    if (fseek(f, 0, SEEK_END) == 0) {
        long n = ftell(f);
        if (n > 0 && fseek(f, 0, SEEK_SET) == 0 && (p = malloc((size_t)n)) && fread(p, 1, (size_t)n, f) != (size_t)n) { free(p); p = NULL; }
        if (p) *size = (size_t)n;
    }
    fclose(f);
    return p;
}

/* The player's DOAXBV disc (the folder holding datahdd.afs and datadvd.afs),
   from RECOMP_DOAXBV_DISC; opened on first use. */
static RecompSuitDisc *doaxbv_disc(void)
{
    static RecompSuitDisc *disc; static bool tried;
    if (!tried) {
        tried = true;
        const char *dir = getenv("RECOMP_DOAXBV_DISC");
        if (dir && *dir) disc = recomp_suit_disc_open(dir);
        fprintf(stderr, "recomp costume inject: DOAXBV disc %s%s\n", dir ? dir : "(RECOMP_DOAXBV_DISC unset)", disc ? "" : " not usable");
    }
    return disc;
}

static RecompSuitPack *suit_pack;

static bool open_image(Image *f, const char *path)
{
    size_t n = 0;
    uint8_t *p = NULL;
    const bool packed = strncmp(path, "pack:", 5) == 0;
    if (packed) { if (!recomp_suit_pack_recipe(suit_pack, (uint32_t)atoi(path + 5), &p, &n)) return false; }
    else p = read_file(path, &n);
    if (!p) return false;
    const size_t len = strlen(path);
    if (packed || (len > 4 && strcmp(path + len - 4, ".dxr") == 0)) {
        uint8_t *image = NULL; size_t size = 0;
        const bool ok = doaxbv_disc() && recomp_suit_recipe_build(p, n, doaxbv_disc(), &image, &size);
        free(p);
        if (!ok) return false;
        p = image; n = size;
    }
    f->p = p; f->n = n; f->at = 0;
    return true;
}

static bool load(Asset *a, unsigned slot, const char *path)
{
    Image image, *f = &image;
    if (!open_image(f, path)) return false;
    char magic[4];
    uint32_t head[4];
    bool ok = read_all(f, magic, 4) && memcmp(magic, "DXC1", 4) == 0 &&
        read_all(f, head, sizeof head) && head[0] >= 2u && head[0] <= 6u && head[1] <= 16u;
    if (ok) {
        a->relative_count = head[1];
        for (uint32_t i = 0; ok && i < head[1]; ++i)
            ok = read_all(f, &a->relative_bone[i], 4) && read_all(f, &a->relative_parent[i], 4) &&
                read_all(f, a->relative[i], 64) &&
                a->relative_bone[i] < DOAX_BONES && a->relative_parent[i] < DOAX_BONES;
        ok = ok && read_all(f, &a->hierarchy_count, 4) && a->hierarchy_count <= 32u;
        for (uint32_t i = 0; ok && i < a->hierarchy_count; ++i)
            ok = read_all(f, &a->hierarchy_bone[i], 4) && read_all(f, &a->hierarchy_parent[i], 4) &&
                read_all(f, a->hierarchy_offset[i], 12) &&
                a->hierarchy_bone[i] < DOAX_BONES && a->hierarchy_parent[i] < DOAX_BONES;
        a->texture_count = head[2];
        a->textures = calloc(head[2] ? head[2] : 1, sizeof(Texture));
        for (uint32_t i = 0; ok && i < head[2]; ++i) {
            uint32_t t[9];
            Texture *x = &a->textures[i];
            ok = read_all(f, t, sizeof t);
            if (!ok) break;
            x->desc.format_byte = t[0]; x->desc.width = t[1]; x->desc.height = t[2];
            x->desc.mip_levels = t[3]; x->desc.bits_per_pixel = t[4]; x->desc.linear = t[5] != 0;
            x->desc.pitch = t[6];
            /* Outside guest RAM and its aliases, so it cannot collide with a game texture key. */
            x->desc.data = 0xd0000000u + slot * 0x00800000u + i * 0x00008000u;
            x->byte_count = t[7]; x->palette_count = t[8];
            x->bytes = malloc(t[7] ? t[7] : 1); x->palette = t[8] ? malloc(t[8]) : NULL;
            ok = read_all(f, x->bytes, t[7]) && (!t[8] || read_all(f, x->palette, t[8]));
        }
        a->part_count = head[3];
        a->parts = calloc(head[3] ? head[3] : 1, sizeof(Part));
        for (uint32_t i = 0; ok && i < head[3]; ++i) {
            Part *p = &a->parts[i];
            p->file_index = i;
            uint32_t h[8];
            ok = read_all(f, h, sizeof h) && h[7] >= 1u && h[7] <= MAX_WORLDS;
            if (!ok) break;
            p->stride = h[0]; p->fvf = h[1]; p->primitive = h[2]; p->vertex_count = h[3];
            p->index_count = h[4]; p->texture = (int32_t)h[5]; p->flags = h[6]; p->world_count = h[7];
            for (uint32_t w = 0; ok && w < p->world_count; ++w)
                ok = read_all(f, &p->bones[w], 4) && read_all(f, p->offsets[w], 64) && p->bones[w] < DOAX_BONES;
            if (ok && head[0] >= 3u) {
                uint32_t present_flag = 0;
                ok = read_all(f, &present_flag, 4) && read_all(f, p->material, sizeof p->material);
                p->has_material = present_flag != 0;
            }
            if (ok && head[0] >= 5u) {
                uint32_t t[3];
                ok = read_all(f, t, sizeof t);
                p->has_alpha_test = true; p->alpha_test = t[0] != 0; p->alpha_func = t[1]; p->alpha_ref = t[2];
            }
            if (ok && head[0] >= 4u && (p->flags & 4u)) {
                p->soft = true;
                p->mask = malloc((size_t)p->vertex_count * sizeof(float) + 4);
                ok = read_all(f, p->soft_params, sizeof p->soft_params);
                if (ok && head[0] >= 6u) {
                    ok = read_all(f, p->source_rotation, sizeof p->source_rotation) &&
                        read_all(f, p->anchor, sizeof p->anchor);
                    p->hang = ok;
                }
                ok = ok && read_all(f, p->mask, (size_t)p->vertex_count * sizeof(float));
            }
            p->vertices = malloc((size_t)p->vertex_count * p->stride + 1);
            p->indices = malloc((size_t)p->index_count * 2 + 2);
            p->skinned = malloc((size_t)p->vertex_count * 32 + 32);
            ok = ok && read_all(f, p->vertices, (size_t)p->vertex_count * p->stride) &&
                read_all(f, p->indices, (size_t)p->index_count * 2) &&
                (p->texture < 0 || (uint32_t)p->texture < a->texture_count);
        }
    }
    free(image.p);
    if (ok) {
        /* Opaque parts first so blended hair and lashes composite over them. */
        for (uint32_t i = 0; i < a->part_count; ++i)
            for (uint32_t j = i + 1; j < a->part_count; ++j)
                if ((a->parts[i].flags & 1u) > (a->parts[j].flags & 1u)) {
                    Part t = a->parts[i]; a->parts[i] = a->parts[j]; a->parts[j] = t;
                }
    }
    return ok;
}

/* Asset pool: every converted suit the session may use. A girl can wear
   about 300, so suits load when chosen into a small cache and the least
   recently used one not on a fighter is freed. Each cache slot owns a
   range of texture keys; the presenter checks texture contents, so a slot
   reused by another suit uploads its textures afresh. */
enum { POOL = 4096, CACHE = 12 };
static struct {
    char *path[POOL];
    char *name[POOL];   /* "Acacia", from the girl's suits.tsv; NULL if unknown */
    int girl[POOL];     /* index into girls[] from doax-<girl>-sNNN.dxc; -1 for other assets */
    int suit_id[POOL];  /* the DOAXBV suit number, -1 if unknown */
    bool fav[POOL];     /* a favorite of its girl (saved, see favorites_save) */
    bool has_head[POOL];
    float head[POOL][2];  /* head offset (m) from the pack */
    bool failed[POOL];
    uint32_t count;
    struct { int index; Asset *asset; uint64_t used; } cache[CACHE];
    uint64_t clock;
    int choice[TEAMS];  /* worn: -1 stock, else pool index */
    int pending[TEAMS]; /* shown while scrolling; worn once the scroll settles */
    uint64_t pending_ms[TEAMS];
    uint32_t key_present;
    uint64_t overlay_until; /* show the suit names until then (ms) */
    /* Each team's fighter, as a DOAX girl (-1 unknown or not one of hers);
       a team with a fixed asset (RECOMP_COSTUME_INJECT_P*) ignores it. */
    int team_girl[TEAMS];
    bool fixed[TEAMS];
    bool fav_only[TEAMS];   /* scrolling visits only the girl's favorites */
    char favorites_path[1024];
} pool;

/* DOA3's women by their DOAX names; Kasumi and Ayane share a skeleton. */
static const char *const girls[] = {"christie", "hitomi", "tina", "kasumi", "helena", "leifang", "ayane"};
enum { GIRL_KASUMI = 3, GIRL_AYANE = 6, GIRLS = 7 };

static int pool_add(const char *path)
{
    for (uint32_t i = 0; i < pool.count; ++i) if (strcmp(pool.path[i], path) == 0) return (int)i;
    if (pool.count == POOL) return -1;
    pool.path[pool.count] = strdup(path);
    pool.girl[pool.count] = -1;
    pool.suit_id[pool.count] = -1;
    const char *b = strrchr(path, '/'); b = b ? b + 1 : path;
    for (int g = 0; g < GIRLS && !strncmp(b, "doax-", 5); ++g) {
        size_t n = strlen(girls[g]);
        if (!strncmp(b + 5, girls[g], n) && b[5 + n] == '-' && b[6 + n] == 's' && b[7 + n] >= '0' && b[7 + n] <= '9')
            { pool.girl[pool.count] = g; pool.suit_id[pool.count] = atoi(b + 7 + n); }
    }
    return (int)pool.count++;
}

static void free_asset(Asset *a)
{
    if (!a) return;
    for (uint32_t i = 0; i < a->texture_count; ++i) { free(a->textures[i].bytes); free(a->textures[i].palette); }
    for (uint32_t i = 0; i < a->part_count; ++i) {
        Part *p = &a->parts[i];
        free(p->vertices); free(p->indices); free(p->skinned); free(p->mask);
    }
    free(a->textures); free(a->parts); free(a);
}

static bool worn(const Asset *a)
{
    for (unsigned f = 0; f < FIGHTERS; ++f) if (inject.fighter_asset[f] == a) return true;
    return false;
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/* doax-<girl>-sNNN.dxc (or doax-<girl>.dxc) -> doax-<girl>-head.txt: "up forward" in cm.
   Without one, Kasumi's measured offset. */
static void head_offset(Asset *a, const char *path)
{
    a->head_up = 0.0675f; a->head_forward = 0.0175f;
    char file[1024];
    const char *b = base_name(path), *dash = strchr(b + 5, '-'), *dot = strrchr(b, '.');
    size_t girl = (size_t)((dash && dash < dot ? dash : dot) - b);
    snprintf(file, sizeof file, "%.*s%.*s-head.txt", (int)(b - path), path, (int)girl, b);
    FILE *f = fopen(file, "r");
    if (!f) return;
    float up, fwd;
    if (fscanf(f, "%f %f", &up, &fwd) == 2) { a->head_up = up / 100.0f; a->head_forward = fwd / 100.0f; }
    fclose(f);
}

static Asset *pool_get(int index)
{
    if (index < 0 || (uint32_t)index >= pool.count || pool.failed[index]) return NULL;
    ++pool.clock;
    int slot = -1;
    for (int k = 0; k < CACHE; ++k)
        if (pool.cache[k].asset && pool.cache[k].index == index) { pool.cache[k].used = pool.clock; return pool.cache[k].asset; }
    for (int k = 0; k < CACHE; ++k) {
        if (pool.cache[k].asset && worn(pool.cache[k].asset)) continue;
        if (slot < 0 || !pool.cache[k].asset || (pool.cache[slot].asset && pool.cache[k].used < pool.cache[slot].used)) slot = k;
        if (!pool.cache[k].asset) break;
    }
    if (slot < 0) return NULL;
    Asset *a = calloc(1, sizeof *a);
    bool ok = a && load(a, (unsigned)slot, pool.path[index]) && a->part_count > 0;
    if (!ok) {
        fprintf(stderr, "recomp costume inject: failed to load %s\n", pool.path[index]);
        free_asset(a); pool.failed[index] = true; return NULL;
    }
    head_offset(a, pool.path[index]);
    if (pool.has_head[index]) { a->head_up = pool.head[index][0]; a->head_forward = pool.head[index][1]; }
    free_asset(pool.cache[slot].asset);
    pool.cache[slot].asset = a; pool.cache[slot].index = index; pool.cache[slot].used = pool.clock;
    return a;
}


static const char *suit_label(int index)
{
    if (index < 0) return "stock";
    return pool.name[index] ? pool.name[index] : base_name(pool.path[index]);
}

static void choose(unsigned f, int index)
{
    pool.choice[f] = pool.pending[f] = index;
    Asset *a = pool_get(index);
    for (unsigned slot = f; slot < FIGHTERS; slot += TEAMS) {
        inject.fighter_asset[slot] = a;
        free(inject.soft_state[slot]);
        inject.soft_state[slot] = NULL;
        inject.history_count[slot] = 0;
    }
    fprintf(stderr, "recomp costume inject: P%u -> %s\n", f + 1, suit_label(index));
}

/* Scrolling moves the pending suit; it is worn once the scroll rests. */
static void scroll(unsigned f, int steps, uint64_t now)
{
    if (!steps || pool.fixed[f]) return;
    const int g = pool.team_girl[f];
    if (g < 0) return;
    /* Stock, then the girl's suits in file (suit number) order. */
    static int list[POOL + 1];
    int n = 0, at = 0;
    list[n++] = -1;
    for (uint32_t i = 0; i < pool.count; ++i)
        if (pool.girl[i] == g && (!pool.fav_only[f] || pool.fav[i])) { if ((int)i == pool.pending[f]) at = n; list[n++] = (int)i; }
    if (n == 1) { pool.overlay_until = now + 2000u; return; }
    at = ((at + steps) % n + n) % n;
    pool.pending[f] = list[at];
    pool.pending_ms[f] = now;
    pool.overlay_until = now + 2000u;
}

/* Favorites: "<girl> <suit number>" lines in suit-favorites.txt beside the
   recipe pack (or RECOMP_COSTUME_FAVORITES), rewritten on every change. */
static void favorites_save(void)
{
    if (!pool.favorites_path[0]) return;
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.tmp", pool.favorites_path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    for (uint32_t i = 0; i < pool.count; ++i)
        if (pool.fav[i] && pool.girl[i] >= 0 && pool.suit_id[i] >= 0) fprintf(f, "%s %d\n", girls[pool.girl[i]], pool.suit_id[i]);
    fclose(f);
    remove(pool.favorites_path);
    rename(tmp, pool.favorites_path);
}

static void favorites_load(void)
{
    const char *named = getenv("RECOMP_COSTUME_FAVORITES"), *pack = getenv("RECOMP_COSTUME_PACK");
    if (named && *named) snprintf(pool.favorites_path, sizeof pool.favorites_path, "%s", named);
    else if (pack && *pack) {
        const char *slash = strrchr(pack, '/'), *bslash = strrchr(pack, '\\');
        if (bslash > slash) slash = bslash;
        snprintf(pool.favorites_path, sizeof pool.favorites_path, "%.*ssuit-favorites.txt", slash ? (int)(slash - pack + 1) : 0, pack);
    }
    FILE *f = pool.favorites_path[0] ? fopen(pool.favorites_path, "r") : NULL;
    if (!f) return;
    char name[32]; int id;
    while (fscanf(f, "%31s %d", name, &id) == 2)
        for (uint32_t i = 0; i < pool.count; ++i)
            if (pool.suit_id[i] == id && pool.girl[i] >= 0 && strcmp(girls[pool.girl[i]], name) == 0) pool.fav[i] = true;
    fclose(f);
}

static void toggle_favorite(unsigned f, uint64_t now)
{
    const int i = pool.choice[f];
    pool.overlay_until = now + 2000u;
    if (i < 0 || pool.fixed[f] || pool.girl[i] < 0) return;
    pool.fav[i] = !pool.fav[i];
    favorites_save();
    fprintf(stderr, "recomp costume inject: P%u %s %s\n", f + 1, suit_label(i), pool.fav[i] ? "is a favorite" : "is no longer a favorite");
}

static void toggle_favorites_only(unsigned f, uint64_t now)
{
    pool.fav_only[f] = !pool.fav_only[f];
    pool.overlay_until = now + 2000u;
}

/* The girl's first favorite, or -1. */
static int first_favorite(int g)
{
    for (uint32_t i = 0; i < pool.count; ++i) if (pool.girl[i] == g && pool.fav[i]) return (int)i;
    return -1;
}

/* A held switch repeats after 350 ms every 60 ms, three suits a step after
   1.5 s and ten after 3.5 s. */
typedef struct { bool down; uint64_t since, last; } Hold;
static int held_steps(Hold *h, bool down, uint64_t now)
{
    if (!down) { h->down = false; return 0; }
    if (!h->down) { h->down = true; h->since = h->last = now; return 1; }
    const uint64_t held = now - h->since;
    if (held < 350u || now - h->last < 60u) return 0;
    h->last = now;
    return held > 3500u ? 10 : held > 1500u ? 3 : 1;
}

/* Pads: holding Back, RB / LB switch the pad's own fighter (port 0 plays
   P1, port 1 P2) and RT / LT the opponent, so one player can dress both. */
enum { PADS = 4 };
static struct { bool back, lb, rb, lt, rt, x, y; } pad_state[PADS];

static bool pad_hook(uint32_t port, const RecompInputGamepad *g)
{
    if (port >= PADS) return false;
    const bool back = (g->buttons & 0x0020u) != 0;   /* XInput BACK */
    pad_state[port].back = back;
    pad_state[port].lb = g->analog_buttons[RECOMP_INPUT_ANALOG_BLACK] > 0x80u;
    pad_state[port].rb = g->analog_buttons[RECOMP_INPUT_ANALOG_WHITE] > 0x80u;
    pad_state[port].lt = g->analog_buttons[RECOMP_INPUT_ANALOG_LTRIG] > 0x80u;
    pad_state[port].rt = g->analog_buttons[RECOMP_INPUT_ANALOG_RTRIG] > 0x80u;
    pad_state[port].x = g->analog_buttons[RECOMP_INPUT_ANALOG_X] > 0x80u;
    pad_state[port].y = g->analog_buttons[RECOMP_INPUT_ANALOG_Y] > 0x80u;
    return back;
}

static void load_names(const char *dir)
{
    char path[1024];
    for (uint32_t i = 0; i < pool.count; ++i) {
        /* doax-<girl>-sNNN.dxc -> doax-<girl>-suits.tsv, line "NNN<TAB>Name" */
        const char *b = base_name(pool.path[i]);
        const char *s = strstr(b, "-s");
        if (!s || !dir) continue;
        int id = atoi(s + 2);
        snprintf(path, sizeof path, "%s/%.*s-suits.tsv", dir, (int)(s - b), b);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        char line[256];
        while (fgets(line, sizeof line, f)) {
            if (atoi(line) != id) continue;
            char *tab = strchr(line, '\t');
            if (!tab) break;
            tab[strcspn(tab, "\r\n")] = 0;
            pool.name[i] = strdup(tab + 1);
            break;
        }
        fclose(f);
    }
}

#ifndef _WIN32
static int compare_names(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static void scan_directory(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) return;
    char *names[POOL]; unsigned n = 0;
    for (struct dirent *e; (e = readdir(d)) != NULL && n < POOL;) {
        size_t len = strlen(e->d_name);
        /* RECOMP_COSTUME_ONLY=<prefix> keeps one girl's suits (doax-kasumi*). */
        const char *only = getenv("RECOMP_COSTUME_ONLY");
        if (only && *only && strncmp(e->d_name, only, strlen(only)) != 0) continue;
        if (len > 4 && (strcmp(e->d_name + len - 4, ".dxc") == 0 || strcmp(e->d_name + len - 4, ".dxr") == 0)) {
            char *full = malloc(strlen(dir) + len + 2);
            sprintf(full, "%s/%s", dir, e->d_name);
            names[n++] = full;
        }
    }
    closedir(d);
    qsort(names, n, sizeof *names, compare_names);
    for (unsigned i = 0; i < n; ++i) { pool_add(names[i]); free(names[i]); }
}
#endif

bool recomp_costume_inject_enabled(void)
{
    if (!inject.configured) {
        inject.configured = true;
        const char *both = getenv("RECOMP_COSTUME_INJECT");
        const char *dir = getenv("RECOMP_COSTUME_DIR");
        const char *mask = getenv("RECOMP_COSTUME_FIGHTERS");
        inject.fighter_mask = mask ? (unsigned)strtoul(mask, NULL, 0) : 3u;
        const char *suit = getenv("RECOMP_COSTUME_SUIT");
        inject.keep_head = !(suit && *suit == '0');
#ifndef _WIN32
        if (dir && *dir) scan_directory(dir);
        /* The shipped pack of suit recipes: every girl's suits, built from the
           player's DOAXBV disc when chosen. */
        const char *pack_path = getenv("RECOMP_COSTUME_PACK");
        if (pack_path && *pack_path && (suit_pack = recomp_suit_pack_open(pack_path))) {
            for (uint32_t i = 0; i < recomp_suit_pack_suit_count(suit_pack); ++i) {
                const RecompSuitPackSuit *su = recomp_suit_pack_suit(suit_pack, i);
                const RecompSuitPackGirl *gi = recomp_suit_pack_girl(suit_pack, su->girl);
                char key[32]; snprintf(key, sizeof key, "pack:%u", i);
                const int index = pool_add(key);
                if (index < 0) break;
                pool.girl[index] = -1;
                for (int g = 0; g < GIRLS; ++g) if (strcmp(girls[g], gi->name) == 0) pool.girl[index] = g;
                pool.name[index] = strdup(su->name);
                pool.suit_id[index] = (int)su->suit;
                pool.has_head[index] = true;
                pool.head[index][0] = gi->head_up / 100.0f; pool.head[index][1] = gi->head_forward / 100.0f;
            }
            fprintf(stderr, "recomp costume inject: %u suits in %s\n", recomp_suit_pack_suit_count(suit_pack), pack_path);
        }
#endif
        const char *names[TEAMS] = {"RECOMP_COSTUME_INJECT_P1", "RECOMP_COSTUME_INJECT_P2"};
        for (unsigned f = 0; f < FIGHTERS; ++f) inject.injected_present[f] = UINT32_MAX;
        for (unsigned f = 0; f < TEAMS; ++f) {
            inject.injected_present[f] = UINT32_MAX;
            pool.choice[f] = pool.pending[f] = -1;
            pool.team_girl[f] = -1;
            const char *path = getenv(names[f]);
            if (!path || !*path) path = both;
            if (!path || !*path || !(inject.fighter_mask & (1u << f))) continue;
            int index = pool_add(path);
            pool.fixed[f] = true;
            pool.choice[f] = pool.pending[f] = index;
            inject.fighter_asset[f] = inject.fighter_asset[f + TEAMS] = pool_get(index);
        }
        load_names(dir);
        favorites_load();
        /* Test option: RECOMP_COSTUME_FAVORITES_ONLY=1 starts both players browsing favorites only. */
        const char *fav_only = getenv("RECOMP_COSTUME_FAVORITES_ONLY");
        if (fav_only && *fav_only == '1') pool.fav_only[0] = pool.fav_only[1] = true;
        inject.enabled = pool.count > 0;
        if (inject.enabled) recomp_input_host_set_combo_hook(pad_hook);
        if (inject.enabled)
            fprintf(stderr, "recomp costume inject: %u suits available; 1 / 2 (Shift back) or Back + RB / LB (own) and RT / LT (opponent) on a pad\n", pool.count);
    }
    return inject.enabled;
}

static void poll_keys(uint32_t present)
{
    if (pool.key_present == present) return;
    pool.key_present = present;
    const uint64_t now = SDL_GetTicks();
    /* Test hook: RECOMP_COSTUME_CYCLE_AT=<present>[,...] wears P1's next suit at those presents. */
    static const char *cycle_at;
    static bool cycle_read;
    if (!cycle_read) { cycle_read = true; cycle_at = getenv("RECOMP_COSTUME_CYCLE_AT"); }
    for (const char *c = cycle_at; c && *c;) {
        char *end;
        unsigned long at = strtoul(c, &end, 10);
        if (end == c) break;
        if (at == present) { scroll(0, 1, now); choose(0, pool.pending[0]); }
        c = *end == ',' ? end + 1 : end;
    }
    /* Test hook: RECOMP_COSTUME_FAVORITE_AT=<present>[,...] marks P1's suit at those presents. */
    static const char *fav_at; static bool fav_read; static uint32_t fav_fired;
    if (!fav_read) { fav_read = true; fav_at = getenv("RECOMP_COSTUME_FAVORITE_AT"); }
    unsigned fav_n = 0;
    for (const char *c = fav_at; c && *c && fav_n < 32; ++fav_n) {
        char *end;
        unsigned long at = strtoul(c, &end, 10);
        if (end == c) break;
        if (at <= present && !(fav_fired & (1u << fav_n))) { fav_fired |= 1u << fav_n; toggle_favorite(0, now); }
        c = *end == ',' ? end + 1 : end;
    }
    /* Keyboard: 1 / 2 (or F5 / F6) cycle P1 / P2, Shift backwards. */
    static Hold keys_hold[TEAMS];
    const bool *keys = SDL_GetKeyboardState(NULL);
    if (keys) {
        const SDL_Scancode codes[TEAMS] = {SDL_SCANCODE_F5, SDL_SCANCODE_F6};
        const SDL_Scancode digits[TEAMS] = {SDL_SCANCODE_1, SDL_SCANCODE_2};
        const int dir = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT] ? -1 : 1;
        for (unsigned f = 0; f < TEAMS; ++f)
            scroll(f, dir * held_steps(&keys_hold[f], keys[codes[f]] || keys[digits[f]], now), now);
    }
    /* Favorites: F / Shift+F mark P1's / P2's suit, G / Shift+G show only
       favorites; on a pad, Back + Y and Back + X for the pad's own fighter. */
    if (keys) {
        static bool f_down, g_down;
        const unsigned who = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT] ? 1u : 0u;
        if (keys[SDL_SCANCODE_F] && !f_down) toggle_favorite(who, now);
        if (keys[SDL_SCANCODE_G] && !g_down) toggle_favorites_only(who, now);
        f_down = keys[SDL_SCANCODE_F]; g_down = keys[SDL_SCANCODE_G];
    }
    static bool pad_y[PADS], pad_x[PADS];
    for (unsigned port = 0; port < PADS; ++port) {
        const bool y = pad_state[port].back && pad_state[port].y, x = pad_state[port].back && pad_state[port].x;
        if (y && !pad_y[port]) toggle_favorite(port & 1u, now);
        if (x && !pad_x[port]) toggle_favorites_only(port & 1u, now);
        pad_y[port] = y; pad_x[port] = x;
    }
    static Hold pad_hold[PADS][4];
    for (unsigned port = 0; port < PADS; ++port) {
        const bool b = pad_state[port].back;
        const unsigned own = port & 1u, other = own ^ 1u;
        scroll(own, held_steps(&pad_hold[port][0], b && pad_state[port].rb, now), now);
        scroll(own, -held_steps(&pad_hold[port][1], b && pad_state[port].lb, now), now);
        scroll(other, held_steps(&pad_hold[port][2], b && pad_state[port].rt, now), now);
        scroll(other, -held_steps(&pad_hold[port][3], b && pad_state[port].lt, now), now);
    }
    /* Wear the pending suit once the scroll has rested for 150 ms. */
    for (unsigned f = 0; f < TEAMS; ++f)
        if (pool.pending[f] != pool.choice[f] && now - pool.pending_ms[f] >= 150u) choose(f, pool.pending[f]);
    /* Both players' suits, while switching and for two seconds after. */
    static bool shown;
    RecompOverlayText text;
    recomp_overlay_clear(&text);
    if (now < pool.overlay_until) {
        recomp_overlay_set_title(&text, "Swimsuit");
        for (unsigned f = 0; f < TEAMS; ++f) {
            const int i = pool.pending[f], g = pool.team_girl[f];
            int pos = 0, total = 0;
            for (uint32_t k = 0; k < pool.count; ++k)
                if (g >= 0 && pool.girl[k] == g && (!pool.fav_only[f] || pool.fav[k])) { ++total; if ((int)k == i) pos = total; }
            const char *mode = pool.fav_only[f] ? " fav" : "";
            if (pool.fixed[f]) recomp_overlay_add_line(&text, "P%u  %s", f + 1, suit_label(i));
            else if (g < 0) recomp_overlay_add_line(&text, "P%u  no suits for this fighter", f + 1);
            else if (!total) recomp_overlay_add_line(&text, "P%u%s  no favorites yet (Back + Y)", f + 1, mode);
            else if (i < 0) recomp_overlay_add_line(&text, "P%u%s  stock", f + 1, mode);
            else recomp_overlay_add_line(&text, "P%u%s  %d / %d  %s%s", f + 1, mode, pos, total, suit_label(i), pool.fav[i] ? "  *" : "");
        }
        recomp_overlay_publish(&text);
        shown = true;
    } else if (shown) {
        recomp_overlay_publish(&text);
        shown = false;
    }
}

/* Rodrigues rotation of v about unit axis k (row-vector frame) by angle. */
static void rotate_axis(float v[3], const float k[3], float angle)
{
    float c = cosf(angle), s = sinf(angle);
    float kv = k[0]*v[0] + k[1]*v[1] + k[2]*v[2];
    float x[3] = {k[1]*v[2] - k[2]*v[1], k[2]*v[0] - k[0]*v[2], k[0]*v[1] - k[1]*v[0]};
    for (unsigned i = 0; i < 3; ++i) v[i] = v[i]*c + x[i]*s + k[i]*kv*(1.0f - c);
}

static void skin(Part *p, float bank[DOAX_BONES][16], const float *soft_offset)
{
    RecompD3dVertexLayout layout;
    if (!recomp_d3d_fvf_layout(p->fvf, &layout)) { p->vertex_count = 0; return; }
    float m[MAX_WORLDS][16];
    for (uint32_t w = 0; w < p->world_count; ++w) multiply(p->offsets[w], bank[p->bones[w]], m[w]);
    const uint32_t weights = layout.blend_weight_count < p->world_count ? layout.blend_weight_count : p->world_count - 1;
    /* Gravity in the bone's frame now and when the rest shape was captured
       (world +Y is up in both games); the swing between them is how far the
       hair must turn about its root to keep hanging. */
    float hang_axis[3] = {0, 0, 0}, hang_angle = 0.0f;
    /* Gravity hang is for hair (soft spring); chest parts only jiggle. */
    if (p->soft && p->hang && p->world_count == 1 && p->soft_params[0] < 100.0f) {
        const float *r = bank[p->bones[0]], *q = p->source_rotation;
        float now[3] = {r[1], r[5], r[9]}, then[3] = {q[1], q[4], q[7]};
        float ln = sqrtf(now[0]*now[0]+now[1]*now[1]+now[2]*now[2]);
        if (ln > 0) for (unsigned c = 0; c < 3; ++c) now[c] /= ln;
        hang_axis[0] = then[1]*now[2] - then[2]*now[1];
        hang_axis[1] = then[2]*now[0] - then[0]*now[2];
        hang_axis[2] = then[0]*now[1] - then[1]*now[0];
        float sine = sqrtf(hang_axis[0]*hang_axis[0]+hang_axis[1]*hang_axis[1]+hang_axis[2]*hang_axis[2]);
        float cosine = then[0]*now[0] + then[1]*now[1] + then[2]*now[2];
        hang_angle = atan2f(sine, cosine);
        if (sine > 1e-6f) for (unsigned c = 0; c < 3; ++c) hang_axis[c] /= sine;
        else hang_angle = 0.0f;
    }
    for (uint32_t v = 0; v < p->vertex_count; ++v) {
        const uint8_t *src = p->vertices + (size_t)v * p->stride;
        float pos[3], nrm[3] = {0, 0, 0}, uv[2] = {0, 0}, wt[MAX_WORLDS] = {1, 0, 0, 0};
        memcpy(pos, src, 12);
        if (layout.normal_offset != RECOMP_D3D_FVF_ABSENT) memcpy(nrm, src + layout.normal_offset, 12);
        if (layout.texcoord_offset != RECOMP_D3D_FVF_ABSENT) memcpy(uv, src + layout.texcoord_offset, 8);
        if (weights) {
            float rest = 1;
            memcpy(wt, src + 12, weights * 4);
            for (uint32_t k = 0; k < weights; ++k) rest -= wt[k];
            wt[weights] = rest;
        }
        float *out = p->skinned + (size_t)v * 8;
        memset(out, 0, 6 * sizeof(float));
        for (uint32_t k = 0; k <= weights; ++k) {
            const float *a = m[k];
            for (unsigned c = 0; c < 3; ++c) {
                out[c] += wt[k] * (pos[0]*a[c] + pos[1]*a[4+c] + pos[2]*a[8+c] + a[12+c]);
                out[3+c] += wt[k] * (nrm[0]*a[c] + nrm[1]*a[4+c] + nrm[2]*a[8+c]);
            }
        }
        if (p->soft && p->world_count > 1 && soft_offset && p->mask[v] > 0.0f) {
            /* Blended part on the chest (a swimsuit top): same spring offset as
               the chest skin, in the chest bone's frame. */
            const float *r = bank[0], w = p->mask[v];
            for (unsigned c = 0; c < 3; ++c)
                out[c] += w * (soft_offset[0]*r[c] + soft_offset[1]*r[4+c] + soft_offset[2]*r[8+c]);
        } else if (p->soft && p->world_count == 1 && p->mask[v] > 0.0f) {
            /* Redo this vertex in bone-local space: hang toward gravity, then jiggle. */
            const float w = p->mask[v];
            const float *o = p->offsets[0], *r = bank[p->bones[0]];
            float local[3], ln[3];
            for (unsigned c = 0; c < 3; ++c) {
                local[c] = pos[0]*o[c] + pos[1]*o[4+c] + pos[2]*o[8+c] + o[12+c];
                ln[c] = nrm[0]*o[c] + nrm[1]*o[4+c] + nrm[2]*o[8+c];
            }
            if (hang_angle > 1e-3f) {
                float d[3] = {local[0]-p->anchor[0], local[1]-p->anchor[1], local[2]-p->anchor[2]};
                rotate_axis(d, hang_axis, hang_angle * w);
                rotate_axis(ln, hang_axis, hang_angle * w);
                for (unsigned c = 0; c < 3; ++c) local[c] = p->anchor[c] + d[c];
            }
            if (soft_offset)
                for (unsigned c = 0; c < 3; ++c) local[c] += w * soft_offset[c];
            for (unsigned c = 0; c < 3; ++c) {
                out[c] = local[0]*r[c] + local[1]*r[4+c] + local[2]*r[8+c] + r[12+c];
                out[3+c] = ln[0]*r[c] + ln[1]*r[4+c] + ln[2]*r[8+c];
            }
        }
        float l = sqrtf(out[3]*out[3] + out[4]*out[4] + out[5]*out[5]);
        if (l > 0) for (unsigned c = 3; c < 6; ++c) out[c] /= l;
        out[6] = uv[0]; out[7] = uv[1];
    }
}

/* The template's lit material is emissive + ambient * scene ambient (plus
   per-light terms). Recover the scene terms through the template's own raw
   material and apply them to this part's source material. */
static float ratio(float value, float template_term, float part_term)
{
    return fabsf(template_term) > 1e-4f ? value / template_term * part_term : part_term * 0.0f;
}

static void relight(RecompD3dDirectionalLighting *l, const float *t, const float *material)
{
    if (!l->enabled || !t || !material) return;
    for (unsigned c = 0; c < 3; ++c) {
        float scene = fabsf(t[4 + c]) > 1e-4f ? (l->ambient_emissive[c] - t[12 + c]) / t[4 + c] : 0.0f;
        l->ambient_emissive[c] = material[12 + c] + material[4 + c] * scene;
        for (unsigned i = 0; i < l->count; ++i)
            l->ambient[i][c] = ratio(l->ambient[i][c], t[4 + c], material[4 + c]);
        for (unsigned i = 0; i < l->count; ++i)
            l->specular[i][c] = ratio(l->specular[i][c], t[8 + c], material[8 + c]);
    }
    memcpy(l->material_diffuse, material, 4 * sizeof(float));
}

static bool head_bone(int b) { return b == 1 || b == 15 || b == 16; }

/* Suit mode skips the source girl's head: her face, eyes and hair (bound to
   the head or eyes) and pendulum hair carried by the chest. */
static bool head_part(const Part *p)
{
    const uint32_t b = p->bones[0];
    if (p->world_count == 1 && (b == 1 || b == 17 || b == 21)) return true;
    return p->world_count == 1 && b == 0 && p->soft && p->soft_params[0] < 100.0f;
}

/* One damped spring per soft part, in its bone's frame, pushed opposite to
   the bone's acceleration (inertia). Two substeps per present at 60 Hz. */
static void step_soft(unsigned f, Asset *a, float bank[DOAX_BONES][16], uint32_t present)
{
    if (!inject.soft_state[f]) inject.soft_state[f] = calloc((size_t)a->part_count * 6u + 6u, sizeof(float));
    if (inject.history_present[f] + 1u != present) inject.history_count[f] = 0;
    float (*h)[DOAX_BONES][3] = inject.history[f];
    /* A scene cut or teleport moves the hip further in one frame than any
       motion does; restart the springs at rest instead of kicking them. */
    if (inject.history_count[f] >= 1u) {
        float jump = 0;
        for (unsigned c = 0; c < 3; ++c) jump += (bank[2][12+c] - h[1][2][c]) * (bank[2][12+c] - h[1][2][c]);
        if (jump > 0.25f * 0.25f) {
            inject.history_count[f] = 0;
            memset(inject.soft_state[f], 0, ((size_t)a->part_count * 6u + 6u) * sizeof(float));
        }
    }
    const float dt = 1.0f / 60.0f;
    static float scale = -1.0f;
    if (scale < 0.0f) {
        const char *text = getenv("RECOMP_COSTUME_JIGGLE");
        scale = text ? (float)atof(text) : 0.7f;
        if (!(scale >= 0.0f)) scale = 1.0f;
    }
    for (uint32_t i = 0; i < a->part_count; ++i) {
        Part *p = &a->parts[i];
        if (!p->soft) continue;
        float *s = inject.soft_state[f] + i * 6u, *v = s + 3;
        /* Blended chest parts spring with the chest bone, like the skin under them. */
        uint32_t b = p->bones[0];
        for (uint32_t k = 1; k < p->world_count; ++k) if (p->bones[k] == 0) b = 0;
        float accel[3] = {0, 0, 0}, local[3];
        if (inject.history_count[f] >= 2u)
            for (unsigned c = 0; c < 3; ++c)
                accel[c] = (bank[b][12+c] - 2.0f*h[1][b][c] + h[0][b][c]) / (dt*dt);
        /* Cap at about 8 g: a single bad sample must not pin the spring at its limit. */
        float amag = sqrtf(accel[0]*accel[0] + accel[1]*accel[1] + accel[2]*accel[2]);
        if (amag > 80.0f) for (unsigned c = 0; c < 3; ++c) accel[c] *= 80.0f / amag;
        for (unsigned c = 0; c < 3; ++c)
            local[c] = accel[0]*bank[b][c*4] + accel[1]*bank[b][c*4+1] + accel[2]*bank[b][c*4+2];
        const float k = p->soft_params[0], damping = p->soft_params[1], gain = p->soft_params[2] * scale, limit = p->soft_params[3];
        for (unsigned step = 0; step < 2; ++step) {
            const float t = dt * 0.5f;
            for (unsigned c = 0; c < 3; ++c) {
                v[c] += (-k*s[c] - damping*v[c] - gain*local[c]) * t;
                s[c] += v[c] * t;
            }
        }
        float len = sqrtf(s[0]*s[0] + s[1]*s[1] + s[2]*s[2]);
        if (len > limit) for (unsigned c = 0; c < 3; ++c) { s[c] *= limit / len; v[c] *= 0.5f; }
    }
    if (getenv("RECOMP_COSTUME_DEBUG") && present % 20u == 0u) {
        float hair = 0, chest = 0;
        for (uint32_t i = 0; i < a->part_count; ++i) {
            if (!a->parts[i].soft) continue;
            const float *s = inject.soft_state[f] + i * 6u;
            float len = sqrtf(s[0]*s[0] + s[1]*s[1] + s[2]*s[2]);
            if (a->parts[i].bones[0] == 1u) hair = fmaxf(hair, len); else chest = fmaxf(chest, len);
        }
        fprintf(stderr, "recomp costume physics: present=%u fighter=%u hair=%.4f chest=%.4f\n", present, f, hair, chest);
    }
    memcpy(h[0], h[1], sizeof h[0]);
    for (unsigned b = 0; b < DOAX_BONES; ++b) memcpy(h[1][b], bank[b] + 12, 3 * sizeof(float));
    if (inject.history_count[f] < 2u) ++inject.history_count[f];
    inject.history_present[f] = present;
}

/* DOAX body skin carries one material (white diffuse and ambient, specular
   0.15 0.10 0.10); suits and accessories differ. */
static bool skin_part(const Part *p)
{
    static int match = -1;
    if (match < 0) { const char *t = getenv("RECOMP_COSTUME_SKIN_MATCH"); match = !(t && *t == '0'); }
    const float *m = p->material;
    if (!match) return false;
    return p->has_material && fabsf(m[0] - 1.0f) < 0.01f && fabsf(m[4] - 1.0f) < 0.01f &&
        fabsf(m[8] - 0.15f) < 0.01f && fabsf(m[9] - 0.10f) < 0.01f && fabsf(m[10] - 0.10f) < 0.01f;
}

static void submit_fighter(unsigned fighter, Asset *a, const float *template_material, float bank[DOAX_BONES][16], const float view[16], const float projection[16],
    const RecompD3dPresenterDrawCommand *template_draw)
{
    float view_projection[16];
    multiply(view, projection, view_projection);
    static const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    for (uint32_t i = 0; i < a->part_count; ++i) {
        Part *p = &a->parts[i];
        if (!p->vertex_count || !p->index_count) continue;
        if (inject.keep_head && head_part(p)) continue;
        /* Debug: RECOMP_COSTUME_SKIP=a-b leaves out asset-file parts a..b (bisecting a bad part). */
        static int skip_lo = -2, skip_hi = -1;
        if (skip_lo == -2) { const char *t = getenv("RECOMP_COSTUME_SKIP"); skip_lo = -1; if (t) sscanf(t, "%d-%d", &skip_lo, &skip_hi); }
        if ((int)p->file_index >= skip_lo && (int)p->file_index <= skip_hi) continue;
        skin(p, bank, p->soft && inject.soft_state[fighter] ? inject.soft_state[fighter] + i * 6u : NULL);
        RecompD3dPresenterCommand command;
        memset(&command, 0, sizeof command);
        command.type = RECOMP_D3D_PRESENTER_COMMAND_DRAW;
        RecompD3dPresenterDrawCommand *d = &command.data.draw;
        *d = *template_draw;
        d->primitive_type = p->primitive;
        d->index_count = p->index_count;
        d->triangle_count = recomp_d3d_draw_triangle_count(p->primitive, p->index_count);
        d->vertex_count = p->vertex_count;
        d->vertex_stride = 32;
        d->fvf = 0x112u;
        d->program_count = 0;
        d->vertex_bytes = p->skinned;
        d->index_bytes = p->indices;
        memcpy(d->transform, view_projection, sizeof view_projection);
        d->has_transform = true;
        d->blend_weight_count = 0;
        d->cull_mode = RECOMP_D3D_CULL_NONE;
        if (d->directional.enabled) {
            memcpy(d->directional.world_transforms[0], identity, sizeof identity);
            memcpy(d->directional.normal_transforms[0], identity, sizeof identity);
            relight(&d->directional, template_material, skin_part(p) && inject.skin_indices[fighter] ? inject.skin_material[fighter] : p->has_material ? p->material : NULL);
        }
        d->has_alpha_mask = false;
        d->has_reflection = false;
        d->use_texture_factor = false;
        d->modulate_texture_factor = false;
        d->texture_is_backbuffer = false;
        d->texture_is_front_buffer = false;
        d->has_texture = p->texture >= 0;
        if (d->has_texture) {
            const Texture *t = &a->textures[p->texture];
            d->texture = t->desc;
            d->texture_bytes = t->bytes;
            d->texture_byte_count = t->byte_count;
            d->palette_bytes = t->palette;
            d->palette_byte_count = t->palette_count;
        }
        if (p->flags & 1u) {
            d->blend.blend_enable = true;
            d->blend.src_factor = RECOMP_D3D_BLEND_SRC_ALPHA;
            d->blend.dst_factor = RECOMP_D3D_BLEND_INV_SRC_ALPHA;
            d->blend.op = RECOMP_D3D_BLEND_OP_ADD;
        } else {
            d->blend.blend_enable = false;
        }
        d->blend.color_write_mask = 15;
        d->depth.depth_write_enable = (p->flags & 2u) != 0;
        if (p->has_alpha_test) {
            d->depth.alpha_test_enable = p->alpha_test;
            d->depth.alpha_func = (RecompD3dCompareFunc)p->alpha_func;
            d->depth.alpha_ref = p->alpha_ref;
        }
        recomp_d3d_presenter_submit(recomp_d3d_frame_adapter_presenter(), &command);
    }
}

/* DOA3's female fighters, by skeleton: bone lengths (upper arm, forearm,
   thigh, shin, hip-chest, chest-head) are fixed per character, measured in
   the attract demo. Suit mode dresses only these; a cutscene extra or a male
   fighter in the same bank keeps the stock model. */
static const float female_skeletons[][6] = {
    {0.2625f, 0.2616f, 0.4300f, 0.4352f, 0.2200f, 0.3128f},   /* Christie */
    {0.2470f, 0.2460f, 0.4000f, 0.4158f, 0.1851f, 0.3026f},   /* Hitomi */
    {0.2540f, 0.2419f, 0.3860f, 0.4424f, 0.2003f, 0.2786f},   /* Tina */
    {0.2397f, 0.2387f, 0.4033f, 0.4193f, 0.1801f, 0.2891f},   /* Kasumi, Ayane */
    {0.2707f, 0.2252f, 0.3860f, 0.4423f, 0.2003f, 0.2786f},   /* Helena */
    {0.2156f, 0.2145f, 0.4150f, 0.4144f, 0.1962f, 0.2827f},   /* Lei Fang */
};

static int female_row(const float bones[DOA3_BONES][16])
{
    static const unsigned seg[6][2] = {{8,4}, {4,5}, {6,7}, {7,3}, {2,0}, {0,1}};
    float len[6];
    for (unsigned i = 0; i < 6; ++i) {
        float d = 0;
        for (unsigned c = 0; c < 3; ++c) d += (bones[seg[i][0]][12+c] - bones[seg[i][1]][12+c]) * (bones[seg[i][0]][12+c] - bones[seg[i][1]][12+c]);
        len[i] = sqrtf(d);
    }
    for (size_t k = 0; k < sizeof female_skeletons / sizeof female_skeletons[0]; ++k) {
        bool same = true;
        for (unsigned i = 0; i < 6 && same; ++i) same = fabsf(len[i] - female_skeletons[k][i]) < 0.002f;
        if (same) return (int)k;
    }
    return -1;
}

/* Rows of female_skeletons as girls (row 3 is Kasumi or Ayane). */
static const int row_girl[] = {0, 1, 2, GIRL_KASUMI, 4, 5};

/* Kasumi or Ayane: Ayane's hair is purple. Counts purple texels (blue above
   red and green) in the colour endpoints of a head draw's DXT blocks, or in
   its pixels when uncompressed. */
static void hair_sample(unsigned f, const RecompD3dPresenterDrawCommand *d, uint32_t present);
static struct { uint32_t present; uint32_t purple, total; bool ayane; } hair[FIGHTERS];

static void hair_sample(unsigned f, const RecompD3dPresenterDrawCommand *d, uint32_t present)
{
    if (!d->has_texture || !d->texture_bytes || !d->texture_byte_count) return;
    if (hair[f].present != present) {
        if (hair[f].total > 2000u) hair[f].ayane = hair[f].purple * 20u > hair[f].total;
        hair[f].present = present; hair[f].purple = hair[f].total = 0;
    }
    const uint8_t *t = d->texture_bytes;
    const uint32_t fmt = d->texture.format_byte, bytes = d->texture_byte_count;
    const bool dxt1 = fmt == RECOMP_D3D_TEXTURE_FORMAT_DXT1;
    const bool dxt = dxt1 || fmt == RECOMP_D3D_TEXTURE_FORMAT_DXT3 || fmt == RECOMP_D3D_TEXTURE_FORMAT_DXT5;
    if (dxt) {
        const uint32_t block = dxt1 ? 8u : 16u, colour = dxt1 ? 0u : 8u;
        for (uint32_t o = 0; o + block <= bytes; o += block * 4u) {
            for (unsigned e = 0; e < 2; ++e) {
                const uint16_t c = (uint16_t)(t[o + colour + e * 2] | (t[o + colour + e * 2 + 1] << 8));
                const int r = (c >> 11) << 3, g = ((c >> 5) & 63) << 2, b = (c & 31) << 3;
                hair[f].purple += b > g + 24 && b * 4 > r * 3 && r > g; ++hair[f].total;
            }
        }
    } else if (fmt == 0x06u || fmt == 0x12u) {
        for (uint32_t o = 0; o + 4 <= bytes; o += 64u) {
            const int b = t[o], g = t[o + 1], r = t[o + 2];
            hair[f].purple += b > g + 24 && b * 4 > r * 3 && r > g; ++hair[f].total;
        }
    }
}

static void note_span(unsigned f, uint32_t present, uint32_t vertex_data)
{
    if (!vertex_data) return;
    if (inject.span_present[f] != present) {
        if (inject.span_present[f] != 0u) {
            inject.last_low[f] = inject.span_low[f]; inject.last_high[f] = inject.span_high[f];
        }
        inject.span_present[f] = present;
        inject.span_low[f] = vertex_data; inject.span_high[f] = vertex_data;
    }
    if (vertex_data < inject.span_low[f]) inject.span_low[f] = vertex_data;
    if (vertex_data > inject.span_high[f]) inject.span_high[f] = vertex_data;
}

bool recomp_costume_inject_draw(uint32_t present, uint32_t vertex_data, const float world[16],
    const float view[16], const float projection[16],
    const RecompD3dPresenterDrawCommand *template_draw,
    const float *template_material)
{
    if (!recomp_costume_inject_enabled()) return false;
    poll_keys(present);
    for (unsigned f = 0; f < FIGHTERS; ++f) {
        float bones[DOA3_BONES][16];
        recomp_guest_load(bones, bank_base[f], sizeof bones);
        const int row = female_row(bones);
        /* The team's girl, from her skeleton (and hair for Kasumi / Ayane).
           A new girl in the slot starts in her stock costume. */
        if (f < TEAMS && inject.keep_head && !pool.fixed[f] && row >= 0) {
            if (row_girl[row] == GIRL_KASUMI && memcmp(bones[1], world, 64) == 0) hair_sample(f, template_draw, present);
            const int g = row_girl[row] == GIRL_KASUMI && hair[f].ayane ? GIRL_AYANE : row_girl[row];
            if (g != pool.team_girl[f]) {
                pool.team_girl[f] = g;
                /* RECOMP_COSTUME_START_FAVORITE=1: start in her first favorite. */
                static int start_fav = -1;
                if (start_fav < 0) { const char *t = getenv("RECOMP_COSTUME_START_FAVORITE"); start_fav = t && *t == '1'; }
                const int first = start_fav ? first_favorite(g) : -1;
                if (first >= 0) choose(f, first);
                else { if (pool.choice[f] >= 0) choose(f, -1); pool.pending[f] = -1; }
                fprintf(stderr, "recomp costume inject: P%u is %s\n", f + 1, girls[g]);
            }
        }
        if (!inject.fighter_asset[f]) continue;
        if (inject.keep_head && row < 0) continue;
        bool match = false, attached = false;
        int matched_bone = -1, attached_bone = -1;
        for (unsigned i = 0; i < DOA3_BONES && !match; ++i)
            if (memcmp(bones[i], world, 64) == 0) { match = true; matched_bone = (int)i; }
        /* Props hung on a bone (ribbons, shoe straps) share its rotation at an offset. */
        for (unsigned i = 0; i < DOA3_BONES && !match && !attached; ++i) {
            float err = 0, dist = 0;
            for (unsigned k = 0; k < 12; ++k) err = fmaxf(err, fabsf(bones[i][k] - world[k]));
            for (unsigned k = 12; k < 15; ++k) dist += (bones[i][k] - world[k]) * (bones[i][k] - world[k]);
            attached = err < 1e-4f && dist < 0.25f && fabsf(bones[i][0]) + fabsf(bones[i][5]) + fabsf(bones[i][10]) < 2.999f;
            if (attached) attached_bone = (int)i;
        }
        /* Suit mode: the fighter keeps its own head, face, hair and eyes
           (DOA3 animates and simulates them); only the body is replaced. */
        if (inject.keep_head && matched_bone == 1 && neckwear(template_draw)) return true;
        if (inject.keep_head && matched_bone == 1 && template_material && !template_draw->blend.blend_enable && template_draw->has_texture) {
            const float *m = template_material;
            bool skin = true;
            for (unsigned c = 0; c < 3; ++c) skin = skin && m[c] > 0.8f && m[c] < 1.4f && m[4+c] > 0.6f && m[4+c] < 1.5f;
            if (present - inject.skin_present[f] > 120u) inject.skin_indices[f] = 0;
            if (skin && template_draw->index_count >= inject.skin_indices[f]) {
                /* Colour only (diffuse, ambient, emissive); the source keeps her
                   own specular, which DOA3 sets far stronger on faces. */
                memcpy(inject.skin_material[f], m, sizeof inject.skin_material[f]);
                inject.skin_material[f][8] = 0.15f; inject.skin_material[f][9] = 0.10f; inject.skin_material[f][10] = 0.10f;
                inject.skin_material[f][16] = 10.0f;
                inject.skin_indices[f] = template_draw->index_count;
                inject.skin_present[f] = present;
            }
        }
        if (inject.keep_head && (head_bone(matched_bone) || head_bone(attached_bone))) return false;
        if (!match) {
            if (attached) return true;
            /* Same model allocation as last present's matched draws, and near the fighter. */
            if (vertex_data && inject.last_high[f] > inject.last_low[f] &&
                vertex_data >= inject.last_low[f] && vertex_data <= inject.last_high[f]) {
                for (unsigned i = 0; i < DOA3_BONES; ++i) {
                    float d = 0;
                    for (unsigned k = 12; k < 15; ++k) d += (bones[i][k]-world[k])*(bones[i][k]-world[k]);
                    if (d < 0.25f) return true;
                }
            }
            continue;
        }
        note_span(f, present, vertex_data);
        if (template_draw->directional.enabled && template_material) {
            inject.lit[f] = template_draw->directional;
            memcpy(inject.lit_material[f], template_material, sizeof inject.lit_material[f]);
            inject.have_lit[f] = true;
        }
        if (inject.injected_present[f] != present) {
            inject.injected_present[f] = present;
            RecompD3dPresenterDrawCommand lit_template;
            if (!template_draw->directional.enabled && inject.have_lit[f]) {
                lit_template = *template_draw;
                lit_template.directional = inject.lit[f];
                template_draw = &lit_template;
                template_material = inject.lit_material[f];
            }
            float bank[DOAX_BONES][16];
            doax_bank(inject.fighter_asset[f], bones, bank);
            if (inject.keep_head) attach_head(inject.fighter_asset[f], bones, bank);
            step_soft(f, inject.fighter_asset[f], bank, present);
            submit_fighter(f, inject.fighter_asset[f], template_material, bank, view, projection, template_draw);
        }
        return true;
    }
    return false;
}
