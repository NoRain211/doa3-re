#include "suit_recipe.h"

#if !defined(_WIN32) && !defined(_FILE_OFFSET_BITS)
#define _FILE_OFFSET_BITS 64
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- the disc's AFS archives -------------------------------------------- */

typedef struct Archive {
    FILE *file;
    uint64_t base;          /* where the archive starts in the file (an ISO image) */
    uint32_t count;
    uint32_t *offset, *size;
} Archive;

struct RecompSuitDisc {
    Archive archive[2]; /* 0 datahdd.afs, 1 datadvd.afs */
};

static bool seek(FILE *f, uint64_t at)
{
#ifdef _WIN32
    return _fseeki64(f, (long long)at, SEEK_SET) == 0;
#else
    return fseeko(f, (off_t)at, SEEK_SET) == 0;
#endif
}

static bool read_archive_table(Archive *a)
{
    uint8_t head[8];
    if (!seek(a->file, a->base) || fread(head, 1, 8, a->file) != 8 || memcmp(head, "AFS", 3) != 0) return false;
    memcpy(&a->count, head + 4, 4);
    a->offset = malloc(a->count * sizeof *a->offset + 1);
    a->size = malloc(a->count * sizeof *a->size + 1);
    if (!a->offset || !a->size) return false;
    for (uint32_t i = 0; i < a->count; ++i) {
        uint32_t e[2];
        if (fread(e, 4, 2, a->file) != 2) return false;
        a->offset[i] = e[0]; a->size[i] = e[1];
    }
    return true;
}

static bool open_archive(Archive *a, const char *directory, const char *name)
{
    char path[2048];
    snprintf(path, sizeof path, "%s/%s", directory, name);
    a->file = fopen(path, "rb");
    a->base = 0;
    return a->file && read_archive_table(a);
}

/* An Xbox disc image (XDVDFS): the volume descriptor is sector 32 of the
   game partition, which starts at 0 in a trimmed image and at one of a few
   fixed offsets in a full one. Directory entries form a binary tree:
   u16 left, right (in dwords), u32 sector, u32 size, u8 attributes, u8 name
   length, name. The archives sit in the root directory. */
static const uint64_t xiso_partitions[] = {0, 0x18300000ull, 0xFD90000ull, 0x2080000ull};

static bool xiso_find(FILE *f, uint64_t partition, uint32_t dir_sector, uint32_t dir_size,
    const char *name, uint64_t *at, uint32_t *size)
{
    uint8_t *dir = malloc(dir_size + 1);
    if (!dir || !seek(f, partition + (uint64_t)dir_sector * 2048u) || fread(dir, 1, dir_size, f) != dir_size) { free(dir); return false; }
    uint32_t stack[256], depth = 0; bool found = false;
    stack[depth++] = 0;
    while (depth && !found) {
        const uint32_t o = stack[--depth] * 4u;
        if (o + 14 > dir_size) continue;
        uint16_t left, right; uint32_t sector, length;
        memcpy(&left, dir + o, 2); memcpy(&right, dir + o + 2, 2);
        memcpy(&sector, dir + o + 4, 4); memcpy(&length, dir + o + 8, 4);
        const uint8_t n = dir[o + 13];
        if (left == 0xFFFFu && right == 0xFFFFu) continue;     /* padding */
        if (o + 14 + n <= dir_size && n == strlen(name)) {
            bool same = true;
            for (uint8_t k = 0; k < n && same; ++k) {
                char a = (char)dir[o + 14 + k], b = name[k];
                if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
                same = a == b;
            }
            if (same) { *at = partition + (uint64_t)sector * 2048u; *size = length; found = true; }
        }
        if (left && depth < 256) stack[depth++] = left;
        if (right && depth < 256) stack[depth++] = right;
    }
    free(dir);
    return found;
}

