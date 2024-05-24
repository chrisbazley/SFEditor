/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map coordinates type definition
 *  Copyright (C) 2018 Christopher Bazley
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public Licence as published by
 *  the Free Software Foundation; either version 2 of the Licence, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public Licence for more details.
 *
 *  You should have received a copy of the GNU General Public Licence
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <math.h>

#include "Macros.h"
#include "Debug.h"
#include "Err.h"
#include "Msgtrans.h"
#include "Reader.h"
#include "Writer.h"

#include "Utils.h"
#include "Hourglass.h"
#include "MapCoord.h"
#include "Reader.h"
#include "Writer.h"

bool MapArea_is_valid(MapArea const *const map_area)
{
  assert(map_area != NULL);
  return (map_area->min.x <= map_area->max.x) &&
         (map_area->min.y <= map_area->max.y);
}

void MapArea_make_valid(MapArea const *const map_area, MapArea *const result)
{
  assert(map_area != NULL);
  assert(result != NULL);

  *result = (MapArea){
    .min = {
      .x = LOWEST(map_area->min.x, map_area->max.x),
      .y = LOWEST(map_area->min.y, map_area->max.y)
    },
    .max = {
      .x = HIGHEST(map_area->min.x, map_area->max.x),
      .y = HIGHEST(map_area->min.y, map_area->max.y),
    },
  };
  assert(MapArea_is_valid(result));
}

MapPoint MapArea_size(MapArea const *const map_area)
{
  assert(MapArea_is_valid(map_area));
  return MapPoint_add(MapPoint_sub(map_area->max, map_area->min), (MapPoint){1,1});
}

bool MapArea_contains(MapArea const *const map_area, MapPoint const point)
{
  bool contains = true;
  assert(MapArea_is_valid(map_area));

  if (point.y < map_area->min.y) {
    DEBUGF("%" PRIMapCoord ",%" PRIMapCoord " is outside bottom\n", point.x, point.y);
    contains = false;
  } else if (point.y > map_area->max.y) {
    DEBUGF("%" PRIMapCoord ",%" PRIMapCoord " is outside top\n", point.x, point.y);
    contains = false;
  } else if (point.x < map_area->min.x) {
    DEBUGF("%" PRIMapCoord ",%" PRIMapCoord " is outside left\n", point.x, point.y);
    contains = false;
  } else if (point.x > map_area->max.x) {
    DEBUGF("%" PRIMapCoord ",%" PRIMapCoord " is outside right\n", point.x, point.y);
    contains = false;
  }

  DEBUG_VERBOSEF("Map area %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord " %s "
         "%" PRIMapCoord ",%" PRIMapCoord "\n",
         map_area->min.x, map_area->min.y, map_area->max.x, map_area->max.y,
         contains ? "contains" : "doesn't contain",
         point.x, point.y);

  return contains;
}

bool MapArea_overlaps(MapArea const *a, MapArea const *b)
{
  bool overlap = true;

  assert(MapArea_is_valid(a));
  assert(MapArea_is_valid(b));

  if (a->max.y < b->min.y) {
    DEBUG("%" PRIMapCoord " is outside bottom", a->max.y);
    overlap = false;
  } else if (a->min.y > b->max.y) {
    DEBUG("%" PRIMapCoord " is outside top", a->min.y);
    overlap = false;
  } else if (a->max.x < b->min.x) {
    DEBUG("%" PRIMapCoord " is outside left", a->max.x);
    overlap = false;
  } else if (a->min.x > b->max.x) {
    DEBUG("%" PRIMapCoord " is outside right", a->min.x);
    overlap = false;
  }

  DEBUGF("Map area %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord " %s "
         "%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord "\n",
         a->min.x, a->min.y, a->max.x, a->max.y,
         overlap ? "overlaps" : "doesn't overlap",
         b->min.x, b->min.y, b->max.x, b->max.y);

  return overlap;
}

bool MapArea_contains_area(MapArea const *map_area, MapArea const *b)
{
  assert(MapArea_is_valid(b));
  return MapArea_contains(map_area, b->min) && MapArea_contains(map_area, b->max);
}

void MapArea_from_points(MapArea *const map_area, MapPoint const a,
  MapPoint const b)
{
  assert(map_area != NULL);
  *map_area = (MapArea){
    .min = {
      .x = LOWEST(b.x, a.x),
      .y = LOWEST(b.y, a.y),
    },
    .max = {
      .x = HIGHEST(b.x, a.x),
      .y = HIGHEST(b.y, a.y),
    },
  };
  assert(MapArea_is_valid(map_area));
}

