/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Plot strategic target information
 *  Copyright (C) 2022 Christopher Bazley
 */

#ifndef DrawInfos_h
#define DrawInfos_h

#include "MapCoord.h"

#define COLLISION_BBOX_IS_SELECTION_BBOX 0

typedef ObjRef DrawInfosReadObjFn(void *cb_arg, MapPoint map_pos);

struct SelectionBitmask;
struct View;
struct TargetInfosData;

void DrawInfos_unknown_to_screen(
  struct View const *view, MapArea const *scr_area, Vertex scr_orig);

typedef size_t DrawInfosReadInfoFn(void *cb_arg, MapPoint *map_pos, int *id);

void DrawInfos_to_screen(
  struct View const *view,
  DrawInfosReadInfoFn *read_info, void *cb_arg,
  struct SelectionBitmask const *selection,
  Vertex scr_orig,
  bool is_ghost, struct SelectionBitmask const *occluded);

MapArea DrawInfos_get_select_bbox(struct View const *view);
MapArea DrawInfos_get_bbox(struct View const *view);

bool DrawInfos_touch_ghost_bbox(struct View const *view,
  MapPoint grid_pos, MapArea const *fine_area);

bool DrawInfos_touch_select_bbox(
  struct View const *view, MapPoint grid_pos,
  MapArea const *fine_area);

bool DrawInfos_in_select_bbox(
  struct View const *view, MapPoint grid_pos,
  MapArea const *fine_area);

void DrawInfos_get_select_area(struct View const *view,
  MapArea const *fine_area, MapArea *overlapping_area);

void DrawInfos_get_overlapping_draw_area(struct View const *view,
  MapArea const *fine_area, MapArea *overlapping_area);

#endif
