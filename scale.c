#include "scale.h"
#include <stdio.h>
#include <string.h>


// Any 1 to 1 interpolate.  Takes 2 rows of source via stride.
#define ANY11I(NAMEANY, ANY_SIMD, TD, TS, SBPP, BPP, MASK)           \
  void NAMEANY(TD* dst_ptr, const TS* src_ptr, ptrdiff_t src_stride, \
               int width, int source_y_fraction) {                   \
    SIMD_ALIGNED(TS temps[64 * 2]);                                  \
    SIMD_ALIGNED(TD tempd[64]);                                      \
    memset(temps, 0, sizeof(temps)); /* for msan */                  \
    int r = width & MASK;                                            \
    int n = width & ~MASK;                                           \
    if (n > 0) {                                                     \
      ANY_SIMD(dst_ptr, src_ptr, src_stride, n, source_y_fraction);  \
    }                                                                \
    memcpy(temps, src_ptr + n * SBPP, r * SBPP * sizeof(TS));        \
    if (source_y_fraction) {                                         \
      memcpy(temps + 64, src_ptr + src_stride + n * SBPP,            \
             r * SBPP * sizeof(TS));                                 \
    }                                                                \
    ANY_SIMD(tempd, temps, 64, MASK + 1, source_y_fraction);         \
    memcpy(dst_ptr + n * BPP, tempd, r * BPP * sizeof(TD));          \
  }


#define SS(width, shift) (((width) + (1 << (shift)) - 1) >> (shift))

#define ANY11(NAMEANY, ANY_SIMD, UVSHIFT, SBPP, BPP, MASK)                \
  void NAMEANY(const uint8_t* src_ptr, uint8_t* dst_ptr, int width) {     \
    SIMD_ALIGNED(uint8_t temp[128 * 2]);                                  \
    memset(temp, 0, 128); /* for YUY2 and msan */                         \
    int r = width & MASK;                                                 \
    int n = width & ~MASK;                                                \
    if (n > 0) {                                                          \
      ANY_SIMD(src_ptr, dst_ptr, n);                                      \
    }                                                                     \
    memcpy(temp, src_ptr + (n >> UVSHIFT) * SBPP, SS(r, UVSHIFT) * SBPP); \
    ANY_SIMD(temp, temp + 128, MASK + 1);                                 \
    memcpy(dst_ptr + n * BPP, temp + 128, r * BPP);                       \
  }

// Definition for ScaleFilterCols, ScaleARGBCols and ScaleARGBFilterCols
#define CANY(NAMEANY, TERP_SIMD, TERP_C, BPP, MASK)                            \
  void NAMEANY(uint8_t* dst_ptr, const uint8_t* src_ptr, int dst_width, int x, \
               int dx) {                                                       \
    int r = dst_width & MASK;                                                  \
    int n = dst_width & ~MASK;                                                 \
    if (n > 0) {                                                               \
      TERP_SIMD(dst_ptr, src_ptr, n, x, dx);                                   \
    }                                                                          \
    TERP_C(dst_ptr + n * BPP, src_ptr, r, x + n * dx, dx);                     \
  }

