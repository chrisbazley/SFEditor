/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Vector type for OS coordinates
 *  Copyright (C) 2019 Christopher Bazley
 */

#ifndef Vertex_h
#define Vertex_h

#include <stdbool.h>
#include <limits.h>
#include "wimp.h" // For BBox

#include "Debug.h"
#include "Macros.h"

/* Holds coordinates */
typedef struct
{
  int x, y;
}
Vertex;

static inline Vertex Vertex_max(Vertex const a, Vertex const b)
{
  Vertex const max = {HIGHEST(a.x, b.x), HIGHEST(a.y, b.y)};
  DEBUGF("max({%d,%d}, {%d,%d}) = {%d,%d}\n",
         a.x, a.y, b.x, b.y, max.x, max.y);
  return max;
}

static inline Vertex Vertex_min(Vertex const a, Vertex const b)
{
  Vertex const min = {LOWEST(a.x, b.x), LOWEST(a.y, b.y)};
  DEBUGF("min({%d,%d}, {%d,%d}) = {%d,%d}\n",
         a.x, a.y, b.x, b.y, min.x, min.y);
  return min;
}

static inline Vertex Vertex_add(Vertex const a, Vertex const b)
{
  Vertex const sum = {a.x + b.x, a.y + b.y};
  DEBUGF("{%d,%d} + {%d,%d} = {%d,%d}\n",
         a.x, a.y, b.x, b.y, sum.x, sum.y);
  return sum;
}

static inline Vertex Vertex_sub(Vertex const a, Vertex const b)
{
  Vertex const diff = {a.x - b.x, a.y - b.y};
  DEBUGF("{%d,%d} - {%d,%d} = {%d,%d}\n",
         a.x, a.y, b.x, b.y, diff.x, diff.y);
  return diff;
}

static inline Vertex Vertex_mul(Vertex const a, Vertex const b)
{
  Vertex const product = {a.x * b.x, a.y * b.y};
  DEBUGF("{%d,%d} * {%d,%d} = {%d,%d}\n",
         a.x, a.y, b.x, b.y, product.x, product.y);
  return product;
}

static inline Vertex Vertex_div(Vertex const a, Vertex const b)
{
  assert(b.x != 0);
  assert(b.y != 0);
  Vertex const quotient = {a.x / b.x, a.y / b.y};
  DEBUGF("{%d,%d} / {%d,%d} = {%d,%d}\n",
         a.x, a.y, b.x, b.y, quotient.x, quotient.y);
  return quotient;
}

static inline Vertex Vertex_mul_log2(Vertex const a,
  int const fac_log2)
{
  Vertex const result = {SIGNED_L_SHIFT(a.x, fac_log2), SIGNED_L_SHIFT(a.y, fac_log2)};
  DEBUGF("{%d,%d} << %d = {%d,%d}\n",
         a.x, a.y, fac_log2, result.x, result.y);
  return result;
}

static inline Vertex Vertex_mul_log2_pair(Vertex const a, Vertex const b)
{
  Vertex const result = {SIGNED_L_SHIFT(a.x, b.x), SIGNED_L_SHIFT(a.y, b.y)};
  DEBUGF("{%d,%d} << {%d,%d} = {%d,%d}\n",
         a.x, a.y, b.x, b.y, result.x, result.y);
  return result;
}

static inline Vertex Vertex_div_log2(Vertex const a,
  int const div_log2)
{
  Vertex const result = {SIGNED_R_SHIFT(a.x, div_log2), SIGNED_R_SHIFT(a.y, div_log2)};
  DEBUGF("{%d,%d} >> %d = {%d,%d}\n",
         a.x, a.y, div_log2, result.x, result.y);
  return result;
}

static inline Vertex Vertex_div_log2_pair(Vertex const a, Vertex const b)
{
  Vertex const result = {SIGNED_R_SHIFT(a.x, b.x), SIGNED_R_SHIFT(a.y, b.y)};
  DEBUGF("{%d,%d} >> {%d,%d} = {%d,%d}\n",
         a.x, a.y, b.x, b.y, result.x, result.y);
  return result;
}

static inline bool Vertex_compare(Vertex const a, Vertex const b)
{
  return (a.x == b.x) && (a.y == b.y);
}

static inline void BBox_set_min(BBox *const bounding_box,
  Vertex const point)
{
  assert(bounding_box != NULL);
  bounding_box->xmin = point.x;
  bounding_box->ymin = point.y;
}

static inline Vertex BBox_get_min(BBox const *const bounding_box)
{
  assert(bounding_box != NULL);
  return (Vertex){bounding_box->xmin, bounding_box->ymin};
}

static inline void BBox_set_max(BBox *const bounding_box,
  Vertex const point)
{
  assert(bounding_box != NULL);
  bounding_box->xmax = point.x;
  bounding_box->ymax = point.y;
}

static inline Vertex BBox_get_max(BBox const *const bounding_box)
{
  assert(bounding_box != NULL);
  return (Vertex){bounding_box->xmax, bounding_box->ymax};
}

static inline BBox BBox_make_invalid(void)
{
  return (BBox){INT_MAX, INT_MAX, INT_MIN, INT_MIN};
}

static inline bool BBox_is_valid(BBox const *const bounding_box)
{
  assert(bounding_box != NULL);
  return (bounding_box->xmin <= bounding_box->xmax) &&
         (bounding_box->ymin <= bounding_box->ymax);
}

static inline void BBox_make(BBox *const bounding_box,
  Vertex const min, Vertex const max)
{
  assert(bounding_box != NULL);
  BBox_set_min(bounding_box, min);
  BBox_set_max(bounding_box, max);
  assert(BBox_is_valid(bounding_box));
}

void BBox_expand(BBox *bounding_box, Vertex point);

void BBox_expand_for_area(BBox *bounding_box, BBox const *b);

void BBox_translate(BBox const *bounding_box,
  Vertex point, BBox *result);

bool BBox_contains(BBox const *container, BBox const *object);

void BBox_intersection(BBox const *a, BBox const *b,
  BBox *result);

static inline int BBox_width(BBox const *const bounding_box)
{
  assert(BBox_is_valid(bounding_box));
  return bounding_box->xmax - bounding_box->xmin;
}

static inline int BBox_height(BBox const *const bounding_box)
{
  assert(BBox_is_valid(bounding_box));
  return bounding_box->ymax - bounding_box->ymin;
}

static inline Vertex BBox_size(BBox const *const bounding_box)
{
  assert(BBox_is_valid(bounding_box));
  return Vertex_sub(BBox_get_max(bounding_box),
                    BBox_get_min(bounding_box));
}

#endif
