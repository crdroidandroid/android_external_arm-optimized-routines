/*
 * Double-precision vector erfc(x) function.
 *
 * Copyright (c) 2019-2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"
#include "v_math.h"
#if V_SUPPORTED

/* Accurate exponential (vector variant of exp_dd).  */
v_f64_t V_NAME (exp_tail) (v_f64_t, v_f64_t);

#define One v_f64 (1.0)
#define AbsMask v_u64 (0x7fffffffffffffff)
#define Scale v_f64 (0x1.0000002p27)

/* Coeffs for polynomial approximation on [0x1.0p-28., 31.].  */
#define PX __v_erfc_data.poly
#define xint __v_erfc_data.interval_bounds

/* Special cases (fall back to scalar calls).  */
VPCS_ATTR
NOINLINE static v_f64_t
specialcase (v_f64_t x, v_f64_t y, v_u64_t cmp)
{
  return v_call_f64 (erfc, x, y, cmp);
}

/* A structure to perform look-up in coeffs and other parameter
   tables.  */
struct entry
{
  v_f64_t P[ERFC_POLY_ORDER + 1];
  v_f64_t xi;
};

static inline struct entry
lookup (v_u64_t i)
{
  struct entry e;
#ifdef SCALAR
  for (int j = 0; j <= ERFC_POLY_ORDER; ++j)
    e.P[j] = PX[i][j];
  e.xi = xint[i];
#else
  for (int j = 0; j <= ERFC_POLY_ORDER; ++j)
    {
      e.P[j][0] = PX[i[0]][j];
      e.P[j][1] = PX[i[1]][j];
    }
  e.xi[0] = xint[i[0]];
  e.xi[1] = xint[i[1]];
#endif
  return e;
}

/* Evaluate order-12 polynomials using pairwise summation and Horner
   scheme.  */
static inline v_f64_t
v_eval_poly (v_f64_t z, struct entry e)
{
  v_f64_t r = e.P[12];
  r = v_fma_f64 (z, r, e.P[11]);
  r = v_fma_f64 (z, r, e.P[10]);
  r = v_fma_f64 (z, r, e.P[9]);
  r = v_fma_f64 (z, r, e.P[8]);
  r = v_fma_f64 (z, r, e.P[7]);
  r = v_fma_f64 (z, r, e.P[6]);
  r = v_fma_f64 (z, r, e.P[5]);
  r = v_fma_f64 (z, r, e.P[4]);
  r = v_fma_f64 (z, r, e.P[3]);
  r = v_fma_f64 (z, r, e.P[2]);
  r = v_fma_f64 (z, r, e.P[1]);
  r = v_fma_f64 (z, r, e.P[0]);

  return r;
}

/* Accurate evaluation of exp(x^2) using compensated product
   (x^2 ~ x*x + e2) and custom exp(y+d) routine for small
   corrections d<<y.  */
static inline v_f64_t
v_eval_gauss (v_f64_t a)
{
  v_f64_t e2;
  v_f64_t a2 = a * a;

  /* Dekker's algorithm.
     tmp = a - Scale * a.
     a_hi = high bits of a.
	  = Scale * a - tmp.
     a_lo = low bits of a.
	  = a - a_hi.  */
  v_f64_t a_hi = -v_fma_f64 (Scale, a, -a);
  a_hi = v_fma_f64 (Scale, a, a_hi);
  v_f64_t a_lo = a - a_hi;

  /* Now assemble error term.  */
  e2 = v_fma_f64 (-a_hi, a_hi, a2);
  e2 = v_fma_f64 (-a_hi, a_lo, e2);
  e2 = v_fma_f64 (-a_lo, a_hi, e2);
  e2 = v_fma_f64 (-a_lo, a_lo, e2);

  /* Fast and accurate evaluation of exp(-a2 + e2) where e2 << a2.  */
  return V_NAME (exp_tail) (-a2, e2);
}

/* Optimized double precision vector complementary error function erfc.
   Max ULP: 3.7ulps.
   Max measured: 3.610 on [5.1183, 5.1184] (at 0x1.47923afd09313p+2).
   __v_erfc(0x1.47923afd09313p+2) got 0x1.ff487ddd86457p-42 want
   0x1.ff487ddd8645bp-42 -0.390493 ulp err -3.10951.  */
VPCS_ATTR
v_f64_t V_NAME (erfc) (v_f64_t x)
{
  v_f64_t z, p, y;
  v_u64_t ix, atop, sign, i, cmp;

  ix = v_as_u64_f64 (x);
  /* Compute fac as early as possible in order to get best performance.  */
  v_f64_t fac = v_as_f64_u64 ((ix >> 63) << 62);
  /* Use 12-bit for small, nan and inf case detection.  */
  atop = (ix >> 52) & 0x7ff;
  cmp = v_cond_u64 (atop - v_u64 (0x3cd) >= v_u64 (0x7ff - 0x3cd));

  struct entry dat;

  /* All entries of the vector are out of bounds, take a short path.
     Use smallest possible number above 28 representable in 12 bits.   */
  v_u64_t out_of_bounds = v_cond_u64 (atop >= v_u64 (0x404));

  /* Use sign to produce either 0 if x > 0, 2 otherwise.  */
  if (v_all_u64 (out_of_bounds) && likely (v_any_u64 (~cmp)))
    return fac;

  /* erfc(|x|) = P(|x|-x_i)*exp(-x^2).  */

  v_f64_t a = v_abs_f64 (x);

  /* Interval bounds are a logarithmic scale, i.e. interval n has
     lower bound 2^(n/4) - 1. Use the exponent of (|x|+1)^4 to obtain
     the interval index.  */
  v_f64_t xp1 = a + v_f64 (1.0);
  xp1 = xp1 * xp1;
  xp1 = xp1 * xp1;
  v_u64_t ixp1 = v_as_u64_f64 (xp1);
  i = (ixp1 >> 52) - v_u64 (1023);

  /* Index cannot exceed number of polynomials.  */
#ifdef SCALAR
  i = i <= (ERFC_NUM_INTERVALS) ? i : ERFC_NUM_INTERVALS;
#else
  i = (v_u64_t){i[0] <= ERFC_NUM_INTERVALS ? i[0] : ERFC_NUM_INTERVALS,
		i[1] <= ERFC_NUM_INTERVALS ? i[1] : ERFC_NUM_INTERVALS};
#endif
  /* Get coeffs of i-th polynomial.  */
  dat = lookup (i);

  /* Evaluate Polynomial: P(|x|-x_i).  */
  z = a - dat.xi;
  p = v_eval_poly (z, dat);

  /* Evaluate Gaussian: exp(-x^2).  */
  v_f64_t e = v_eval_gauss (a);

  /* Copy sign.  */
  sign = v_as_u64_f64 (x) & ~AbsMask;
  p = v_as_f64_u64 (v_as_u64_f64 (p) ^ sign);

  /* Assemble result as 2.0 - p * e if x < 0, p * e otherwise.  */
  y = v_fma_f64 (p, e, fac);

  /* No need to fix value of y if x is out of bound, as
     P[ERFC_NUM_INTERVALS]=0.  */
  if (unlikely (v_any_u64 (cmp)))
    return specialcase (x, y, cmp);
  return y;
}
VPCS_ALIAS
#endif
