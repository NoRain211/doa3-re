#ifndef DOAXBV_RECOMP_MATH_H
#define DOAXBV_RECOMP_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

/* Deterministic double-precision math for x87 transcendental emulation.
   Results depend only on IEEE double arithmetic, not on the host CRT, so two
   machines running the same build compute the same bits. Each function is
   ported from musl v1.2.5 (MIT); see recomp_math.c. */
double recomp_sin(double x);
double recomp_cos(double x);
double recomp_tan(double x);
double recomp_atan2(double y, double x);
double recomp_log2(double x);
double recomp_log1p(double x);
double recomp_expm1(double x);

#ifdef __cplusplus
}
#endif

#endif
