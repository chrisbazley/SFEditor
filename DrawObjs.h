/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Plot area of the objects grid
 *  Copyright (C) 2021 Christopher Bazley
 */

#ifndef DrawObjs_h
#define DrawObjs_h

#include "SFInit.h"
#include "PalEntry.h"
#include "Triggers.h"
#include "MapCoord.h"
#include "Obj.h"
#include "Hill.h"
#include "ObjGfxMesh.h"

#define COLLISION_BBOX_IS_SELECTION_BBOX 0

typedef ObjRef DrawObjsReadObjFn(void *cb_arg, MapPoint map_pos);

typedef HillType DrawObjsReadHillFn(void *cb_arg, MapPoint map_pos,
  unsigned char (*colours)[Hill_MaxPolygons],
  unsigned char (*heights)[HillCorner_Count]);

struct TriggersData;
struct ObjEditSelection;
struct View;
struct ObjGfxMeshes;
struct CloudColData;

void DrawObjs_to_screen(
  PolyColData const *poly_colours,
  struct HillColData const *hill_colours,
  struct CloudColData const *clouds,
  struct ObjGfxMeshes *meshes,
  struct View const *view,
  MapArea const *scr_area,
  DrawObjsReadObjFn *read_obj,
  DrawObjsReadHillFn *read_hill, void *cb_arg,
  struct TriggersData *triggers,
  struct ObjEditSelection const *selection,
  Vertex const scr_orig,
  bool is_ghost, struct ObjEditSelection const *occluded);

void DrawObjs_unknown_to_screen(
  struct View const *view,
  MapArea const *area,
  Vertex const scr_orig);

MapArea DrawObjs_get_auto_bbox(struct ObjGfxMeshes *meshes,
  struct View const *view,
  ObjRef obj_ref);

MapArea DrawObjs_get_bbox_with_triggers(struct ObjGfxMeshes *meshes,
  struct View const *view,
  ObjRef obj_ref);

MapArea DrawObjs_get_trigger_bbox(struct ObjGfxMeshes *meshes, struct View const *view,
  ObjRef obj_ref, MapPoint const pos,
  TriggerFullParam const fparam);

MapArea DrawObjs_get_bbox(struct ObjGfxMeshes *meshes,
  struct View const *view, ObjRef obj_ref);

MapArea DrawObjs_get_select_bbox(struct ObjGfxMeshes *meshes,
  struct View const *view, ObjRef obj_ref);

void DrawObjs_get_overlapping_draw_area(struct ObjGfxMeshes *meshes,
  struct View const *view,
  MapArea const *map_area, MapArea *overlapping_area);

void DrawObjs_get_overlapping_select_area(struct ObjGfxMeshes *meshes,
  struct View const *view,
  MapArea const *map_area, MapArea *overlapping_area);

bool DrawObjs_touch_select_bbox(
  struct ObjGfxMeshes *meshes, struct View const *view,
  ObjRef obj_ref, MapPoint grid_pos,
  MapArea const *map_area);

bool DrawObjs_in_select_bbox(
  struct ObjGfxMeshes *meshes, struct View const *view,
  ObjRef obj_ref, MapPoint grid_pos,
  MapArea const *map_area);

bool DrawObjs_touch_ghost_bbox(struct ObjGfxMeshes *meshes,
  struct View const *view, bool triggers,
  ObjRef obj_ref, MapPoint grid_pos, MapArea const *map_area);

#endif
