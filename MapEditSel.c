/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map/animations editing mode selection
 *  Copyright (C) 2019 Christopher Bazley
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

#include <stdbool.h>

#include "flex.h"
#include "utils.h"
#include "MapEditSel.h"
#include "MapCoord.h"
#include "Shapes.h"
#include "Map.h"

enum {
  MapEditSelection_NBytes = (Map_Area + CHAR_BIT - 1) / CHAR_BIT,
};

static void redraw(MapEditSelection *const selection, MapArea const *area)
{
  assert(selection);
  if (selection->redraw_cb) {
    DEBUGF("Redraw selection changed area %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord "\n", area->min.x, area->min.y, area->max.x, area->max.y);
    selection->redraw_cb(area, selection->redraw_arg);
  } else {
    DEBUGF("No handler to redraw selection changed area %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord "\n", area->min.x, area->min.y, area->max.x, area->max.y);
  }
}

static void clear_bounds(MapEditSelection *const selection)
{
  assert(selection != NULL);
  assert(selection->num_selected == 0);
  selection->max_bounds_are_min = true;
  selection->max_bounds = MapArea_make_invalid();
}

static inline void expand_bounds(MapEditSelection *const selection,
  MapArea const *const map_area)
{
  assert(selection != NULL);
  MapArea_expand_for_area(&selection->max_bounds, map_area);
}

static void maximise_bounds(MapEditSelection *const selection)
{
  assert(selection != NULL);
  assert(selection->num_selected == Map_Area);
  selection->max_bounds_are_min = true;
  selection->max_bounds = (MapArea){
    {0, 0},
    {Map_Size-1, Map_Size-1}
  };
}

static inline bool is_selected(MapEditSelection const *const selection,
  MapPoint const pos)
{
  assert(selection != NULL);
  size_t const index = map_coords_to_index(pos);
  size_t const char_index = index / CHAR_BIT;
  unsigned char const mask = 1u << (index % CHAR_BIT);
  assert(char_index < MapEditSelection_NBytes);
  return ((unsigned char *)selection->flex)[char_index] & mask;
}

static inline void validate_selection(MapEditSelection const *const selection)
{
#ifdef NDEBUG
  NOT_USED(selection);
#else
  assert(selection);
  assert(selection->num_selected >= 0);
  assert(selection->num_selected <= Map_Area);
  assert(selection->flex);

  if (!MapArea_is_valid(&selection->max_bounds)) {
    assert(selection->num_selected == 0);
  }

  size_t count = 0;
  MapAreaIter iter;
  for (MapPoint p = map_get_first(&iter);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    if (is_selected(selection, p)) {
      assert(map_bbox_contains(&selection->max_bounds, p));
      count++;
    }
  }

  DEBUGF("%zu tiles are selected (expected %zu)\n", selection->num_selected, count);
  assert(count == selection->num_selected);
#endif
}

static inline void select_in_map(MapEditSelection const *const selection,
  MapPoint const pos)
{
  assert(selection != NULL);
  size_t const index = map_coords_to_index(pos);
  size_t const char_index = index / CHAR_BIT;
  unsigned char const mask = 1u << (index % CHAR_BIT);
  assert(char_index < MapEditSelection_NBytes);
  ((unsigned char *)selection->flex)[char_index] |= mask;
}

static inline void deselect_in_map(MapEditSelection const *const selection,
  MapPoint const pos)
{
  assert(selection != NULL);
  size_t const index = map_coords_to_index(pos);
  size_t const char_index = index / CHAR_BIT;
  unsigned char const mask = 1u << (index % CHAR_BIT);
  assert(char_index < MapEditSelection_NBytes);
  ((unsigned char *)selection->flex)[char_index] &= ~mask;
}

static void update_bounds_for_deselect(MapEditSelection *const selection,
  size_t const prev_num_selected)
{
  assert(selection != NULL);

  if (MapEditSelection_is_none(selection)) {
    clear_bounds(selection);
  } else if (prev_num_selected != selection->num_selected) {
    selection->max_bounds_are_min = false;
  }
}

static void select_and_inc(MapEditSelection *const selection,
  MapPoint const pos)
{
  select_in_map(selection, pos);
  assert(selection->num_selected >= 0);
  ++selection->num_selected;
  DEBUGF("%zu tiles selected after select\n", selection->num_selected);
}

static void deselect_and_dec(MapEditSelection *const selection,
  MapPoint const pos)
{
  deselect_in_map(selection, pos);
  assert(selection->num_selected > 0);
  --selection->num_selected;
  DEBUGF("%zu tiles selected after deselect\n", selection->num_selected);
}

static MapArea limit_max_bounds(MapEditSelection const *const selection)
{
  assert(selection);

  return (MapArea){
    .min = selection->max_bounds.min,
    .max = {
      LOWEST(selection->max_bounds.max.x,
             selection->max_bounds.min.x + Map_Size - 1),
      LOWEST(selection->max_bounds.max.y,
             selection->max_bounds.min.y + Map_Size - 1)
    }
  };
}

SFError MapEditSelection_init(MapEditSelection *const selection,
  void (*const redraw_cb)(MapArea const *, void *), void *const redraw_arg)
{
  assert(selection != NULL);

  *selection = (MapEditSelection){.num_selected = 0,
    .redraw_cb = redraw_cb, .redraw_arg = redraw_arg};
  clear_bounds(selection);
  if (!flex_alloc(&selection->flex, MapEditSelection_NBytes)) {
    return SFERROR(NoMem);
  }

  memset_flex(&selection->flex, 0, MapEditSelection_NBytes);
  validate_selection(selection);
  return SFERROR(OK);
}

bool MapEditSelection_get_bounds(MapEditSelection *const selection,
  MapArea *const bounds)
{
  validate_selection(selection);
  assert(bounds != NULL);
  DEBUG("Will find bounds of selection %p", (void *)selection);

  if (MapEditSelection_is_none(selection)) {
    return false;
  }

  if (MapEditSelection_is_all(selection)) {
    *bounds = (MapArea){{0, 0}, {Map_Size-1, Map_Size-1}};
    return true;
  }

  if (selection->max_bounds_are_min) {
    /* If we don't limit max_bounds then it upsets callers. */
    *bounds = limit_max_bounds(selection);
    return true;
  }

  MapArea min_bounds = MapArea_make_invalid();

  MapEditSelIter iter;
  for (MapPoint p = MapEditSelIter_get_first(&iter, selection);
       !MapEditSelIter_done(&iter);
       p = MapEditSelIter_get_next(&iter))
  {
    MapArea_expand(&min_bounds, p);
  }

  assert(MapArea_is_valid(&min_bounds));
  DEBUG("Selection bounds are x %" PRIMapCoord ",%" PRIMapCoord
        "  y %" PRIMapCoord ",%" PRIMapCoord,
        min_bounds.min.x, min_bounds.max.x,
        min_bounds.min.y, min_bounds.max.y);
  *bounds = min_bounds;

  selection->max_bounds_are_min = true;
  selection->max_bounds = min_bounds;

  validate_selection(selection);
  return true;
}

MapPoint MapEditSelIter_get_first(MapEditSelIter *const iter,
  MapEditSelection *const selection)
{
  assert(iter != NULL);
  validate_selection(selection);

  *iter = (MapEditSelIter){
    .remaining = MapEditSelection_size(selection),
    .selection = selection,
  };

  if (iter->remaining == 0) {
    DEBUG ("No map locations selected!");
    iter->done = true;
    assert(MapEditSelIter_done(iter));
    return (MapPoint){-1, -1};
  }

  /* If we don't limit max_bounds then we might double-count the same
     location because of coordinate wrap-around. */
  MapArea bounds = limit_max_bounds(selection);

  MapPoint const p = MapAreaIter_get_first(&iter->area_iter, &bounds);
  assert(!MapAreaIter_done(&iter->area_iter));

  if (is_selected(selection, map_wrap_coords(p))) {
    --iter->remaining;
    assert(!MapEditSelIter_done(iter));
    return p;
  }

  return MapEditSelIter_get_next(iter);
}

MapPoint MapEditSelIter_get_next(MapEditSelIter *const iter)
{
  assert(iter != NULL);
  assert(!MapEditSelIter_done(iter));

  MapEditSelection *const selection = iter->selection;
  validate_selection(selection);

  if (iter->remaining > 0) {
    for (MapPoint p = MapAreaIter_get_next(&iter->area_iter);
         !MapAreaIter_done(&iter->area_iter);
         p = MapAreaIter_get_next(&iter->area_iter))
    {
      if (is_selected(selection, map_wrap_coords(p))) {
        --iter->remaining;
        assert(!MapEditSelIter_done(iter));
        return p;
      }
    }
    assert(!"Fewer selected locations than at start");
  }
  iter->done = true;
  assert(MapEditSelIter_done(iter));
  return (MapPoint){-1, -1};
}

static void invert_one(MapPoint pos, MapEditSelection *const selection)
{
  pos = map_wrap_coords(pos);
  if (is_selected(selection, pos)) {
    deselect_and_dec(selection, pos);
    selection->max_bounds_are_min = false;
  } else {
    select_and_inc(selection, pos);
  }
}

void MapEditSelection_invert_area(MapEditSelection *const selection,
  MapArea const *const map_area, bool const do_redraw)
{
  validate_selection(selection);

  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, map_area);
      !MapAreaIter_done(&iter);
      p = MapAreaIter_get_next(&iter))
  {
    invert_one(p, selection);
  }

  if (MapEditSelection_is_none(selection)) {
    clear_bounds(selection);
  } else {
    /* If we did only deselect and not select then this does nothing
       because such points should lie within existing bounds */
    expand_bounds(selection, map_area);
  }
  if (do_redraw) {
    redraw(selection, map_area);
  }
  validate_selection(selection);
}