void MapArea_intersection(MapArea const *const a, MapArea const *const b,
  MapArea *const result)
{
  assert(MapArea_is_valid(a));
  assert(MapArea_is_valid(b));
  assert(result);

  DEBUG("Find intersection of bounding box %"PRIMapCoord",%"PRIMapCoord",%"PRIMapCoord",%"PRIMapCoord
        " and %"PRIMapCoord",%"PRIMapCoord",%"PRIMapCoord",%"PRIMapCoord"",
        a->min.x, a->min.y, a->max.x, a->max.y,
        b->min.x, b->min.y, b->max.x, b->max.y);

  *result = (MapArea){.min = {.x = HIGHEST(a->min.x, b->min.x),
                              .y = HIGHEST(a->min.y, b->min.y)},
                      .max = {.x = LOWEST(a->max.x, b->max.x),
                              .y = LOWEST(a->max.y, b->max.y)}};

  DEBUG("Intersection is %"PRIMapCoord",%"PRIMapCoord",%"PRIMapCoord",%"PRIMapCoord" (%s)",
        result->min.x, result->min.y, result->max.x, result->max.y,
        MapArea_is_valid(result) ? "valid" : "invalid");
}

void MapArea_expand(MapArea *const map_area, MapPoint const point)
{
  assert(map_area != NULL);
  DEBUG("Will expand map area %" PRIMapCoord ",%" PRIMapCoord
        ",%" PRIMapCoord ",%" PRIMapCoord
        " to include point %" PRIMapCoord ",%" PRIMapCoord,
        map_area->min.x, map_area->min.y, map_area->max.x, map_area->max.y,
        point.x, point.y);

  if (point.x < map_area->min.x) {
    map_area->min.x = point.x;
  }
  if (point.y < map_area->min.y) {
    map_area->min.y = point.y;
  }
  if (point.x > map_area->max.x) {
    map_area->max.x = point.x;
  }
  if (point.y > map_area->max.y) {
    map_area->max.y = point.y;
  }

  DEBUG("Map area is now %" PRIMapCoord ",%" PRIMapCoord
        ",%" PRIMapCoord ",%" PRIMapCoord,
        map_area->min.x, map_area->min.y,
        map_area->max.x, map_area->max.y);

  assert(MapArea_is_valid(map_area));
}

void MapArea_expand_for_area(MapArea *const map_area, MapArea const *const b)
{
  assert(map_area != NULL);
  DEBUG("Will expand map area %" PRIMapCoord ",%" PRIMapCoord
        ",%" PRIMapCoord ",%" PRIMapCoord
        " to include %" PRIMapCoord ",%" PRIMapCoord
        ",%" PRIMapCoord ",%" PRIMapCoord,
        map_area->min.x, map_area->min.y, map_area->max.x, map_area->max.y,
        b->min.x, b->min.y, b->max.x, b->max.y);

  assert(MapArea_is_valid(b));

  if (map_area->min.x > b->min.x) {
    map_area->min.x = b->min.x;
  }
  if (map_area->min.y > b->min.y) {
    map_area->min.y = b->min.y;
  }
  if (map_area->max.x < b->max.x) {
    map_area->max.x = b->max.x;
  }
  if (map_area->max.y < b->max.y) {
    map_area->max.y = b->max.y;
  }

  DEBUG("Map area is now %" PRIMapCoord ",%" PRIMapCoord
        ",%" PRIMapCoord ",%" PRIMapCoord,
        map_area->min.x, map_area->min.y,
        map_area->max.x, map_area->max.y);

  assert(MapArea_is_valid(map_area));
}

bool MapArea_compare(MapArea const *const a, MapArea const *const b)
{
  assert(a != NULL);
  assert(b != NULL);
  return MapPoint_compare(a->min, b->min) && MapPoint_compare(a->max, b->max);
}

void MapArea_translate(MapArea const *const map_area, MapPoint const point,
  MapArea *const result)
{
  assert(MapArea_is_valid(map_area));
  assert(result != NULL);
  MapArea const translated = (MapArea) {
    .min = MapPoint_add(map_area->min, point),
    .max = MapPoint_add(map_area->max, point),
  };

  DEBUG("Translated map area %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord
        " by %" PRIMapCoord ",%" PRIMapCoord
        " to %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord,
        map_area->min.x, map_area->min.y, map_area->max.x, map_area->max.y,
        point.x, point.y, translated.min.x, translated.min.y, translated.max.x, translated.max.y);

  *result = translated;
}

