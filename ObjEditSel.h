/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Objects editing mode selection
 *  Copyright (C) 2021 Christopher Bazley
 */

#ifndef ObjEditSel_h
#define ObjEditSel_h

#include <stdbool.h>

#include "MapCoord.h"
#include "Obj.h"
#include "SFError.h"

typedef struct ObjEditSelection
{
  void *flex;
  MapArea max_bounds;
  size_t num_selected;
  void (*redraw_cb)(MapPoint, void *);
  void *redraw_arg;
}
ObjEditSelection;

typedef struct
{
  MapAreaIter area_iter;
  ObjEditSelection *selection;
  size_t remaining;
  bool done;
}
ObjEditSelIter;

SFError ObjEditSelection_init(ObjEditSelection *selection,
  void (*redraw_cb)(MapPoint, void *), void *redraw_arg);

void ObjEditSelection_copy(ObjEditSelection *dst, ObjEditSelection *src);

bool ObjEditSelection_get_bounds(
  ObjEditSelection *selection, MapArea *bounds);

MapPoint ObjEditSelIter_get_first(ObjEditSelIter *iter,
  ObjEditSelection *selection);

MapPoint ObjEditSelIter_get_next(ObjEditSelIter *iter);

static inline bool ObjEditSelIter_done(ObjEditSelIter *iter)
{
  assert(iter);
  assert(!iter->done || iter->remaining == 0);
  return iter->done;
}

void ObjEditSelection_select(ObjEditSelection *selection,
  MapPoint pos);

void ObjEditSelection_deselect(ObjEditSelection *selection,
  MapPoint pos);

void ObjEditSelection_invert(ObjEditSelection *selection,
  MapPoint pos, bool do_redraw);

void ObjEditSelection_select_area(ObjEditSelection *selection,
  MapArea const *map_area);

void ObjEditSelection_deselect_area(ObjEditSelection *selection,
  MapArea const *map_area);

bool ObjEditSelection_is_selected(
  ObjEditSelection const *selection, MapPoint pos);

static inline bool ObjEditSelection_is_none(
  ObjEditSelection const *const selection)
{
  assert(selection != NULL);
  return selection->num_selected == 0;
}

static inline bool ObjEditSelection_is_all(
  ObjEditSelection const *const selection)
{
  assert(selection != NULL);
  return selection->num_selected == Obj_Area;
}

static inline size_t ObjEditSelection_size(
  ObjEditSelection const *const selection)
{
  assert(selection != NULL);
  return selection->num_selected;
}

void ObjEditSelection_clear(ObjEditSelection *selection);

void ObjEditSelection_destroy(ObjEditSelection *selection);

bool ObjEditSelection_for_each_changed(ObjEditSelection const *a,
  ObjEditSelection const *b,
  MapArea const *map_area,
  void (*callback)(MapPoint, void *), void *cb_arg);

bool ObjEditSelection_for_each(
  ObjEditSelection *selection,
  void (*callback)(MapPoint, void *), void *cb_arg);

#endif
