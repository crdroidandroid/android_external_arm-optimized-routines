/*
 * AdvSIMD vector PCS variant of __v_erfcf.
 *
 * Copyright (c) 2019-2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS strong_alias (__vn_erfcf, _ZGVnN4v_erfcf)
#include "v_erfcf_1u.c"
#endif