void MapArea_mul(MapArea const *const map_area, MapPoint const point,
  MapArea *const result)
{
  assert(MapArea_is_valid(map_area));
  assert(result != NULL);
  *result = (MapArea) {
    .min = MapPoint_mul(map_area->min, point),
    .max = MapPoint_mul(map_area->max, point),
  };

  DEBUG("Multiplied map area by %" PRIMapCoord ",%" PRIMapCoord
        " to %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord,
        point.x, point.y, result->min.x, result->min.y, result->max.x, result->max.y);
}

void MapArea_div(MapArea const *const map_area, MapPoint const point,
  MapArea *const result)
{
  assert(MapArea_is_valid(map_area));
  assert(result != NULL);
  *result = (MapArea) {
    .min = MapPoint_div(map_area->min, point),
    .max = MapPoint_div_up(map_area->max, point),
  };

  DEBUG("Divided map area by %" PRIMapCoord ",%" PRIMapCoord
        " to %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord,
        point.x, point.y, result->min.x, result->min.y, result->max.x, result->max.y);
}

void MapArea_div_log2(MapArea const *const map_area,
  int const div_log2, MapArea *const result)
{
  assert(MapArea_is_valid(map_area));
  assert(result != NULL);
  *result = (MapArea) {
    .min = MapPoint_div_log2(map_area->min, div_log2),
    .max = MapPoint_div_up_log2(map_area->max, div_log2),
  };

  DEBUG("Map area >> %d "
        "to %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord,
        div_log2, result->min.x, result->min.y, result->max.x, result->max.y);
}

void MapArea_reflect_y(MapArea const *const map_area, MapArea *const result)
{
  assert(MapArea_is_valid(map_area));
  assert(result != NULL);
  *result = (MapArea) {
    .min = {
      .x = map_area->min.x,
      .y = -map_area->max.y,
    },
    .max = {
      .x = map_area->max.x,
      .y = -map_area->min.y,
    },
  };

  DEBUGF("Reflected area %" PRIMapCoord ",%" PRIMapCoord
         ",%" PRIMapCoord ",%" PRIMapCoord "\n",
    result->min.x, result->min.y, result->max.x, result->max.y);
}

MapPoint MapPoint_abs_diff(MapPoint const a, MapPoint const b)
{
  MapPoint const diff = {
    MapCoord_abs_diff(a.x, b.x),
    MapCoord_abs_diff(a.y, b.y)
  };
  assert(diff.x >= 0);
  assert(diff.y >= 0);
  DEBUGF("Abs. diff between %" PRIMapCoord ",%" PRIMapCoord
         " and %" PRIMapCoord ",%" PRIMapCoord
         " is %" PRIMapCoord ",%" PRIMapCoord "\n",
         a.x, a.y, b.x, b.y, diff.x, diff.y);
  return diff;
}

MapCoord MapPoint_dist(MapPoint const a, MapPoint const b)
{
  MapPoint const d = MapPoint_abs_diff(a, b);
  MapCoord const dist = (MapCoord)(
    sqrt( (d.x * d.x) + (d.y * d.y)) + 0.5);

  return dist;
}

MapCoord MapCoord_opp_to_adj(const MapCoord opp, const MapCoord hyp_squared)
{
  /* Using Pythagoras's theorem, compute the distance a from o and h squared:
     ,/| /|\
  h,/  |  |adj
  /____| \|/
 <-opp->
  */
  double const fadj = sqrt((double)(hyp_squared - (opp * opp)));
  MapCoord const adj = (MapCoord)(fadj + 0.5);
  DEBUGF("Adjacent is %" PRIMapCoord " (%f) for triangle with hypotenuse² %" PRIMapCoord
         " and opposite %" PRIMapCoord "\n", adj, fadj, hyp_squared, opp);
  return adj;
}

MapCoord MapPoint_pgram_area(MapPoint const a,
  MapPoint const b, MapPoint const c)
{
  /* Use the shoelace formula (Gauss's area formula)
     to determine the area of a parallelogram:
       ______c
      /_____/
     a      b
  */
  return (a.x * b.y) + (b.x * c.y) + (c.x * a.y);
}


bool MapPoint_clockwise(MapPoint const a,
  MapPoint const b, MapPoint const c)
{
  // FIXME: can just use sign?
  return (MapPoint_pgram_area(a, b, c) >
          MapPoint_pgram_area(a, c, b));
}

