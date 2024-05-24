/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Objects grid and ground_checks editing functions
 *  Copyright (C) 2001 Christopher Bazley
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

#include "stdlib.h"
#include <stdbool.h>
#include <assert.h>
#include <limits.h>
#include "toolbox.h"

#include "err.h"
#include "Macros.h"
#include "msgtrans.h"
#include "hourglass.h"
#include "Debug.h"

#include "Obj.h"
#include "ObjectsEdit.h"
#include "ObjEditCtx.h"
#include "ObjEditChg.h"
#include "utils.h"
#include "MapCoord.h"
#include "ObjGfxMesh.h"
#include "Shapes.h"
#include "Triggers.h"
#include "ObjEditSel.h"

/* Value 0 doesn't work properly yet because:
 - Hard to place objects given that no ghost is drawn when overlapping
 - Moves fail if the overlapped objects are the source objects
 - Snakes rely on overplotting
 */
#define DELETE_OVERLAPPED 1

/* ---------------- Private functions ---------------- */

static MapPoint get_coll_size(ObjGfxMeshes *const meshes, ObjRef const obj_ref)
{
  if (!objects_ref_is_object(obj_ref)) {
    return (MapPoint){0,0};
  }

  if (objects_ref_to_num(obj_ref) >= ObjGfxMeshes_get_ground_count(meshes)) {
    DEBUGF("Bad object reference %zu\n", objects_ref_to_num(obj_ref));
    return (MapPoint){0,0};
  }

  return ObjGfxMeshes_get_collision_size(meshes, obj_ref);
}

static void redraw_trigger2(ObjEditContext const *const objects, ObjRef const obj_ref, MapPoint const pos,
  TriggerFullParam const fparam)
{
  assert(objects);
  if (objects->redraw_trig_cb) {
    objects->redraw_trig_cb(pos, obj_ref, fparam, objects->session);
  }
}

static void redraw_trigger(ObjEditContext const *const objects, MapPoint const pos,
  TriggerFullParam const fparam)
{
  ObjRef const obj_ref = ObjectsEdit_read_ref(objects, pos);
  redraw_trigger2(objects, obj_ref, objects_wrap_coords(pos), fparam);
}

static ObjectsData *get_write_objects(ObjEditContext const *const objects)
{
  assert(objects != NULL);
  return objects->overlay != NULL ? objects->overlay : objects->base;
}

static ObjRef read_base_core(ObjEditContext const *const objects,
  MapPoint const pos)
{
  assert(objects != NULL);
  assert(objects_coords_in_range(pos));

  ObjRef ref = objects_ref_mask();
  if (objects->base != NULL) {
    ref = objects_get_ref(objects->base, pos);
    assert(!objects_ref_is_mask(ref));
  }
  return ref;
}

static ObjRef read_overlay_core(ObjEditContext const *const objects,
  MapPoint const pos)
{
  assert(objects != NULL);
  assert(objects_coords_in_range(pos));

  ObjRef ref = objects_ref_mask();
  if (objects->overlay != NULL) {
    ref = objects_get_ref(objects->overlay, pos);
  }
  return ref;
}

static ObjRef filter_overlay_ref(ObjEditContext const *const objects,
  MapPoint const pos, ObjRef ref)
{
  assert(objects != NULL);
  assert(objects_coords_in_range(pos));
  return objects_ref_is_mask(ref) ? read_base_core(objects, pos) : ref;
}

static ObjRef read_ref_core(ObjEditContext const *const objects,
  MapPoint const pos)
{
  return filter_overlay_ref(objects, pos, read_overlay_core(objects, pos));
}

