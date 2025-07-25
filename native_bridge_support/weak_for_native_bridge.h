/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#ifdef __ANDROID_NATIVE_BRIDGE__
__attribute__((__weak__, __noinline__)) float expf(float x);
__attribute__((__weak__, __noinline__)) float powf(float x, float y);
#endif