static bool open_iso(RecompSuitDisc *d, const char *path)
{
    for (size_t p = 0; p < sizeof xiso_partitions / sizeof xiso_partitions[0]; ++p) {
        FILE *f = fopen(path, "rb");
        if (!f) return false;
        uint8_t vd[28];
        const uint64_t part = xiso_partitions[p];
        if (seek(f, part + 32u * 2048u) && fread(vd, 1, sizeof vd, f) == sizeof vd &&
            memcmp(vd, "MICROSOFT*XBOX*MEDIA", 20) == 0) {
            uint32_t root, root_size; memcpy(&root, vd + 20, 4); memcpy(&root_size, vd + 24, 4);
            static const char *const names[2] = {"datahdd.afs", "datadvd.afs"};
            bool ok = true;
            for (int i = 0; i < 2 && ok; ++i) {
                uint32_t size;
                ok = xiso_find(f, part, root, root_size, names[i], &d->archive[i].base, &size);
                if (ok) {
                    d->archive[i].file = i == 0 ? f : fopen(path, "rb");
                    ok = d->archive[i].file && read_archive_table(&d->archive[i]);
                }
            }
            if (ok) return true;
            if (!d->archive[0].file) fclose(f);
            return false;
        }
        fclose(f);
    }
    return false;
}

RecompSuitDisc *recomp_suit_disc_open(const char *directory)
{
    RecompSuitDisc *d = calloc(1, sizeof *d);
    if (!d) return NULL;
    /* A disc image (.iso), or a folder holding the extracted archives. */
    FILE *probe = fopen(directory, "rb");
    bool image = false;
    if (probe) { uint8_t b[4]; image = fread(b, 1, 4, probe) == 4; fclose(probe); }
    if (image ? !open_iso(d, directory) :
        (!open_archive(&d->archive[0], directory, "datahdd.afs") ||
         !open_archive(&d->archive[1], directory, "datadvd.afs"))) {
        recomp_suit_disc_close(d);
        return NULL;
    }
    return d;
}

void recomp_suit_disc_close(RecompSuitDisc *d)
{
    if (!d) return;
    for (int i = 0; i < 2; ++i) {
        if (d->archive[i].file) fclose(d->archive[i].file);
        free(d->archive[i].offset); free(d->archive[i].size);
    }
    free(d);
}

/* Bytes [offset, offset + n) of an archive entry. */
static bool disc_read(RecompSuitDisc *d, uint32_t arc, uint32_t entry, uint32_t offset, void *out, size_t n)
{
    if (arc > 1) return false;
    Archive *a = &d->archive[arc];
    if (entry >= a->count || (uint64_t)offset + n > a->size[entry]) return false;
    if (!seek(a->file, a->base + a->offset[entry] + offset)) return false;
    return fread(out, 1, n, a->file) == n;
}

/* ---- recipe reading and image writing ----------------------------------- */

typedef struct Cursor { const uint8_t *p; size_t n, at; bool ok; } Cursor;

static bool take(Cursor *c, void *out, size_t n)
{
    if (!c->ok || c->at + n > c->n) { c->ok = false; if (out) memset(out, 0, n); return false; }
    if (out) memcpy(out, c->p + c->at, n);
    c->at += n;
    return true;
}
static uint32_t u32(Cursor *c) { uint32_t v = 0; take(c, &v, 4); return v; }

typedef struct Loc { uint32_t arc, entry, offset; } Loc;
static Loc loc(Cursor *c)
{
    uint8_t a[4] = {0}; Loc l;
    take(c, a, 4); l.arc = a[0]; l.entry = u32(c); l.offset = u32(c);
    return l;
}

typedef struct Out { uint8_t *p; size_t n, cap; bool ok; } Out;
static void put(Out *o, const void *data, size_t n)
{
    if (!o->ok) return;
    if (o->n + n > o->cap) {
        size_t cap = o->cap ? o->cap * 2 : 1u << 20;
        while (cap < o->n + n) cap *= 2;
        uint8_t *p = realloc(o->p, cap);
        if (!p) { o->ok = false; return; }
        o->p = p; o->cap = cap;
    }
    memcpy(o->p + o->n, data, n);
    o->n += n;
}

/* ---- vertex layout (mirror of the converter's fvf_layout) --------------- */

