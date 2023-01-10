
#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <string.h> /* memcpy */

#include "cpu_id.h"
#include "scale.h"

// Supported filtering.
typedef enum FilterMode {
  kFilterNone = 0,      // Point sample; Fastest.
  kFilterLinear = 1,    // Filter horizontally only.
  kFilterBilinear = 2,  // Faster than box, but lower quality scaling down.
  kFilterBox = 3        // Highest quality.
} FilterModeEnum;

static __inline int Abs(int v) {
  return v >= 0 ? v : -v;
}

void RGB24ToARGBRow_C(const uint8_t* src_rgb24, uint8_t* dst_argb, int width) {
  int x;
  for (x = 0; x < width; ++x) {
    uint8_t b = src_rgb24[0];
    uint8_t g = src_rgb24[1];
    uint8_t r = src_rgb24[2];
    dst_argb[0] = b;
    dst_argb[1] = g;
    dst_argb[2] = r;
    dst_argb[3] = 255u;
    dst_argb += 4;
    src_rgb24 += 3;
  }
}

void (*RGB24ToARGBRow)(const uint8_t* src_rgb, uint8_t* dst_argb, int width);

int RGB24ToARGB(const uint8_t* src_rgb24,
                int src_stride_rgb24,
                uint8_t* dst_argb,
                int dst_stride_argb,
                int width,
                int height) {
  int y;
  if (!src_rgb24 || !dst_argb || width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_rgb24 = src_rgb24 + (height - 1) * src_stride_rgb24;
    src_stride_rgb24 = -src_stride_rgb24;
  }
  // Coalesce rows.
  if (src_stride_rgb24 == width * 3 && dst_stride_argb == width * 4) {
    width *= height;
    height = 1;
    src_stride_rgb24 = dst_stride_argb = 0;
  }

  for (y = 0; y < height; ++y) {
    RGB24ToARGBRow(src_rgb24, dst_argb, width);
    src_rgb24 += src_stride_rgb24;
    dst_argb += dst_stride_argb;
  }
  return 0;
}

void ARGBToRGB24Row_C(const uint8_t* src_argb, uint8_t* dst_rgb, int width) {
  int x;
  for (x = 0; x < width; ++x) {
    uint8_t b = src_argb[0];
    uint8_t g = src_argb[1];
    uint8_t r = src_argb[2];
    dst_rgb[0] = b;
    dst_rgb[1] = g;
    dst_rgb[2] = r;
    dst_rgb += 3;
    src_argb += 4;
  }
}

void (*ARGBToRGB24Row)(const uint8_t* src_argb, uint8_t* dst_rgb, int width);