static bool write_ref_core(ObjEditContext const *const objects, MapPoint const pos,
  ObjRef const ref_num, ObjEditChanges *const change_info)
{
  assert(objects->overlay || !objects_ref_is_mask(ref_num));
  MapPoint const wrapped_pos = objects_wrap_coords(pos);

  /* Read overlay grid (if any) and fallback to base grid if masked.
     We don't actually know that either is currently enabled. */
  read_ref_core(objects, wrapped_pos);

  /* Write to overlay grid (if any) otherwise to base grid, and return the previous ref */
  ObjRef const old_ref = objects_update_ref(get_write_objects(objects), wrapped_pos, ref_num);
  if (objects_ref_is_equal(old_ref, ref_num)) {
    return false;
  }

  ObjEditChanges_change_ref(change_info);

  if (objects->redraw_obj_cb) {
    bool const has_triggers = objects->triggers && triggers_check_locn(objects->triggers, wrapped_pos);
    objects->redraw_obj_cb(wrapped_pos, read_base_core(objects, wrapped_pos), old_ref,
                         ref_num, has_triggers, objects->session);
  }
  return true;
}

static void triggers_wipe_bbox(ObjEditContext const *const objects, MapArea const *const map_area,
  TriggersWipeAction const wipe_action, ObjEditChanges *const change_info)
{
  assert(objects);
  /* Wipe any triggers within a given map area.
     Bounding box coordinates are inclusive */
  DEBUG("Wiping triggers from x:%" PRIMapCoord ",%" PRIMapCoord
        " y:%" PRIMapCoord ",%" PRIMapCoord,
        map_area->min.x, map_area->max.x,
        map_area->min.y, map_area->max.y);

  if (objects->triggers == NULL || wipe_action == TriggersWipeAction_None) {
    return;
  }

  if (wipe_action == TriggersWipeAction_BreakChain) {
    TriggersChainIter chain_iter;
    TriggerFullParam fparam;
    for (MapPoint p = TriggersChainIter_get_first(&chain_iter, objects->triggers, map_area, &fparam);
         !TriggersChainIter_done(&chain_iter);
         p = TriggersChainIter_get_next(&chain_iter, &fparam))
    {
      assert(fparam.param.action == TriggerAction_ChainReaction);

      TriggersChainIter_del_current(&chain_iter);
      ObjEditChanges_delete_trig(change_info);

      redraw_trigger(objects, p, fparam);
    }
  }

  TriggersIter iter;
  TriggerFullParam fparam;
  for (MapPoint p = TriggersIter_get_first(&iter, objects->triggers, map_area, &fparam);
       !TriggersIter_done(&iter);
       p = TriggersIter_get_next(&iter, &fparam))
  {
    assert(fparam.param.action != TriggerAction_Dummy);

    TriggersIter_del_current(&iter);
    ObjEditChanges_delete_trig(change_info);
    redraw_trigger(objects, p, fparam);
  }

  triggers_cleanup(objects->triggers);
}

static void triggers_wipe_locn(ObjEditContext const *const objects, MapPoint const map_pos,
  TriggersWipeAction const wipe_action, ObjEditChanges *const change_info)
{
  triggers_wipe_bbox(objects, &(MapArea){map_pos, map_pos}, wipe_action, change_info);
}

