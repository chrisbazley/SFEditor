/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map coordinates type definition
 *  Copyright (C) 2018 Christopher Bazley
 */

#ifndef MapCoord_h
#define MapCoord_h

#include <stdbool.h>
#include <limits.h>

#include "Debug.h"
#include "Macros.h"
#include "Vertex.h"

struct Reader;
struct Writer;

typedef enum {
  MapAngle_First,
  MapAngle_North = MapAngle_First,
  MapAngle_East,
  MapAngle_South,
  MapAngle_West,
  MapAngle_Count,
} MapAngle;

typedef long int MapCoord;

#define PRIMapCoord "ld"

enum {MAP_COORDS_LIMIT_LOG2 = 30};

#define MAP_COORDS_LIMIT ((MapCoord)1 << MAP_COORDS_LIMIT_LOG2) // 0x4000 0000

MapCoord MapCoord_opp_to_adj(MapCoord opp, MapCoord hyp_squared);

static inline MapCoord MapCoord_abs_diff(
  const MapCoord a, const MapCoord b)
{
  MapCoord const diff = (a > b) ? (a - b) : (b - a);
  assert(diff >= 0);
  return diff;
}

typedef struct MapPoint
{
  MapCoord x, y;
}
MapPoint;

/* Swap the contents of two l-values. */
#define MAP_POINT_SWAP(a, b) do { \
  MapPoint const temp = (a); \
  (a) = (b); \
  (b) = temp; \
} while (0)

static inline MapPoint MapPoint_max(MapPoint const a, MapPoint const b)
{
  return (MapPoint){HIGHEST(a.x, b.x), HIGHEST(a.y, b.y)};
}

static inline MapPoint MapPoint_sub(MapPoint const minuend,
  MapPoint const subtrahend)
{
  MapPoint const result = {
    minuend.x - subtrahend.x,
    minuend.y - subtrahend.y
  };
  DEBUG("%" PRIMapCoord ",%" PRIMapCoord " - %" PRIMapCoord ",%" PRIMapCoord
        " = %" PRIMapCoord ",%" PRIMapCoord,
        minuend.x, minuend.y, subtrahend.x, subtrahend.y, result.x, result.y);
  if (subtrahend.x >= 0) {
    assert(result.x <= minuend.x);
  } else {
    assert(result.x > minuend.x);
  }
  if (subtrahend.y >= 0) {
    assert(result.y <= minuend.y);
  } else {
    assert(result.y > minuend.y);
  }
  return result;
}

static inline MapPoint MapPoint_mul(MapPoint const point,
  MapPoint const factor)
{
  MapPoint const product = {
    point.x * factor.x,
    point.y * factor.y
  };
  DEBUG("%" PRIMapCoord ",%" PRIMapCoord " * %" PRIMapCoord ",%" PRIMapCoord
        " = %" PRIMapCoord ",%" PRIMapCoord,
        point.x, point.y, factor.x, factor.y, product.x, product.y);
  return product;
}

static inline MapPoint MapPoint_div(MapPoint dividend, MapPoint const divisor)
{
  assert(divisor.x != 0);
  assert(divisor.y != 0);

  /* Round towards negative infinity instead of towards zero */
  if (dividend.x < 0) {
    dividend.x -= divisor.x - 1;
  }

  if (dividend.y < 0) {
    dividend.y -= divisor.y - 1;
  }

  MapPoint const quotient = {dividend.x / divisor.x, dividend.y / divisor.y};

  DEBUG("floor(%" PRIMapCoord ",%" PRIMapCoord " / %" PRIMapCoord ",%" PRIMapCoord
        ") = %" PRIMapCoord ",%" PRIMapCoord,
        dividend.x, dividend.y, divisor.x, divisor.y, quotient.x, quotient.y);

  return quotient;
}

static inline MapPoint MapPoint_div_up(MapPoint dividend, MapPoint const divisor)
{
  assert(divisor.x != 0);
  assert(divisor.y != 0);

  /* Round towards infinity instead of towards zero */
  if (dividend.x >= 0) {
    dividend.x += divisor.x - 1;
  }

  if (dividend.y >= 0) {
    dividend.y += divisor.y - 1;
  }

  MapPoint const quotient = {dividend.x / divisor.x, dividend.y / divisor.y};

  DEBUG("ceil(%" PRIMapCoord ",%" PRIMapCoord " / %" PRIMapCoord ",%" PRIMapCoord
        ") = %" PRIMapCoord ",%" PRIMapCoord,
        dividend.x, dividend.y, divisor.x, divisor.y, quotient.x, quotient.y);

  return quotient;
}

static inline MapPoint MapPoint_mul_log2(MapPoint const a,
  int const fac_log2)
{
  MapPoint const result = {SIGNED_L_SHIFT(a.x, fac_log2), SIGNED_L_SHIFT(a.y, fac_log2)};
  DEBUGF("{%" PRIMapCoord ",%" PRIMapCoord "} << %d = {%" PRIMapCoord ",%" PRIMapCoord "}\n",
         a.x, a.y, fac_log2, result.x, result.y);
  return result;
}

static inline MapPoint MapPoint_div_log2(MapPoint const a,
  int const div_log2)
{
  MapPoint const result = {SIGNED_R_SHIFT(a.x, div_log2), SIGNED_R_SHIFT(a.y, div_log2)};
  DEBUGF("{%" PRIMapCoord ",%" PRIMapCoord "} >> %d = {%" PRIMapCoord ",%" PRIMapCoord "}\n",
         a.x, a.y, div_log2, result.x, result.y);
  return result;
}