int ARGBToRGB24(const uint8_t* src_argb,
                int src_stride_argb,
                uint8_t* dst_rgb24,
                int dst_stride_rgb24,
                int width,
                int height) {
  int y;
  if (!src_argb || !dst_rgb24 || width <= 0 || height == 0) {
    return -1;
  }
  if (height < 0) {
    height = -height;
    src_argb = src_argb + (height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  // Coalesce rows.
  if (src_stride_argb == width * 4 && dst_stride_rgb24 == width * 3) {
    width *= height;
    height = 1;
    src_stride_argb = dst_stride_rgb24 = 0;
  }

  for (y = 0; y < height; ++y) {
    ARGBToRGB24Row(src_argb, dst_rgb24, width);
    src_argb += src_stride_argb;
    dst_rgb24 += dst_stride_rgb24;
  }
  return 0;
}

// Blend 2 rows into 1.
static inline void HalfRow_C(const uint8_t* src_uv,
                      ptrdiff_t src_uv_stride,
                      uint8_t* dst_uv,
                      int width) {
  int x;
  for (x = 0; x < width; ++x) {
    dst_uv[x] = (src_uv[x] + src_uv[src_uv_stride + x] + 1) >> 1;
  }
}

// C version 2x2 -> 2x1.
void InterpolateRow_C(uint8_t* dst_ptr,
                      const uint8_t* src_ptr,
                      ptrdiff_t src_stride,
                      int width,
                      int source_y_fraction) {
  int y1_fraction = source_y_fraction;
  int y0_fraction = 256 - y1_fraction;
  const uint8_t* src_ptr1 = src_ptr + src_stride;
  int x;
  assert(source_y_fraction >= 0);
  assert(source_y_fraction < 256);

  if (y1_fraction == 0) {
    memcpy(dst_ptr, src_ptr, width);
    return;
  }
  if (y1_fraction == 128) {
    HalfRow_C(src_ptr, src_stride, dst_ptr, width);
    return;
  }
  for (x = 0; x < width; ++x) {
    dst_ptr[0] =
        (src_ptr[0] * y0_fraction + src_ptr1[0] * y1_fraction + 128) >> 8;
    ++src_ptr;
    ++src_ptr1;
    ++dst_ptr;
  }
}

// TODO(fbarchard): Replace 0x7f ^ f with 128-f.  bug=607.
// Mimics SSSE3 blender
#define BLENDER1(a, b, f) ((a) * (0x7f ^ f) + (b)*f) >> 7
#define BLENDERC(a, b, f, s) \
  (uint32_t)(BLENDER1(((a) >> s) & 255, ((b) >> s) & 255, f) << s)
#define BLENDER(a, b, f)                                                 \
  BLENDERC(a, b, f, 24) | BLENDERC(a, b, f, 16) | BLENDERC(a, b, f, 8) | \
      BLENDERC(a, b, f, 0)

void ScaleARGBFilterCols_C(uint8_t* dst_argb,
                           const uint8_t* src_argb,
                           int dst_width,
                           int x,
                           int dx) {
  const uint32_t* src = (const uint32_t*)(src_argb);
  uint32_t* dst = (uint32_t*)(dst_argb);
  int j;
  for (j = 0; j < dst_width - 1; j += 2) {
    int xi = x >> 16;
    int xf = (x >> 9) & 0x7f;
    uint32_t a = src[xi];
    uint32_t b = src[xi + 1];
    dst[0] = BLENDER(a, b, xf);
    x += dx;
    xi = x >> 16;
    xf = (x >> 9) & 0x7f;
    a = src[xi];
    b = src[xi + 1];
    dst[1] = BLENDER(a, b, xf);
    x += dx;
    dst += 2;
  }
  if (dst_width & 1) {
    int xi = x >> 16;
    int xf = (x >> 9) & 0x7f;
    uint32_t a = src[xi];
    uint32_t b = src[xi + 1];
    dst[0] = BLENDER(a, b, xf);
  }
}

// Scales a single row of pixels using point sampling.
void ScaleARGBCols_C(uint8_t* dst_argb,
                     const uint8_t* src_argb,
                     int dst_width,
                     int x,
                     int dx) {
  const uint32_t* src = (const uint32_t*)(src_argb);
  uint32_t* dst = (uint32_t*)(dst_argb);
  int j;
  for (j = 0; j < dst_width - 1; j += 2) {
    dst[0] = src[x >> 16];
    x += dx;
    dst[1] = src[x >> 16];
    x += dx;
    dst += 2;
  }
  if (dst_width & 1) {
    dst[0] = src[x >> 16];
  }
}


// Scales a single row of pixels up by 2x using point sampling.
void ScaleARGBColsUp2_C(uint8_t* dst_argb,
                        const uint8_t* src_argb,
                        int dst_width,
                        int x,
                        int dx) {
  const uint32_t* src = (const uint32_t*)(src_argb);
  uint32_t* dst = (uint32_t*)(dst_argb);
  int j;
  (void)x;
  (void)dx;
  for (j = 0; j < dst_width - 1; j += 2) {
    dst[1] = dst[0] = src[0];
    src += 1;
    dst += 2;
  }
  if (dst_width & 1) {
    dst[0] = src[0];
  }
}

#define align_buffer_64(var, size)                                           \
  uint8_t* var##_mem = (uint8_t*)(malloc((size) + 63));         /* NOLINT */ \
  uint8_t* var = (uint8_t*)(((intptr_t)(var##_mem) + 63) & ~63) /* NOLINT */

#define free_aligned_buffer_64(var) \
  free(var##_mem);                  \
  var = 0

void (*InterpolateRow)(uint8_t * dst_argb, const uint8_t* src_argb,
                         ptrdiff_t src_stride, int dst_width,
                         int source_y_fraction);

void (*ScaleARGBFilterCols)(uint8_t * dst_argb, const uint8_t* src_argb,
                              int dst_width, int x, int dx);

// Scale ARGB up with bilinear interpolation.
static void ScaleARGBBilinearUp(int src_width,
                                int src_height,
                                int dst_width,
                                int dst_height,
                                int src_stride,
                                int dst_stride,
                                const uint8_t* src_argb,
                                uint8_t* dst_argb,
                                int x,
                                int dx,
                                int y,
                                int dy,
                                enum FilterMode filtering) {
  int j;
  const int max_y = (src_height - 1) << 16;

  if (y > max_y) {
    y = max_y;
  }

  {
    int yi = y >> 16;
    const uint8_t* src = src_argb + yi * (int64_t)src_stride;

    // Allocate 2 rows of ARGB.
    const int kRowSize = (dst_width * 4 + 31) & ~31;
    align_buffer_64(row, kRowSize * 2);

    uint8_t* rowptr = row;
    int rowstride = kRowSize;
    int lasty = yi;

    ScaleARGBFilterCols(rowptr, src, dst_width, x, dx);
    if (src_height > 1) {
      src += src_stride;
    }
    ScaleARGBFilterCols(rowptr + rowstride, src, dst_width, x, dx);
    if (src_height > 2) {
      src += src_stride;
    }

    for (j = 0; j < dst_height; ++j) {
      yi = y >> 16;
      if (yi != lasty) {
        if (y > max_y) {
          y = max_y;
          yi = y >> 16;
          src = src_argb + yi * (int64_t)src_stride;
        }
        if (yi != lasty) {
          ScaleARGBFilterCols(rowptr, src, dst_width, x, dx);
          rowptr += rowstride;
          rowstride = -rowstride;
          lasty = yi;
          if ((y + 65536) < max_y) {
            src += src_stride;
          }
        }
      }
      if (filtering == kFilterLinear) {
        InterpolateRow(dst_argb, rowptr, 0, dst_width * 4, 0);
      } else {
        int yf = (y >> 8) & 255;
        InterpolateRow(dst_argb, rowptr, rowstride, dst_width * 4, yf);
      }
      dst_argb += dst_stride;
      y += dy;
    }
    free_aligned_buffer_64(row);
  }
}

// Divide num by div and return as 16.16 fixed point result.
int FixedDiv_C(int num, int div) {
  return (int)(((int64_t)(num) << 16) / div);
}

// Divide num - 1 by div - 1 and return as 16.16 fixed point result.
int FixedDiv1_C(int num, int div) {
  return (int)((((int64_t)(num) << 16) - 0x00010001) / (div - 1));
}

#define CENTERSTART(dx, s) (dx < 0) ? -((-dx >> 1) + s) : ((dx >> 1) + s)

// Compute slope values for stepping.
void ScaleSlope(int src_width,
                int src_height,
                int dst_width,
                int dst_height,
                enum FilterMode filtering,
                int* x,
                int* y,
                int* dx,
                int* dy) {
  assert(x != NULL);
  assert(y != NULL);
  assert(dx != NULL);
  assert(dy != NULL);
  assert(src_width != 0);
  assert(src_height != 0);
  assert(dst_width > 0);
  assert(dst_height > 0);
  // Check for 1 pixel and avoid FixedDiv overflow.
  if (dst_width == 1 && src_width >= 32768) {
    dst_width = src_width;
  }
  if (dst_height == 1 && src_height >= 32768) {
    dst_height = src_height;
  }
  if (filtering == kFilterBox) {
    // Scale step for point sampling duplicates all pixels equally.
    *dx = FixedDiv(Abs(src_width), dst_width);
    *dy = FixedDiv(src_height, dst_height);
    *x = 0;
    *y = 0;
  } else if (filtering == kFilterBilinear) {
    // Scale step for bilinear sampling renders last pixel once for upsample.
    if (dst_width <= Abs(src_width)) {
      *dx = FixedDiv(Abs(src_width), dst_width);
      *x = CENTERSTART(*dx, -32768);  // Subtract 0.5 (32768) to center filter.
    } else if (src_width > 1 && dst_width > 1) {
      *dx = FixedDiv1(Abs(src_width), dst_width);
      *x = 0;
    }
    if (dst_height <= src_height) {
      *dy = FixedDiv(src_height, dst_height);
      *y = CENTERSTART(*dy, -32768);  // Subtract 0.5 (32768) to center filter.
    } else if (src_height > 1 && dst_height > 1) {
      *dy = FixedDiv1(src_height, dst_height);
      *y = 0;
    }
  } else if (filtering == kFilterLinear) {
    // Scale step for bilinear sampling renders last pixel once for upsample.
    if (dst_width <= Abs(src_width)) {
      *dx = FixedDiv(Abs(src_width), dst_width);
      *x = CENTERSTART(*dx, -32768);  // Subtract 0.5 (32768) to center filter.
    } else if (src_width > 1 && dst_width > 1) {
      *dx = FixedDiv1(Abs(src_width), dst_width);
      *x = 0;
    }
    *dy = FixedDiv(src_height, dst_height);
    *y = *dy >> 1;
  } else {
    // Scale step for point sampling duplicates all pixels equally.
    *dx = FixedDiv(Abs(src_width), dst_width);
    *dy = FixedDiv(src_height, dst_height);
    *x = CENTERSTART(*dx, 0);
    *y = CENTERSTART(*dy, 0);
  }
  // Negative src_width means horizontally mirror.
  if (src_width < 0) {
    *x += (dst_width - 1) * *dx;
    *dx = -*dx;
    // src_width = -src_width;   // Caller must do this.
  }
}
#undef CENTERSTART

static void ScaleARGB(const uint8_t* src,
                      int src_stride,
                      int src_width,
                      int src_height,
                      uint8_t* dst,
                      int dst_stride,
                      int dst_width,
                      int dst_height,
                      enum FilterMode filtering) {
  // Initial source x/y coordinate and step values as 16.16 fixed point.
  int x = 0;
  int y = 0;
  int dx = 0;   /* 水平方向缩放系数，<= 0xffff表示放大 */
  int dy = 0;   /* 垂直方向缩放系数，<= 0xffff表示放大 */

  ScaleSlope(src_width, src_height, dst_width, dst_height, filtering, &x, &y,
             &dx, &dy);
  src_width = Abs(src_width);

  if (filtering && dy < 65536) {
    ScaleARGBBilinearUp(src_width, src_height, dst_width, dst_height,
                        src_stride, dst_stride, src, dst, x, dx, y, dy,
                        filtering);
    return;
  }
#if 0
  if (filtering) {
    ScaleARGBBilinearDown(src_width, src_height, dst_width, dst_height,
                          src_stride, dst_stride, src, dst, x, dx, y, dy,
                          filtering);
    return;
  }
  ScaleARGBSimple(src_width, src_height, dst_width, dst_height, src_stride,
                  dst_stride, src, dst, x, dx, y, dy);
#endif
}

int ARGBScale(const uint8_t* src_argb,
              int src_stride_argb,
              int src_width,
              int src_height,
              uint8_t* dst_argb,
              int dst_stride_argb,
              int dst_width,
              int dst_height,
              enum FilterMode filtering) {
  if (!src_argb || src_width == 0 || src_height == 0 || src_width > 32768 ||
      src_height > 32768 || !dst_argb || dst_width <= 0 || dst_height <= 0) {
    return -1;
  }
  ScaleARGB(src_argb, src_stride_argb, src_width, src_height, dst_argb,
            dst_stride_argb, dst_width, dst_height, filtering);
  return 0;
}

int RGBScale(const uint8_t* src_rgb,
             int src_stride_rgb,
             int src_width,
             int src_height,
             uint8_t* dst_rgb,
             int dst_stride_rgb,
             int dst_width,
             int dst_height,
             enum FilterMode filtering) {
    int r;
    uint8_t* src_argb =
        (uint8_t*)malloc(src_width * src_height * 4 + dst_width * dst_height * 4);
    uint8_t* dst_argb = src_argb + src_width * src_height * 4;

    if (!src_argb) {
        return 1;
    }

    r = RGB24ToARGB(src_rgb, src_stride_rgb, src_argb, src_width * 4, src_width,
                    src_height);
    if (!r) {
        r = ARGBScale(src_argb, src_width * 4, src_width, src_height, dst_argb,
                    dst_width * 4, dst_width, dst_height, filtering);
        if (!r) {
            r = ARGBToRGB24(dst_argb, dst_width * 4, dst_rgb, dst_stride_rgb,
                            dst_width, dst_height);
        }
    }
    free(src_argb);
    return r;
}

void RGB24ToARGBInit(int width)
{
    RGB24ToARGBRow = RGB24ToARGBRow_C;
    if (width <= 0) {
        return;
    }

#if defined(HAS_RGB24TOARGBROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    RGB24ToARGBRow = RGB24ToARGBRow_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      RGB24ToARGBRow = RGB24ToARGBRow_SSSE3;
    }
  }
#endif
#if defined(HAS_RGB24TOARGBROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    RGB24ToARGBRow = RGB24ToARGBRow_Any_NEON;
    if (IS_ALIGNED(width, 8)) {
      RGB24ToARGBRow = RGB24ToARGBRow_NEON;
    }
  }
#endif
#if defined(HAS_RGB24TOARGBROW_MSA)
  if (TestCpuFlag(kCpuHasMSA)) {
    RGB24ToARGBRow = RGB24ToARGBRow_Any_MSA;
    if (IS_ALIGNED(width, 16)) {
      RGB24ToARGBRow = RGB24ToARGBRow_MSA;
    }
  }
#endif
#if defined(HAS_RGB24TOARGBROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    RGB24ToARGBRow = RGB24ToARGBRow_Any_LSX;
    if (IS_ALIGNED(width, 16)) {
      RGB24ToARGBRow = RGB24ToARGBRow_LSX;
    }
  }
#endif
#if defined(HAS_RGB24TOARGBROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    RGB24ToARGBRow = RGB24ToARGBRow_Any_LASX;
    if (IS_ALIGNED(width, 32)) {
      RGB24ToARGBRow = RGB24ToARGBRow_LASX;
    }
  }
#endif
}

void ARGBScaleInit( int src_width,
                    int dst_width,
                    enum FilterMode filtering )
{
    InterpolateRow = InterpolateRow_C;
    ScaleARGBFilterCols = filtering ? ScaleARGBFilterCols_C : ScaleARGBCols_C;
#if defined(HAS_INTERPOLATEROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    InterpolateRow = InterpolateRow_Any_SSSE3;
    if (IS_ALIGNED(dst_width, 4)) {
      InterpolateRow = InterpolateRow_SSSE3;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    InterpolateRow = InterpolateRow_Any_AVX2;
    if (IS_ALIGNED(dst_width, 8)) {
      InterpolateRow = InterpolateRow_AVX2;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    InterpolateRow = InterpolateRow_Any_NEON;
    if (IS_ALIGNED(dst_width, 4)) {
      InterpolateRow = InterpolateRow_NEON;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_MSA)
  if (TestCpuFlag(kCpuHasMSA)) {
    InterpolateRow = InterpolateRow_Any_MSA;
    if (IS_ALIGNED(dst_width, 8)) {
      InterpolateRow = InterpolateRow_MSA;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_LSX)
  if (TestCpuFlag(kCpuHasLSX)) {
    InterpolateRow = InterpolateRow_Any_LSX;
    if (IS_ALIGNED(dst_width, 8)) {
      InterpolateRow = InterpolateRow_LSX;
    }
  }
#endif
#if 0
    /* 暂不支持超大分辨率 */
  if (src_width >= 32768) {
    ScaleARGBFilterCols =
        filtering ? ScaleARGBFilterCols64_C : ScaleARGBCols64_C;
  }
#endif
#if defined(HAS_SCALEARGBFILTERCOLS_SSSE3)
  if (filtering && TestCpuFlag(kCpuHasSSSE3) && src_width < 32768) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_SSSE3;
  }
#endif
#if defined(HAS_SCALEARGBFILTERCOLS_NEON)
  if (filtering && TestCpuFlag(kCpuHasNEON)) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_Any_NEON;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBFilterCols = ScaleARGBFilterCols_NEON;
    }
  }
#endif
#if defined(HAS_SCALEARGBFILTERCOLS_MSA)
  if (filtering && TestCpuFlag(kCpuHasMSA)) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_Any_MSA;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleARGBFilterCols = ScaleARGBFilterCols_MSA;
    }
  }