static void clear_overlapped(ObjEditContext const *const objects,
  MapPoint const grid_pos, ObjRef const value,
  ObjEditChanges *const change_info, ObjGfxMeshes *const meshes)
{
  MapPoint const wrapped_pos = objects_wrap_coords(grid_pos);
  ObjRef const new_disp_ref = filter_overlay_ref(objects, wrapped_pos, value);
  if (objects_ref_is_mask(new_disp_ref)) {
    return;
  }

  MapPoint const my_coll_size = get_coll_size(meshes, new_disp_ref);
  MapArea const my_obj_area = {MapPoint_sub(grid_pos, my_coll_size), MapPoint_add(grid_pos, my_coll_size)};
  MapPoint const max_coll_size = ObjGfxMeshes_get_max_collision_size(meshes);

  MapArea const overlapping_area = {
    MapPoint_sub(my_obj_area.min, max_coll_size),
    MapPoint_add(my_obj_area.max, max_coll_size)
  };

  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, &overlapping_area);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    if (MapPoint_compare(objects_wrap_coords(p), wrapped_pos)) {
      continue;
    }

    ObjRef const obj_ref = ObjectsEdit_read_ref(objects, p);
    if (objects_ref_is_none(obj_ref) || objects_ref_is_mask(obj_ref)) {
      continue;
    }

    MapPoint const coll_size = get_coll_size(meshes, obj_ref);
    MapArea const obj_area = {MapPoint_sub(p, coll_size), MapPoint_add(p, coll_size)};
    if (!objects_overlap(&my_obj_area, &obj_area)) {
      continue;
    }

    DEBUGF("Delete object %zu at %" PRIMapCoord ",%" PRIMapCoord
           " (fully occluded by object ref %zu at %" PRIMapCoord ",%" PRIMapCoord ")\n",
           objects_ref_to_num(obj_ref), p.x, p.y,
           objects_ref_to_num(new_disp_ref), grid_pos.x, grid_pos.y);

    if (objects->prechange_cb) {
      objects->prechange_cb(&(MapArea){p, p}, objects->session);
    }

    triggers_wipe_locn(objects, p, TriggersWipeAction_BreakChain, change_info);

    write_ref_core(objects, p, objects_ref_none(), change_info);
  }
}

static void write_ref(ObjEditContext const *const objects,
  MapPoint const grid_pos, ObjRef const value, TriggersWipeAction wipe_action,
  ObjEditChanges *const change_info, ObjGfxMeshes *const meshes)
{
  if (objects_ref_is_none(value)) {
    wipe_action = TriggersWipeAction_BreakChain;
  }

  triggers_wipe_locn(objects, grid_pos, wipe_action, change_info);

  if (write_ref_core(objects, grid_pos, value, change_info) ||
      objects_ref_is_none(value)) {
    clear_overlapped(objects, grid_pos, value, change_info, meshes);
  }
}

/* ---------------- Public functions ---------------- */

void ObjectsEdit_crop_overlay(ObjEditContext const *const objects,
  ObjEditChanges *const change_info)
{
  /* Removes wastage from ground objects overlay
     (refs equal to those overridden) */
  assert(objects != NULL);

  if (objects->base != NULL && objects->overlay != NULL) {
    DEBUG("Will crop objects overlay");
    MapAreaIter iter;
    for (MapPoint p = objects_get_first(&iter);
         !MapAreaIter_done(&iter);
         p = MapAreaIter_get_next(&iter))
    {
      if (objects->triggers && triggers_check_locn(objects->triggers, p)) {
        continue;
      }

      ObjRef const cur_ref = objects_get_ref(objects->overlay, p);
      if (!objects_ref_is_mask(cur_ref) &&
          objects_ref_is_equal(objects_get_ref(objects->base, p), cur_ref)) {
        DEBUG("Cropping overlay location at %" PRIMapCoord ",%" PRIMapCoord, p.x, p.y);
        write_ref_core(objects, p, objects_ref_mask(), change_info);
      }
    }
  }
}

typedef struct {
  ObjEditContext *objects;
  ObjRef obj_ref;
  ObjEditChanges *change_info;
  ObjGfxMeshes *meshes;
} WriteShapeContext;

static size_t read_shape(MapPoint const pos, void *const arg)
{
  DEBUGF("Read shape pos {%" PRIMapCoord ", %" PRIMapCoord "}\n", pos.x, pos.y);

  WriteShapeContext *const context = arg;
  assert(context != NULL);

  return objects_ref_to_num(ObjectsEdit_read_ref(context->objects, pos));
}

static void write_shape(MapArea const *map_area, void *arg)
{
  DEBUGF("Write shape area {%" PRIMapCoord ", %" PRIMapCoord
    ", %" PRIMapCoord ", %" PRIMapCoord "}\n",
    map_area->min.x, map_area->min.y, map_area->max.x, map_area->max.y);

  WriteShapeContext *const context = arg;
  assert(context != NULL);

  ObjectsEdit_fill_area(context->objects, map_area, context->obj_ref,
                        context->change_info, context->meshes);
}

