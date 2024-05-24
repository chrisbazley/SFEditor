/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Objects editing mode selection
 *  Copyright (C) 2021 Christopher Bazley
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
#include <stdint.h>

#include "flex.h"
#include "err.h"
#include "msgtrans.h"
#include "utils.h"
#include "ObjEditSel.h"
#include "MapCoord.h"
#include "Obj.h"

enum {
  ObjEditSelection_NBytes = (Obj_Area + CHAR_BIT - 1) / CHAR_BIT,
};

static void clear_bounds(ObjEditSelection *const selection)
{
  assert(selection != NULL);
  assert(selection->num_selected == 0);
  selection->max_bounds = MapArea_make_invalid();
}

static inline void expand_bounds(ObjEditSelection *const selection,
  MapArea const *const map_area)
{
  assert(selection != NULL);
  MapArea_expand_for_area(&selection->max_bounds, map_area);
}

static inline bool is_selected(ObjEditSelection const *const selection,
  MapPoint const pos)
{
  assert(selection != NULL);
  size_t const index = objects_coords_to_index(pos);
  size_t const char_index = index / CHAR_BIT;
  unsigned char const mask = 1u << (index % CHAR_BIT);
  assert(char_index < ObjEditSelection_NBytes);
  return ((unsigned char *)selection->flex)[char_index] & mask;
}

static inline void validate_selection(ObjEditSelection const *const selection)
{
#ifndef NDEBUG
  assert(selection);
  assert(selection->num_selected >= 0);
  assert(selection->num_selected <= Obj_Area);
  assert(selection->flex);

  if (!MapArea_is_valid(&selection->max_bounds)) {
    assert(selection->num_selected == 0);
  }

  size_t count = 0;
  MapAreaIter iter;
  for (MapPoint p = objects_get_first(&iter);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    if (is_selected(selection, p)) {
      assert(objects_bbox_contains(&selection->max_bounds, p));
      count++;
    }
  }

  DEBUGF("%zu objects are selected (expected %zu)\n", selection->num_selected, count);
  assert(count == selection->num_selected);
#else
  NOT_USED(selection);
#endif
}

static inline void select_in_map(ObjEditSelection const *const selection,
  MapPoint const pos)
{
  assert(selection != NULL);
  size_t const index = objects_coords_to_index(pos);
  size_t const char_index = index / CHAR_BIT;
  unsigned char const mask = 1u << (index % CHAR_BIT);
  assert(char_index < ObjEditSelection_NBytes);
  ((unsigned char *)selection->flex)[char_index] |= mask;
}

static inline void deselect_in_map(ObjEditSelection const *const selection,
  MapPoint const pos)
{
  assert(selection != NULL);
  size_t const index = objects_coords_to_index(pos);
  size_t const char_index = index / CHAR_BIT;
  unsigned char const mask = 1u << (index % CHAR_BIT);
  assert(char_index < ObjEditSelection_NBytes);
  ((unsigned char *)selection->flex)[char_index] &= ~mask;
}

static void update_bounds_for_deselect(ObjEditSelection *const selection)
{
  assert(selection != NULL);

  if (ObjEditSelection_is_none(selection)) {
    clear_bounds(selection);
  }
}

static void select_and_inc(ObjEditSelection *const selection, MapPoint const pos)
{
  select_in_map(selection, pos);
  assert(selection->num_selected >= 0);
  ++selection->num_selected;
  DEBUGF("%ld objects selected after select\n", selection->num_selected);
}

static void deselect_and_dec(ObjEditSelection *const selection, MapPoint const pos)
{
  deselect_in_map(selection, pos);
  assert(selection->num_selected > 0);
  --selection->num_selected;
  DEBUGF("%ld objects selected after deselect\n", selection->num_selected);
}

