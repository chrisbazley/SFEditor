/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Objects grid and triggers editing functions
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef objectsedit_h
#define objectsedit_h

#include <stdbool.h>
#include "MapCoord.h"
#include "Triggers.h"
#include "Obj.h"

struct ObjGfxMeshes;
struct ObjEditChanges;
struct ObjEditSelection;

typedef enum {
  TriggersWipeAction_None,
  TriggersWipeAction_BreakChain,
  TriggersWipeAction_KeepChain,
} TriggersWipeAction;

/* Is depended on by ObjEditSel. Do not add reverse dependency! */

typedef struct ObjEditContext ObjEditContext;

void ObjectsEdit_crop_overlay(ObjEditContext const *objects,
  struct ObjEditChanges *change_info);

void ObjectsEdit_plot_tri(ObjEditContext *objects, MapPoint vertex_A,
  MapPoint vertex_B, MapPoint vertex_C, ObjRef value,
  struct ObjEditChanges *change_info, struct ObjGfxMeshes *const meshes);

void ObjectsEdit_plot_rect(ObjEditContext *objects, MapPoint vertex_A,
  MapPoint vertex_B, ObjRef value, struct ObjEditChanges *change_info,
  struct ObjGfxMeshes *const meshes);

void ObjectsEdit_plot_circ(ObjEditContext *objects, MapPoint centre,
  MapCoord radius, ObjRef value, struct ObjEditChanges *change_info,
  struct ObjGfxMeshes *meshes);

void ObjectsEdit_plot_line(ObjEditContext *objects, MapPoint start,
  MapPoint end, ObjRef value, MapCoord thickness, struct ObjEditChanges *change_info,
  struct ObjGfxMeshes *meshes);

void ObjectsEdit_global_replace(ObjEditContext *objects,
  ObjRef find, ObjRef replace, struct ObjEditChanges *change_info,
  struct ObjGfxMeshes *meshes);

void ObjectsEdit_flood_fill(ObjEditContext *objects,
  ObjRef replace, MapPoint pos, struct ObjEditChanges *change_info,
  struct ObjGfxMeshes *meshes);

void ObjectsEdit_fill_area(ObjEditContext const *objects,
  MapArea const *area, ObjRef value,
  struct ObjEditChanges *change_info, struct ObjGfxMeshes *meshes);

void ObjectsEdit_write_ref(ObjEditContext const *objects, MapPoint pos,
  ObjRef const ref_num, TriggersWipeAction wipe_action,
  struct ObjEditChanges *change_info, struct ObjGfxMeshes *meshes);

ObjRef ObjectsEdit_read_ref(ObjEditContext const *objects, MapPoint pos);
ObjRef ObjectsEdit_read_base(ObjEditContext const *objects, MapPoint pos);
ObjRef ObjectsEdit_read_overlay(ObjEditContext const *objects, MapPoint pos);

bool ObjectsEdit_check_ref_range(ObjEditContext const *objects, size_t num_refs);

typedef ObjRef ObjectsEditReadFn(void *cb_arg, MapPoint map_pos);

void ObjectsEdit_copy_to_area(ObjEditContext const *objects,
  MapArea const *area, ObjectsEditReadFn *read, void *cb_arg,
  struct ObjEditChanges *change_info, struct ObjGfxMeshes *meshes);

bool ObjectsEdit_can_copy_to_area(ObjEditContext const *objects,
  MapArea const *area, ObjectsEditReadFn *read, void *cb_arg,
  struct ObjGfxMeshes *meshes, struct ObjEditSelection *occluded);

void ObjectsEdit_fill_selected(ObjEditContext const *objects,
  struct ObjEditSelection *selected, ObjRef obj_ref,
  struct ObjEditChanges *change_info,
  struct ObjGfxMeshes *meshes);

void ObjectsEdit_wipe_triggers(ObjEditContext const *objects,
  struct ObjEditSelection *selected, struct ObjEditChanges *change_info);

bool ObjectsEdit_write_ref_n_triggers(ObjEditContext const *objects,
  MapPoint pos, ObjRef ref_num, TriggerFullParam const *fparam, size_t nitems,
  struct ObjEditChanges *change_info, struct ObjGfxMeshes *meshes);

bool ObjectsEdit_add_trigger(ObjEditContext const *objects,
  MapPoint pos, TriggerFullParam fparam,
  struct ObjEditChanges *change_info);

bool ObjectsEdit_can_place(ObjEditContext const *objects, MapPoint grid_pos,
   ObjRef const value, struct ObjGfxMeshes *meshes,
   struct ObjEditSelection *occluded);

#endif