static void write_flood(MapArea const *map_area, void *arg)
{
  DEBUGF("Write flooded area {%" PRIMapCoord ", %" PRIMapCoord
    ", %" PRIMapCoord ", %" PRIMapCoord "}\n",
    map_area->min.x, map_area->min.y, map_area->max.x, map_area->max.y);
  assert(MapArea_is_valid(map_area));

  WriteShapeContext *const context = arg;
  assert(context != NULL);

  if (context->objects->prechange_cb) {
    context->objects->prechange_cb(map_area, context->objects->session);
  }

  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, map_area);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    if (objects_can_place(p)) {
      write_ref(context->objects, p, context->obj_ref, TriggersWipeAction_None,
                context->change_info, context->meshes);
    }
  }
}

void ObjectsEdit_plot_tri(ObjEditContext *const objects, MapPoint const vertex_A,
  MapPoint const vertex_B, MapPoint const vertex_C, ObjRef const value,
  ObjEditChanges *const change_info, ObjGfxMeshes *const meshes)
{
  WriteShapeContext context = {
    .objects = objects,
    .obj_ref = value,
    .change_info = change_info,
    .meshes = meshes,
  };
  Shapes_tri(write_shape, &context, vertex_A, vertex_B, vertex_C);
}

void ObjectsEdit_plot_rect(ObjEditContext *const objects, MapPoint const vertex_A,
  MapPoint const vertex_B, ObjRef const value, ObjEditChanges *const change_info,
  ObjGfxMeshes *const meshes)
{
  WriteShapeContext context = {
    .objects = objects,
    .obj_ref = value,
    .change_info = change_info,
    .meshes = meshes,
  };
  Shapes_rect(write_shape, &context, vertex_A, vertex_B);
}

void ObjectsEdit_plot_line(ObjEditContext *const objects, MapPoint const start,
  MapPoint const end, ObjRef const value, MapCoord const thickness,
  ObjEditChanges *const change_info,
  ObjGfxMeshes *const meshes)
{
  WriteShapeContext context = {
    .objects = objects,
    .obj_ref = value,
    .change_info = change_info,
    .meshes = meshes,
  };
  Shapes_line(write_shape, &context, start, end, thickness);
}

void ObjectsEdit_plot_circ(ObjEditContext *const objects,
  MapPoint const centre, MapCoord const radius, ObjRef const value,
  ObjEditChanges *const change_info, ObjGfxMeshes *const meshes)
{
  WriteShapeContext context = {
    .objects = objects,
    .obj_ref = value,
    .change_info = change_info,
    .meshes = meshes,
  };
  Shapes_circ(write_shape, &context, centre, radius);
}

void ObjectsEdit_global_replace(ObjEditContext *const objects,
  ObjRef const find, ObjRef const replace, ObjEditChanges *const change_info,
  ObjGfxMeshes *const meshes)
{
  assert(objects != NULL);
  assert(objects->overlay || !objects_ref_is_mask(replace));
  DEBUG("Will globally replace object %zu with %zu",
        objects_ref_to_num(find), objects_ref_to_num(replace));

  if (objects_ref_is_equal(find, replace)) {
    return;
  }

  MapAreaIter iter;
  for (MapPoint p = objects_get_first(&iter);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    ObjRef const obj_ref = read_ref_core(objects, p);
    if (objects_ref_is_equal(obj_ref, find) && objects_can_place(p)) {
      write_ref(objects, p, replace, TriggersWipeAction_None, change_info, meshes);
    }
  }
}