static void redraw(ObjEditSelection *const selection, MapPoint const pos)
{
  assert(selection);
  if (selection->redraw_cb) {
    DEBUGF("Redraw selection changed pos %" PRIMapCoord ",%" PRIMapCoord "\n", pos.x, pos.y);
    selection->redraw_cb(pos, selection->redraw_arg);
  } else {
    DEBUGF("No handler to redraw selection changed pos %" PRIMapCoord ",%" PRIMapCoord "\n", pos.x, pos.y);
  }
}


static MapArea limit_max_bounds(ObjEditSelection const *const selection)
{
  assert(selection);

  return (MapArea){
    .min = selection->max_bounds.min,
    .max = {
      LOWEST(selection->max_bounds.max.x,
             selection->max_bounds.min.x + Obj_Size - 1),
      LOWEST(selection->max_bounds.max.y,
             selection->max_bounds.min.y + Obj_Size - 1)
    }
  };
}

SFError ObjEditSelection_init(ObjEditSelection *const selection,
  void (*const redraw_cb)(MapPoint, void *), void *const redraw_arg)
{
  assert(selection != NULL);

  *selection = (ObjEditSelection){.num_selected = 0,
    .redraw_cb = redraw_cb, .redraw_arg = redraw_arg};
  clear_bounds(selection);
  if (!flex_alloc(&selection->flex, ObjEditSelection_NBytes)) {
    report_error(SFERROR(NoMem), "", "");
    return SFERROR(NoMem);
  }

  memset_flex(&selection->flex, 0, ObjEditSelection_NBytes);
  validate_selection(selection);
  return SFERROR(OK);
}

void ObjEditSelection_copy(ObjEditSelection *const dst, ObjEditSelection *const src)
{
  assert(dst != NULL);
  assert(src != NULL);
  dst->max_bounds = src->max_bounds;
  dst->num_selected = src->num_selected;
  memcpy_flex(&dst->flex, &src->flex, ObjEditSelection_NBytes);
}

bool ObjEditSelection_get_bounds(ObjEditSelection *const selection,
  MapArea *const bounds)
{
  validate_selection(selection);
  assert(bounds != NULL);
  DEBUG("Will find bounds of selection %p", (void *)selection);

  if (ObjEditSelection_is_none(selection)) {
    return false;
  }

  if (ObjEditSelection_is_all(selection)) {
    *bounds = (MapArea){{0, 0}, {Obj_Size-1, Obj_Size-1}};
    return true;
  }

  MapArea min_bounds = MapArea_make_invalid();

  ObjEditSelIter iter;
  for (MapPoint p = ObjEditSelIter_get_first(&iter, selection);
       !ObjEditSelIter_done(&iter);
       p = ObjEditSelIter_get_next(&iter))
  {
    MapArea_expand(&min_bounds, p);
  }

  assert(MapArea_is_valid(&min_bounds));
  DEBUG("Selection bounds are x %" PRIMapCoord ",%" PRIMapCoord
        "  y %" PRIMapCoord ",%" PRIMapCoord,
        min_bounds.min.x, min_bounds.max.x,
        min_bounds.min.y, min_bounds.max.y);

  selection->max_bounds = min_bounds;
  *bounds = min_bounds;

  validate_selection(selection);
  return true;
}

MapPoint ObjEditSelIter_get_first(ObjEditSelIter *const iter,
  ObjEditSelection *const selection)
{
  assert(iter != NULL);
  validate_selection(selection);

  *iter = (ObjEditSelIter){
    .remaining = ObjEditSelection_size(selection),
    .selection = selection,
  };

  if (iter->remaining == 0) {
    DEBUG ("No objects selected!");
    iter->done = true;
    assert(ObjEditSelIter_done(iter));
    return (MapPoint){-1, -1};
  }

  /* If we don't limit max_bounds then we might double-count the same
     location because of coordinate wrap-around. */
  MapArea bounds = limit_max_bounds(selection);

  MapPoint const p = MapAreaIter_get_first(&iter->area_iter, &bounds);
  assert(!MapAreaIter_done(&iter->area_iter));

  if (is_selected(selection, objects_wrap_coords(p))) {
    --iter->remaining;
    assert(!ObjEditSelIter_done(iter));
    return p;
  }

  return ObjEditSelIter_get_next(iter);
}