#endif
#if defined(HAS_SCALEARGBFILTERCOLS_LSX)
  if (filtering && TestCpuFlag(kCpuHasLSX)) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_Any_LSX;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleARGBFilterCols = ScaleARGBFilterCols_LSX;
    }
  }
#endif
#if defined(HAS_SCALEARGBCOLS_SSE2)
  if (!filtering && TestCpuFlag(kCpuHasSSE2) && src_width < 32768) {
    ScaleARGBFilterCols = ScaleARGBCols_SSE2;
  }
#endif
#if defined(HAS_SCALEARGBCOLS_NEON)
  if (!filtering && TestCpuFlag(kCpuHasNEON)) {
    ScaleARGBFilterCols = ScaleARGBCols_Any_NEON;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleARGBFilterCols = ScaleARGBCols_NEON;
    }
  }
#endif
#if defined(HAS_SCALEARGBCOLS_MSA)
  if (!filtering && TestCpuFlag(kCpuHasMSA)) {
    ScaleARGBFilterCols = ScaleARGBCols_Any_MSA;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBFilterCols = ScaleARGBCols_MSA;
    }
  }
#endif
#if defined(HAS_SCALEARGBCOLS_LSX)
  if (!filtering && TestCpuFlag(kCpuHasLSX)) {
    ScaleARGBFilterCols = ScaleARGBCols_Any_LSX;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBFilterCols = ScaleARGBCols_LSX;
    }
  }