typedef struct Layout { uint32_t weights; int normal; uint32_t stride; } Layout;
static Layout layout(uint32_t fvf, uint32_t stride)
{
    Layout l;
    const uint32_t position = fvf & 0x0Eu;
    l.weights = position >= 0x06u ? (position - 4u) / 2u : 0u;
    uint32_t offset = 12u + 4u * l.weights;
    l.normal = (fvf & 0x10u) ? (int)offset : -1;
    l.stride = stride;
    return l;
}

/* Row-vector transforms: v' = (v, 1) M, normals v M3. */
static void xform(const float v[3], const float m[16], float out[3], bool point)
{
    float r[3];
    for (int c = 0; c < 3; ++c)
        r[c] = v[0] * m[c] + v[1] * m[4 + c] + v[2] * m[8 + c] + (point ? m[12 + c] : 0.0f);
    memcpy(out, r, sizeof r);
}

/* Vertex rows a piece's provenance produces: n rows of `stride` bytes and
   their FVF. */
typedef struct Rows { uint8_t *data; uint32_t n, stride, fvf; } Rows;

static bool provenance(Cursor *c, RecompSuitDisc *d, Rows *r);

static bool trim_own(Cursor *c, Rows *r, uint32_t **own_out, uint32_t *own_count)
{
    const uint32_t count = u32(c);
    if (!c->ok) return false;
    uint32_t *own = malloc((size_t)count * 4 + 4);
    if (!own) return false;
    take(c, own, (size_t)count * 4);
    if (!c->ok) { free(own); return false; }
    if (count) {
        uint8_t *t = malloc((size_t)count * r->stride);
        if (!t) { free(own); return false; }
        for (uint32_t i = 0; i < count; ++i) {
            if (own[i] >= r->n) { free(t); free(own); return false; }
            memcpy(t + (size_t)i * r->stride, r->data + (size_t)own[i] * r->stride, r->stride);
        }
        free(r->data); r->data = t; r->n = count;
    }
    if (own_out) { *own_out = own; *own_count = count; } else free(own);
    return true;
}

/* The innermost copy/record step's own list (index trimming) is returned
   through these when asked. */
static uint32_t *g_own; static uint32_t g_own_count;