bool MapPoint_read(MapPoint *const point, Reader *const reader)
{
  int32_t x, y;

  assert(point);
  if (!reader_fread_int32(&x, reader) ||
      !reader_fread_int32(&y, reader))
  {
    return false;
  }
  *point = (MapPoint){x, y};
  return true;
}

void MapPoint_write(MapPoint const point, Writer *const writer)
{
  writer_fwrite_int32(point.x, writer);
  writer_fwrite_int32(point.y, writer);
}

bool MapArea_read(MapArea *const map_area, Reader *const reader)
{
  return MapPoint_read(&map_area->min, reader) &&
         MapPoint_read(&map_area->max, reader);
}

void MapArea_write(MapArea const *const map_area, Writer *const writer)
{
  MapPoint_write(map_area->min, writer);
  MapPoint_write(map_area->max, writer);
}

MapPoint MapAreaIter_get_first(MapAreaIter *const iter,
  MapArea const *const map_area)
{
  assert(iter != NULL);
  assert(MapArea_is_valid(map_area));

  *iter = (MapAreaIter){
    .map_pos = map_area->min,
    .map_area = *map_area,
    .done = false,
  };

  return MapAreaIter_get_next(iter);
}

MapPoint MapAreaIter_get_next(MapAreaIter *const iter)
{
  assert(iter != NULL);
  MapArea const *const map_area = &iter->map_area;
  assert(MapArea_is_valid(map_area));
  assert(!iter->done);

  if (iter->map_pos.x > map_area->max.x) {
    /* Proceed to the following row */
    ++iter->map_pos.y;

    if (iter->map_pos.y > map_area->max.y) {
      /* Finished */
      iter->done = true;
      return (MapPoint){-1, -1};
    }

    iter->map_pos.x = map_area->min.x;
  }

  MapPoint const map_pos = iter->map_pos;
  ++iter->map_pos.x;
  assert(MapArea_contains(map_area, map_pos));
  return map_pos;
}