void MapEditSelection_invert(MapEditSelection *const selection, MapPoint pos)
{
  validate_selection(selection);

  invert_one(pos, selection);
  if (MapEditSelection_is_none(selection)) {
    clear_bounds(selection);
  } else {
    MapArea_expand(&selection->max_bounds, pos);
  }
  redraw(selection, &(MapArea){pos, pos});
  validate_selection(selection);
}

static bool select_one(MapPoint pos, MapEditSelection *const selection)
{
  pos = map_wrap_coords(pos);
  if (!is_selected(selection, pos)) {
    select_and_inc(selection, pos);
    return true;
  }
  return false;
}

void MapEditSelection_select_area(MapEditSelection *const selection,
  MapArea const *const map_area)
{
  validate_selection(selection);

  if (MapEditSelection_is_all(selection)) {
    return; /* nothing to do */
  }

  bool any_selected = false;
  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, map_area);
      !MapAreaIter_done(&iter);
      p = MapAreaIter_get_next(&iter))
  {
    if (select_one(p, selection)) {
      any_selected = true;
    }
  }

  if (any_selected) {
    expand_bounds(selection, map_area);
    redraw(selection, map_area);
  }
  validate_selection(selection);
}

void MapEditSelection_select(MapEditSelection *const selection,
  MapPoint pos)
{
  validate_selection(selection);

  if (MapEditSelection_is_all(selection)) {
    return; /* nothing to do */
  }

  if (select_one(pos, selection)) {
    MapArea_expand(&selection->max_bounds, pos);
    redraw(selection, &(MapArea){pos, pos});
  }
  validate_selection(selection);
}