static bool provenance(Cursor *c, RecompSuitDisc *d, Rows *r)
{
    uint8_t kind[4] = {0};
    take(c, kind, 4);
    if (!c->ok) return false;
    if (kind[0] == 1) {                                   /* copy */
        Loc l = loc(c); uint32_t len = u32(c); r->stride = u32(c); r->fvf = u32(c);
        if (!c->ok || !r->stride) return false;
        r->n = len / r->stride;
        r->data = malloc((size_t)r->n * r->stride + 1);
        if (!r->data || !disc_read(d, l.arc, l.entry, l.offset, r->data, (size_t)r->n * r->stride)) return false;
        free(g_own); g_own = NULL; g_own_count = 0;
        return trim_own(c, r, &g_own, &g_own_count);
    }
    if (kind[0] == 2) {                                   /* CPU-skinned file record */
        Loc l = loc(c); uint32_t type = u32(c), nv = u32(c), has_a = u32(c);
        float A[16]; take(c, A, sizeof A);
        r->stride = u32(c); r->fvf = u32(c);
        const uint32_t welds = u32(c);
        if (!c->ok || type > 4 || r->stride < 32) return false;
        const uint32_t fst = 32u + 4u * type, cw = (r->stride - 32u) / 4u;
        uint8_t *file = malloc((size_t)nv * fst + 1);
        r->n = nv;
        r->data = calloc((size_t)nv, r->stride);
        if (!file || !r->data || !disc_read(d, l.arc, l.entry, l.offset, file, (size_t)nv * fst)) { free(file); return false; }
        if (!has_a) { memset(A, 0, sizeof A); A[0] = A[5] = A[10] = A[15] = 1.0f; }
        for (uint32_t i = 0; i < nv; ++i) {
            const uint8_t *s = file + (size_t)i * fst;
            uint8_t *o = r->data + (size_t)i * r->stride;
            float p[3], n[3], w[4] = {0};
            memcpy(p, s, 12); memcpy(w, s + 12, 4 * type); memcpy(n, s + 12 + 4 * type, 12);
            xform(p, A, p, true); xform(n, A, n, false);
            const float len = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) + 1e-12f;
            for (int k = 0; k < 3; ++k) n[k] /= len;
            memcpy(o, p, 12);
            for (uint32_t k = 0; k < cw; ++k) { float v = k < type ? w[k] : 0.0f; memcpy(o + 12 + 4 * k, &v, 4); }
            memcpy(o + 12 + 4 * cw, n, 12);
            memcpy(o + 24 + 4 * cw, s + 24 + 4 * type, 8);
        }
        free(file);
        /* Welds: the game moves a joint's parent-side ring onto the parent
           piece; with bit 31 set the parent's normal (after its position)
           comes along too. */
        for (uint32_t i = 0; i < welds; ++i) {
            const uint32_t kw = u32(c), k = kw & 0x7FFFFFFFu; Loc wl = loc(c); float M[16]; take(c, M, sizeof M);
            float p[6];
            const size_t want = (kw & 0x80000000u) ? 24 : 12;
            if (!c->ok || k >= nv || !disc_read(d, wl.arc, wl.entry, wl.offset, p, want)) return false;
            uint8_t *o = r->data + (size_t)k * r->stride;
            xform(p, M, p, true);
            memcpy(o, p, 12);
            if (want == 24) {
                float n[3]; xform(p + 3, M, n, false);
                const float len = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) + 1e-12f;
                for (int q = 0; q < 3; ++q) n[q] /= len;
                memcpy(o + 12 + 4 * cw, n, 12);
            }
        }
        free(g_own); g_own = NULL; g_own_count = 0;
        return trim_own(c, r, &g_own, &g_own_count);
    }
    if (kind[0] == 3) {                                   /* mirror of another piece */
        float M[16]; take(c, M, sizeof M);
        if (!provenance(c, d, r)) return false;
        const Layout lay = layout(r->fvf, r->stride);
        for (uint32_t i = 0; i < r->n; ++i) {
            uint8_t *o = r->data + (size_t)i * r->stride;
            float p[3]; memcpy(p, o, 12); xform(p, M, p, true); memcpy(o, p, 12);
            if (lay.normal >= 0) { float n[3]; memcpy(n, o + lay.normal, 12); xform(n, M, n, false); memcpy(o + lay.normal, n, 12); }
        }
        return true;
    }
    if (kind[0] == 4) {                                   /* re-bound to the chest */
        const uint32_t src_fvf = u32(c), src_stride = u32(c), nw = u32(c), mats = u32(c);
        if (!c->ok || mats == 0 || mats > 4 || nw > 3) return false;
        float M[4][16];
        for (uint32_t i = 0; i < mats; ++i) take(c, M[i], sizeof M[i]);
        Rows s = {0};
        if (!provenance(c, d, &s)) { free(s.data); return false; }
        (void)src_fvf; (void)src_stride;
        const Layout lay = layout(s.fvf, s.stride);
        const uint32_t rest = lay.normal >= 0 ? (uint32_t)lay.normal + 12u : 12u + 4u * lay.weights;
        r->fvf = (s.fvf & ~0x0Eu) | 0x02u;
        r->stride = 12u + (lay.normal >= 0 ? 12u : 0u) + (s.stride - rest);
        r->n = s.n;
        r->data = malloc((size_t)r->n * r->stride + 1);
        if (!r->data) { free(s.data); return false; }
        for (uint32_t i = 0; i < s.n; ++i) {
            const uint8_t *v = s.data + (size_t)i * s.stride;
            uint8_t *o = r->data + (size_t)i * r->stride;
            float w[4] = {0}, sum = 0.0f;
            for (uint32_t k = 0; k < nw; ++k) { memcpy(&w[k], v + 12 + 4 * k, 4); sum += w[k]; }
            const uint32_t terms = nw ? nw + 1u : 1u;
            if (nw) w[nw] = 1.0f - sum; else w[0] = 1.0f;
            float p[3], L[3] = {0}, nrm[3] = {0}, n[3];
            memcpy(p, v, 12);
            if (lay.normal >= 0) memcpy(n, v + lay.normal, 12);
            for (uint32_t k = 0; k < terms && k < mats; ++k) {
                float t[3];
                xform(p, M[k], t, true); for (int q = 0; q < 3; ++q) L[q] += w[k] * t[q];
                if (lay.normal >= 0) { xform(n, M[k], t, false); for (int q = 0; q < 3; ++q) nrm[q] += w[k] * t[q]; }
            }
            memcpy(o, L, 12);
            uint32_t at = 12;
            if (lay.normal >= 0) { memcpy(o + at, nrm, 12); at += 12; }
            memcpy(o + at, v + rest, s.stride - rest);
        }
        free(s.data);
        return true;
    }
    return false;
}

