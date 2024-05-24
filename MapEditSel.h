/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map/animations editing mode selection
 *  Copyright (C) 2019 Christopher Bazley
 */

#ifndef MapEditSel_h
#define MapEditSel_h

#include <stdbool.h>

#include "MapCoord.h"
#include "Map.h"
#include "SFError.h"

typedef struct MapEditSelection
{
  void *flex;
  MapArea max_bounds;
  size_t num_selected;
  bool max_bounds_are_min;
  void (*redraw_cb)(MapArea const *, void *);
  void *redraw_arg;
}
MapEditSelection;

SFError MapEditSelection_init(MapEditSelection *selection,
  void (*redraw_cb)(MapArea const *, void *), void *redraw_arg);

bool MapEditSelection_get_bounds(
  MapEditSelection *selection, MapArea *bounds);

typedef struct
{
  MapAreaIter area_iter;
  MapEditSelection *selection;
  size_t remaining;
  bool done;
}
MapEditSelIter;

MapPoint MapEditSelIter_get_first(MapEditSelIter *iter,
  MapEditSelection *selection);

MapPoint MapEditSelIter_get_next(MapEditSelIter *iter);

static inline bool MapEditSelIter_done(MapEditSelIter *iter)
{
  assert(iter);
  assert(!iter->done || iter->remaining == 0);
  return iter->done;
}

void MapEditSelection_select(MapEditSelection *selection,
  MapPoint pos);

void MapEditSelection_deselect(MapEditSelection *selection, MapPoint pos);

void MapEditSelection_invert(MapEditSelection *selection, MapPoint pos);

void MapEditSelection_select_area(MapEditSelection *selection,
  MapArea const *map_area);

void MapEditSelection_deselect_area(MapEditSelection *selection,
  MapArea const *map_area);

void MapEditSelection_invert_area(MapEditSelection *selection,
  MapArea const *map_area, bool do_redraw);

bool MapEditSelection_is_selected(
  MapEditSelection const *selection, MapPoint pos);

static inline bool MapEditSelection_is_none(
  MapEditSelection const *const selection)
{
  assert(selection != NULL);
  return selection->num_selected == 0;
}

static inline bool MapEditSelection_is_all(
  MapEditSelection const *const selection)
{
  assert(selection != NULL);
  return selection->num_selected == Map_Area;
}

static inline size_t MapEditSelection_size(
  MapEditSelection const *const selection)
{
  assert(selection != NULL);
  return selection->num_selected;
}

void MapEditSelection_clear(MapEditSelection *selection);

void MapEditSelection_select_all(MapEditSelection *selection);

void MapEditSelection_select_tri(MapEditSelection *selection,
  MapPoint vertex_A, MapPoint vertex_B,
  MapPoint vertex_C);

void MapEditSelection_invert_rect(MapEditSelection *selection,
  MapPoint vertex_A, MapPoint vertex_B, bool do_redraw);

void MapEditSelection_select_rect(MapEditSelection *selection,
  MapPoint vertex_A, MapPoint vertex_B);

void MapEditSelection_select_circ(MapEditSelection *selection,
  MapPoint centre, MapCoord radius);

void MapEditSelection_select_line(MapEditSelection *selection,
  MapPoint start, MapPoint end, MapCoord thickness);

void MapEditSelection_destroy(MapEditSelection *selection);

#endif