MapPoint ObjEditSelIter_get_next(ObjEditSelIter *const iter)
{
  assert(iter != NULL);
  assert(!ObjEditSelIter_done(iter));

  ObjEditSelection *const selection = iter->selection;
  validate_selection(selection);

  if (iter->remaining > 0) {
    for (MapPoint p = MapAreaIter_get_next(&iter->area_iter);
         !MapAreaIter_done(&iter->area_iter);
         p = MapAreaIter_get_next(&iter->area_iter))
    {
      if (is_selected(selection, objects_wrap_coords(p))) {
        --iter->remaining;
        assert(!ObjEditSelIter_done(iter));
        return p;
      }
    }
    assert(!"Fewer objects selected than at start");
  }
  iter->done = true;
  assert(ObjEditSelIter_done(iter));
  return (MapPoint){-1, -1};
}

static void invert_one(ObjEditSelection *const selection,
  MapPoint const pos, bool const do_redraw)
{
  MapPoint const wrapped_pos = objects_wrap_coords(pos);
  if (is_selected(selection, wrapped_pos)) {
    deselect_and_dec(selection, wrapped_pos);
  } else {
    select_and_inc(selection, wrapped_pos);
  }
  if (do_redraw) {
    redraw(selection, pos);
  }
}

void ObjEditSelection_invert(ObjEditSelection *const selection,
  MapPoint const pos, bool const do_redraw)
{
  validate_selection(selection);

  invert_one(selection, pos, do_redraw);
  if (ObjEditSelection_is_none(selection)) {
    clear_bounds(selection);
  } else {
    MapArea_expand(&selection->max_bounds, pos);
  }
  validate_selection(selection);
}

static bool select_one(ObjEditSelection *const selection,
  MapPoint pos)
{
  MapPoint const wrapped_pos = objects_wrap_coords(pos);
  if (!is_selected(selection, wrapped_pos)) {
    select_and_inc(selection, wrapped_pos);
    redraw(selection, pos);
    return true;
  }
  return false;
}

static bool deselect_one(ObjEditSelection *const selection,
  MapPoint pos)
{
  MapPoint const wrapped_pos = objects_wrap_coords(pos);
  if (is_selected(selection, wrapped_pos)) {
    deselect_and_dec(selection, wrapped_pos);
    redraw(selection, pos);
    return true;
  }
  return false;
}

void ObjEditSelection_select_area(ObjEditSelection *const selection,
  MapArea const *const map_area)
{
  validate_selection(selection);

  if (ObjEditSelection_is_all(selection)) {
    return; /* nothing to do */
  }

  bool any_selected = false;
  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, map_area);
      !MapAreaIter_done(&iter);
      p = MapAreaIter_get_next(&iter))
  {
    if (select_one(selection, p)) {
      any_selected = true;
    }
  }

  if (any_selected) {
    expand_bounds(selection, map_area);
  }
  validate_selection(selection);
}

void ObjEditSelection_deselect_area(ObjEditSelection *const selection,
  MapArea const *const map_area)
{
  validate_selection(selection);

  if (ObjEditSelection_is_none(selection)) {
    return; /* nothing to do */
  }

  bool any_deselected = false;
  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, map_area);
      !MapAreaIter_done(&iter);
      p = MapAreaIter_get_next(&iter))
  {
    if (deselect_one(selection, p)) {
      any_deselected = true;
    }
  }

  if (any_deselected) {
    update_bounds_for_deselect(selection);
  }
  validate_selection(selection);
}

void ObjEditSelection_select(ObjEditSelection *const selection,
  MapPoint pos)
{
  validate_selection(selection);

  if (ObjEditSelection_is_all(selection)) {
    return; /* nothing to do */
  }

  if (select_one(selection, pos)) {
    MapArea_expand(&selection->max_bounds, pos);
  }
  validate_selection(selection);
}