/* ---- the asset ---------------------------------------------------------- */

bool recomp_suit_recipe_build(const uint8_t *recipe, size_t recipe_size, RecompSuitDisc *d,
    uint8_t **image, size_t *image_size)
{
    if (!recipe || !d || !image || !image_size || recipe_size < 12 || memcmp(recipe, "DXR1", 4) != 0) return false;
    Cursor c = {recipe, recipe_size, 4, true};
    Out o = {NULL, 0, 0, true};
    if (u32(&c) != 1u) return false;
    const uint32_t head_len = u32(&c);
    if (!c.ok || c.at + head_len > c.n) return false;
    put(&o, c.p + c.at, head_len); c.at += head_len;
    const uint32_t textures = u32(&c);
    for (uint32_t t = 0; t < textures && c.ok && o.ok; ++t) {
        uint32_t desc[7]; take(&c, desc, sizeof desc);
        Loc l = loc(&c); uint32_t len = u32(&c);
        Loc pl = loc(&c); uint32_t pal_len = u32(&c);
        if (!c.ok) break;
        uint32_t h[9]; memcpy(h, desc, sizeof desc); h[7] = len; h[8] = pal_len;
        put(&o, h, sizeof h);
        uint8_t *buf = malloc((size_t)len + pal_len + 1);
        if (!buf || !disc_read(d, l.arc, l.entry, l.offset, buf, len) ||
            (pal_len && !disc_read(d, pl.arc, pl.entry, pl.offset, buf + len, pal_len))) { free(buf); c.ok = false; break; }
        put(&o, buf, (size_t)len + pal_len);
        free(buf);
    }
    const uint32_t parts = u32(&c);
    for (uint32_t pi = 0; pi < parts && c.ok && o.ok; ++pi) {
        uint32_t h[7]; take(&c, h, sizeof h);              /* stride fvf primitive count texture flags binds */
        if (!c.ok || h[6] == 0 || h[6] > 4) { c.ok = false; break; }
        const size_t binds_at = c.at; take(&c, NULL, (size_t)h[6] * 68);
        const size_t material_at = c.at; take(&c, NULL, 4 + 68 + 12);
        const size_t soft_at = c.at; size_t soft_len = 0;
        if (h[5] & 4u) { soft_len = 64 + (size_t)h[3] * 4; take(&c, NULL, soft_len); }
        const uint32_t weight_count = u32(&c); const size_t weights_at = c.at; take(&c, NULL, (size_t)weight_count * 4);
        Loc il = loc(&c); const uint32_t idx_len = u32(&c);
        Rows r = {0};
        if (!c.ok || !provenance(&c, d, &r)) { free(r.data); c.ok = false; break; }
        /* Compose the final layout: position, the recipe's weights, normal, the rest. */
        const Layout lb = layout(r.fvf, r.stride), lf = layout(h[1], h[0]);
        uint8_t *vtx = r.data; bool composed = false;
        if (r.fvf != h[1] || weight_count) {
            if (r.n != h[3] || (lf.weights && weight_count != r.n * lf.weights)) { free(r.data); c.ok = false; break; }
            vtx = calloc((size_t)r.n, h[0]); composed = true;
            const uint32_t rest = lb.normal >= 0 ? (uint32_t)lb.normal + 12u : 12u + 4u * lb.weights;
            for (uint32_t i = 0; i < r.n && vtx; ++i) {
                const uint8_t *s = r.data + (size_t)i * r.stride; uint8_t *v = vtx + (size_t)i * h[0];
                uint32_t at = 0;
                memcpy(v, s, 12); at = 12;
                if (lf.weights) { memcpy(v + at, c.p + weights_at + (size_t)i * lf.weights * 4, lf.weights * 4); at += lf.weights * 4; }
                if (lf.normal >= 0) { if (lb.normal >= 0) memcpy(v + at, s + lb.normal, 12); at += 12; }
                if (at + (r.stride - rest) <= h[0]) memcpy(v + at, s + rest, r.stride - rest);
            }
        }
        if (!vtx || r.n != h[3]) { if (composed) free(vtx); free(r.data); c.ok = false; break; }
        /* Indices from the disc, remapped to the trimmed vertices. */
        uint16_t *idx = malloc((size_t)idx_len + 2);
        if (!idx || !disc_read(d, il.arc, il.entry, il.offset, idx, idx_len)) { free(idx); if (composed) free(vtx); free(r.data); c.ok = false; break; }
        if (g_own && g_own_count) {
            uint32_t top = 0;
            for (uint32_t i = 0; i < g_own_count; ++i) if (g_own[i] > top) top = g_own[i];
            for (uint32_t i = 0; i < idx_len / 2; ++i) if (idx[i] > top) top = idx[i];
            int32_t *remap = calloc((size_t)top + 1, sizeof *remap);
            if (remap) {
                for (uint32_t i = 0; i < g_own_count; ++i) remap[g_own[i]] = (int32_t)i;
                for (uint32_t i = 0; i < idx_len / 2; ++i) idx[i] = (uint16_t)remap[idx[i]];
                free(remap);
            }
        }
        const uint32_t ph[8] = {h[0], h[1], h[2], h[3], idx_len / 2, h[4], h[5], h[6]};
        put(&o, ph, sizeof ph);
        put(&o, c.p + binds_at, (size_t)h[6] * 68);
        put(&o, c.p + material_at, 4 + 68 + 12);
        if (soft_len) put(&o, c.p + soft_at, soft_len);
        put(&o, vtx, (size_t)h[3] * h[0]);
        put(&o, idx, idx_len);
        free(idx); if (composed) free(vtx); free(r.data);
    }
    free(g_own); g_own = NULL; g_own_count = 0;
    if (!c.ok || !o.ok) { free(o.p); return false; }
    *image = o.p; *image_size = o.n;
    return true;
}