#endif
  if (!filtering && src_width * 2 == dst_width) {
    ScaleARGBFilterCols = ScaleARGBColsUp2_C;
#if defined(HAS_SCALEARGBCOLSUP2_SSE2)
    if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(dst_width, 8)) {
      ScaleARGBFilterCols = ScaleARGBColsUp2_SSE2;
    }
#endif
  }
}

void ARGBToRGB24Init(int width)
{
    ARGBToRGB24Row = ARGBToRGB24Row_C;

#if defined(HAS_ARGBTORGB24ROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ARGBToRGB24Row = ARGBToRGB24Row_Any_SSSE3;
    if (IS_ALIGNED(width, 16)) {
      ARGBToRGB24Row = ARGBToRGB24Row_SSSE3;
    }
  }
#endif
#if defined(HAS_ARGBTORGB24ROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ARGBToRGB24Row = ARGBToRGB24Row_Any_AVX2;
    if (IS_ALIGNED(width, 32)) {
      ARGBToRGB24Row = ARGBToRGB24Row_AVX2;
    }
  }
#endif
#if defined(HAS_ARGBTORGB24ROW_AVX512VBMI)
  if (TestCpuFlag(kCpuHasAVX512VBMI)) {
    ARGBToRGB24Row = ARGBToRGB24Row_Any_AVX512VBMI;
    if (IS_ALIGNED(width, 32)) {
      ARGBToRGB24Row = ARGBToRGB24Row_AVX512VBMI;
    }
  }
