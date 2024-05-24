/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map area collection
 *  Copyright (C) 2023 Christopher Bazley
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

#include <stddef.h>
#include <string.h>
#include "Debug.h"
#include "MapCoord.h"
#include "MapAreaCol.h"
#include "MapAreaColData.h"

void MapAreaCol_init(MapAreaColData *const coll, int const size_log2)
{
  assert(coll);
  coll->count = 0;
  coll->size_log2 = size_log2;
}

static MapCoord calc_scaled_area(int const size_log2, MapArea const *const map_area)
{
  return MapPoint_area(MapPoint_div_log2(MapArea_size(map_area), size_log2 / 2));
}

static void delete_area(MapAreaColData *const coll, size_t const k)
{
  /* Consider:
     |   k   | k + 1 |  ...  | count
     We need to move count - (k + 1) == count - 1 - k objects.
   */
  assert(coll);
  assert(k < coll->count);
  assert(coll->count > 0);
  coll->count--;
  memmove(&coll->areas[k], &coll->areas[k + 1], sizeof(coll->areas[0]) * (coll->count - k));
}

static void merge_overlapping(MapAreaColData *const coll)
{
  assert(coll);
  bool merged;
  do {
    merged = false;
    size_t const count = coll->count;
    int const size_log2 = coll->size_log2;
    for (size_t i = 0; i < count && !merged; ++i) {
      for (size_t k = i + 1; k < count && !merged; ++k) {
        if (!MapArea_overlaps(&coll->areas[i].bbox, &coll->areas[k].bbox)) {
          continue;
        }

        MapArea_expand_for_area(&coll->areas[i].bbox, &coll->areas[k].bbox);
        coll->areas[i].area = calc_scaled_area(size_log2, &coll->areas[i].bbox);
        delete_area(coll, k);
        DEBUGF("Merged overlapping map area %zu into %zu (%zu remain)\n", k, i, count);
        merged = true;
      }
    }
  } while (merged);
}

void MapAreaCol_add(MapAreaColData *const coll, MapArea const *const area)
{
  assert(coll);
  assert(MapArea_is_valid(area));

  size_t best = SIZE_MAX;
  MapAreaColEntry best_candidate = {{0}};
  MapCoord best_area_diff = MAP_COORDS_LIMIT;

  size_t const count = coll->count;
  int const size_log2 = coll->size_log2;
  for (size_t i = 0; i < count; ++i) {
    // If the new box is contained entirely by an existing box then ignore it.
    if (MapArea_contains_area(&coll->areas[i].bbox, area)) {
      DEBUGF("Discard rectangle within map area %zu\n", i);
      return;
    }

    /* If the new box overlaps an existing box then it will end
       up merged with it one way or another so stop iterating immediately. */
    if (MapArea_overlaps(&coll->areas[i].bbox, area)) {
      DEBUGF("Expand overlapping map area %zu\n", i);
      MapArea_expand_for_area(&coll->areas[i].bbox, area);
      coll->areas[i].area = calc_scaled_area(size_log2, &coll->areas[i].bbox);
      merge_overlapping(coll);
      return;
    }

    if (count < MapAreaColMax) {
      continue; // Space to insert a new box so don't consider merging it
    }

    // Consider the cost of merging the new box with each existing box
    MapAreaColEntry candidate = {
      .bbox = coll->areas[i].bbox,
    };

    MapArea_expand_for_area(&candidate.bbox, area);
    candidate.area = calc_scaled_area(size_log2, &candidate.bbox);

    assert(coll->areas[i].area < candidate.area);
    MapCoord const area_diff = candidate.area - coll->areas[i].area;
    if (area_diff < best_area_diff) {
      DEBUGF("Map area %d is new best candidate (extra area is %" PRIMapCoord ")\n",
             i, area_diff);
      best_area_diff = area_diff;
      best_candidate = candidate;
      best = i;
    }
  }

  if (count >= MapAreaColMax) {
    // Consider the alternative cost of merging any two existing boxes
    size_t best_i = SIZE_MAX, best_k = SIZE_MAX;
    MapAreaColEntry best_i_candidate = {{0}};

    for (size_t i = 0; i < count; ++i) {
      for (size_t k = i + 1; k < count; ++k) {
        MapAreaColEntry candidate = {
          .bbox = coll->areas[i].bbox,
        };

        MapArea_expand_for_area(&candidate.bbox, &coll->areas[k].bbox);
        candidate.area = calc_scaled_area(size_log2, &candidate.bbox);

        assert(coll->areas[i].area < candidate.area);
        MapCoord const area_diff = candidate.area - coll->areas[i].area - coll->areas[k].area;
        if (area_diff < best_area_diff) {
          DEBUGF("Merged map area %zu and %zu is new best candidate (extra area is %" PRIMapCoord ")\n",
                 i, k, area_diff);
          best_area_diff = area_diff;
          best_i_candidate = candidate;
          best_i = i;
          best_k = k;
        }
      }
    }

    if (best_i != SIZE_MAX) {
      // Cheaper to merge two existing boxes
      assert(best_k > best_i);
      assert(best_i < count);
      coll->areas[best_i] = best_i_candidate;
      delete_area(coll, best_k);
      DEBUGF("Merged overlapping map area %zu into %zu (%zu remain)\n", best_k, best_i, count);
    }
  }

  // Count may have been decremented by merging two existing boxes (above)
  if (coll->count < MapAreaColMax) {
    DEBUGF("Adding new map area %zu\n", count);
    coll->areas[coll->count++] = (MapAreaColEntry){
      .bbox = *area,
      .area = calc_scaled_area(size_log2, area),
    };
  } else {
    assert(best < coll->count);
    DEBUGF("Extending existing map area %zu\n", best);
    coll->areas[best] = best_candidate;
    merge_overlapping(coll);
  }
}

MapArea const *MapAreaColIter_get_first(MapAreaColIter *const iter, MapAreaColData const *const coll)
{
  assert(iter);
  *iter = (MapAreaColIter){.coll = coll, .next = 0};
  return MapAreaColIter_get_next(iter);
}

MapArea const *MapAreaColIter_get_next(MapAreaColIter *const iter)
{
  assert(iter);
  MapAreaColData const *const coll = iter->coll;
  assert(coll);
  assert(iter->next <= coll->count);
  if (iter->next < coll->count) {
    return &coll->areas[iter->next++].bbox;
  }
  return NULL;
}