/* ---- the shipped pack --------------------------------------------------- */

typedef struct PackSuit { RecompSuitPackSuit info; uint32_t first, count; } PackSuit;

struct RecompSuitPack {
    FILE *file;
    uint32_t girl_count, suit_count, chunk_count, ref_count;
    RecompSuitPackGirl *girls;
    PackSuit *suits;
    uint32_t *refs;
    uint32_t *chunk_offset, *chunk_length;
    uint64_t data;            /* file offset of the chunk data */
};

RecompSuitPack *recomp_suit_pack_open(const char *path)
{
    RecompSuitPack *k = calloc(1, sizeof *k);
    if (!k) return NULL;
    k->file = fopen(path, "rb");
    uint8_t magic[4]; uint32_t head[5];
    bool ok = k->file && fread(magic, 1, 4, k->file) == 4 && memcmp(magic, "DXP1", 4) == 0 &&
        fread(head, 4, 5, k->file) == 5 && head[0] == 1u && head[1] <= 64u && head[2] <= 65536u &&
        head[3] <= (1u << 24) && head[4] <= (1u << 24);
    if (ok) {
        k->girl_count = head[1]; k->suit_count = head[2]; k->chunk_count = head[3]; k->ref_count = head[4];
        k->girls = calloc(k->girl_count + 1u, sizeof *k->girls);
        k->suits = calloc(k->suit_count + 1u, sizeof *k->suits);
        k->refs = malloc(((size_t)k->ref_count + 1u) * 4u);
        k->chunk_offset = malloc(((size_t)k->chunk_count + 1u) * 4u);
        k->chunk_length = malloc(((size_t)k->chunk_count + 1u) * 4u);
        ok = k->girls && k->suits && k->refs && k->chunk_offset && k->chunk_length;
    }
    for (uint32_t i = 0; ok && i < k->girl_count; ++i) {
        ok = fread(k->girls[i].name, 1, 16, k->file) == 16 && fread(&k->girls[i].head_up, 4, 1, k->file) == 1 &&
            fread(&k->girls[i].head_forward, 4, 1, k->file) == 1;
        k->girls[i].name[15] = 0;
    }
    for (uint32_t i = 0; ok && i < k->suit_count; ++i) {
        PackSuit *s = &k->suits[i];
        ok = fread(&s->info.girl, 4, 1, k->file) == 1 && fread(&s->info.suit, 4, 1, k->file) == 1 &&
            fread(s->info.name, 1, 48, k->file) == 48 && fread(&s->first, 4, 1, k->file) == 1 &&
            fread(&s->count, 4, 1, k->file) == 1 && s->info.girl < k->girl_count &&
            (uint64_t)s->first + s->count <= k->ref_count;
        s->info.name[47] = 0;
    }
    ok = ok && fread(k->refs, 4, k->ref_count, k->file) == k->ref_count;
    for (uint32_t i = 0; ok && i < k->ref_count; ++i) ok = k->refs[i] < k->chunk_count;
    for (uint32_t i = 0; ok && i < k->chunk_count; ++i)
        ok = fread(&k->chunk_offset[i], 4, 1, k->file) == 1 && fread(&k->chunk_length[i], 4, 1, k->file) == 1;
    if (ok) {
#ifdef _WIN32
        k->data = (uint64_t)_ftelli64(k->file);
#else
        k->data = (uint64_t)ftello(k->file);
#endif
    }
    if (!ok) { recomp_suit_pack_close(k); return NULL; }
    return k;
}

