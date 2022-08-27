/*
 * Single-precision vector log10 function.
 *
 * Copyright (c) 2020-2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "mathlib.h"
#include "v_math.h"
#if V_SUPPORTED

static const float Poly[] = {
  /* Use order 9 for log10(1+x), i.e. order 8 for log10(1+x)/x, with x in
     [-1/3, 1/3] (offset=2/3). Max. relative error: 0x1.068ee468p-25.  */
  -0x1.bcb79cp-3f, 0x1.2879c8p-3f, -0x1.bcd472p-4f, 0x1.6408f8p-4f,
  -0x1.246f8p-4f,  0x1.f0e514p-5f, -0x1.0fc92cp-4f, 0x1.f5f76ap-5f};
#define P8 v_f32 (Poly[7])
#define P7 v_f32 (Poly[6])
#define P6 v_f32 (Poly[5])
#define P5 v_f32 (Poly[4])
#define P4 v_f32 (Poly[3])
#define P3 v_f32 (Poly[2])
#define P2 v_f32 (Poly[1])
#define P1 v_f32 (Poly[0])

#define Ln2 v_f32 (0x1.62e43p-1f) /* 0x3f317218.  */
#define Log10_2 v_f32 (0x1.344136p-2f)
#define InvLn10 v_f32 (0x1.bcb7b2p-2f)
#define Min v_u32 (0x00800000)
#define Max v_u32 (0x7f800000)
#define Mask v_u32 (0x007fffff)
#define Off v_u32 (0x3f2aaaab) /* 0.666667.  */

VPCS_ATTR
NOINLINE static v_f32_t
specialcase (v_f32_t x, v_f32_t y, v_u32_t cmp)
{
  /* Fall back to scalar code.  */
  return v_call_f32 (log10f, x, y, cmp);
}

/* Our fast implementation of v_log10f uses a similar approach as v_logf.
   With the same offset as v_logf (i.e., 2/3) it delivers about 3.3ulps with
   order 9. This is more efficient than using a low order polynomial computed in
   double precision.
   Maximum error: 3.305ulps (nearest rounding.)
   __v_log10f(0x1.555c16p+0) got 0x1.ffe2fap-4
			    want 0x1.ffe2f4p-4 -0.304916 ulp err 2.80492.  */
VPCS_ATTR
v_f32_t V_NAME (log10f) (v_f32_t x)
{
  v_f32_t n, o, p, q, r, r2, y;
  v_u32_t u, cmp;

  u = v_as_u32_f32 (x);
  cmp = v_cond_u32 (u - Min >= Max - Min);

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  u -= Off;
  n = v_to_f32_s32 (v_as_s32_u32 (u) >> 23); /* signextend.  */
  u &= Mask;
  u += Off;
  r = v_as_f32_u32 (u) - v_f32 (1.0f);

  /* y = log10(1+r) + n*log10(2).  */
  r2 = r * r;
  /* (n*ln2 + r)*InvLn10 + r2*(P1 + r*P2 + r2*(P3 + r*P4 + r2*(P5 + r*P6 +
     r2*(P7+r*P8))).  */
  o = v_fma_f32 (P8, r, P7);
  p = v_fma_f32 (P6, r, P5);
  q = v_fma_f32 (P4, r, P3);
  y = v_fma_f32 (P2, r, P1);
  p = v_fma_f32 (o, r2, p);
  q = v_fma_f32 (p, r2, q);
  y = v_fma_f32 (q, r2, y);
  /* Using p = Log10(2)*n + r*InvLn(10) is slightly faster
     but less accurate.  */
  p = v_fma_f32 (Ln2, n, r);
  y = v_fma_f32 (y, r2, p * InvLn10);

  if (unlikely (v_any_u32 (cmp)))
    return specialcase (x, y, cmp);
  return y;
}
VPCS_ALIAS
#endif