void ObjectsEdit_flood_fill(ObjEditContext *const objects,
  ObjRef const replace, MapPoint const pos, ObjEditChanges *const change_info,
  ObjGfxMeshes *const meshes)
{
  DEBUG("Will locally replace with %zu (flood at %" PRIMapCoord ",%" PRIMapCoord ")",
        objects_ref_to_num(replace), pos.x, pos.y);

  ObjRef const find = ObjectsEdit_read_ref(objects, pos);
  if (objects_ref_is_equal(find, replace) || !objects_can_place(pos)) {
    return;
  }

  WriteShapeContext context = {
    .objects = objects,
    .obj_ref = replace,
    .change_info = change_info,
    .meshes = meshes,
  };

  hourglass_on();
  bool const success = Shapes_flood(read_shape, write_flood, &context,
                                    objects_ref_to_num(find), pos, Obj_Size);
  hourglass_off();

  if (!success)
  {
    report_error(SFERROR(NoMem), "", "");
  }
}

void ObjectsEdit_fill_area(ObjEditContext const *const objects,
  MapArea const *const area, ObjRef const value,
  ObjEditChanges *const change_info, ObjGfxMeshes *const meshes)
{
  assert(objects != NULL);
  assert(MapArea_is_valid(area));

  if (objects->prechange_cb) {
    objects->prechange_cb(area, objects->session);
  }

  //triggers_wipe_bbox(objects, area, TriggersWipeAction_BreakChain, change_info);

  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, area);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    if (ObjectsEdit_can_place(objects, p, value, meshes, NULL)) {
      write_ref(objects, p, value, TriggersWipeAction_BreakChain,
                change_info, meshes);
    }
  }
}

void ObjectsEdit_fill_selected(ObjEditContext const *const objects,
  ObjEditSelection *const selected, ObjRef const obj_ref, ObjEditChanges *const change_info,
  ObjGfxMeshes *const meshes)
{
  ObjEditSelIter iter;
  for (MapPoint p = ObjEditSelIter_get_first(&iter, selected);
       !ObjEditSelIter_done(&iter);
       p = ObjEditSelIter_get_next(&iter))
  {
    if (!objects_can_place(p)) {
      continue;
    }

    if (objects->prechange_cb) {
      objects->prechange_cb(&(MapArea){p,p}, objects->session);
    }

    write_ref(objects, p, obj_ref, TriggersWipeAction_None, change_info, meshes);
  }
}

void ObjectsEdit_wipe_triggers(ObjEditContext const *const objects,
  ObjEditSelection *const selected, ObjEditChanges *const change_info)
{
  assert(objects);
  if (!objects->triggers) {
    return;
  }

  MapArea sel_area;
  if (!ObjEditSelection_get_bounds(selected, &sel_area)) {
    return;
  }

  TriggersIter iter;
  TriggerFullParam fparam;
  for (MapPoint p = TriggersIter_get_first(&iter, objects->triggers, &sel_area, &fparam);
       !TriggersIter_done(&iter);
       p = TriggersIter_get_next(&iter, &fparam))
  {
    assert(fparam.param.action != TriggerAction_Dummy);
    if (ObjEditSelection_is_selected(selected, p)) {
      TriggersIter_del_current(&iter);
      ObjEditChanges_delete_trig(change_info);
      redraw_trigger(objects, p, fparam);
    }
  }
}

bool ObjectsEdit_add_trigger(ObjEditContext const *const objects, MapPoint const pos,
  TriggerFullParam const fparam, ObjEditChanges *const change_info)
{
  if (!objects->triggers) {
    return true;
  }

  if (report_error(triggers_add(objects->triggers, pos, fparam), "", "")) {
    return false;
  }

  redraw_trigger(objects, pos, fparam);
  ObjEditChanges_add_trig(change_info);
  return true;
}