bool MapArea_split(MapArea const *const area, int const size_log2,
                   bool (*const callback)(MapArea const *, void *),
                   void *const cb_arg)
{
  assert(MapArea_is_valid(area));
  assert(callback);

  MapCoord const size = ((MapCoord)1 << size_log2);
  MapCoord const coord_max = size - 1;

  DEBUGF("Split map area: x %" PRIMapCoord ",%" PRIMapCoord
         " y %" PRIMapCoord ",%" PRIMapCoord " (incl) for size %" PRIMapCoord "\n",
         area->min.x, area->max.x,
         area->min.y, area->max.y, size);

  MapArea clipped_area = *area;
  MapCoord const x_size_minus_1 = clipped_area.max.x - clipped_area.min.x;
  if (x_size_minus_1 >= coord_max) {
    /* large ranges always end up covering all */
    DEBUGF("Whole x range covered\n");
    clipped_area.min.x = 0;
    clipped_area.max.x = coord_max;
  } else {
    if (clipped_area.min.x < 0) {
      MapCoord const wrapped_min_x = (MapCoord)((unsigned long)clipped_area.min.x & (unsigned long)coord_max);

      if (clipped_area.max.x < 0) {
        DEBUG("Wrapping map area x %" PRIMapCoord ",%" PRIMapCoord " below x range",
              clipped_area.min.x, clipped_area.max.x);
        clipped_area.min.x = wrapped_min_x;
        clipped_area.max.x = wrapped_min_x + x_size_minus_1;
        /* May yet be split below if the recalculated max.x is too big */
      } else {
        DEBUG("Splitting map area x %" PRIMapCoord ",%" PRIMapCoord " at min.x == 0",
              clipped_area.min.x, clipped_area.max.x);
        MapArea const split_area = {
          { wrapped_min_x, clipped_area.min.y },
          { coord_max, clipped_area.max.y }
        };

        /* recurse */
        if (MapArea_split(&split_area, size_log2, callback, cb_arg)) {
          return true;
        }
        clipped_area.min.x = 0;
      }
    }

    while (clipped_area.max.x > coord_max) {
      if (clipped_area.min.x > coord_max) {
        DEBUG("Wrapping map area x %" PRIMapCoord ",%" PRIMapCoord " above x range",
              clipped_area.min.x, clipped_area.max.x);
        clipped_area.min.x = (MapCoord)((unsigned long)clipped_area.min.x & (unsigned long)coord_max);
        clipped_area.max.x = clipped_area.min.x + x_size_minus_1;
        /* May yet be split on next iteration if the recalculated max.x is still too big */
      } else {
        DEBUG("Splitting map area x %" PRIMapCoord ",%" PRIMapCoord " at max.x == %" PRIMapCoord,
              clipped_area.min.x, clipped_area.max.x, size);
        MapArea const split_area = {
          { 0, clipped_area.min.y },
          { (MapCoord)((unsigned long)clipped_area.max.x & (unsigned long)coord_max), clipped_area.max.y }
        };

        /* recurse */
        if (MapArea_split(&split_area, size_log2, callback, cb_arg)) {
          return true;
        }
        clipped_area.max.x = coord_max;
      }
    }
  }

  MapCoord const y_size_minus_1 = clipped_area.max.y - clipped_area.min.y;
  if (y_size_minus_1 >= coord_max) {
    /* large ranges always end up covering all */
    DEBUGF("Whole y range covered\n");
    clipped_area.min.y = 0;
    clipped_area.max.y = coord_max;
  } else {
    if (clipped_area.min.y < 0) {
      MapCoord const wrapped_min_y = (MapCoord)((unsigned long)clipped_area.min.y & (unsigned long)coord_max);

      if (clipped_area.max.y < 0) {
        DEBUG("Wrapping map area y %" PRIMapCoord ",%" PRIMapCoord " below y range",
              clipped_area.min.y, clipped_area.max.y);
        clipped_area.min.y = wrapped_min_y;
        clipped_area.max.y = wrapped_min_y + y_size_minus_1;
        /* May yet be split below if the recalculated max.y is too big */
      } else {
        DEBUG("Splitting map area y %" PRIMapCoord ",%" PRIMapCoord " at min.y == 0",
              clipped_area.min.y, clipped_area.max.y);
        MapArea const split_area = {
          { clipped_area.min.x, wrapped_min_y },
          { clipped_area.max.x, coord_max }
        };

        /* recurse */
        if (MapArea_split(&split_area, size_log2, callback, cb_arg)) {
          return true;
        }
        clipped_area.min.y = 0;
      }
    }

    while (clipped_area.max.y > coord_max) {
      if (clipped_area.min.y > coord_max) {
        DEBUG("Wrapping map area y %" PRIMapCoord ",%" PRIMapCoord " above y range",
              clipped_area.min.y, clipped_area.max.y);
        clipped_area.min.y = (MapCoord)((unsigned long)clipped_area.min.y & (unsigned long)coord_max);
        clipped_area.max.y = clipped_area.min.y + y_size_minus_1;
        /* May yet be split on next iteration if the recalculated max.y is still too big */
      } else {
        DEBUG("Splitting map area y %" PRIMapCoord ",%" PRIMapCoord " at max.y == %" PRIMapCoord,
              clipped_area.min.y, clipped_area.max.y, size);
        MapArea const split_area = {
          { clipped_area.min.x, 0 },
          { clipped_area.max.x, (MapCoord)((unsigned long)clipped_area.max.y & (unsigned long)coord_max) }
        };

        /* recurse */
        if (MapArea_split(&split_area, size_log2, callback, cb_arg)) {
          return true;
        }
        clipped_area.max.y = coord_max;
      }
    }
  }

  DEBUGF("Final map area: x %" PRIMapCoord ",%" PRIMapCoord
         " y %" PRIMapCoord ",%" PRIMapCoord " (incl)\n",
         clipped_area.min.x, clipped_area.max.x,
         clipped_area.min.y, clipped_area.max.y);

  assert(MapArea_is_valid(&clipped_area));
  return callback(&clipped_area, cb_arg);
}

void MapArea_rotate(MapAngle const angle, MapArea const *const map_area, MapArea *const result)
{
  assert(angle >= MapAngle_First);
  assert(angle < MapAngle_Count);
  assert(MapArea_is_valid(map_area));
  assert(result);

  switch (angle) {
  case MapAngle_Count:
  case MapAngle_North:
    *result = *map_area;
    break;
  case MapAngle_East:
    *result = (MapArea){.min = {-map_area->max.y, map_area->min.x}, .max = {-map_area->min.y, map_area->max.x}};
    break;
  case MapAngle_South:
    *result = (MapArea){.min = {-map_area->max.x, -map_area->max.y}, .max = {-map_area->min.x, -map_area->min.y}};
    break;
  case MapAngle_West:
    *result = (MapArea){.min = {map_area->min.y, -map_area->max.x}, .max = {map_area->max.y, -map_area->min.x}};
    break;
  }

  DEBUG("Rotated map area: x %" PRIMapCoord ",%" PRIMapCoord
        " y %" PRIMapCoord ",%" PRIMapCoord " to "
        "x %" PRIMapCoord ",%" PRIMapCoord
        " y %" PRIMapCoord ",%" PRIMapCoord,
        map_area->min.x, map_area->max.x, map_area->min.y, map_area->max.y,
        result->min.x, result->max.x, result->min.y, result->max.y);

  assert(MapArea_is_valid(result));
}