#endif
#if defined(HAS_ARGBTORGB24ROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ARGBToRGB24Row = ARGBToRGB24Row_Any_NEON;
    if (IS_ALIGNED(width, 16)) {
      ARGBToRGB24Row = ARGBToRGB24Row_NEON;
    }
  }
#endif
#if defined(HAS_ARGBTORGB24ROW_MSA)
  if (TestCpuFlag(kCpuHasMSA)) {
    ARGBToRGB24Row = ARGBToRGB24Row_Any_MSA;
    if (IS_ALIGNED(width, 16)) {
      ARGBToRGB24Row = ARGBToRGB24Row_MSA;
    }
  }
#endif
#if defined(HAS_ARGBTORGB24ROW_LASX)
  if (TestCpuFlag(kCpuHasLASX)) {
    ARGBToRGB24Row = ARGBToRGB24Row_Any_LASX;
    if (IS_ALIGNED(width, 32)) {
      ARGBToRGB24Row = ARGBToRGB24Row_LASX;
    }
  }
#endif
}

void ScaleInit( int src_width,
                int dst_width,
                enum FilterMode filtering )
{
    RGB24ToARGBInit(src_width);
    ARGBScaleInit(src_width, dst_width, filtering);
    ARGBToRGB24Init(dst_width);
}