bool ObjectsEdit_write_ref_n_triggers(ObjEditContext const *const objects, MapPoint const pos,
  ObjRef const ref_num, TriggerFullParam const *const fparam, size_t const nitems,
  ObjEditChanges *const change_info, ObjGfxMeshes *const meshes)
{
  assert(objects);
  assert(nitems >= 0);

  if (!objects_can_place(pos)) {
    return false;
  }

  bool matching = true;

  if (objects->triggers) {
    bool *const matched = calloc(nitems, sizeof(*matched));
    if (!matched) {
      report_error(SFERROR(NoMem), "", "");
      return false;
    }

    TriggersIter iter;
    TriggerFullParam ex_fparam;
    size_t matched_count = 0;
    for (TriggersIter_get_first(&iter, objects->triggers, &(MapArea){pos, pos}, &ex_fparam);
         !TriggersIter_done(&iter) && matching;
         TriggersIter_get_next(&iter, &ex_fparam)) {
      assert(ex_fparam.param.action != TriggerAction_Dummy);

      /* Search for an existing trigger in the replacement set */
      bool ex_matched = false;
      for (size_t i = 0; i < nitems && !ex_matched; ++i) {
        assert(fparam[i].param.action != TriggerAction_Dummy);

        /* Only allow each trigger in the replacement set to match once */
        if (matched[i]) {
          continue;
        }

        if (fparam[i].param.action == ex_fparam.param.action &&
            fparam[i].param.value == ex_fparam.param.value &&
            MapPoint_compare(fparam[i].next_coords, ex_fparam.next_coords)) {
          matched[i] = true;
          ex_matched = true;
          ++matched_count;
        }
      }

      /* Stop as soon as an existing trigger isn't found in the replacement set */
      if (!ex_matched) {
        DEBUGF("Existing trigger not replaced\n");
        matching = false;
      }
    }

    /* Every member of the replacement set must match with an existing trigger */
    if (matching && matched_count < nitems) {
      DEBUGF("Not all replacement triggers were matched (%zu < %zu)\n", matched_count, nitems);
      matching = false;
    }

    free(matched);
  }

  MapPoint const wrapped_pos = objects_wrap_coords(pos);
  ObjRef const old_ref = read_ref_core(objects, wrapped_pos);
  if (matching && objects_ref_is_equal(old_ref, ref_num)) {
    DEBUGF("Nothing to do\n");
    return true;
  }

  /* We might not be able to replace all dummy triggers at the modified map
     location (if more existing chains target that location than the number
     of triggers to be added) and we might need to add additional dummy
     triggers at other map locations (if adding new chains), so at best
     this is a heuristic. Assume the best case: no new chains and all
     existing triggers replaced. */
  if (objects->triggers) {
    size_t const total_count = triggers_get_count(objects->triggers);
    size_t const free_count = TriggersMax - total_count;
    size_t const max_del_count = triggers_count_locn(objects->triggers, wrapped_pos);
    DEBUGF("Add %zu triggers, currently %zu slots free, may reclaim up to %zu\n",
           nitems, free_count, max_del_count);
    if (nitems > free_count + max_del_count) {
      DEBUGF("Heuristic failed:\n");
      report_error(SFERROR(NumActions), "", "");
      return false;
    }
  }

  if (objects->prechange_cb) {
    objects->prechange_cb(&(MapArea){wrapped_pos,wrapped_pos}, objects->session);
  }


  TriggersWipeAction wipe_action;
  if (objects_ref_is_equal(old_ref, ref_num)) {
    wipe_action = TriggersWipeAction_KeepChain; /* ...to the same object with different triggers */
  } else if (matching) {
    wipe_action = TriggersWipeAction_None; /* A different object with the same triggers */
  } else {
    wipe_action = TriggersWipeAction_KeepChain; /* ...to a different object with different triggers */
  }

  write_ref(objects, wrapped_pos, ref_num, wipe_action, change_info, meshes);

  if (objects->triggers && (wipe_action != TriggersWipeAction_None)) {
    ObjRef const new_disp_ref = filter_overlay_ref(objects, wrapped_pos, ref_num);

    for (size_t i = nitems; i-- > 0; ) {
      if (report_error(triggers_add(objects->triggers, wrapped_pos, fparam[i]), "", "")) {
        triggers_wipe_locn(objects, wrapped_pos, TriggersWipeAction_KeepChain, change_info);
        ObjEditChanges_delete_trig(change_info);
        return false;
      }

      ObjEditChanges_add_trig(change_info);
      redraw_trigger2(objects, new_disp_ref, wrapped_pos, fparam[i]);
    }
  }

  return true;
}