void recomp_suit_pack_close(RecompSuitPack *k)
{
    if (!k) return;
    if (k->file) fclose(k->file);
    free(k->girls); free(k->suits); free(k->refs); free(k->chunk_offset); free(k->chunk_length);
    free(k);
}

uint32_t recomp_suit_pack_girl_count(const RecompSuitPack *k) { return k ? k->girl_count : 0u; }
const RecompSuitPackGirl *recomp_suit_pack_girl(const RecompSuitPack *k, uint32_t i)
{
    return k && i < k->girl_count ? &k->girls[i] : NULL;
}
uint32_t recomp_suit_pack_suit_count(const RecompSuitPack *k) { return k ? k->suit_count : 0u; }
const RecompSuitPackSuit *recomp_suit_pack_suit(const RecompSuitPack *k, uint32_t i)
{
    return k && i < k->suit_count ? &k->suits[i].info : NULL;
}

bool recomp_suit_pack_recipe(RecompSuitPack *k, uint32_t index, uint8_t **recipe, size_t *size)
{
    if (!k || index >= k->suit_count || !recipe || !size) return false;
    const PackSuit *s = &k->suits[index];
    size_t total = 0;
    for (uint32_t i = 0; i < s->count; ++i) total += k->chunk_length[k->refs[s->first + i]];
    uint8_t *out = malloc(total + 1);
    if (!out) return false;
    size_t at = 0;
    for (uint32_t i = 0; i < s->count; ++i) {
        const uint32_t c = k->refs[s->first + i];
        if (!seek(k->file, k->data + k->chunk_offset[c]) || fread(out + at, 1, k->chunk_length[c], k->file) != k->chunk_length[c]) {
            free(out); return false;
        }
        at += k->chunk_length[c];
    }
    *recipe = out; *size = total;
    return true;
}