// This module is for GCC x86 and x64.
#if !defined(LIBYUV_DISABLE_X86) && (defined(__x86_64__) || defined(__i386__))
void ScaleARGBCols_SSE2(uint8_t* dst_argb,
                        const uint8_t* src_argb,
                        int dst_width,
                        int x,
                        int dx) {
  intptr_t x0, x1;
  asm volatile(
      "movd        %5,%%xmm2                     \n"
      "movd        %6,%%xmm3                     \n"
      "pshufd      $0x0,%%xmm2,%%xmm2            \n"
      "pshufd      $0x11,%%xmm3,%%xmm0           \n"
      "paddd       %%xmm0,%%xmm2                 \n"
      "paddd       %%xmm3,%%xmm3                 \n"
      "pshufd      $0x5,%%xmm3,%%xmm0            \n"
      "paddd       %%xmm0,%%xmm2                 \n"
      "paddd       %%xmm3,%%xmm3                 \n"
      "pshufd      $0x0,%%xmm3,%%xmm3            \n"
      "pextrw      $0x1,%%xmm2,%k0               \n"
      "pextrw      $0x3,%%xmm2,%k1               \n"
      "cmp         $0x0,%4                       \n"
      "jl          99f                           \n"
      "sub         $0x4,%4                       \n"
      "jl          49f                           \n"

      LABELALIGN
      "40:                                       \n"
      "movd        0x00(%3,%0,4),%%xmm0          \n"
      "movd        0x00(%3,%1,4),%%xmm1          \n"
      "pextrw      $0x5,%%xmm2,%k0               \n"
      "pextrw      $0x7,%%xmm2,%k1               \n"
      "paddd       %%xmm3,%%xmm2                 \n"
      "punpckldq   %%xmm1,%%xmm0                 \n"
      "movd        0x00(%3,%0,4),%%xmm1          \n"
      "movd        0x00(%3,%1,4),%%xmm4          \n"
      "pextrw      $0x1,%%xmm2,%k0               \n"
      "pextrw      $0x3,%%xmm2,%k1               \n"
      "punpckldq   %%xmm4,%%xmm1                 \n"
      "punpcklqdq  %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%2)                   \n"
      "lea         0x10(%2),%2                   \n"
      "sub         $0x4,%4                       \n"
      "jge         40b                           \n"

      "49:                                       \n"
      "test        $0x2,%4                       \n"
      "je          29f                           \n"
      "movd        0x00(%3,%0,4),%%xmm0          \n"
      "movd        0x00(%3,%1,4),%%xmm1          \n"
      "pextrw      $0x5,%%xmm2,%k0               \n"
      "punpckldq   %%xmm1,%%xmm0                 \n"
      "movq        %%xmm0,(%2)                   \n"
      "lea         0x8(%2),%2                    \n"
      "29:                                       \n"
      "test        $0x1,%4                       \n"
      "je          99f                           \n"
      "movd        0x00(%3,%0,4),%%xmm0          \n"
      "movd        %%xmm0,(%2)                   \n"
      "99:                                       \n"
      : "=&a"(x0),       // %0
        "=&d"(x1),       // %1
        "+r"(dst_argb),  // %2
        "+r"(src_argb),  // %3
        "+r"(dst_width)  // %4
      : "rm"(x),         // %5
        "rm"(dx)         // %6
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}

// Reads 4 pixels, duplicates them and writes 8 pixels.
// Alignment requirement: src_argb 16 byte aligned, dst_argb 16 byte aligned.
void ScaleARGBColsUp2_SSE2(uint8_t* dst_argb,
                           const uint8_t* src_argb,
                           int dst_width,
                           int x,
                           int dx) {
  (void)x;
  (void)dx;
  asm volatile(LABELALIGN
      "1:                                        \n"
      "movdqu      (%1),%%xmm0                   \n"
      "lea         0x10(%1),%1                   \n"
      "movdqa      %%xmm0,%%xmm1                 \n"
      "punpckldq   %%xmm0,%%xmm0                 \n"
      "punpckhdq   %%xmm1,%%xmm1                 \n"
      "movdqu      %%xmm0,(%0)                   \n"
      "movdqu      %%xmm1,0x10(%0)               \n"
      "lea         0x20(%0),%0                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"

               : "+r"(dst_argb),  // %0
                 "+r"(src_argb),  // %1
                 "+r"(dst_width)  // %2
                 ::"memory",
                 "cc", "xmm0", "xmm1");
}

// Shuffle table for arranging 2 pixels into pairs for pmaddubsw
static const uvec8 kShuffleColARGB = {
    0u, 4u,  1u, 5u,  2u,  6u,  3u,  7u,  // bbggrraa 1st pixel
    8u, 12u, 9u, 13u, 10u, 14u, 11u, 15u  // bbggrraa 2nd pixel
};

// Shuffle table for duplicating 2 fractions into 8 bytes each
static const uvec8 kShuffleFractions = {
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u,
};

// Bilinear row filtering combines 4x2 -> 4x1. SSSE3 version
void ScaleARGBFilterCols_SSSE3(uint8_t* dst_argb,
                               const uint8_t* src_argb,
                               int dst_width,
                               int x,
                               int dx) {
  intptr_t x0, x1;
  asm volatile(
      "movdqa      %0,%%xmm4                     \n"
      "movdqa      %1,%%xmm5                     \n"
      :
      : "m"(kShuffleColARGB),   // %0
        "m"(kShuffleFractions)  // %1
  );

  asm volatile(
      "movd        %5,%%xmm2                     \n"
      "movd        %6,%%xmm3                     \n"
      "pcmpeqb     %%xmm6,%%xmm6                 \n"
      "psrlw       $0x9,%%xmm6                   \n"
      "pextrw      $0x1,%%xmm2,%k3               \n"
      "sub         $0x2,%2                       \n"
      "jl          29f                           \n"
      "movdqa      %%xmm2,%%xmm0                 \n"
      "paddd       %%xmm3,%%xmm0                 \n"
      "punpckldq   %%xmm0,%%xmm2                 \n"
      "punpckldq   %%xmm3,%%xmm3                 \n"
      "paddd       %%xmm3,%%xmm3                 \n"
      "pextrw      $0x3,%%xmm2,%k4               \n"

      LABELALIGN
      "2:                                        \n"
      "movdqa      %%xmm2,%%xmm1                 \n"
      "paddd       %%xmm3,%%xmm2                 \n"
      "movq        0x00(%1,%3,4),%%xmm0          \n"
      "psrlw       $0x9,%%xmm1                   \n"
      "movhps      0x00(%1,%4,4),%%xmm0          \n"
      "pshufb      %%xmm5,%%xmm1                 \n"
      "pshufb      %%xmm4,%%xmm0                 \n"
      "pxor        %%xmm6,%%xmm1                 \n"
      "pmaddubsw   %%xmm1,%%xmm0                 \n"
      "psrlw       $0x7,%%xmm0                   \n"
      "pextrw      $0x1,%%xmm2,%k3               \n"
      "pextrw      $0x3,%%xmm2,%k4               \n"
      "packuswb    %%xmm0,%%xmm0                 \n"
      "movq        %%xmm0,(%0)                   \n"
      "lea         0x8(%0),%0                    \n"
      "sub         $0x2,%2                       \n"
      "jge         2b                            \n"

      LABELALIGN
      "29:                                       \n"
      "add         $0x1,%2                       \n"
      "jl          99f                           \n"
      "psrlw       $0x9,%%xmm2                   \n"
      "movq        0x00(%1,%3,4),%%xmm0          \n"
      "pshufb      %%xmm5,%%xmm2                 \n"
      "pshufb      %%xmm4,%%xmm0                 \n"
      "pxor        %%xmm6,%%xmm2                 \n"
      "pmaddubsw   %%xmm2,%%xmm0                 \n"
      "psrlw       $0x7,%%xmm0                   \n"
      "packuswb    %%xmm0,%%xmm0                 \n"
      "movd        %%xmm0,(%0)                   \n"

      LABELALIGN
      "99:                                       \n"  // clang-format error.

      : "+r"(dst_argb),    // %0
        "+r"(src_argb),    // %1
        "+rm"(dst_width),  // %2
        "=&r"(x0),         // %3
        "=&r"(x1)          // %4
      : "rm"(x),           // %5
        "rm"(dx)           // %6
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}

// Divide num by div and return as 16.16 fixed point result.
int FixedDiv_X86(int num, int div) {
  asm volatile(
      "cdq                                       \n"
      "shld        $0x10,%%eax,%%edx             \n"
      "shl         $0x10,%%eax                   \n"
      "idiv        %1                            \n"
      "mov         %0, %%eax                     \n"
      : "+a"(num)  // %0
      : "c"(div)   // %1
      : "memory", "cc", "edx");
  return num;
}

// Divide num - 1 by div - 1 and return as 16.16 fixed point result.
int FixedDiv1_X86(int num, int div) {
  asm volatile(
      "cdq                                       \n"
      "shld        $0x10,%%eax,%%edx             \n"
      "shl         $0x10,%%eax                   \n"
      "sub         $0x10001,%%eax                \n"
      "sbb         $0x0,%%edx                    \n"
      "sub         $0x1,%1                       \n"
      "idiv        %1                            \n"
      "mov         %0, %%eax                     \n"
      : "+a"(num)  // %0
      : "c"(div)   // %1
      : "memory", "cc", "edx");
  return num;
}

#ifdef HAS_RGB24TOARGBROW_SSSE3
// Shuffle table for converting RGB24 to ARGB.
static const uvec8 kShuffleMaskRGB24ToARGB = {
    0u, 1u, 2u, 12u, 3u, 4u, 5u, 13u, 6u, 7u, 8u, 14u, 9u, 10u, 11u, 15u};

void RGB24ToARGBRow_SSSE3(const uint8_t* src_rgb24,
                          uint8_t* dst_argb,
                          int width) {
  asm volatile(
      "pcmpeqb     %%xmm5,%%xmm5                 \n"  // 0xff000000
      "pslld       $0x18,%%xmm5                  \n"
      "movdqa      %3,%%xmm4                     \n"

      LABELALIGN
      "1:                                        \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      0x20(%0),%%xmm3               \n"
      "lea         0x30(%0),%0                   \n"
      "movdqa      %%xmm3,%%xmm2                 \n"
      "palignr     $0x8,%%xmm1,%%xmm2            \n"
      "pshufb      %%xmm4,%%xmm2                 \n"
      "por         %%xmm5,%%xmm2                 \n"
      "palignr     $0xc,%%xmm0,%%xmm1            \n"
      "pshufb      %%xmm4,%%xmm0                 \n"
      "movdqu      %%xmm2,0x20(%1)               \n"
      "por         %%xmm5,%%xmm0                 \n"
      "pshufb      %%xmm4,%%xmm1                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "por         %%xmm5,%%xmm1                 \n"
      "palignr     $0x4,%%xmm3,%%xmm3            \n"
      "pshufb      %%xmm4,%%xmm3                 \n"
      "movdqu      %%xmm1,0x10(%1)               \n"
      "por         %%xmm5,%%xmm3                 \n"
      "movdqu      %%xmm3,0x30(%1)               \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_rgb24),              // %0
        "+r"(dst_argb),               // %1
        "+r"(width)                   // %2
      : "m"(kShuffleMaskRGB24ToARGB)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif

#ifdef HAS_RGB24TOARGBROW_SSSE3
// Shuffle table for converting ARGB to RGB24.
static const uvec8 kShuffleMaskARGBToRGB24 = {
    0u, 1u, 2u, 4u, 5u, 6u, 8u, 9u, 10u, 12u, 13u, 14u, 128u, 128u, 128u, 128u};

void ARGBToRGB24Row_SSSE3(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(

      "movdqa      %3,%%xmm6                     \n"

      LABELALIGN
      "1:                                        \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      0x20(%0),%%xmm2               \n"
      "movdqu      0x30(%0),%%xmm3               \n"
      "lea         0x40(%0),%0                   \n"
      "pshufb      %%xmm6,%%xmm0                 \n"
      "pshufb      %%xmm6,%%xmm1                 \n"
      "pshufb      %%xmm6,%%xmm2                 \n"
      "pshufb      %%xmm6,%%xmm3                 \n"
      "movdqa      %%xmm1,%%xmm4                 \n"
      "psrldq      $0x4,%%xmm1                   \n"
      "pslldq      $0xc,%%xmm4                   \n"
      "movdqa      %%xmm2,%%xmm5                 \n"
      "por         %%xmm4,%%xmm0                 \n"
      "pslldq      $0x8,%%xmm5                   \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "por         %%xmm5,%%xmm1                 \n"
      "psrldq      $0x8,%%xmm2                   \n"
      "pslldq      $0x4,%%xmm3                   \n"
      "por         %%xmm3,%%xmm2                 \n"
      "movdqu      %%xmm1,0x10(%1)               \n"
      "movdqu      %%xmm2,0x20(%1)               \n"
      "lea         0x30(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src),                    // %0
        "+r"(dst),                    // %1
        "+r"(width)                   // %2
      : "m"(kShuffleMaskARGBToRGB24)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif

#if defined(HAS_RGB24TOARGBROW_SSSE3)
ANY11(RGB24ToARGBRow_Any_SSSE3, RGB24ToARGBRow_SSSE3, 0, 3, 4, 15)
#endif

#if defined(HAS_ARGBTORGB24ROW_SSSE3)
ANY11(ARGBToRGB24Row_Any_SSSE3, ARGBToRGB24Row_SSSE3, 0, 4, 3, 15)
#endif

#ifdef HAS_INTERPOLATEROW_SSSE3
// Bilinear filter 16x2 -> 16x1
void InterpolateRow_SSSE3(uint8_t* dst_ptr,
                          const uint8_t* src_ptr,
                          ptrdiff_t src_stride,
                          int width,
                          int source_y_fraction) {
  asm volatile(
      "sub         %1,%0                         \n"
      "cmp         $0x0,%3                       \n"
      "je          100f                          \n"
      "cmp         $0x80,%3                      \n"
      "je          50f                           \n"

      "movd        %3,%%xmm0                     \n"
      "neg         %3                            \n"
      "add         $0x100,%3                     \n"
      "movd        %3,%%xmm5                     \n"
      "punpcklbw   %%xmm0,%%xmm5                 \n"
      "punpcklwd   %%xmm5,%%xmm5                 \n"
      "pshufd      $0x0,%%xmm5,%%xmm5            \n"
      "mov         $0x80808080,%%eax             \n"
      "movd        %%eax,%%xmm4                  \n"
      "pshufd      $0x0,%%xmm4,%%xmm4            \n"

      // General purpose row blend.
      LABELALIGN
      "1:                                        \n"
      "movdqu      (%1),%%xmm0                   \n"
      "movdqu      0x00(%1,%4,1),%%xmm2          \n"
      "movdqa      %%xmm0,%%xmm1                 \n"
      "punpcklbw   %%xmm2,%%xmm0                 \n"
      "punpckhbw   %%xmm2,%%xmm1                 \n"
      "psubb       %%xmm4,%%xmm0                 \n"
      "psubb       %%xmm4,%%xmm1                 \n"
      "movdqa      %%xmm5,%%xmm2                 \n"
      "movdqa      %%xmm5,%%xmm3                 \n"
      "pmaddubsw   %%xmm0,%%xmm2                 \n"
      "pmaddubsw   %%xmm1,%%xmm3                 \n"
      "paddw       %%xmm4,%%xmm2                 \n"
      "paddw       %%xmm4,%%xmm3                 \n"
      "psrlw       $0x8,%%xmm2                   \n"
      "psrlw       $0x8,%%xmm3                   \n"
      "packuswb    %%xmm3,%%xmm2                 \n"
      "movdqu      %%xmm2,0x00(%1,%0,1)          \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "jmp         99f                           \n"

      // Blend 50 / 50.
      LABELALIGN
      "50:                                       \n"
      "movdqu      (%1),%%xmm0                   \n"
      "movdqu      0x00(%1,%4,1),%%xmm1          \n"
      "pavgb       %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,0x00(%1,%0,1)          \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          50b                           \n"
      "jmp         99f                           \n"

      // Blend 100 / 0 - Copy row unchanged.
      LABELALIGN
      "100:                                      \n"
      "movdqu      (%1),%%xmm0                   \n"
      "movdqu      %%xmm0,0x00(%1,%0,1)          \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          100b                          \n"

      "99:                                       \n"
      : "+r"(dst_ptr),               // %0
        "+r"(src_ptr),               // %1
        "+rm"(width),                // %2
        "+r"(source_y_fraction)      // %3
      : "r"((intptr_t)(src_stride))  // %4
      : "memory", "cc", "eax", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_INTERPOLATEROW_SSSE3

#ifdef HAS_INTERPOLATEROW_SSSE3
ANY11I(InterpolateRow_Any_SSSE3, InterpolateRow_SSSE3, uint8_t, uint8_t, 1, 1, 15)
#endif


// Scale up horizontally 2 times using linear filter.
#define SUH2LANY(NAME, SIMD, C, MASK, PTYPE)                       \
  void NAME(const PTYPE* src_ptr, PTYPE* dst_ptr, int dst_width) { \
    int work_width = (dst_width - 1) & ~1;                         \
    int r = work_width & MASK;                                     \
    int n = work_width & ~MASK;                                    \
    dst_ptr[0] = src_ptr[0];                                       \
    if (work_width > 0) {                                          \
      if (n != 0) {                                                \
        SIMD(src_ptr, dst_ptr + 1, n);                             \
      }                                                            \
      C(src_ptr + (n / 2), dst_ptr + n + 1, r);                    \
    }                                                              \
    dst_ptr[dst_width - 1] = src_ptr[(dst_width - 1) / 2];         \
  }

// Even the C versions need to be wrapped, because boundary pixels have to
// be handled differently

SUH2LANY(ScaleRowUp2_Linear_Any_C,
         ScaleRowUp2_Linear_C,
         ScaleRowUp2_Linear_C,
         0,
         uint8_t)


#ifdef HAS_SCALEROWUP2LINEAR_SSE2
void ScaleRowUp2_Linear_SSE2(const uint8_t* src_ptr,
                             uint8_t* dst_ptr,
                             int dst_width) {
  asm volatile(
      "pxor        %%xmm0,%%xmm0                 \n"  // 0
      "pcmpeqw     %%xmm6,%%xmm6                 \n"
      "psrlw       $15,%%xmm6                    \n"
      "psllw       $1,%%xmm6                     \n"  // all 2

      LABELALIGN
      "1:                                        \n"
      "movq        (%0),%%xmm1                   \n"  // 01234567
      "movq        1(%0),%%xmm2                  \n"  // 12345678
      "movdqa      %%xmm1,%%xmm3                 \n"
      "punpcklbw   %%xmm2,%%xmm3                 \n"  // 0112233445566778
      "punpcklbw   %%xmm1,%%xmm1                 \n"  // 0011223344556677
      "punpcklbw   %%xmm2,%%xmm2                 \n"  // 1122334455667788
      "movdqa      %%xmm1,%%xmm4                 \n"
      "punpcklbw   %%xmm0,%%xmm4                 \n"  // 00112233 (16)
      "movdqa      %%xmm2,%%xmm5                 \n"
      "punpcklbw   %%xmm0,%%xmm5                 \n"  // 11223344 (16)
      "paddw       %%xmm5,%%xmm4                 \n"
      "movdqa      %%xmm3,%%xmm5                 \n"
      "paddw       %%xmm6,%%xmm4                 \n"
      "punpcklbw   %%xmm0,%%xmm5                 \n"  // 01122334 (16)
      "paddw       %%xmm5,%%xmm5                 \n"
      "paddw       %%xmm4,%%xmm5                 \n"  // 3*near+far+2 (lo)
      "psrlw       $2,%%xmm5                     \n"  // 3/4*near+1/4*far (lo)

      "punpckhbw   %%xmm0,%%xmm1                 \n"  // 44556677 (16)
      "punpckhbw   %%xmm0,%%xmm2                 \n"  // 55667788 (16)
      "paddw       %%xmm2,%%xmm1                 \n"
      "punpckhbw   %%xmm0,%%xmm3                 \n"  // 45566778 (16)
      "paddw       %%xmm6,%%xmm1                 \n"
      "paddw       %%xmm3,%%xmm3                 \n"
      "paddw       %%xmm3,%%xmm1                 \n"  // 3*near+far+2 (hi)
      "psrlw       $2,%%xmm1                     \n"  // 3/4*near+1/4*far (hi)

      "packuswb    %%xmm1,%%xmm5                 \n"
      "movdqu      %%xmm5,(%1)                   \n"

      "lea         0x8(%0),%0                    \n"
      "lea         0x10(%1),%1                   \n"  // 8 sample to 16 sample
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),   // %0
        "+r"(dst_ptr),   // %1
        "+r"(dst_width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}

SUH2LANY(ScaleRowUp2_Linear_Any_SSE2,
         ScaleRowUp2_Linear_SSE2,
         ScaleRowUp2_Linear_C,
         15,
         uint8_t)
#endif

static const uvec8 kLinearMadd31 = {3, 1, 1, 3, 3, 1, 1, 3,
                                    3, 1, 1, 3, 3, 1, 1, 3};

#ifdef HAS_SCALEROWUP2LINEAR_SSSE3
void ScaleRowUp2_Linear_SSSE3(const uint8_t* src_ptr,
                              uint8_t* dst_ptr,
                              int dst_width) {
  asm volatile(
      "pcmpeqw     %%xmm4,%%xmm4                 \n"
      "psrlw       $15,%%xmm4                    \n"
      "psllw       $1,%%xmm4                     \n"  // all 2
      "movdqa      %3,%%xmm3                     \n"

      LABELALIGN
      "1:                                        \n"
      "movq        (%0),%%xmm0                   \n"  // 01234567
      "movq        1(%0),%%xmm1                  \n"  // 12345678
      "punpcklwd   %%xmm0,%%xmm0                 \n"  // 0101232345456767
      "punpcklwd   %%xmm1,%%xmm1                 \n"  // 1212343456567878
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpckhdq   %%xmm1,%%xmm2                 \n"  // 4545565667677878
      "punpckldq   %%xmm1,%%xmm0                 \n"  // 0101121223233434
      "pmaddubsw   %%xmm3,%%xmm2                 \n"  // 3*near+far (hi)
      "pmaddubsw   %%xmm3,%%xmm0                 \n"  // 3*near+far (lo)
      "paddw       %%xmm4,%%xmm0                 \n"  // 3*near+far+2 (lo)
      "paddw       %%xmm4,%%xmm2                 \n"  // 3*near+far+2 (hi)
      "psrlw       $2,%%xmm0                     \n"  // 3/4*near+1/4*far (lo)
      "psrlw       $2,%%xmm2                     \n"  // 3/4*near+1/4*far (hi)
      "packuswb    %%xmm2,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "lea         0x8(%0),%0                    \n"
      "lea         0x10(%1),%1                   \n"  // 8 sample to 16 sample
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),      // %0
        "+r"(dst_ptr),      // %1
        "+r"(dst_width)     // %2
      : "m"(kLinearMadd31)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}

SUH2LANY(ScaleRowUp2_Linear_Any_SSSE3,
         ScaleRowUp2_Linear_SSSE3,
         ScaleRowUp2_Linear_C,
         15,
         uint8_t)
#endif

#undef SUH2LANY

// Scale up 2 times using bilinear filter.
// This function produces 2 rows at a time.
#define SU2BLANY(NAME, SIMD, C, MASK, PTYPE)                              \
  void NAME(const PTYPE* src_ptr, ptrdiff_t src_stride, PTYPE* dst_ptr,   \
            ptrdiff_t dst_stride, int dst_width) {                        \
    int work_width = (dst_width - 1) & ~1;                                \
    int r = work_width & MASK;                                            \
    int n = work_width & ~MASK;                                           \
    const PTYPE* sa = src_ptr;                                            \
    const PTYPE* sb = src_ptr + src_stride;                               \
    PTYPE* da = dst_ptr;                                                  \
    PTYPE* db = dst_ptr + dst_stride;                                     \
    da[0] = (3 * sa[0] + sb[0] + 2) >> 2;                                 \
    db[0] = (sa[0] + 3 * sb[0] + 2) >> 2;                                 \
    if (work_width > 0) {                                                 \
      if (n != 0) {                                                       \
        SIMD(sa, sb - sa, da + 1, db - da, n);                            \
      }                                                                   \
      C(sa + (n / 2), sb - sa, da + n + 1, db - da, r);                   \
    }                                                                     \
    da[dst_width - 1] =                                                   \
        (3 * sa[(dst_width - 1) / 2] + sb[(dst_width - 1) / 2] + 2) >> 2; \
    db[dst_width - 1] =                                                   \
        (sa[(dst_width - 1) / 2] + 3 * sb[(dst_width - 1) / 2] + 2) >> 2; \
  }

SU2BLANY(ScaleRowUp2_Bilinear_Any_C,
         ScaleRowUp2_Bilinear_C,
         ScaleRowUp2_Bilinear_C,
         0,
         uint8_t)

#ifdef HAS_SCALEROWUP2BILINEAR_SSE2
void ScaleRowUp2_Bilinear_SSE2(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               ptrdiff_t dst_stride,
                               int dst_width) {
  asm volatile(
      LABELALIGN
      "1:                                        \n"
      "pxor        %%xmm0,%%xmm0                 \n"  // 0
      // above line
      "movq        (%0),%%xmm1                   \n"  // 01234567
      "movq        1(%0),%%xmm2                  \n"  // 12345678
      "movdqa      %%xmm1,%%xmm3                 \n"
      "punpcklbw   %%xmm2,%%xmm3                 \n"  // 0112233445566778
      "punpcklbw   %%xmm1,%%xmm1                 \n"  // 0011223344556677
      "punpcklbw   %%xmm2,%%xmm2                 \n"  // 1122334455667788

      "movdqa      %%xmm1,%%xmm4                 \n"
      "punpcklbw   %%xmm0,%%xmm4                 \n"  // 00112233 (16)
      "movdqa      %%xmm2,%%xmm5                 \n"
      "punpcklbw   %%xmm0,%%xmm5                 \n"  // 11223344 (16)
      "paddw       %%xmm5,%%xmm4                 \n"  // near+far
      "movdqa      %%xmm3,%%xmm5                 \n"
      "punpcklbw   %%xmm0,%%xmm5                 \n"  // 01122334 (16)
      "paddw       %%xmm5,%%xmm5                 \n"  // 2*near
      "paddw       %%xmm5,%%xmm4                 \n"  // 3*near+far (1, lo)

      "punpckhbw   %%xmm0,%%xmm1                 \n"  // 44556677 (16)
      "punpckhbw   %%xmm0,%%xmm2                 \n"  // 55667788 (16)
      "paddw       %%xmm2,%%xmm1                 \n"
      "punpckhbw   %%xmm0,%%xmm3                 \n"  // 45566778 (16)
      "paddw       %%xmm3,%%xmm3                 \n"  // 2*near
      "paddw       %%xmm3,%%xmm1                 \n"  // 3*near+far (1, hi)

      // below line
      "movq        (%0,%3),%%xmm6                \n"  // 01234567
      "movq        1(%0,%3),%%xmm2               \n"  // 12345678
      "movdqa      %%xmm6,%%xmm3                 \n"
      "punpcklbw   %%xmm2,%%xmm3                 \n"  // 0112233445566778
      "punpcklbw   %%xmm6,%%xmm6                 \n"  // 0011223344556677
      "punpcklbw   %%xmm2,%%xmm2                 \n"  // 1122334455667788

      "movdqa      %%xmm6,%%xmm5                 \n"
      "punpcklbw   %%xmm0,%%xmm5                 \n"  // 00112233 (16)
      "movdqa      %%xmm2,%%xmm7                 \n"
      "punpcklbw   %%xmm0,%%xmm7                 \n"  // 11223344 (16)
      "paddw       %%xmm7,%%xmm5                 \n"  // near+far
      "movdqa      %%xmm3,%%xmm7                 \n"
      "punpcklbw   %%xmm0,%%xmm7                 \n"  // 01122334 (16)
      "paddw       %%xmm7,%%xmm7                 \n"  // 2*near
      "paddw       %%xmm7,%%xmm5                 \n"  // 3*near+far (2, lo)

      "punpckhbw   %%xmm0,%%xmm6                 \n"  // 44556677 (16)
      "punpckhbw   %%xmm0,%%xmm2                 \n"  // 55667788 (16)
      "paddw       %%xmm6,%%xmm2                 \n"  // near+far
      "punpckhbw   %%xmm0,%%xmm3                 \n"  // 45566778 (16)
      "paddw       %%xmm3,%%xmm3                 \n"  // 2*near
      "paddw       %%xmm3,%%xmm2                 \n"  // 3*near+far (2, hi)

      // xmm4 xmm1
      // xmm5 xmm2
      "pcmpeqw     %%xmm0,%%xmm0                 \n"
      "psrlw       $15,%%xmm0                    \n"
      "psllw       $3,%%xmm0                     \n"  // all 8

      "movdqa      %%xmm4,%%xmm3                 \n"
      "movdqa      %%xmm5,%%xmm6                 \n"
      "paddw       %%xmm3,%%xmm3                 \n"  // 6*near+2*far (1, lo)
      "paddw       %%xmm0,%%xmm6                 \n"  // 3*near+far+8 (2, lo)
      "paddw       %%xmm4,%%xmm3                 \n"  // 9*near+3*far (1, lo)
      "paddw       %%xmm6,%%xmm3                 \n"  // 9 3 3 1 + 8 (1, lo)
      "psrlw       $4,%%xmm3                     \n"  // ^ div by 16

      "movdqa      %%xmm1,%%xmm7                 \n"
      "movdqa      %%xmm2,%%xmm6                 \n"
      "paddw       %%xmm7,%%xmm7                 \n"  // 6*near+2*far (1, hi)
      "paddw       %%xmm0,%%xmm6                 \n"  // 3*near+far+8 (2, hi)
      "paddw       %%xmm1,%%xmm7                 \n"  // 9*near+3*far (1, hi)
      "paddw       %%xmm6,%%xmm7                 \n"  // 9 3 3 1 + 8 (1, hi)
      "psrlw       $4,%%xmm7                     \n"  // ^ div by 16

      "packuswb    %%xmm7,%%xmm3                 \n"
      "movdqu      %%xmm3,(%1)                   \n"  // save above line

      "movdqa      %%xmm5,%%xmm3                 \n"
      "paddw       %%xmm0,%%xmm4                 \n"  // 3*near+far+8 (1, lo)
      "paddw       %%xmm3,%%xmm3                 \n"  // 6*near+2*far (2, lo)
      "paddw       %%xmm3,%%xmm5                 \n"  // 9*near+3*far (2, lo)
      "paddw       %%xmm4,%%xmm5                 \n"  // 9 3 3 1 + 8 (lo)
      "psrlw       $4,%%xmm5                     \n"  // ^ div by 16

      "movdqa      %%xmm2,%%xmm3                 \n"
      "paddw       %%xmm0,%%xmm1                 \n"  // 3*near+far+8 (1, hi)
      "paddw       %%xmm3,%%xmm3                 \n"  // 6*near+2*far (2, hi)
      "paddw       %%xmm3,%%xmm2                 \n"  // 9*near+3*far (2, hi)
      "paddw       %%xmm1,%%xmm2                 \n"  // 9 3 3 1 + 8 (hi)
      "psrlw       $4,%%xmm2                     \n"  // ^ div by 16

      "packuswb    %%xmm2,%%xmm5                 \n"
      "movdqu      %%xmm5,(%1,%4)                \n"  // save below line

      "lea         0x8(%0),%0                    \n"
      "lea         0x10(%1),%1                   \n"  // 8 sample to 16 sample
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),                // %0
        "+r"(dst_ptr),                // %1
        "+r"(dst_width)               // %2
      : "r"((intptr_t)(src_stride)),  // %3
        "r"((intptr_t)(dst_stride))   // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}

SU2BLANY(ScaleRowUp2_Bilinear_Any_SSE2,
         ScaleRowUp2_Bilinear_SSE2,
         ScaleRowUp2_Bilinear_C,
         15,
         uint8_t)
#endif

#ifdef HAS_SCALEROWUP2BILINEAR_SSSE3
void ScaleRowUp2_Bilinear_SSSE3(const uint8_t* src_ptr,
                                ptrdiff_t src_stride,
                                uint8_t* dst_ptr,
                                ptrdiff_t dst_stride,
                                int dst_width) {
  asm volatile(
      "pcmpeqw     %%xmm6,%%xmm6                 \n"
      "psrlw       $15,%%xmm6                    \n"
      "psllw       $3,%%xmm6                     \n"  // all 8
      "movdqa      %5,%%xmm7                     \n"

      LABELALIGN
      "1:                                        \n"
      "movq        (%0),%%xmm0                   \n"  // 01234567
      "movq        1(%0),%%xmm1                  \n"  // 12345678
      "punpcklwd   %%xmm0,%%xmm0                 \n"  // 0101232345456767
      "punpcklwd   %%xmm1,%%xmm1                 \n"  // 1212343456567878
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpckhdq   %%xmm1,%%xmm2                 \n"  // 4545565667677878
      "punpckldq   %%xmm1,%%xmm0                 \n"  // 0101121223233434
      "pmaddubsw   %%xmm7,%%xmm2                 \n"  // 3*near+far (1, hi)
      "pmaddubsw   %%xmm7,%%xmm0                 \n"  // 3*near+far (1, lo)

      "movq        (%0,%3),%%xmm1                \n"
      "movq        1(%0,%3),%%xmm4               \n"
      "punpcklwd   %%xmm1,%%xmm1                 \n"
      "punpcklwd   %%xmm4,%%xmm4                 \n"
      "movdqa      %%xmm1,%%xmm3                 \n"
      "punpckhdq   %%xmm4,%%xmm3                 \n"
      "punpckldq   %%xmm4,%%xmm1                 \n"
      "pmaddubsw   %%xmm7,%%xmm3                 \n"  // 3*near+far (2, hi)
      "pmaddubsw   %%xmm7,%%xmm1                 \n"  // 3*near+far (2, lo)

      // xmm0 xmm2
      // xmm1 xmm3

      "movdqa      %%xmm0,%%xmm4                 \n"
      "movdqa      %%xmm1,%%xmm5                 \n"
      "paddw       %%xmm0,%%xmm4                 \n"  // 6*near+2*far (1, lo)
      "paddw       %%xmm6,%%xmm5                 \n"  // 3*near+far+8 (2, lo)
      "paddw       %%xmm0,%%xmm4                 \n"  // 9*near+3*far (1, lo)
      "paddw       %%xmm5,%%xmm4                 \n"  // 9 3 3 1 + 8 (1, lo)
      "psrlw       $4,%%xmm4                     \n"  // ^ div by 16 (1, lo)

      "movdqa      %%xmm1,%%xmm5                 \n"
      "paddw       %%xmm1,%%xmm5                 \n"  // 6*near+2*far (2, lo)
      "paddw       %%xmm6,%%xmm0                 \n"  // 3*near+far+8 (1, lo)
      "paddw       %%xmm1,%%xmm5                 \n"  // 9*near+3*far (2, lo)
      "paddw       %%xmm0,%%xmm5                 \n"  // 9 3 3 1 + 8 (2, lo)
      "psrlw       $4,%%xmm5                     \n"  // ^ div by 16 (2, lo)

      "movdqa      %%xmm2,%%xmm0                 \n"
      "movdqa      %%xmm3,%%xmm1                 \n"
      "paddw       %%xmm2,%%xmm0                 \n"  // 6*near+2*far (1, hi)
      "paddw       %%xmm6,%%xmm1                 \n"  // 3*near+far+8 (2, hi)
      "paddw       %%xmm2,%%xmm0                 \n"  // 9*near+3*far (1, hi)
      "paddw       %%xmm1,%%xmm0                 \n"  // 9 3 3 1 + 8 (1, hi)
      "psrlw       $4,%%xmm0                     \n"  // ^ div by 16 (1, hi)

      "movdqa      %%xmm3,%%xmm1                 \n"
      "paddw       %%xmm3,%%xmm1                 \n"  // 6*near+2*far (2, hi)
      "paddw       %%xmm6,%%xmm2                 \n"  // 3*near+far+8 (1, hi)
      "paddw       %%xmm3,%%xmm1                 \n"  // 9*near+3*far (2, hi)
      "paddw       %%xmm2,%%xmm1                 \n"  // 9 3 3 1 + 8 (2, hi)
      "psrlw       $4,%%xmm1                     \n"  // ^ div by 16 (2, hi)

      "packuswb    %%xmm0,%%xmm4                 \n"
      "movdqu      %%xmm4,(%1)                   \n"  // store above
      "packuswb    %%xmm1,%%xmm5                 \n"
      "movdqu      %%xmm5,(%1,%4)                \n"  // store below

      "lea         0x8(%0),%0                    \n"
      "lea         0x10(%1),%1                   \n"  // 8 sample to 16 sample
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),                // %0
        "+r"(dst_ptr),                // %1
        "+r"(dst_width)               // %2
      : "r"((intptr_t)(src_stride)),  // %3
        "r"((intptr_t)(dst_stride)),  // %4
        "m"(kLinearMadd31)            // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}

SU2BLANY(ScaleRowUp2_Bilinear_Any_SSSE3,
         ScaleRowUp2_Bilinear_SSSE3,
         ScaleRowUp2_Bilinear_C,
         15,
         uint8_t)
#endif

#undef SU2BLANY

// Constant for making pixels signed to avoid pmaddubsw
// saturation.
static const uvec8 kFsub80 = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
                              0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};

// Constant for making pixels unsigned and adding .5 for rounding.
static const uvec16 kFadd40 = {0x4040, 0x4040, 0x4040, 0x4040,
                               0x4040, 0x4040, 0x4040, 0x4040};

// Bilinear column filtering. SSSE3 version.
void ScaleFilterCols_SSSE3(uint8_t* dst_ptr,
                           const uint8_t* src_ptr,
                           int dst_width,
                           int x,
                           int dx) {
  intptr_t x0, x1, temp_pixel;
  asm volatile(
      "movd        %6,%%xmm2                     \n"
      "movd        %7,%%xmm3                     \n"
      "movl        $0x04040000,%k2               \n"
      "movd        %k2,%%xmm5                    \n"
      "pcmpeqb     %%xmm6,%%xmm6                 \n"
      "psrlw       $0x9,%%xmm6                   \n"  // 0x007f007f
      "pcmpeqb     %%xmm7,%%xmm7                 \n"
      "psrlw       $15,%%xmm7                    \n"  // 0x00010001

      "pextrw      $0x1,%%xmm2,%k3               \n"
      "subl        $0x2,%5                       \n"
      "jl          29f                           \n"
      "movdqa      %%xmm2,%%xmm0                 \n"
      "paddd       %%xmm3,%%xmm0                 \n"
      "punpckldq   %%xmm0,%%xmm2                 \n"
      "punpckldq   %%xmm3,%%xmm3                 \n"
      "paddd       %%xmm3,%%xmm3                 \n"
      "pextrw      $0x3,%%xmm2,%k4               \n"

      LABELALIGN
      "2:                                        \n"
      "movdqa      %%xmm2,%%xmm1                 \n"
      "paddd       %%xmm3,%%xmm2                 \n"
      "movzwl      0x00(%1,%3,1),%k2             \n"
      "movd        %k2,%%xmm0                    \n"
      "psrlw       $0x9,%%xmm1                   \n"
      "movzwl      0x00(%1,%4,1),%k2             \n"
      "movd        %k2,%%xmm4                    \n"
      "pshufb      %%xmm5,%%xmm1                 \n"
      "punpcklwd   %%xmm4,%%xmm0                 \n"
      "psubb       %8,%%xmm0                     \n"  // make pixels signed.
      "pxor        %%xmm6,%%xmm1                 \n"  // 128 - f = (f ^ 127 ) +
                                                      // 1
      "paddusb     %%xmm7,%%xmm1                 \n"
      "pmaddubsw   %%xmm0,%%xmm1                 \n"
      "pextrw      $0x1,%%xmm2,%k3               \n"
      "pextrw      $0x3,%%xmm2,%k4               \n"
      "paddw       %9,%%xmm1                     \n"  // make pixels unsigned.
      "psrlw       $0x7,%%xmm1                   \n"
      "packuswb    %%xmm1,%%xmm1                 \n"
      "movd        %%xmm1,%k2                    \n"
      "mov         %w2,(%0)                      \n"
      "lea         0x2(%0),%0                    \n"
      "subl        $0x2,%5                       \n"
      "jge         2b                            \n"

      LABELALIGN
      "29:                                       \n"
      "addl        $0x1,%5                       \n"
      "jl          99f                           \n"
      "movzwl      0x00(%1,%3,1),%k2             \n"
      "movd        %k2,%%xmm0                    \n"
      "psrlw       $0x9,%%xmm2                   \n"
      "pshufb      %%xmm5,%%xmm2                 \n"
      "psubb       %8,%%xmm0                     \n"  // make pixels signed.
      "pxor        %%xmm6,%%xmm2                 \n"
      "paddusb     %%xmm7,%%xmm2                 \n"
      "pmaddubsw   %%xmm0,%%xmm2                 \n"
      "paddw       %9,%%xmm2                     \n"  // make pixels unsigned.
      "psrlw       $0x7,%%xmm2                   \n"
      "packuswb    %%xmm2,%%xmm2                 \n"
      "movd        %%xmm2,%k2                    \n"
      "mov         %b2,(%0)                      \n"
      "99:                                       \n"
      : "+r"(dst_ptr),      // %0
        "+r"(src_ptr),      // %1
        "=&a"(temp_pixel),  // %2
        "=&r"(x0),          // %3
        "=&r"(x1),          // %4
#if defined(__x86_64__)
        "+rm"(dst_width)  // %5
#else
        "+m"(dst_width)  // %5
#endif
      : "rm"(x),   // %6
        "rm"(dx),  // %7
#if defined(__x86_64__)
        "x"(kFsub80),  // %8
        "x"(kFadd40)   // %9
#else
        "m"(kFsub80),    // %8
        "m"(kFadd40)     // %9
#endif
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}


// Scale bi-planar plane up horizontally 2 times using linear filter.
#define SBUH2LANY(NAME, SIMD, C, MASK, PTYPE)                         \
  void NAME(const PTYPE* src_ptr, PTYPE* dst_ptr, int dst_width) {    \
    int work_width = (dst_width - 1) & ~1;                            \
    int r = work_width & MASK;                                        \
    int n = work_width & ~MASK;                                       \
    dst_ptr[0] = src_ptr[0];                                          \
    dst_ptr[1] = src_ptr[1];                                          \
    if (work_width > 0) {                                             \
      if (n != 0) {                                                   \
        SIMD(src_ptr, dst_ptr + 2, n);                                \
      }                                                               \
      C(src_ptr + n, dst_ptr + 2 * n + 2, r);                         \
    }                                                                 \
    dst_ptr[2 * dst_width - 2] = src_ptr[((dst_width + 1) & ~1) - 2]; \
    dst_ptr[2 * dst_width - 1] = src_ptr[((dst_width + 1) & ~1) - 1]; \
  }

SBUH2LANY(ScaleUVRowUp2_Linear_Any_C,
          ScaleUVRowUp2_Linear_C,
          ScaleUVRowUp2_Linear_C,
          0,
          uint8_t)

static const uvec8 kUVLinearMadd31 = {3, 1, 3, 1, 1, 3, 1, 3,
                                      3, 1, 3, 1, 1, 3, 1, 3};

#ifdef HAS_SCALEUVROWUP2LINEAR_SSSE3
void ScaleUVRowUp2_Linear_SSSE3(const uint8_t* src_ptr,
                                uint8_t* dst_ptr,
                                int dst_width) {
  asm volatile(
      "pcmpeqw     %%xmm4,%%xmm4                 \n"
      "psrlw       $15,%%xmm4                    \n"
      "psllw       $1,%%xmm4                     \n"  // all 2
      "movdqa      %3,%%xmm3                     \n"

      LABELALIGN
      "1:                                        \n"
      "movq        (%0),%%xmm0                   \n"  // 00112233 (1u1v)
      "movq        2(%0),%%xmm1                  \n"  // 11223344 (1u1v)
      "punpcklbw   %%xmm1,%%xmm0                 \n"  // 0101121223233434 (2u2v)
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpckhdq   %%xmm0,%%xmm2                 \n"  // 2323232334343434 (2u2v)
      "punpckldq   %%xmm0,%%xmm0                 \n"  // 0101010112121212 (2u2v)
      "pmaddubsw   %%xmm3,%%xmm2                 \n"  // 3*near+far (1u1v16, hi)
      "pmaddubsw   %%xmm3,%%xmm0                 \n"  // 3*near+far (1u1v16, lo)
      "paddw       %%xmm4,%%xmm0                 \n"  // 3*near+far+2 (lo)
      "paddw       %%xmm4,%%xmm2                 \n"  // 3*near+far+2 (hi)
      "psrlw       $2,%%xmm0                     \n"  // 3/4*near+1/4*far (lo)
      "psrlw       $2,%%xmm2                     \n"  // 3/4*near+1/4*far (hi)
      "packuswb    %%xmm2,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"

      "lea         0x8(%0),%0                    \n"
      "lea         0x10(%1),%1                   \n"  // 4 uv to 8 uv
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),        // %0
        "+r"(dst_ptr),        // %1
        "+r"(dst_width)       // %2
      : "m"(kUVLinearMadd31)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}

SBUH2LANY(ScaleUVRowUp2_Linear_Any_SSSE3,
          ScaleUVRowUp2_Linear_SSSE3,
          ScaleUVRowUp2_Linear_C,
          7,
          uint8_t)
#endif

#undef SBUH2LANY

// Scale bi-planar plane up 2 times using bilinear filter.
// This function produces 2 rows at a time.
#define SBU2BLANY(NAME, SIMD, C, MASK, PTYPE)                           \
  void NAME(const PTYPE* src_ptr, ptrdiff_t src_stride, PTYPE* dst_ptr, \
            ptrdiff_t dst_stride, int dst_width) {                      \
    int work_width = (dst_width - 1) & ~1;                              \
    int r = work_width & MASK;                                          \
    int n = work_width & ~MASK;                                         \
    const PTYPE* sa = src_ptr;                                          \
    const PTYPE* sb = src_ptr + src_stride;                             \
    PTYPE* da = dst_ptr;                                                \
    PTYPE* db = dst_ptr + dst_stride;                                   \
    da[0] = (3 * sa[0] + sb[0] + 2) >> 2;                               \
    db[0] = (sa[0] + 3 * sb[0] + 2) >> 2;                               \
    da[1] = (3 * sa[1] + sb[1] + 2) >> 2;                               \
    db[1] = (sa[1] + 3 * sb[1] + 2) >> 2;                               \
    if (work_width > 0) {                                               \
      if (n != 0) {                                                     \
        SIMD(sa, sb - sa, da + 2, db - da, n);                          \
      }                                                                 \
      C(sa + n, sb - sa, da + 2 * n + 2, db - da, r);                   \
    }                                                                   \
    da[2 * dst_width - 2] = (3 * sa[((dst_width + 1) & ~1) - 2] +       \
                             sb[((dst_width + 1) & ~1) - 2] + 2) >>     \
                            2;                                          \
    db[2 * dst_width - 2] = (sa[((dst_width + 1) & ~1) - 2] +           \
                             3 * sb[((dst_width + 1) & ~1) - 2] + 2) >> \
                            2;                                          \
    da[2 * dst_width - 1] = (3 * sa[((dst_width + 1) & ~1) - 1] +       \
                             sb[((dst_width + 1) & ~1) - 1] + 2) >>     \
                            2;                                          \
    db[2 * dst_width - 1] = (sa[((dst_width + 1) & ~1) - 1] +           \
                             3 * sb[((dst_width + 1) & ~1) - 1] + 2) >> \
                            2;                                          \
  }

SBU2BLANY(ScaleUVRowUp2_Bilinear_Any_C,
          ScaleUVRowUp2_Bilinear_C,
          ScaleUVRowUp2_Bilinear_C,
          0,
          uint8_t)

#ifdef HAS_SCALEUVROWUP2BILINEAR_SSSE3
void ScaleUVRowUp2_Bilinear_SSSE3(const uint8_t* src_ptr,
                                  ptrdiff_t src_stride,
                                  uint8_t* dst_ptr,
                                  ptrdiff_t dst_stride,
                                  int dst_width) {
  asm volatile(
      "pcmpeqw     %%xmm6,%%xmm6                 \n"
      "psrlw       $15,%%xmm6                    \n"
      "psllw       $3,%%xmm6                     \n"  // all 8
      "movdqa      %5,%%xmm7                     \n"

      LABELALIGN
      "1:                                        \n"
      "movq        (%0),%%xmm0                   \n"  // 00112233 (1u1v)
      "movq        2(%0),%%xmm1                  \n"  // 11223344 (1u1v)
      "punpcklbw   %%xmm1,%%xmm0                 \n"  // 0101121223233434 (2u2v)
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpckhdq   %%xmm0,%%xmm2                 \n"  // 2323232334343434 (2u2v)
      "punpckldq   %%xmm0,%%xmm0                 \n"  // 0101010112121212 (2u2v)
      "pmaddubsw   %%xmm7,%%xmm2                 \n"  // 3*near+far (1u1v16, hi)
      "pmaddubsw   %%xmm7,%%xmm0                 \n"  // 3*near+far (1u1v16, lo)

      "movq        (%0,%3),%%xmm1                \n"
      "movq        2(%0,%3),%%xmm4               \n"
      "punpcklbw   %%xmm4,%%xmm1                 \n"
      "movdqa      %%xmm1,%%xmm3                 \n"
      "punpckhdq   %%xmm1,%%xmm3                 \n"
      "punpckldq   %%xmm1,%%xmm1                 \n"
      "pmaddubsw   %%xmm7,%%xmm3                 \n"  // 3*near+far (2, hi)
      "pmaddubsw   %%xmm7,%%xmm1                 \n"  // 3*near+far (2, lo)

      // xmm0 xmm2
      // xmm1 xmm3

      "movdqa      %%xmm0,%%xmm4                 \n"
      "movdqa      %%xmm1,%%xmm5                 \n"
      "paddw       %%xmm0,%%xmm4                 \n"  // 6*near+2*far (1, lo)
      "paddw       %%xmm6,%%xmm5                 \n"  // 3*near+far+8 (2, lo)
      "paddw       %%xmm0,%%xmm4                 \n"  // 9*near+3*far (1, lo)
      "paddw       %%xmm5,%%xmm4                 \n"  // 9 3 3 1 + 8 (1, lo)
      "psrlw       $4,%%xmm4                     \n"  // ^ div by 16 (1, lo)

      "movdqa      %%xmm1,%%xmm5                 \n"
      "paddw       %%xmm1,%%xmm5                 \n"  // 6*near+2*far (2, lo)
      "paddw       %%xmm6,%%xmm0                 \n"  // 3*near+far+8 (1, lo)
      "paddw       %%xmm1,%%xmm5                 \n"  // 9*near+3*far (2, lo)
      "paddw       %%xmm0,%%xmm5                 \n"  // 9 3 3 1 + 8 (2, lo)
      "psrlw       $4,%%xmm5                     \n"  // ^ div by 16 (2, lo)

      "movdqa      %%xmm2,%%xmm0                 \n"
      "movdqa      %%xmm3,%%xmm1                 \n"
      "paddw       %%xmm2,%%xmm0                 \n"  // 6*near+2*far (1, hi)
      "paddw       %%xmm6,%%xmm1                 \n"  // 3*near+far+8 (2, hi)
      "paddw       %%xmm2,%%xmm0                 \n"  // 9*near+3*far (1, hi)
      "paddw       %%xmm1,%%xmm0                 \n"  // 9 3 3 1 + 8 (1, hi)
      "psrlw       $4,%%xmm0                     \n"  // ^ div by 16 (1, hi)

      "movdqa      %%xmm3,%%xmm1                 \n"
      "paddw       %%xmm3,%%xmm1                 \n"  // 6*near+2*far (2, hi)
      "paddw       %%xmm6,%%xmm2                 \n"  // 3*near+far+8 (1, hi)
      "paddw       %%xmm3,%%xmm1                 \n"  // 9*near+3*far (2, hi)
      "paddw       %%xmm2,%%xmm1                 \n"  // 9 3 3 1 + 8 (2, hi)
      "psrlw       $4,%%xmm1                     \n"  // ^ div by 16 (2, hi)

      "packuswb    %%xmm0,%%xmm4                 \n"
      "movdqu      %%xmm4,(%1)                   \n"  // store above
      "packuswb    %%xmm1,%%xmm5                 \n"
      "movdqu      %%xmm5,(%1,%4)                \n"  // store below

      "lea         0x8(%0),%0                    \n"
      "lea         0x10(%1),%1                   \n"  // 4 uv to 8 uv
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_ptr),                // %0
        "+r"(dst_ptr),                // %1
        "+r"(dst_width)               // %2
      : "r"((intptr_t)(src_stride)),  // %3
        "r"((intptr_t)(dst_stride)),  // %4
        "m"(kUVLinearMadd31)          // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}

SBU2BLANY(ScaleUVRowUp2_Bilinear_Any_SSSE3,
          ScaleUVRowUp2_Bilinear_SSSE3,
          ScaleUVRowUp2_Bilinear_C,
          7,
          uint8_t)
#endif

#endif  // defined(__x86_64__) || defined(__i386__)

#if !defined(LIBYUV_DISABLE_MSA) && defined(__mips_msa)

#include <msa.h>
#include <stdint.h>

#define ALPHA_VAL (-1)

#define ST_B(RTYPE, in, pdst) *((RTYPE*)(pdst)) = (in) /* NOLINT */
#define ST_UB(...) ST_B(v16u8, __VA_ARGS__)

/* Description : Store two vectors with stride each having 16 'byte' sized
                 elements
   Arguments   : Inputs - in0, in1, pdst, stride
   Details     : Store 16 byte elements from 'in0' to (pdst)
                 Store 16 byte elements from 'in1' to (pdst + stride)
*/
#define ST_B2(RTYPE, in0, in1, pdst, stride) \
  {                                          \
    ST_B(RTYPE, in0, (pdst));                \
    ST_B(RTYPE, in1, (pdst) + stride);       \
  }
#define ST_UB2(...) ST_B2(v16u8, __VA_ARGS__)

#define ST_B4(RTYPE, in0, in1, in2, in3, pdst, stride)   \
  {                                                      \
    ST_B2(RTYPE, in0, in1, (pdst), stride);              \
    ST_B2(RTYPE, in2, in3, (pdst) + 2 * stride, stride); \
  }
#define ST_UB4(...) ST_B4(v16u8, __VA_ARGS__)

#define LOAD_INDEXED_DATA(srcp, indx0, out0) \
  {                                          \
    out0[0] = srcp[indx0[0]];                \
    out0[1] = srcp[indx0[1]];                \
    out0[2] = srcp[indx0[2]];                \
    out0[3] = srcp[indx0[3]];                \
  }

void ScaleARGBFilterCols_MSA(uint8_t* dst_argb,
                             const uint8_t* src_argb,
                             int dst_width,
                             int x,
                             int dx) {
  const uint32_t* src = (const uint32_t*)(src_argb);
  int j;
  v4u32 src0, src1, src2, src3;
  v4u32 vec0, vec1, vec2, vec3;
  v16u8 reg0, reg1, reg2, reg3, reg4, reg5, reg6, reg7;
  v16u8 mult0, mult1, mult2, mult3;
  v8u16 tmp0, tmp1, tmp2, tmp3;
  v16u8 dst0, dst1;
  v4u32 vec_x = (v4u32)__msa_fill_w(x);
  v4u32 vec_dx = (v4u32)__msa_fill_w(dx);
  v4u32 vec_const = {0, 1, 2, 3};
  v16u8 const_0x7f = (v16u8)__msa_fill_b(0x7f);

  vec0 = vec_dx * vec_const;
  vec1 = vec_dx * 4;
  vec_x += vec0;

  for (j = 0; j < dst_width - 1; j += 8) {
    vec2 = vec_x >> 16;
    reg0 = (v16u8)(vec_x >> 9);
    vec_x += vec1;
    vec3 = vec_x >> 16;
    reg1 = (v16u8)(vec_x >> 9);
    vec_x += vec1;
    reg0 = reg0 & const_0x7f;
    reg1 = reg1 & const_0x7f;
    reg0 = (v16u8)__msa_shf_b((v16i8)reg0, 0);
    reg1 = (v16u8)__msa_shf_b((v16i8)reg1, 0);
    reg2 = reg0 ^ const_0x7f;
    reg3 = reg1 ^ const_0x7f;
    mult0 = (v16u8)__msa_ilvr_b((v16i8)reg0, (v16i8)reg2);
    mult1 = (v16u8)__msa_ilvl_b((v16i8)reg0, (v16i8)reg2);
    mult2 = (v16u8)__msa_ilvr_b((v16i8)reg1, (v16i8)reg3);
    mult3 = (v16u8)__msa_ilvl_b((v16i8)reg1, (v16i8)reg3);
    LOAD_INDEXED_DATA(src, vec2, src0);
    LOAD_INDEXED_DATA(src, vec3, src1);
    vec2 += 1;
    vec3 += 1;
    LOAD_INDEXED_DATA(src, vec2, src2);
    LOAD_INDEXED_DATA(src, vec3, src3);
    reg4 = (v16u8)__msa_ilvr_b((v16i8)src2, (v16i8)src0);
    reg5 = (v16u8)__msa_ilvl_b((v16i8)src2, (v16i8)src0);
    reg6 = (v16u8)__msa_ilvr_b((v16i8)src3, (v16i8)src1);
    reg7 = (v16u8)__msa_ilvl_b((v16i8)src3, (v16i8)src1);
    tmp0 = __msa_dotp_u_h(reg4, mult0);
    tmp1 = __msa_dotp_u_h(reg5, mult1);
    tmp2 = __msa_dotp_u_h(reg6, mult2);
    tmp3 = __msa_dotp_u_h(reg7, mult3);
    tmp0 >>= 7;
    tmp1 >>= 7;
    tmp2 >>= 7;
    tmp3 >>= 7;
    dst0 = (v16u8)__msa_pckev_b((v16i8)tmp1, (v16i8)tmp0);
    dst1 = (v16u8)__msa_pckev_b((v16i8)tmp3, (v16i8)tmp2);
    __msa_st_b(dst0, dst_argb, 0);
    __msa_st_b(dst1, dst_argb, 16);
    dst_argb += 32;
  }
}

void ScaleARGBCols_MSA(uint8_t* dst_argb,
                       const uint8_t* src_argb,
                       int dst_width,
                       int x,
                       int dx) {
  const uint32_t* src = (const uint32_t*)(src_argb);
  uint32_t* dst = (uint32_t*)(dst_argb);
  int j;
  v4i32 x_vec = __msa_fill_w(x);
  v4i32 dx_vec = __msa_fill_w(dx);
  v4i32 const_vec = {0, 1, 2, 3};
  v4i32 vec0, vec1, vec2;
  v4i32 dst0;

  vec0 = dx_vec * const_vec;
  vec1 = dx_vec * 4;
  x_vec += vec0;

  for (j = 0; j < dst_width; j += 4) {
    vec2 = x_vec >> 16;
    x_vec += vec1;
    LOAD_INDEXED_DATA(src, vec2, dst0);
    __msa_st_w(dst0, dst, 0);
    dst += 4;
  }
}

void RGB24ToARGBRow_MSA(const uint8_t* src_rgb24,
                        uint8_t* dst_argb,
                        int width) {
  int x;
  v16u8 src0, src1, src2;
  v16u8 vec0, vec1, vec2;
  v16u8 dst0, dst1, dst2, dst3;
  v16u8 alpha = (v16u8)__msa_ldi_b(ALPHA_VAL);
  v16i8 shuffler = {0, 1, 2, 16, 3, 4, 5, 17, 6, 7, 8, 18, 9, 10, 11, 19};

  for (x = 0; x < width; x += 16) {
    src0 = (v16u8)__msa_ld_b((void*)src_rgb24, 0);
    src1 = (v16u8)__msa_ld_b((void*)src_rgb24, 16);
    src2 = (v16u8)__msa_ld_b((void*)src_rgb24, 32);
    vec0 = (v16u8)__msa_sldi_b((v16i8)src1, (v16i8)src0, 12);
    vec1 = (v16u8)__msa_sldi_b((v16i8)src2, (v16i8)src1, 8);
    vec2 = (v16u8)__msa_sldi_b((v16i8)src2, (v16i8)src2, 4);
    dst0 = (v16u8)__msa_vshf_b(shuffler, (v16i8)alpha, (v16i8)src0);
    dst1 = (v16u8)__msa_vshf_b(shuffler, (v16i8)alpha, (v16i8)vec0);
    dst2 = (v16u8)__msa_vshf_b(shuffler, (v16i8)alpha, (v16i8)vec1);
    dst3 = (v16u8)__msa_vshf_b(shuffler, (v16i8)alpha, (v16i8)vec2);
    ST_UB4(dst0, dst1, dst2, dst3, dst_argb, 16);
    src_rgb24 += 48;
    dst_argb += 64;
  }
}

void InterpolateRow_MSA(uint8_t* dst_ptr,
                        const uint8_t* src_ptr,
                        ptrdiff_t src_stride,
                        int width,
                        int32_t source_y_fraction) {
  int32_t y1_fraction = source_y_fraction;
  int32_t y0_fraction = 256 - y1_fraction;
  uint16_t y_fractions;
  const uint8_t* s = src_ptr;
  const uint8_t* t = src_ptr + src_stride;
  int x;
  v16u8 src0, src1, src2, src3, dst0, dst1;
  v8u16 vec0, vec1, vec2, vec3, y_frac;

  if (0 == y1_fraction) {
    memcpy(dst_ptr, src_ptr, width);
    return;
  }

  if (128 == y1_fraction) {
    for (x = 0; x < width; x += 32) {
      src0 = (v16u8)__msa_ld_b((void*)s, 0);
      src1 = (v16u8)__msa_ld_b((void*)s, 16);
      src2 = (v16u8)__msa_ld_b((void*)t, 0);
      src3 = (v16u8)__msa_ld_b((void*)t, 16);
      dst0 = __msa_aver_u_b(src0, src2);
      dst1 = __msa_aver_u_b(src1, src3);
      ST_UB2(dst0, dst1, dst_ptr, 16);
      s += 32;
      t += 32;
      dst_ptr += 32;
    }
    return;
  }

  y_fractions = (uint16_t)(y0_fraction + (y1_fraction << 8));
  y_frac = (v8u16)__msa_fill_h(y_fractions);

  for (x = 0; x < width; x += 32) {
    src0 = (v16u8)__msa_ld_b((void*)s, 0);
    src1 = (v16u8)__msa_ld_b((void*)s, 16);
    src2 = (v16u8)__msa_ld_b((void*)t, 0);
    src3 = (v16u8)__msa_ld_b((void*)t, 16);
    vec0 = (v8u16)__msa_ilvr_b((v16i8)src2, (v16i8)src0);
    vec1 = (v8u16)__msa_ilvl_b((v16i8)src2, (v16i8)src0);
    vec2 = (v8u16)__msa_ilvr_b((v16i8)src3, (v16i8)src1);
    vec3 = (v8u16)__msa_ilvl_b((v16i8)src3, (v16i8)src1);
    vec0 = (v8u16)__msa_dotp_u_h((v16u8)vec0, (v16u8)y_frac);
    vec1 = (v8u16)__msa_dotp_u_h((v16u8)vec1, (v16u8)y_frac);
    vec2 = (v8u16)__msa_dotp_u_h((v16u8)vec2, (v16u8)y_frac);
    vec3 = (v8u16)__msa_dotp_u_h((v16u8)vec3, (v16u8)y_frac);
    vec0 = (v8u16)__msa_srari_h((v8i16)vec0, 8);
    vec1 = (v8u16)__msa_srari_h((v8i16)vec1, 8);
    vec2 = (v8u16)__msa_srari_h((v8i16)vec2, 8);
    vec3 = (v8u16)__msa_srari_h((v8i16)vec3, 8);
    dst0 = (v16u8)__msa_pckev_b((v16i8)vec1, (v16i8)vec0);
    dst1 = (v16u8)__msa_pckev_b((v16i8)vec3, (v16i8)vec2);
    ST_UB2(dst0, dst1, dst_ptr, 16);
    s += 32;
    t += 32;
    dst_ptr += 32;
  }
}


void ARGBToRGB24Row_MSA(const uint8_t* src_argb, uint8_t* dst_rgb, int width) {
  int x;
  v16u8 src0, src1, src2, src3, dst0, dst1, dst2;
  v16i8 shuffler0 = {0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 16, 17, 18, 20};
  v16i8 shuffler1 = {5,  6,  8,  9,  10, 12, 13, 14,
                     16, 17, 18, 20, 21, 22, 24, 25};
  v16i8 shuffler2 = {10, 12, 13, 14, 16, 17, 18, 20,
                     21, 22, 24, 25, 26, 28, 29, 30};

  for (x = 0; x < width; x += 16) {
    src0 = (v16u8)__msa_ld_b((void*)src_argb, 0);
    src1 = (v16u8)__msa_ld_b((void*)src_argb, 16);
    src2 = (v16u8)__msa_ld_b((void*)src_argb, 32);
    src3 = (v16u8)__msa_ld_b((void*)src_argb, 48);
    dst0 = (v16u8)__msa_vshf_b(shuffler0, (v16i8)src1, (v16i8)src0);
    dst1 = (v16u8)__msa_vshf_b(shuffler1, (v16i8)src2, (v16i8)src1);
    dst2 = (v16u8)__msa_vshf_b(shuffler2, (v16i8)src3, (v16i8)src2);
    ST_UB2(dst0, dst1, dst_rgb, 16);
    ST_UB(dst2, (dst_rgb + 32));
    src_argb += 64;
    dst_rgb += 48;
  }
}

#ifdef HAS_SCALEARGBFILTERCOLS_MSA
CANY(ScaleARGBFilterCols_Any_MSA,
     ScaleARGBFilterCols_MSA,
     ScaleARGBFilterCols_C,
     4,
     7)
#endif

#ifdef HAS_SCALEARGBCOLS_MSA
CANY(ScaleARGBCols_Any_MSA, ScaleARGBCols_MSA, ScaleARGBCols_C, 4, 3)
#endif

#ifdef HAS_RGB24TOARGBROW_MSA
ANY11(RGB24ToARGBRow_Any_MSA, RGB24ToARGBRow_MSA, 0, 3, 4, 15)
#endif

#ifdef HAS_INTERPOLATEROW_MSA
ANY11I(InterpolateRow_Any_MSA, InterpolateRow_MSA, uint8_t, uint8_t, 1, 1, 31)
#endif

#ifdef HAS_ARGBTORGB24ROW_MSA
ANY11(ARGBToRGB24Row_Any_MSA, ARGBToRGB24Row_MSA, 0, 4, 3, 15)
#endif

#endif // defined(__mips_msa)

#if !defined(LIBYUV_DISABLE_NEON) && defined(__ARM_NEON__) && \
    !defined(__aarch64__)
void RGB24ToARGBRow_NEON(const uint8_t* src_rgb24,
                         uint8_t* dst_argb,
                         int width) {
  asm volatile(
      "vmov.u8     d4, #255                      \n"  // Alpha
      "1:                                        \n"
      "vld3.8      {d1, d2, d3}, [%0]!           \n"  // load 8 pixels of RGB24.
      "subs        %2, %2, #8                    \n"  // 8 processed per loop.
      "vst4.8      {d1, d2, d3, d4}, [%1]!       \n"  // store 8 pixels of ARGB.
      "bgt         1b                            \n"
      : "+r"(src_rgb24),  // %0
        "+r"(dst_argb),   // %1
        "+r"(width)       // %2
      :
      : "cc", "memory", "d1", "d2", "d3", "d4"  // Clobber List
  );
}

// Bilinear filter 16x2 -> 16x1
void InterpolateRow_NEON(uint8_t* dst_ptr,
                         const uint8_t* src_ptr,
                         ptrdiff_t src_stride,
                         int dst_width,
                         int source_y_fraction) {
  int y1_fraction = source_y_fraction;
  asm volatile(
      "cmp         %4, #0                        \n"
      "beq         100f                          \n"
      "add         %2, %1                        \n"
      "cmp         %4, #128                      \n"
      "beq         50f                           \n"

      "vdup.8      d5, %4                        \n"
      "rsb         %4, #256                      \n"
      "vdup.8      d4, %4                        \n"
      // General purpose row blend.
      "1:                                        \n"
      "vld1.8      {q0}, [%1]!                   \n"
      "vld1.8      {q1}, [%2]!                   \n"
      "subs        %3, %3, #16                   \n"
      "vmull.u8    q13, d0, d4                   \n"
      "vmull.u8    q14, d1, d4                   \n"
      "vmlal.u8    q13, d2, d5                   \n"
      "vmlal.u8    q14, d3, d5                   \n"
      "vrshrn.u16  d0, q13, #8                   \n"
      "vrshrn.u16  d1, q14, #8                   \n"
      "vst1.8      {q0}, [%0]!                   \n"
      "bgt         1b                            \n"
      "b           99f                           \n"

      // Blend 50 / 50.
      "50:                                       \n"
      "vld1.8      {q0}, [%1]!                   \n"
      "vld1.8      {q1}, [%2]!                   \n"
      "subs        %3, %3, #16                   \n"
      "vrhadd.u8   q0, q1                        \n"
      "vst1.8      {q0}, [%0]!                   \n"
      "bgt         50b                           \n"
      "b           99f                           \n"

      // Blend 100 / 0 - Copy row unchanged.
      "100:                                      \n"
      "vld1.8      {q0}, [%1]!                   \n"
      "subs        %3, %3, #16                   \n"
      "vst1.8      {q0}, [%0]!                   \n"
      "bgt         100b                          \n"

      "99:                                       \n"
      : "+r"(dst_ptr),     // %0
        "+r"(src_ptr),     // %1
        "+r"(src_stride),  // %2
        "+r"(dst_width),   // %3
        "+r"(y1_fraction)  // %4
      :
      : "cc", "memory", "q0", "q1", "d4", "d5", "q13", "q14");
}


// TODO(Yang Zhang): Investigate less load instructions for
// the x/dx stepping
#define LOAD2_DATA32_LANE(dn1, dn2, n)                       \
  "lsr        %5, %3, #16                                \n" \
  "add        %6, %1, %5, lsl #2                         \n" \
  "add        %3, %3, %4                                 \n" \
  "vld2.32    {" #dn1 "[" #n "], " #dn2 "[" #n "]}, [%6] \n"

void ScaleARGBFilterCols_NEON(uint8_t* dst_argb,
                              const uint8_t* src_argb,
                              int dst_width,
                              int x,
                              int dx) {
  int dx_offset[4] = {0, 1, 2, 3};
  int* tmp = dx_offset;
  const uint8_t* src_tmp = src_argb;
  asm volatile (
      "vdup.32     q0, %3                        \n"  // x
      "vdup.32     q1, %4                        \n"  // dx
      "vld1.32     {q2}, [%5]                    \n"  // 0 1 2 3
      "vshl.i32    q9, q1, #2                    \n"  // 4 * dx
      "vmul.s32    q1, q1, q2                    \n"
      "vmov.i8     q3, #0x7f                     \n"  // 0x7F
      "vmov.i16    q15, #0x7f                    \n"  // 0x7F
    // x         , x + 1 * dx, x + 2 * dx, x + 3 * dx
      "vadd.s32    q8, q1, q0                    \n"
      "1:                                        \n"
    // d0, d1: a
    // d2, d3: b
    LOAD2_DATA32_LANE(d0, d2, 0)
    LOAD2_DATA32_LANE(d0, d2, 1)
    LOAD2_DATA32_LANE(d1, d3, 0)
    LOAD2_DATA32_LANE(d1, d3, 1)
    "vshrn.i32   d22, q8, #9                   \n"
    "vand.16     d22, d22, d30                 \n"
    "vdup.8      d24, d22[0]                   \n"
    "vdup.8      d25, d22[2]                   \n"
    "vdup.8      d26, d22[4]                   \n"
    "vdup.8      d27, d22[6]                   \n"
    "vext.8      d4, d24, d25, #4              \n"
    "vext.8      d5, d26, d27, #4              \n"  // f
    "veor.8      q10, q2, q3                   \n"  // 0x7f ^ f
    "vmull.u8    q11, d0, d20                  \n"
    "vmull.u8    q12, d1, d21                  \n"
    "vmull.u8    q13, d2, d4                   \n"
    "vmull.u8    q14, d3, d5                   \n"
    "vadd.i16    q11, q11, q13                 \n"
    "vadd.i16    q12, q12, q14                 \n"
    "vshrn.i16   d0, q11, #7                   \n"
    "vshrn.i16   d1, q12, #7                   \n"

    "vst1.32     {d0, d1}, [%0]!               \n"  // store pixels
    "vadd.s32    q8, q8, q9                    \n"
    "subs        %2, %2, #4                    \n"  // 4 processed per loop
    "bgt         1b                            \n"
  : "+r"(dst_argb),         // %0
    "+r"(src_argb),         // %1
    "+r"(dst_width),        // %2
    "+r"(x),                // %3
    "+r"(dx),               // %4
    "+r"(tmp),              // %5
    "+r"(src_tmp)           // %6
  :
  : "memory", "cc", "q0", "q1", "q2", "q3", "q8", "q9",
    "q10", "q11", "q12", "q13", "q14", "q15"
  );
}

#undef LOAD2_DATA32_LANE


// TODO(Yang Zhang): Investigate less load instructions for
// the x/dx stepping
#define LOAD1_DATA32_LANE(dn, n)                 \
  "lsr        %5, %3, #16                    \n" \
  "add        %6, %1, %5, lsl #2             \n" \
  "add        %3, %3, %4                     \n" \
  "vld1.32    {" #dn "[" #n "]}, [%6]        \n"

void ScaleARGBCols_NEON(uint8_t* dst_argb,
                        const uint8_t* src_argb,
                        int dst_width,
                        int x,
                        int dx) {
  int tmp;
  const uint8_t* src_tmp = src_argb;
  asm volatile(
      "1:                                        \n"
      // clang-format off
      LOAD1_DATA32_LANE(d0, 0)
      LOAD1_DATA32_LANE(d0, 1)
      LOAD1_DATA32_LANE(d1, 0)
      LOAD1_DATA32_LANE(d1, 1)
      LOAD1_DATA32_LANE(d2, 0)
      LOAD1_DATA32_LANE(d2, 1)
      LOAD1_DATA32_LANE(d3, 0)
      LOAD1_DATA32_LANE(d3, 1)
      // clang-format on
      "vst1.32     {q0, q1}, [%0]!               \n"  // store pixels
      "subs        %2, %2, #8                    \n"  // 8 processed per loop
      "bgt         1b                            \n"
      : "+r"(dst_argb),   // %0
        "+r"(src_argb),   // %1
        "+r"(dst_width),  // %2
        "+r"(x),          // %3
        "+r"(dx),         // %4
        "=&r"(tmp),       // %5
        "+r"(src_tmp)     // %6
      :
      : "memory", "cc", "q0", "q1");
}

#undef LOAD1_DATA32_LANE

void ARGBToRGB24Row_NEON(const uint8_t* src_argb,
                         uint8_t* dst_rgb24,
                         int width) {
  asm volatile(
      "1:                                        \n"
      "vld4.8      {d0, d2, d4, d6}, [%0]!       \n"  // load 16 pixels of ARGB.
      "vld4.8      {d1, d3, d5, d7}, [%0]!       \n"
      "subs        %2, %2, #16                   \n"  // 16 processed per loop.
      "vst3.8      {d0, d2, d4}, [%1]!           \n"  // store 16 RGB24 pixels.
      "vst3.8      {d1, d3, d5}, [%1]!           \n"
      "bgt         1b                            \n"
      : "+r"(src_argb),   // %0
        "+r"(dst_rgb24),  // %1
        "+r"(width)       // %2
      :
      : "cc", "memory", "q0", "q1", "q2", "q3"  // Clobber List
  );
}

#endif

#if defined(__aarch64__)

void RGB24ToARGBRow_NEON(const uint8_t* src_rgb24,
                         uint8_t* dst_argb,
                         int width) {
  asm volatile(
      "movi        v4.8b, #255                   \n"  // Alpha
      "1:                                        \n"
      "ld3         {v1.8b,v2.8b,v3.8b}, [%0], #24 \n"  // load 8 pixels of
                                                       // RGB24.
      "prfm        pldl1keep, [%0, 448]          \n"
      "subs        %w2, %w2, #8                  \n"  // 8 processed per loop.
      "st4         {v1.8b,v2.8b,v3.8b,v4.8b}, [%1], #32 \n"  // store 8 ARGB
      "b.gt        1b                            \n"
      : "+r"(src_rgb24),  // %0
        "+r"(dst_argb),   // %1
        "+r"(width)       // %2
      :
      : "cc", "memory", "v1", "v2", "v3", "v4"  // Clobber List
  );
}

// Bilinear filter 16x2 -> 16x1
void InterpolateRow_NEON(uint8_t* dst_ptr,
                         const uint8_t* src_ptr,
                         ptrdiff_t src_stride,
                         int dst_width,
                         int source_y_fraction) {
  int y1_fraction = source_y_fraction;
  int y0_fraction = 256 - y1_fraction;
  const uint8_t* src_ptr1 = src_ptr + src_stride;
  asm volatile(
      "cmp         %w4, #0                       \n"
      "b.eq        100f                          \n"
      "cmp         %w4, #128                     \n"
      "b.eq        50f                           \n"

      "dup         v5.16b, %w4                   \n"
      "dup         v4.16b, %w5                   \n"
      // General purpose row blend.
      "1:                                        \n"
      "ld1         {v0.16b}, [%1], #16           \n"
      "ld1         {v1.16b}, [%2], #16           \n"
      "subs        %w3, %w3, #16                 \n"
      "umull       v2.8h, v0.8b,  v4.8b          \n"
      "prfm        pldl1keep, [%1, 448]          \n"
      "umull2      v3.8h, v0.16b, v4.16b         \n"
      "prfm        pldl1keep, [%2, 448]          \n"
      "umlal       v2.8h, v1.8b,  v5.8b          \n"
      "umlal2      v3.8h, v1.16b, v5.16b         \n"
      "rshrn       v0.8b,  v2.8h, #8             \n"
      "rshrn2      v0.16b, v3.8h, #8             \n"
      "st1         {v0.16b}, [%0], #16           \n"
      "b.gt        1b                            \n"
      "b           99f                           \n"

      // Blend 50 / 50.
      "50:                                       \n"
      "ld1         {v0.16b}, [%1], #16           \n"
      "ld1         {v1.16b}, [%2], #16           \n"
      "subs        %w3, %w3, #16                 \n"
      "prfm        pldl1keep, [%1, 448]          \n"
      "urhadd      v0.16b, v0.16b, v1.16b        \n"
      "prfm        pldl1keep, [%2, 448]          \n"
      "st1         {v0.16b}, [%0], #16           \n"
      "b.gt        50b                           \n"
      "b           99f                           \n"

      // Blend 100 / 0 - Copy row unchanged.
      "100:                                      \n"
      "ld1         {v0.16b}, [%1], #16           \n"
      "subs        %w3, %w3, #16                 \n"
      "prfm        pldl1keep, [%1, 448]          \n"
      "st1         {v0.16b}, [%0], #16           \n"
      "b.gt        100b                          \n"

      "99:                                       \n"
      : "+r"(dst_ptr),      // %0
        "+r"(src_ptr),      // %1
        "+r"(src_ptr1),     // %2
        "+r"(dst_width),    // %3
        "+r"(y1_fraction),  // %4
        "+r"(y0_fraction)   // %5
      :
      : "cc", "memory", "v0", "v1", "v3", "v4", "v5");
}

// TODO(Yang Zhang): Investigate less load instructions for
// the x/dx stepping
#define LOAD2_DATA32_LANE(vn1, vn2, n)                  \
  "lsr        %5, %3, #16                           \n" \
  "add        %6, %1, %5, lsl #2                    \n" \
  "add        %3, %3, %4                            \n" \
  "ld2        {" #vn1 ".s, " #vn2 ".s}[" #n "], [%6]  \n"

void ScaleARGBFilterCols_NEON(uint8_t* dst_argb,
                              const uint8_t* src_argb,
                              int dst_width,
                              int x,
                              int dx) {
  int dx_offset[4] = {0, 1, 2, 3};
  int* tmp = dx_offset;
  const uint8_t* src_tmp = src_argb;
  int64_t x64 = (int64_t)x;    // NOLINT
  int64_t dx64 = (int64_t)dx;  // NOLINT
  asm volatile (
      "dup         v0.4s, %w3                    \n"  // x
      "dup         v1.4s, %w4                    \n"  // dx
      "ld1         {v2.4s}, [%5]                 \n"  // 0 1 2 3
      "shl         v6.4s, v1.4s, #2              \n"  // 4 * dx
      "mul         v1.4s, v1.4s, v2.4s           \n"
      "movi        v3.16b, #0x7f                 \n"  // 0x7F
      "movi        v4.8h, #0x7f                  \n"  // 0x7F
    // x         , x + 1 * dx, x + 2 * dx, x + 3 * dx
      "add         v5.4s, v1.4s, v0.4s           \n"
      "1:                                        \n"
    // d0, d1: a
    // d2, d3: b
    LOAD2_DATA32_LANE(v0, v1, 0)
    LOAD2_DATA32_LANE(v0, v1, 1)
    LOAD2_DATA32_LANE(v0, v1, 2)
    LOAD2_DATA32_LANE(v0, v1, 3)
    "shrn       v2.4h, v5.4s, #9               \n"
    "and        v2.8b, v2.8b, v4.8b            \n"
    "dup        v16.8b, v2.b[0]                \n"
    "dup        v17.8b, v2.b[2]                \n"
    "dup        v18.8b, v2.b[4]                \n"
    "dup        v19.8b, v2.b[6]                \n"
    "ext        v2.8b, v16.8b, v17.8b, #4      \n"
    "ext        v17.8b, v18.8b, v19.8b, #4     \n"
    "ins        v2.d[1], v17.d[0]              \n"  // f
    "eor        v7.16b, v2.16b, v3.16b         \n"  // 0x7f ^ f
    "umull      v16.8h, v0.8b, v7.8b           \n"
    "umull2     v17.8h, v0.16b, v7.16b         \n"
    "umull      v18.8h, v1.8b, v2.8b           \n"
    "umull2     v19.8h, v1.16b, v2.16b         \n"
    "prfm       pldl1keep, [%1, 448]           \n"  // prefetch 7 lines ahead
    "add        v16.8h, v16.8h, v18.8h         \n"
    "add        v17.8h, v17.8h, v19.8h         \n"
    "shrn       v0.8b, v16.8h, #7              \n"
    "shrn2      v0.16b, v17.8h, #7             \n"
    "st1     {v0.4s}, [%0], #16                \n"  // store pixels
    "add     v5.4s, v5.4s, v6.4s               \n"
    "subs    %w2, %w2, #4                      \n"  // 4 processed per loop
    "b.gt       1b                             \n"
  : "+r"(dst_argb),         // %0
    "+r"(src_argb),         // %1
    "+r"(dst_width),        // %2
    "+r"(x64),              // %3
    "+r"(dx64),             // %4
    "+r"(tmp),              // %5
    "+r"(src_tmp)           // %6
  :
  : "memory", "cc", "v0", "v1", "v2", "v3", "v4", "v5",
    "v6", "v7", "v16", "v17", "v18", "v19"
  );
}

#undef LOAD2_DATA32_LANE

// TODO(Yang Zhang): Investigate less load instructions for
// the x/dx stepping
#define LOAD1_DATA32_LANE(vn, n)                 \
  "lsr        %5, %3, #16                    \n" \
  "add        %6, %1, %5, lsl #2             \n" \
  "add        %3, %3, %4                     \n" \
  "ld1        {" #vn ".s}[" #n "], [%6]      \n"

void ScaleARGBCols_NEON(uint8_t* dst_argb,
                        const uint8_t* src_argb,
                        int dst_width,
                        int x,
                        int dx) {
  const uint8_t* src_tmp = src_argb;
  int64_t x64 = (int64_t)x;    // NOLINT
  int64_t dx64 = (int64_t)dx;  // NOLINT
  int64_t tmp64;
  asm volatile(
      "1:                                        \n"
      // clang-format off
      LOAD1_DATA32_LANE(v0, 0)
      LOAD1_DATA32_LANE(v0, 1)
      LOAD1_DATA32_LANE(v0, 2)
      LOAD1_DATA32_LANE(v0, 3)
      LOAD1_DATA32_LANE(v1, 0)
      LOAD1_DATA32_LANE(v1, 1)
      LOAD1_DATA32_LANE(v1, 2)
      LOAD1_DATA32_LANE(v1, 3)
      "prfm        pldl1keep, [%1, 448]          \n"  // prefetch 7 lines ahead
      // clang-format on
      "st1         {v0.4s, v1.4s}, [%0], #32     \n"  // store pixels
      "subs        %w2, %w2, #8                  \n"  // 8 processed per loop
      "b.gt        1b                            \n"
      : "+r"(dst_argb),   // %0
        "+r"(src_argb),   // %1
        "+r"(dst_width),  // %2
        "+r"(x64),        // %3
        "+r"(dx64),       // %4
        "=&r"(tmp64),     // %5
        "+r"(src_tmp)     // %6
      :
      : "memory", "cc", "v0", "v1");
}

#undef LOAD1_DATA32_LANE

void ARGBToRGB24Row_NEON(const uint8_t* src_argb,
                         uint8_t* dst_rgb24,
                         int width) {
  asm volatile(
      "1:                                        \n"
      "ld4         {v0.16b,v1.16b,v2.16b,v3.16b}, [%0], #64 \n"  // load 16 ARGB
      "subs        %w2, %w2, #16                 \n"  // 16 pixels per loop.
      "prfm        pldl1keep, [%0, 448]          \n"
      "st3         {v0.16b,v1.16b,v2.16b}, [%1], #48 \n"  // store 8 RGB24
      "b.gt        1b                            \n"
      : "+r"(src_argb),   // %0
        "+r"(dst_rgb24),  // %1
        "+r"(width)       // %2
      :
      : "cc", "memory", "v0", "v1", "v2", "v3"  // Clobber List
  );
}
#endif

#ifdef HAS_RGB24TOARGBROW_NEON
ANY11(RGB24ToARGBRow_Any_NEON, RGB24ToARGBRow_NEON, 0, 3, 4, 7)
#endif

#ifdef HAS_INTERPOLATEROW_NEON
ANY11I(InterpolateRow_Any_NEON, InterpolateRow_NEON, uint8_t, uint8_t, 1, 1, 15)
#endif

#ifdef HAS_SCALEARGBFILTERCOLS_NEON
CANY(ScaleARGBFilterCols_Any_NEON,
     ScaleARGBFilterCols_NEON,
     ScaleARGBFilterCols_C,
     4,
     3)
#endif

#ifdef HAS_SCALEARGBCOLS_NEON
CANY(ScaleARGBCols_Any_NEON, ScaleARGBCols_NEON, ScaleARGBCols_C, 4, 7)
#endif

#if defined(HAS_ARGBTORGB24ROW_NEON)
ANY11(ARGBToRGB24Row_Any_NEON, ARGBToRGB24Row_NEON, 0, 4, 3, 15)
#endif