void ObjectsEdit_write_ref(ObjEditContext const *const objects, MapPoint const pos,
  ObjRef const ref_num, TriggersWipeAction const wipe_action,
  ObjEditChanges *const change_info, ObjGfxMeshes *const meshes)
{
  DEBUG("Putting ref no. %zu at objects location %" PRIMapCoord ",%" PRIMapCoord,
    objects_ref_to_num(ref_num), pos.x, pos.y);

  assert(objects != NULL);

  if (!objects_can_place(pos)) {
    return;
  }

  if (objects->prechange_cb) {
    objects->prechange_cb(&(MapArea){pos,pos}, objects->session);
  }

  write_ref(objects, pos, ref_num, wipe_action, change_info, meshes);
}

ObjRef ObjectsEdit_read_ref(ObjEditContext const *const objects, MapPoint const pos)
{
  DEBUG_VERBOSE("Reading ref at %" PRIMapCoord ",%" PRIMapCoord,
        pos.x, pos.y);

  return read_ref_core(objects, objects_wrap_coords(pos));
}

ObjRef ObjectsEdit_read_base(ObjEditContext const *const objects, MapPoint const pos)
{
  DEBUG_VERBOSE("Reading base at %" PRIMapCoord ",%" PRIMapCoord,
        pos.x, pos.y);

  return read_base_core(objects, objects_wrap_coords(pos));
}

ObjRef ObjectsEdit_read_overlay(ObjEditContext const *const objects, MapPoint pos)
{
  DEBUG_VERBOSE("Reading overlay at %" PRIMapCoord ",%" PRIMapCoord,
        pos.x, pos.y);

  return read_overlay_core(objects, objects_wrap_coords(pos));
}

bool ObjectsEdit_check_ref_range(ObjEditContext const *const objects,
  size_t const num_refs)
{
  /* Returns true if the refs are all valid */
  MapAreaIter iter;
  for (MapPoint p = objects_get_first(&iter);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    if (objects->base != NULL) {
      ObjRef const objects_ref = objects_get_ref(objects->base, p);
      if (objects_ref_is_object(objects_ref) &&
          objects_ref_to_num(objects_ref) >= num_refs) {
        DEBUG("Base ref %zu at location %" PRIMapCoord ",%" PRIMapCoord
              " not in range 0,%zu", objects_ref_to_num(objects_ref), p.x, p.y,
              num_refs - 1);
        return false;
      }
    }

    if (objects->overlay != NULL) {
      ObjRef const objects_ref = objects_get_ref(objects->overlay, p);
      if (objects_ref_is_object(objects_ref) &&
          objects_ref_to_num(objects_ref) >= num_refs) {
        DEBUG("Overlay ref %zu at location %" PRIMapCoord ",%" PRIMapCoord
              " not in range 0,%zu", objects_ref_to_num(objects_ref), p.x, p.y,
              num_refs - 1);
        return false;
      }
    }
  }
  return true;
}

void ObjectsEdit_copy_to_area(ObjEditContext const *const objects,
  MapArea const *const area, ObjectsEditReadFn *const read, void *const cb_arg,
  ObjEditChanges *const change_info, ObjGfxMeshes *const meshes)
{
  assert(objects != NULL);
  assert(MapArea_is_valid(area));
  assert(read != NULL);

  if (objects->prechange_cb) {
    objects->prechange_cb(area, objects->session);
  }

  //triggers_wipe_bbox(objects, area, TriggersWipeAction_BreakChain, change_info);

  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, area);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    ObjRef const obj_ref = read(cb_arg, MapPoint_sub(p, area->min));
    if (ObjectsEdit_can_place(objects, p, obj_ref, meshes, NULL)) {
      write_ref(objects, p, obj_ref, TriggersWipeAction_BreakChain,
                change_info, meshes);
    }
  }
}