void ObjEditSelection_deselect(ObjEditSelection *const selection,
  MapPoint const pos)
{
  validate_selection(selection);

  if (ObjEditSelection_is_none(selection)) {
    return; /* nothing to do */
  }

  if (deselect_one(selection, pos)) {
    update_bounds_for_deselect(selection);
  }
  validate_selection(selection);
}

bool ObjEditSelection_is_selected(ObjEditSelection const *const selection,
  MapPoint const pos)
{
  assert(selection);

  if (ObjEditSelection_is_none(selection)) {
    return false; /* nothing selected */
  }

  return is_selected(selection, objects_wrap_coords(pos));
}

void ObjEditSelection_clear(ObjEditSelection *const selection)
{
  validate_selection(selection);

  if (ObjEditSelection_is_none(selection)) {
    return; /* nothing to do */
  }

  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, &selection->max_bounds);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    MapPoint const wrapped_pos = objects_wrap_coords(p);
    if (is_selected(selection, wrapped_pos)) {
      deselect_in_map(selection, wrapped_pos);
      redraw(selection, p);
    }
  }

  selection->num_selected = 0;
  clear_bounds(selection);
  DEBUGF("Cleared selection\n");
  validate_selection(selection);
}

void ObjEditSelection_destroy(ObjEditSelection *const selection)
{
  validate_selection(selection);
  flex_free(&selection->flex);
}

bool ObjEditSelection_for_each_changed(
  ObjEditSelection const *const a, ObjEditSelection const *const b,
  MapArea const *const map_area, void (*const callback)(MapPoint, void *),
  void *const cb_arg)
{
  assert(!map_area || MapArea_is_valid(map_area));
  assert(callback);
  DEBUG("Iterate over changes between selection %p and %p",
        (void *)a, (void *)b);

  if (ObjEditSelection_is_none(a) && ObjEditSelection_is_none(b)) {
    return false;
  }

  if (ObjEditSelection_is_all(a) && ObjEditSelection_is_all(b)) {
    return false;
  }

  MapArea check_bounds = limit_max_bounds(a);
  if (!ObjEditSelection_is_none(b)) {
    MapArea const b_bounds = limit_max_bounds(b);
    MapArea_expand_for_area(&check_bounds, &b_bounds);
  }

  if (map_area) {
    MapArea_intersection(map_area, &check_bounds, &check_bounds);
  }

  MapAreaIter iter;
  bool changed = false;
  for (MapPoint p = MapAreaIter_get_first(&iter, &check_bounds);
       !MapAreaIter_done(&iter);
        p = MapAreaIter_get_next(&iter))
  {
    MapPoint const wrapped_pos = objects_wrap_coords(p);
    if (is_selected(a, wrapped_pos) == is_selected(b, wrapped_pos)) {
      continue;
    }

    DEBUGF("Selection state changed at %" PRIMapCoord ",%" PRIMapCoord "\n",
           p.x, p.y);
    callback(p, cb_arg);
    changed = true;
  }
  return changed;
}

bool ObjEditSelection_for_each(
  ObjEditSelection *const selection,
  void (*const callback)(MapPoint, void *),
  void *const cb_arg)
{
  DEBUG("Iterate over selection %p", (void *)selection);

  if (ObjEditSelection_is_none(selection)) {
    return false;
  }

  ObjEditSelIter iter;
  bool changed = false;
  for (MapPoint p = ObjEditSelIter_get_first(&iter, selection);
       !ObjEditSelIter_done(&iter);
       p = ObjEditSelIter_get_next(&iter))
  {
    DEBUGF("Selected object at %" PRIMapCoord ",%" PRIMapCoord "\n", p.x, p.y);
    callback(p, cb_arg);
    changed = true;
  }
  return changed;
}