#define SRC_IMG_WIDTH   2560
#define SRC_IMG_HEIGHT  1440

#include <sys/time.h>   /* gettimeofday */

int main()
{
    FILE *file_img = fopen("test_rgb_400.bmp", "r+t");
    int memsize = SRC_IMG_WIDTH*SRC_IMG_HEIGHT*3;
    unsigned char *img_read = (unsigned char *)malloc(memsize);
    fread(img_read, 1, memsize, file_img);
    memsize<<=2;
    unsigned char *dst_rgb = (unsigned char *)malloc(memsize);

    struct  timeval    tv;
    gettimeofday(&tv, NULL);
    std::cout<<tv.tv_sec*1000 + tv.tv_usec/1000 << std::endl;

    ScaleInit(SRC_IMG_WIDTH, SRC_IMG_WIDTH *2, kFilterLinear);

    RGBScale(img_read, SRC_IMG_WIDTH *3, SRC_IMG_WIDTH, SRC_IMG_HEIGHT, dst_rgb, SRC_IMG_WIDTH *3 *2, SRC_IMG_WIDTH *2, SRC_IMG_HEIGHT*2, kFilterLinear);

    FILE *file_out = fopen("dst_rgb.bmp", "w+t");
    fwrite(dst_rgb, memsize, 1, file_out);

    gettimeofday(&tv, NULL);
    std::cout<<tv.tv_sec*1000 + tv.tv_usec/1000 << std::endl;

    fflush(file_out);
    fclose(file_img);
    fclose(file_out);

    return 0;
}