bool ObjectsEdit_can_place(ObjEditContext const *const objects, MapPoint const grid_pos, ObjRef const value,
  ObjGfxMeshes *const meshes, ObjEditSelection *const occluded)
{
  if (!objects_can_place(grid_pos)) {
    DEBUGF("Can't place object %zu at %" PRIMapCoord ",%" PRIMapCoord " (map limit)\n",
           objects_ref_to_num(value), grid_pos.x, grid_pos.y);
    return false;
  }

#if DELETE_OVERLAPPED
  if (occluded)
#endif
  {
    MapPoint const wrapped_pos = objects_wrap_coords(grid_pos);
    ObjRef const new_disp_ref = filter_overlay_ref(objects, wrapped_pos, value);

    MapPoint const my_coll_size = get_coll_size(meshes, new_disp_ref);
    MapArea const my_obj_area = {MapPoint_sub(grid_pos, my_coll_size), MapPoint_add(grid_pos, my_coll_size)};
    MapPoint const max_coll_size = ObjGfxMeshes_get_max_collision_size(meshes);

    MapArea const overlapping_area = {
      MapPoint_sub(my_obj_area.min, max_coll_size),
      MapPoint_add(my_obj_area.max, max_coll_size)
    };

    MapAreaIter iter;
    for (MapPoint p = MapAreaIter_get_first(&iter, &overlapping_area);
         !MapAreaIter_done(&iter);
         p = MapAreaIter_get_next(&iter))
    {
      ObjRef const obj_ref = ObjectsEdit_read_ref(objects, p);
      if (objects_ref_is_mask(obj_ref) || objects_ref_is_none(obj_ref)) {
        continue;
      }

      MapPoint const coll_size = get_coll_size(meshes, obj_ref);
      MapArea const obj_area = {MapPoint_sub(p, coll_size), MapPoint_add(p, coll_size)};

      if (objects_overlap(&my_obj_area, &obj_area)) {
        DEBUGF("Object %zu at %" PRIMapCoord ",%" PRIMapCoord
               " overlaps object ref %zu at %" PRIMapCoord ",%" PRIMapCoord "\n",
               objects_ref_to_num(value), grid_pos.x, grid_pos.y,
               objects_ref_to_num(obj_ref), p.x, p.y);

#if DELETE_OVERLAPPED
        ObjEditSelection_select(occluded, p);
#else
        // None is allowed to be placed anywhere (for deletion)
        if (objects_ref_is_none(value)) {
          if (occluded) {
            ObjEditSelection_select(occluded, p);
          }
        } else {
          DEBUGF("Can't place object %zu at %" PRIMapCoord ",%" PRIMapCoord " (occupied)\n",
                 objects_ref_to_num(value), grid_pos.x, grid_pos.y);
        }
        return objects_ref_is_none(value);
#endif
      }
    }
  }

  DEBUGF("Can place object %zu at %" PRIMapCoord ",%" PRIMapCoord " (vacant)\n",
         objects_ref_to_num(value), grid_pos.x, grid_pos.y);

  return true;
}

bool ObjectsEdit_can_copy_to_area(ObjEditContext const *const objects,
  MapArea const *const area, ObjectsEditReadFn *const read, void *const cb_arg,
  ObjGfxMeshes *const meshes, ObjEditSelection *const occluded)
{
  assert(objects != NULL);
  assert(MapArea_is_valid(area));
  assert(read != NULL);

  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, area);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    ObjRef const obj_ref = read(cb_arg, MapPoint_sub(p, area->min));
    if (!ObjectsEdit_can_place(objects, p, obj_ref, meshes, occluded)) {
      return false;
    }
  }

  return true;
}