static bool deselect_one(MapPoint pos, MapEditSelection *const selection)
{
  pos = map_wrap_coords(pos);
  if (is_selected(selection, pos)) {
    deselect_and_dec(selection, pos);
    return true;
  }
  return false;
}

void MapEditSelection_deselect(MapEditSelection *const selection,
  MapPoint const pos)
{
  validate_selection(selection);

  if (MapEditSelection_is_none(selection)) {
    return; /* nothing to do */
  }

  size_t const prev = selection->num_selected;
  if (deselect_one(pos, selection)) {
    update_bounds_for_deselect(selection, prev);
    redraw(selection, &(MapArea){pos, pos});
  }
  validate_selection(selection);
}

void MapEditSelection_deselect_area(MapEditSelection *const selection,
  MapArea const *const map_area)
{
  validate_selection(selection);

  if (MapEditSelection_is_none(selection)) {
    return; /* nothing to do */
  }

  size_t const prev = selection->num_selected;
  bool any_deselected = false;
  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, map_area);
      !MapAreaIter_done(&iter);
      p = MapAreaIter_get_next(&iter))
  {
    if (deselect_one(p, selection)) {
      any_deselected = true;
    }
  }

  if (any_deselected) {
    update_bounds_for_deselect(selection, prev);
    redraw(selection, map_area);
  }
  validate_selection(selection);
}

typedef struct {
  MapEditSelection *selection;
  bool do_redraw;
} InvertAreaData;