static inline MapPoint MapPoint_div_up_log2(MapPoint const dividend,
  int const div_log2)
{
  if (div_log2 < 0) {
    return MapPoint_mul_log2(dividend, -div_log2);
  }

  MapPoint const quotient = {
    (dividend.x + ((MapCoord)1 << div_log2) - 1) >> div_log2,
    (dividend.y + ((MapCoord)1 << div_log2) - 1) >> div_log2
  };
  DEBUG("ceil(%" PRIMapCoord ",%" PRIMapCoord " >> %d "
        "= %" PRIMapCoord ",%" PRIMapCoord,
        dividend.x, dividend.y, div_log2, quotient.x, quotient.y);
  return quotient;
}

static inline MapPoint MapPoint_add(MapPoint const a, MapPoint const b)
{
  MapPoint const sum = {
    a.x + b.x,
    a.y + b.y
  };
  DEBUG("%" PRIMapCoord ",%" PRIMapCoord " + %" PRIMapCoord ",%" PRIMapCoord
        " = %" PRIMapCoord ",%" PRIMapCoord,
        a.x, a.y, b.x, b.y, sum.x, sum.y);
  if (b.x >= 0) {
    assert(sum.x >= a.x);
  } else {
    assert(sum.x < a.x);
  }
  if (b.y >= 0) {
    assert(sum.y >= a.y);
  } else {
    assert(sum.y < a.y);
  }
  return sum;
}

static inline bool MapPoint_compare(MapPoint const a, MapPoint const b)
{
  return (a.x == b.x) && (a.y == b.y);
}

static inline MapPoint MapPoint_swap(MapPoint const point)
{
  return (MapPoint){point.y, point.x};
}

static inline MapCoord MapPoint_area(MapPoint const point)
{
  assert(point.x <= MAP_COORDS_LIMIT / (point.y ? point.y : 1));
  return point.x * point.y;
}

MapPoint MapPoint_abs_diff(MapPoint a, MapPoint b);

MapCoord MapPoint_dist(MapPoint a, MapPoint b);

MapCoord MapPoint_pgram_area(MapPoint a, MapPoint b, MapPoint c);

bool MapPoint_clockwise(MapPoint a, MapPoint b, MapPoint c);

typedef struct MapArea
{
  MapPoint min;
  MapPoint max;
}
MapArea;

MapPoint MapArea_size(MapArea const *map_area);
bool MapArea_is_valid(MapArea const *map_area);
void MapArea_make_valid(MapArea const *map_area, MapArea *result);
bool MapArea_contains(MapArea const *map_area, MapPoint point);
bool MapArea_overlaps(MapArea const *a, MapArea const *b);
bool MapArea_contains_area(MapArea const *map_area, MapArea const *b);
void MapArea_intersection(MapArea const *a, MapArea const *b, MapArea *result);
void MapArea_from_points(MapArea *map_area, MapPoint a, MapPoint b);
void MapArea_expand(MapArea *map_area, MapPoint point);
void MapArea_expand_for_area(MapArea *map_area, MapArea const *b);
bool MapArea_compare(MapArea const *a, MapArea const *b);
void MapArea_translate(MapArea const *map_area, MapPoint point, MapArea *result);
void MapArea_reflect_y(MapArea const *map_area, MapArea *result);

void MapArea_mul(MapArea const *map_area,
  MapPoint point, MapArea *result);

void MapArea_div(MapArea const *map_area,
  MapPoint point, MapArea *result);

void MapArea_div_log2(MapArea const *map_area,
  int div_log2, MapArea *result);

static inline MapArea MapArea_make_invalid(void)
{
  return (MapArea){{MAP_COORDS_LIMIT, MAP_COORDS_LIMIT}, {-MAP_COORDS_LIMIT, -MAP_COORDS_LIMIT}};
}

static inline MapArea MapArea_make_max(void)
{
  return (MapArea){{0, 0}, {MAP_COORDS_LIMIT, MAP_COORDS_LIMIT}};
}

bool MapPoint_read(MapPoint *point, struct Reader *reader);
void MapPoint_write(MapPoint point, struct Writer *writer);

bool MapArea_read(MapArea *map_area, struct Reader *reader);
void MapArea_write(MapArea const *map_area, struct Writer *writer);

typedef struct
{
  MapArea map_area;
  MapPoint map_pos;
  bool done;
}
MapAreaIter;

MapPoint MapAreaIter_get_first(MapAreaIter *iter, MapArea const *map_area);
MapPoint MapAreaIter_get_next(MapAreaIter *iter);

static inline bool MapAreaIter_done(MapAreaIter const *const iter)
{
  assert(iter);
  assert(MapArea_is_valid(&iter->map_area));
  return iter->done;
}

bool MapArea_split(MapArea const *area, int size_log2,
                   bool (*callback)(MapArea const *, void *), void *cb_arg);

static inline Vertex MapPoint_to_vertex(MapPoint const point)
{
  return (Vertex){point.x, point.y};
}

static inline MapPoint MapPoint_from_vertex(Vertex const vertex)
{
  return (MapPoint){vertex.x, vertex.y};
}

void MapArea_rotate(MapAngle angle, MapArea const *map_area, MapArea *result);
void MapArea_derotate(MapAngle angle, MapArea const *map_area, MapArea *result);

void MapArea_split_diff(MapArea const *a, MapArea const *b,
                         void (*callback)(MapArea const *, void *),
                         void *cb_arg);

#endif
