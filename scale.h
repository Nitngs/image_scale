/*
 *  Copyright 2013 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef INCLUDE_LIBYUV_SCALE_ROW_H_
#define INCLUDE_LIBYUV_SCALE_ROW_H_

#include "basic_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__pnacl__) || defined(__CLR_VER) ||            \
    (defined(__native_client__) && defined(__x86_64__)) || \
    (defined(__i386__) && !defined(__SSE__) && !defined(__clang__))
#define LIBYUV_DISABLE_X86
#endif
#if defined(__native_client__)
#define LIBYUV_DISABLE_NEON
#endif
// MemorySanitizer does not support assembly code yet. http://crbug.com/344505
#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
#define LIBYUV_DISABLE_X86
#endif
#endif
// GCC >= 4.7.0 required for AVX2.
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#if (__GNUC__ > 4) || (__GNUC__ == 4 && (__GNUC_MINOR__ >= 7))
#define GCC_HAS_AVX2 1
#endif  // GNUC >= 4.7
#endif  // __GNUC__

// clang >= 3.4.0 required for AVX2.
#if defined(__clang__) && (defined(__x86_64__) || defined(__i386__))
#if (__clang_major__ > 3) || (__clang_major__ == 3 && (__clang_minor__ >= 4))
#define CLANG_HAS_AVX2 1
#endif  // clang >= 3.4
#endif  // __clang__

// Visual C 2012 required for AVX2.
#if defined(_M_IX86) && !defined(__clang__) && defined(_MSC_VER) && \
    _MSC_VER >= 1700
#define VISUALC_HAS_AVX2 1
#endif  // VisualStudio >= 2012

// The following are available on all x86 platforms:
#if !defined(LIBYUV_DISABLE_X86) && \
    (defined(_M_IX86) || defined(__x86_64__) || defined(__i386__))
#define HAS_FIXEDDIV1_X86
#define HAS_FIXEDDIV_X86
#define HAS_SCALEARGBCOLS_SSE2
#define HAS_SCALEARGBCOLSUP2_SSE2
#define HAS_SCALEARGBFILTERCOLS_SSSE3
#define HAS_SCALEFILTERCOLS_SSSE3

#define HAS_ARGBTORGB24ROW_SSSE3
#define HAS_RGB24TOARGBROW_SSSE3
#define HAS_INTERPOLATEROW_SSSE3

#endif

// The following are available for gcc/clang x86 platforms:
#if !defined(LIBYUV_DISABLE_X86) && (defined(__x86_64__) || defined(__i386__))
#define HAS_SCALEROWUP2LINEAR_SSE2
#define HAS_SCALEROWUP2LINEAR_SSSE3
#define HAS_SCALEROWUP2BILINEAR_SSE2
#define HAS_SCALEROWUP2BILINEAR_SSSE3
#define HAS_SCALEUVROWUP2LINEAR_SSSE3
#define HAS_SCALEUVROWUP2BILINEAR_SSSE3
#endif

// The following are available on Neon platforms:
#if !defined(LIBYUV_DISABLE_NEON) && \
    (defined(__ARM_NEON__) || defined(LIBYUV_NEON) || defined(__aarch64__))
#define HAS_SCALEARGBCOLS_NEON
#define HAS_SCALEARGBFILTERCOLS_NEON
#define HAS_SCALEROWUP2LINEAR_NEON

#define HAS_RGB24TOARGBROW_NEON
#define HAS_INTERPOLATEROW_NEON
#define HAS_ARGBTORGB24ROW_NEON
#endif

#if !defined(LIBYUV_DISABLE_MSA) && defined(__mips_msa)
#define HAS_SCALEARGBCOLS_MSA
#define HAS_SCALEARGBFILTERCOLS_MSA

#define HAS_RGB24TOARGBROW_MSA
#define HAS_INTERPOLATEROW_MSA
#define HAS_ARGBTORGB24ROW_MSA

#endif

#if !defined(LIBYUV_DISABLE_LSX) && defined(__loongarch_sx)
#define HAS_SCALEFILTERCOLS_LSX
#define HAS_SCALEADDROW_LSX
#define HAS_SCALEARGBCOLS_LSX
#define HAS_SCALEARGBFILTERCOLS_LSX
#endif


#if defined(_MSC_VER) && !defined(__CLR_VER) && !defined(__clang__)
#if defined(VISUALC_HAS_AVX2)
#define SIMD_ALIGNED(var) __declspec(align(32)) var
#else
#define SIMD_ALIGNED(var) __declspec(align(16)) var
#endif
#define LIBYUV_NOINLINE __declspec(noinline)
typedef __declspec(align(16)) int16_t vec16[8];
typedef __declspec(align(16)) int32_t vec32[4];
typedef __declspec(align(16)) float vecf32[4];
typedef __declspec(align(16)) int8_t vec8[16];
typedef __declspec(align(16)) uint16_t uvec16[8];
typedef __declspec(align(16)) uint32_t uvec32[4];
typedef __declspec(align(16)) uint8_t uvec8[16];
typedef __declspec(align(32)) int16_t lvec16[16];
typedef __declspec(align(32)) int32_t lvec32[8];
typedef __declspec(align(32)) int8_t lvec8[32];
typedef __declspec(align(32)) uint16_t ulvec16[16];
typedef __declspec(align(32)) uint32_t ulvec32[8];
typedef __declspec(align(32)) uint8_t ulvec8[32];
#elif !defined(__pnacl__) && (defined(__GNUC__) || defined(__clang__))
// Caveat GCC 4.2 to 4.7 have a known issue using vectors with const.
#if defined(CLANG_HAS_AVX2) || defined(GCC_HAS_AVX2)
#define SIMD_ALIGNED(var) var __attribute__((aligned(32)))
#else
#define SIMD_ALIGNED(var) var __attribute__((aligned(16)))
#endif
#define LIBYUV_NOINLINE __attribute__((noinline))
typedef int16_t __attribute__((vector_size(16))) vec16;
typedef int32_t __attribute__((vector_size(16))) vec32;
typedef float __attribute__((vector_size(16))) vecf32;
typedef int8_t __attribute__((vector_size(16))) vec8;
typedef uint16_t __attribute__((vector_size(16))) uvec16;
typedef uint32_t __attribute__((vector_size(16))) uvec32;
typedef uint8_t __attribute__((vector_size(16))) uvec8;
typedef int16_t __attribute__((vector_size(32))) lvec16;
typedef int32_t __attribute__((vector_size(32))) lvec32;
typedef int8_t __attribute__((vector_size(32))) lvec8;
typedef uint16_t __attribute__((vector_size(32))) ulvec16;
typedef uint32_t __attribute__((vector_size(32))) ulvec32;
typedef uint8_t __attribute__((vector_size(32))) ulvec8;
#else
#define SIMD_ALIGNED(var) var
#define LIBYUV_NOINLINE
typedef int16_t vec16[8];
typedef int32_t vec32[4];
typedef float vecf32[4];
typedef int8_t vec8[16];
typedef uint16_t uvec16[8];
typedef uint32_t uvec32[4];
typedef uint8_t uvec8[16];
typedef int16_t lvec16[16];
typedef int32_t lvec32[8];
typedef int8_t lvec8[32];
typedef uint16_t ulvec16[16];
typedef uint32_t ulvec32[8];
typedef uint8_t ulvec8[32];
#endif

#if defined(__native_client__)
#define LABELALIGN ".p2align 5\n"
#else
#define LABELALIGN
#endif

#define IS_ALIGNED(p, a) (!((uintptr_t)(p) & ((a)-1)))

// Divide num by div and return as 16.16 fixed point result.
int FixedDiv_C(int num, int div);
int FixedDiv_X86(int num, int div);
int FixedDiv_MIPS(int num, int div);
// Divide num - 1 by div - 1 and return as 16.16 fixed point result.
int FixedDiv1_C(int num, int div);
int FixedDiv1_X86(int num, int div);
int FixedDiv1_MIPS(int num, int div);

#ifdef HAS_FIXEDDIV_X86
#define FixedDiv FixedDiv_X86
#define FixedDiv1 FixedDiv1_X86
#elif defined HAS_FIXEDDIV_MIPS
#define FixedDiv FixedDiv_MIPS
#define FixedDiv1 FixedDiv1_MIPS
#else
#define FixedDiv FixedDiv_C
#define FixedDiv1 FixedDiv1_C
#endif

// ARGB Column functions
void ScaleARGBCols_SSE2(uint8_t* dst_argb,
                        const uint8_t* src_argb,
                        int dst_width,
                        int x,
                        int dx);
void ScaleARGBFilterCols_SSSE3(uint8_t* dst_argb,
                               const uint8_t* src_argb,
                               int dst_width,
                               int x,
                               int dx);
void ScaleARGBColsUp2_SSE2(uint8_t* dst_argb,
                           const uint8_t* src_argb,
                           int dst_width,
                           int x,
                           int dx);

void ScaleARGBFilterCols_NEON(uint8_t* dst_argb,
                              const uint8_t* src_argb,
                              int dst_width,
                              int x,
                              int dx);
void ScaleARGBCols_NEON(uint8_t* dst_argb,
                        const uint8_t* src_argb,
                        int dst_width,
                        int x,
                        int dx);
void ScaleARGBFilterCols_Any_NEON(uint8_t* dst_ptr,
                                  const uint8_t* src_ptr,
                                  int dst_width,
                                  int x,
                                  int dx);
void ScaleARGBCols_Any_NEON(uint8_t* dst_ptr,
                            const uint8_t* src_ptr,
                            int dst_width,
                            int x,
                            int dx);

void ScaleARGBFilterCols_C(uint8_t* dst_argb,
                           const uint8_t* src_argb,
                           int dst_width,
                           int x,
                           int dx);
void ScaleARGBFilterCols_MSA(uint8_t* dst_argb,
                             const uint8_t* src_argb,
                             int dst_width,
                             int x,
                             int dx);

void ScaleARGBCols_C(uint8_t* dst_argb,
                     const uint8_t* src_argb,
                     int dst_width,
                     int x,
                     int dx);

void ScaleARGBCols_MSA(uint8_t* dst_argb,
                       const uint8_t* src_argb,
                       int dst_width,
                       int x,
                       int dx);
void ScaleARGBFilterCols_Any_MSA(uint8_t* dst_ptr,
                                 const uint8_t* src_ptr,
                                 int dst_width,
                                 int x,
                                 int dx);
void ScaleARGBCols_Any_MSA(uint8_t* dst_ptr,
                           const uint8_t* src_ptr,
                           int dst_width,
                           int x,
                           int dx);

void ScaleFilterCols_NEON(uint8_t* dst_ptr,
                          const uint8_t* src_ptr,
                          int dst_width,
                          int x,
                          int dx);

void ScaleFilterCols_Any_NEON(uint8_t* dst_ptr,
                              const uint8_t* src_ptr,
                              int dst_width,
                              int x,
                              int dx);

void ScaleFilterCols_MSA(uint8_t* dst_ptr,
                         const uint8_t* src_ptr,
                         int dst_width,
                         int x,
                         int dx);
void ScaleFilterCols_Any_MSA(uint8_t* dst_ptr,
                             const uint8_t* src_ptr,
                             int dst_width,
                             int x,
                             int dx);
void ScaleFilterCols_LSX(uint8_t* dst_ptr,
                         const uint8_t* src_ptr,
                         int dst_width,
                         int x,
                         int dx);
void ScaleARGBFilterCols_LSX(uint8_t* dst_argb,
                             const uint8_t* src_argb,
                             int dst_width,
                             int x,
                             int dx);
void ScaleARGBCols_LSX(uint8_t* dst_argb,
                       const uint8_t* src_argb,
                       int dst_width,
                       int x,
                       int dx);
void ScaleFilterCols_Any_LSX(uint8_t* dst_ptr,
                             const uint8_t* src_ptr,
                             int dst_width,
                             int x,
                             int dx);
void ScaleARGBCols_Any_LSX(uint8_t* dst_ptr,
                           const uint8_t* src_ptr,
                           int dst_width,
                           int x,
                           int dx);
void ScaleARGBFilterCols_Any_LSX(uint8_t* dst_ptr,
                                 const uint8_t* src_ptr,
                                 int dst_width,
                                 int x,
                                 int dx);

void RGB24ToARGBRow_SSSE3(const uint8_t* src_rgb24,
                          uint8_t* dst_argb,
                          int width);

void RGB24ToARGBRow_Any_SSSE3(const uint8_t* src_ptr,
                              uint8_t* dst_ptr,
                              int width);

void ARGBToRGB24Row_SSSE3(const uint8_t* src, uint8_t* dst, int width);

void ARGBToRGB24Row_Any_SSSE3(const uint8_t* src_ptr,
                              uint8_t* dst_ptr,
                              int width);

void InterpolateRow_SSSE3(uint8_t* dst_ptr,
                          const uint8_t* src_ptr,
                          ptrdiff_t src_stride,
                          int dst_width,
                          int source_y_fraction);

void InterpolateRow_Any_SSSE3(uint8_t* dst_ptr,
                              const uint8_t* src_ptr,
                              ptrdiff_t src_stride_ptr,
                              int width,
                              int source_y_fraction);

void RGB24ToARGBRow_MSA(const uint8_t* src_rgb24, uint8_t* dst_argb, int width);
void RGB24ToARGBRow_Any_MSA(const uint8_t* src_ptr,
                            uint8_t* dst_ptr,
                            int width);

void InterpolateRow_MSA(uint8_t* dst_ptr,
                        const uint8_t* src_ptr,
                        ptrdiff_t src_stride,
                        int width,
                        int source_y_fraction);

void InterpolateRow_Any_MSA(uint8_t* dst_ptr,
                            const uint8_t* src_ptr,
                            ptrdiff_t src_stride_ptr,
                            int width,
                            int source_y_fraction);

void ARGBToRGB24Row_MSA(const uint8_t* src_argb, uint8_t* dst_rgb, int width);
void ARGBToRGB24Row_Any_MSA(const uint8_t* src_ptr,
                            uint8_t* dst_ptr,
                            int width);

void RGB24ToARGBRow_NEON(const uint8_t* src_rgb24,
                         uint8_t* dst_argb,
                         int width);

void RGB24ToARGBRow_Any_NEON(const uint8_t* src_ptr,
                             uint8_t* dst_ptr,
                             int width);

void InterpolateRow_NEON(uint8_t* dst_ptr,
                         const uint8_t* src_ptr,
                         ptrdiff_t src_stride,
                         int dst_width,
                         int source_y_fraction);

void InterpolateRow_Any_NEON(uint8_t* dst_ptr,
                             const uint8_t* src_ptr,
                             ptrdiff_t src_stride_ptr,
                             int width,
                             int source_y_fraction);

void ARGBToRGB24Row_NEON(const uint8_t* src_argb,
                         uint8_t* dst_rgb24,
                         int width);

void ARGBToRGB24Row_Any_NEON(const uint8_t* src_ptr,
                             uint8_t* dst_ptr,
                             int width);

void ScaleRowUp2_Linear_C(const uint8_t* src_ptr,
                          uint8_t* dst_ptr,
                          int dst_width);

void ScaleRowUp2_Linear_Any_C(const uint8_t* src_ptr,
                              uint8_t* dst_ptr,
                              int dst_width);

void ScaleRowUp2_Linear_SSE2(const uint8_t* src_ptr,
                             uint8_t* dst_ptr,
                             int dst_width);

void ScaleRowUp2_Linear_Any_SSE2(const uint8_t* src_ptr,
                                 uint8_t* dst_ptr,
                                 int dst_width);

void ScaleRowUp2_Linear_SSSE3(const uint8_t* src_ptr,
                              uint8_t* dst_ptr,
                              int dst_width);

void ScaleRowUp2_Linear_Any_SSSE3(const uint8_t* src_ptr,
                                  uint8_t* dst_ptr,
                                  int dst_width);

void ScaleRowUp2_Bilinear_C(const uint8_t* src_ptr,
                            ptrdiff_t src_stride,
                            uint8_t* dst_ptr,
                            ptrdiff_t dst_stride,
                            int dst_width);

void ScaleRowUp2_Bilinear_Any_C(const uint8_t* src_ptr,
                                ptrdiff_t src_stride,
                                uint8_t* dst_ptr,
                                ptrdiff_t dst_stride,
                                int dst_width);

void ScaleRowUp2_Bilinear_SSE2(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               ptrdiff_t dst_stride,
                               int dst_width);

void ScaleRowUp2_Bilinear_Any_SSE2(const uint8_t* src_ptr,
                                   ptrdiff_t src_stride,
                                   uint8_t* dst_ptr,
                                   ptrdiff_t dst_stride,
                                   int dst_width);

void ScaleRowUp2_Bilinear_SSSE3(const uint8_t* src_ptr,
                                ptrdiff_t src_stride,
                                uint8_t* dst_ptr,
                                ptrdiff_t dst_stride,
                                int dst_width);

void ScaleRowUp2_Bilinear_Any_SSSE3(const uint8_t* src_ptr,
                                    ptrdiff_t src_stride,
                                    uint8_t* dst_ptr,
                                    ptrdiff_t dst_stride,
                                    int dst_width);

void ScaleCols_C(uint8_t* dst_ptr,
                 const uint8_t* src_ptr,
                 int dst_width,
                 int x,
                 int dx);

void ScaleFilterCols_C(uint8_t* dst_ptr,
                       const uint8_t* src_ptr,
                       int dst_width,
                       int x,
                       int dx);

void ScaleColsUp2_C(uint8_t* dst_ptr,
                    const uint8_t* src_ptr,
                    int dst_width,
                    int,
                    int);

void ScaleFilterCols_SSSE3(uint8_t* dst_ptr,
                           const uint8_t* src_ptr,
                           int dst_width,
                           int x,
                           int dx);

void ScaleUVRowUp2_Linear_Any_C(const uint8_t* src_ptr,
                                uint8_t* dst_ptr,
                                int dst_width);

void ScaleUVRowUp2_Linear_C(const uint8_t* src_ptr,
                            uint8_t* dst_ptr,
                            int dst_width);

void ScaleUVRowUp2_Linear_Any_SSSE3(const uint8_t* src_ptr,
                                    uint8_t* dst_ptr,
                                    int dst_width);

void ScaleUVRowUp2_Linear_SSSE3(const uint8_t* src_ptr,
                                uint8_t* dst_ptr,
                                int dst_width);

void ScaleUVRowUp2_Bilinear_Any_C(const uint8_t* src_ptr,
                                  ptrdiff_t src_stride,
                                  uint8_t* dst_ptr,
                                  ptrdiff_t dst_stride,
                                  int dst_width);

void ScaleUVRowUp2_Bilinear_C(const uint8_t* src_ptr,
                              ptrdiff_t src_stride,
                              uint8_t* dst_ptr,
                              ptrdiff_t dst_stride,
                              int dst_width);

void ScaleUVRowUp2_Bilinear_Any_SSSE3(const uint8_t* src_ptr,
                                      ptrdiff_t src_stride,
                                      uint8_t* dst_ptr,
                                      ptrdiff_t dst_stride,
                                      int dst_width);
void ScaleUVRowUp2_Bilinear_SSSE3(const uint8_t* src_ptr,
                                  ptrdiff_t src_stride,
                                  uint8_t* dst_ptr,
                                  ptrdiff_t dst_stride,
                                  int dst_width);

void ScaleUVCols_C(uint8_t* dst_uv,
                   const uint8_t* src_uv,
                   int dst_width,
                   int x,
                   int dx);

void ScaleUVFilterCols_C(uint8_t* dst_uv,
                         const uint8_t* src_uv,
                         int dst_width,
                         int x,
                         int dx);

void ScaleUVColsUp2_C(uint8_t* dst_uv,
                      const uint8_t* src_uv,
                      int dst_width,
                      int,
                      int);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // INCLUDE_LIBYUV_SCALE_ROW_H_