static void invert_area_cb(MapArea const *const map_area, void *const arg)
{
  DEBUGF("Invert area {%" PRIMapCoord ", %" PRIMapCoord
    ", %" PRIMapCoord ", %" PRIMapCoord "}\n",
    map_area->min.x, map_area->min.y, map_area->max.x, map_area->max.y);
  assert(MapArea_is_valid(map_area));

  const InvertAreaData *const data = arg;
  MapEditSelection_invert_area(data->selection, map_area, data->do_redraw);
}

static void select_area_cb(MapArea const *const map_area, void *const arg)
{
  DEBUGF("Select area {%" PRIMapCoord ", %" PRIMapCoord
    ", %" PRIMapCoord ", %" PRIMapCoord "}\n",
    map_area->min.x, map_area->min.y, map_area->max.x, map_area->max.y);
  assert(MapArea_is_valid(map_area));

  MapEditSelection *const selection = arg;
  MapEditSelection_select_area(selection, map_area);
}

void MapEditSelection_select_tri(MapEditSelection *const selection,
  MapPoint const vertex_A, MapPoint const vertex_B, MapPoint const vertex_C)
{
  validate_selection(selection);
  Shapes_tri(select_area_cb, selection,
    vertex_A, vertex_B, vertex_C);
  validate_selection(selection);
}

void MapEditSelection_invert_rect(MapEditSelection *const selection,
  MapPoint const vertex_A, MapPoint const vertex_B, bool const do_redraw)
{
  validate_selection(selection);
  InvertAreaData data = {.do_redraw = do_redraw, .selection = selection};
  Shapes_rect(invert_area_cb, &data, vertex_A, vertex_B);
  validate_selection(selection);
}

void MapEditSelection_select_rect(MapEditSelection *const selection,
  MapPoint const vertex_A, MapPoint const vertex_B)
{
  validate_selection(selection);
  Shapes_rect(select_area_cb, selection, vertex_A, vertex_B);
  validate_selection(selection);
}

void MapEditSelection_select_circ(MapEditSelection *const selection,
  MapPoint const centre, MapCoord const radius)
{
  validate_selection(selection);
  Shapes_circ(select_area_cb, selection, centre, radius);
  validate_selection(selection);
}

void MapEditSelection_select_line(MapEditSelection *const selection,
  MapPoint const start, MapPoint const end, MapCoord const thickness)
{
  validate_selection(selection);
  Shapes_line(select_area_cb, selection, start, end, thickness);
  validate_selection(selection);
}

bool MapEditSelection_is_selected(MapEditSelection const *const selection,
  MapPoint const pos)
{
  assert(selection);

  if (MapEditSelection_is_none(selection)) {
    return false; /* nothing selected */
  }

  return is_selected(selection, map_wrap_coords(pos));
}

void MapEditSelection_clear(MapEditSelection *const selection)
{
  validate_selection(selection);

  if (MapEditSelection_is_none(selection))
    return; /* nothing to do */

  MapArea const *redraw_bounds = &selection->max_bounds;
  MapArea min_bounds = MapArea_make_invalid();

  if (MapEditSelection_is_all(selection)) {
    DEBUGF("Everything is selected\n");
    memset_flex(&selection->flex, 0, MapEditSelection_NBytes);
  } else {
    DEBUGF("Deselect individually\n");

    MapAreaIter iter;
    for (MapPoint p = MapAreaIter_get_first(&iter, &selection->max_bounds);
         !MapAreaIter_done(&iter);
         p = MapAreaIter_get_next(&iter))
    {
      if (!selection->max_bounds_are_min)
      {
        MapArea_expand(&min_bounds, p);
      }
      deselect_in_map(selection, map_wrap_coords(p));
    }

    if (!selection->max_bounds_are_min)
    {
      assert(MapArea_is_valid(&min_bounds));
      redraw_bounds = &min_bounds;
    }
  }

  redraw(selection, redraw_bounds);

  selection->num_selected = 0;
  clear_bounds(selection);
  DEBUGF("Cleared selection\n");
  validate_selection(selection);
}

void MapEditSelection_select_all(MapEditSelection *const selection)
{
  validate_selection(selection);

  if (MapEditSelection_is_all(selection)) {
    return; /* nothing to do */
  }

  memset_flex(&selection->flex, UINT8_MAX, MapEditSelection_NBytes);
  selection->num_selected = Map_Area;
  maximise_bounds(selection);
  redraw(selection, &selection->max_bounds);
  DEBUGF("Selected all\n");
  validate_selection(selection);
}

void MapEditSelection_destroy(MapEditSelection *const selection)
{
  validate_selection(selection);
  flex_free(&selection->flex);
}
