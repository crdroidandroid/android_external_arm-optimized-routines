/*
 * Double-precision vector atan2(x) function.
 *
 * Copyright (c) 2021-2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#if SV_SUPPORTED

#include "sv_atan_common.h"

/* Useful constants.  */
#define PiOver2 sv_f64 (0x1.921fb54442d18p+0)
#define SignMask sv_u64 (0x8000000000000000)

/* Special cases i.e. 0, infinity, nan (fall back to scalar calls).  */
__attribute__ ((noinline)) static sv_f64_t
specialcase (sv_f64_t y, sv_f64_t x, sv_f64_t ret, const svbool_t cmp)
{
  return sv_call2_f64 (atan2, y, x, ret, cmp);
}

/* Returns a predicate indicating true if the input is the bit representation of
   0, infinity or nan.  */
static inline svbool_t
zeroinfnan (sv_u64_t i, const svbool_t pg)
{
  return svcmpge_u64 (pg, svsub_n_u64_x (pg, svlsl_n_u64_x (pg, i, 1), 1),
		      sv_u64 (2 * asuint64 (INFINITY) - 1));
}

/* Fast implementation of SVE atan2. Errors are greatest when y and
   x are reasonably close together. Maximum observed error is 2.0 ulps:
   sv_atan2(0x1.8d9621df2f329p+2, 0x1.884cf49437972p+2)
   got 0x1.958cd0e8c618bp-1 want 0x1.958cd0e8c618dp-1.  */
sv_f64_t
__sv_atan2_x (sv_f64_t y, sv_f64_t x, const svbool_t pg)
{
  sv_u64_t ix = sv_as_u64_f64 (x);
  sv_u64_t iy = sv_as_u64_f64 (y);

  svbool_t cmp_x = zeroinfnan (ix, pg);
  svbool_t cmp_y = zeroinfnan (iy, pg);
  svbool_t cmp_xy = svorr_b_z (pg, cmp_x, cmp_y);

  sv_u64_t sign_x = svand_u64_x (pg, ix, SignMask);
  sv_u64_t sign_y = svand_u64_x (pg, iy, SignMask);
  sv_u64_t sign_xy = sveor_u64_x (pg, sign_x, sign_y);

  sv_f64_t ax = svabs_f64_x (pg, x);
  sv_f64_t ay = svabs_f64_x (pg, y);

  svbool_t pred_xlt0 = svcmplt_f64 (pg, x, sv_f64 (0.0));
  svbool_t pred_aygtax = svcmpgt_f64 (pg, ay, ax);

  /* Set up z for call to atan.  */
  sv_f64_t n = svsel_f64 (pred_aygtax, svneg_f64_x (pg, ax), ay);
  sv_f64_t d = svsel_f64 (pred_aygtax, ay, ax);
  sv_f64_t z = svdiv_f64_x (pg, n, d);

  /* Work out the correct shift.  */
  sv_f64_t shift = svsel_f64 (pred_xlt0, sv_f64 (-2.0), sv_f64 (0.0));
  shift = svsel_f64 (pred_aygtax, svadd_n_f64_x (pg, shift, 1.0), shift);
  shift = svmul_f64_x (pg, shift, PiOver2);

  sv_f64_t ret = __sv_atan_common (pg, pg, z, z, shift);

  /* Account for the sign of x and y.  */
  ret = sv_as_f64_u64 (sveor_u64_x (pg, sv_as_u64_f64 (ret), sign_xy));

  if (unlikely (svptest_any (pg, cmp_xy)))
    {
      return specialcase (y, x, ret, cmp_xy);
    }

  return ret;
}

strong_alias (__sv_atan2_x, _ZGVsMxvv_atan2)

#endif