void MapArea_derotate(MapAngle const angle, MapArea const *const map_area, MapArea *const result)
{
  assert(angle >= MapAngle_First);
  assert(angle < MapAngle_Count);
  assert(MapArea_is_valid(map_area));
  assert(result);

  switch (angle) {
  case MapAngle_Count:
  case MapAngle_North:
    *result = *map_area;
    break;
  case MapAngle_East:
    *result = (MapArea){.min = {map_area->min.y, -map_area->max.x}, .max = {map_area->max.y, -map_area->min.x}};
    break;
  case MapAngle_South:
    *result = (MapArea){.min = {-map_area->max.x, -map_area->max.y}, .max = {-map_area->min.x, -map_area->min.y}};
    break;
  case MapAngle_West:
    *result = (MapArea){.min = {-map_area->max.y, map_area->min.x}, .max = {-map_area->min.y, map_area->max.x}};
    break;
  }

  DEBUG("Derotated map area: x %" PRIMapCoord ",%" PRIMapCoord
        " y %" PRIMapCoord ",%" PRIMapCoord " to "
        "x %" PRIMapCoord ",%" PRIMapCoord
        " y %" PRIMapCoord ",%" PRIMapCoord,
        map_area->min.x, map_area->max.x, map_area->min.y, map_area->max.y,
        result->min.x, result->max.x, result->min.y, result->max.y);

  assert(MapArea_is_valid(result));
}

void MapArea_split_diff(MapArea const *const a, MapArea const *const b,
                         void (*const callback)(MapArea const *, void *),
                         void *const cb_arg)
{
  assert(MapArea_is_valid(a));
  assert(MapArea_is_valid(b));
  assert(callback);

  DEBUGF("Split difference between map area: x %" PRIMapCoord ",%" PRIMapCoord
         " y %" PRIMapCoord ",%" PRIMapCoord " (incl) "
         "and x %" PRIMapCoord ",%" PRIMapCoord
         " y %" PRIMapCoord ",%" PRIMapCoord " (incl)\n",
         a->min.x, a->max.x, a->min.y, a->max.y,
         b->min.x, b->max.x, b->min.y, b->max.y);

  // x border extends to corner including any y diff
  MapCoord const ymin = LOWEST(a->min.y, b->min.y);
  MapCoord const ymax = HIGHEST(a->max.y, b->max.y);

  if (a->max.x != b->max.x) {
    MapArea const max_x_change = {
      .min = {
        .x = LOWEST(a->max.x, b->max.x) + 1,
        .y = ymin
      },
      .max = {
        .x = HIGHEST(a->max.x, b->max.x),
        .y = ymax
      },
    };
    callback(&max_x_change, cb_arg);
  }

  if (a->min.x != b->min.x) {
    MapArea const min_x_change = {
      .min = {
        .x = LOWEST(a->min.x, b->min.x),
        .y = ymin
      },
      .max = {
        .x = HIGHEST(a->min.x, b->min.x) - 1,
        .y = ymax
      },
    };
    callback(&min_x_change, cb_arg);
  }

  // don't include any corner regions (handled above)
  MapCoord const xmin = HIGHEST(a->min.x, b->min.x);
  MapCoord const xmax = LOWEST(a->max.x, b->max.x);

  if (a->max.y != b->max.y) {
    MapArea const max_y_change = {
      .min = {
        .x = xmin,
        .y = LOWEST(a->max.y, b->max.y) + 1,
      },
      .max = {
        .x = xmax,
        .y = HIGHEST(a->max.y, b->max.y),
      },
    };
    callback(&max_y_change, cb_arg);
  }

  if (a->min.y != b->min.y) {
    MapArea const min_y_change = {
      .min = {
        .x = xmin,
        .y = LOWEST(a->min.y, b->min.y),
      },
      .max = {
        .x = xmax,
        .y = HIGHEST(a->min.y, b->min.y) - 1,
      },
    };
    callback(&min_y_change, cb_arg);
  }
}
