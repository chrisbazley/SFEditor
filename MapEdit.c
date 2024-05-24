/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map/animations editing mode
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
#include "stdio.h"
#include <assert.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <limits.h>
#include "stdlib.h"
#include <math.h>

#include "Macros.h"
#include "Debug.h"
#include "err.h"
#include "hourglass.h"
#include "msgtrans.h"

#include "Utils.h"
#include "MapEdit.h"
#include "MapAnims.h"
#include "MapCoord.h"
#include "MapEditChg.h"
#include "MapEditSel.h"
#include "Shapes.h"
#include "MapEditCtx.h"
#include "Smooth.h"
#include "Map.h"

/* ---------------- Private functions ---------------- */

static MapData *get_write_map(MapEditContext const *const map)
{
  assert(map != NULL);
  return map->overlay != NULL ? map->overlay : map->base;
}

static MapRef read_overlay_core(MapEditContext const *const map,
  MapPoint const pos)
{
  assert(map != NULL);
  assert(map_coords_in_range(pos));

  MapRef tile = map_ref_mask();
  if (map->overlay != NULL) {
    tile = map_get_tile(map->overlay, pos);
  }
  return tile;
}

static MapRef read_tile_core(MapEditContext const *const map,
  MapPoint const pos)
{
  MapRef tile = read_overlay_core(map, pos);
  if (map_ref_is_mask(tile) && map->base != NULL) {
    tile = map_get_tile(map->base, pos); /* read from base map */
    assert(!map_ref_is_mask(tile));
  }
  return tile;
}

static void write_tile_core(MapData *const map, MapPoint const pos,
  MapRef const tile_num, MapEditChanges *const change_info,
  MapArea *const redraw_area)
{
  assert(map_coords_in_range(pos));

  if (map_ref_is_equal(map_update_tile(map, pos, tile_num), tile_num)) {
    return;
  }

  MapArea_expand(redraw_area, pos);
  MapEditChanges_change_tile(change_info);
}

static void reverse_anim(MapAnimParam *const param)
{
  /* Reverse the order of frames within animation */
  assert(param != NULL);

  for (size_t f = 0; f < AnimsNFrames / 2; f++) {
    const size_t f2 = AnimsNFrames - 1 - f;
    DEBUG("Swapping frame %zu and frame %zu", f, f2);
    MapRef const tmp = param->tiles[f];
    param->tiles[f] = param->tiles[f2];
    param->tiles[f2] = tmp;
  } /* next f */
}

static bool replace_frame(MapAnimParam *const param,
  MapRef const find, MapRef const replace)
{
  assert(param != NULL);

  bool changed = false;
  for (size_t f = 0; f < AnimsNFrames; f++) {
    if (map_ref_is_equal(param->tiles[f], find)) {
      DEBUGF("Replacing frame %zu of animation\n", f);
      param->tiles[f] = replace;
      changed = true;
    }
  }
  return changed;
}

static void wipe_anims(MapEditContext const *const map, MapArea const *map_area,
  MapEditChanges *const change_info)
{
  if (map->anims == NULL) {
    return;
  }

  /* Wipe any animations within a given map area.
     Bounding box coordinates are inclusive */
  DEBUG("Wiping animations from x:%" PRIMapCoord ",%" PRIMapCoord
        " y:%" PRIMapCoord ",%" PRIMapCoord,
        map_area->min.x, map_area->max.x,
        map_area->min.y, map_area->max.y);

  MapAnimParam param;
  MapAnimsIter iter;
  for ((void)MapAnimsIter_get_first(&iter, map->anims, map_area, &param);
       !MapAnimsIter_done(&iter);
       (void)MapAnimsIter_get_next(&iter, &param))
  {
    MapAnimsIter_del_current(&iter);
    MapEditChanges_delete_anim(change_info);
  }
}

static void wipe_anim(MapEditContext const *const map, MapPoint map_pos,
  MapEditChanges *const change_info)
{
  if (map->anims == NULL) {
    return;
  }

  if (MapAnims_check_locn(map->anims, map_pos)) {
    MapArea const map_area = {map_pos, map_pos};
    wipe_anims(map, &map_area, change_info);
  }
}

static void fill_core(MapEditContext const *const map,
  MapArea const *const area, MapRef const tile_num,
  MapEditChanges *const change_info, MapArea *const redraw_area)
{
  assert(map != NULL);
  assert(map->overlay || !map_ref_is_mask(tile_num));
  assert(MapArea_is_valid(area));

  if (map->prechange_cb) {
    map->prechange_cb(area, map->session);
  }

  wipe_anims(map, area, change_info);

  MapData *const gmap = get_write_map(map);
  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, area);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    write_tile_core(gmap, map_wrap_coords(p), tile_num, change_info,
                    redraw_area);
  }
}

static void do_redraw(MapEditContext const *const map, MapArea const *const redraw_area)
{
  assert(map);
  assert(redraw_area);
  DEBUGF("do_redraw %" PRIMapCoord ", %" PRIMapCoord ", %" PRIMapCoord ", %" PRIMapCoord "\n",
         redraw_area->min.x, redraw_area->min.y, redraw_area->max.x, redraw_area->max.y);

  if (MapArea_is_valid(redraw_area) && map->redraw_cb) {
    map->redraw_cb(redraw_area, map->session);
  }
}

/* ---------------- Public functions ---------------- */

void MapEdit_reverse_selected(MapEditContext const *const map,
  MapEditSelection *const selected, MapEditChanges *const change_info)
{
  if (!map->anims) {
    return;
  }

  MapArea bounds;
  if (!MapEditSelection_get_bounds(selected, &bounds)) {
    return; /* nothing selected! */
  }

  MapArea redraw_area = MapArea_make_invalid();

  MapAnimsIter iter;
  MapAnimParam param;
  for (MapPoint p = MapAnimsIter_get_first(&iter, map->anims, &bounds, &param);
       !MapAnimsIter_done(&iter);
       p = MapAnimsIter_get_next(&iter, &param))
  {
    if (MapEditSelection_is_selected(selected, p)) {
      if (map->prechange_cb) {
        map->prechange_cb(&(MapArea){p,p}, map->session);
      }

      reverse_anim(&param);
      MapAnimsIter_replace_current(&iter, param);
      MapArea_expand(&redraw_area, p);
      MapEditChanges_change_anim(change_info);
    }
  }

  do_redraw(map, &redraw_area);
}

void MapEdit_delete_selected(MapEditContext const *const map,
  MapEditSelection *const selected, MapEditChanges *const change_info)
{
  if (!map->anims) {
    return;
  }

  MapArea bounds;
  if (!MapEditSelection_get_bounds(selected, &bounds)) {
    return; /* nothing selected! */
  }

  MapArea redraw_area = MapArea_make_invalid();

  MapAnimsIter iter;
  MapAnimParam param;
  for (MapPoint p = MapAnimsIter_get_first(&iter, map->anims, &bounds, &param);
       !MapAnimsIter_done(&iter);
       p = MapAnimsIter_get_next(&iter, &param))
  {
    if (MapEditSelection_is_selected(selected, p)) {
      if (map->prechange_cb) {
        map->prechange_cb(&(MapArea){p,p}, map->session);
      }

      MapAnimsIter_del_current(&iter);
      MapArea_expand(&redraw_area, p);
      MapEditChanges_delete_anim(change_info);
    }
  }

  do_redraw(map, &redraw_area);
}

void MapEdit_fill_selection(MapEditContext const *const map,
  MapEditSelection *const selected, MapRef const tile,
  MapEditChanges *const change_info)
{
  MapArea redraw_area = MapArea_make_invalid();

  MapEditSelIter iter;
  for (MapPoint p = MapEditSelIter_get_first(&iter, selected);
       !MapEditSelIter_done(&iter);
       p = MapEditSelIter_get_next(&iter))
  {
    wipe_anim(map, p, change_info);
    write_tile_core(get_write_map(map), map_wrap_coords(p), tile,
      change_info, &redraw_area);
  }

  do_redraw(map, &redraw_area);
}

void MapEdit_smooth_selection(MapEditContext const *const map,
  MapEditSelection *const selected, MapTexGroups *const groups_data,
  MapEditChanges *const change_info)
{
  MapEditSelIter iter;
  for (MapPoint p = MapEditSelIter_get_first(&iter, selected);
       !MapEditSelIter_done(&iter);
       p = MapEditSelIter_get_next(&iter))
  {
    MapTexGroups_smooth(map, groups_data, p, change_info);
  }
}

void MapEdit_crop_overlay(MapEditContext const *const map,
  MapEditChanges *const change_info)
{
  /* Removes wastage from ground map overlay
     (tiles equal to those overridden) */
  assert(map != NULL);

  if (map->base == NULL || map->overlay == NULL) {
    return;
  }

  DEBUG("Will crop map overlay");
  MapArea redraw_area = MapArea_make_invalid();

  MapAreaIter iter;
  for (MapPoint p = map_get_first(&iter);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    if (map->anims && MapAnims_check_locn(map->anims, p)) {
      continue;
    }

    MapRef const cur_tile = map_get_tile(map->overlay, p);
    if (!map_ref_is_mask(cur_tile) &&
        map_ref_is_equal(map_get_tile(map->base, p), cur_tile)) {
      DEBUG("Cropping overlay location at %" PRIMapCoord ",%" PRIMapCoord,
        p.x, p.y);

      map_set_tile(map->overlay, p, map_ref_mask());
      MapArea_expand(&redraw_area, p);
      MapEditChanges_change_tile(change_info);
    }
  }

  do_redraw(map, &redraw_area);
}

typedef struct {
  MapEditContext const *map;
  MapRef tile_num;
  MapEditChanges *change_info;
  MapArea redraw_area;
} WriteShapeContext;

static size_t read_shape(MapPoint const pos, void *const arg)
{
  DEBUGF("Read shape pos {%" PRIMapCoord ", %" PRIMapCoord "}\n", pos.x, pos.y);

  WriteShapeContext *const context = arg;
  assert(context != NULL);

  return map_ref_to_num(MapEdit_read_tile(context->map, pos));
}

static void plot_shape(MapArea const *const map_area, void *const arg)
{
  DEBUGF("Write shape area {%" PRIMapCoord ", %" PRIMapCoord
    ", %" PRIMapCoord ", %" PRIMapCoord "}\n",
    map_area->min.x, map_area->min.y, map_area->max.x, map_area->max.y);

  WriteShapeContext *const context = arg;
  assert(context != NULL);
  fill_core(context->map, map_area, context->tile_num, context->change_info, &context->redraw_area);
}

void MapEdit_plot_tri(MapEditContext const *const map,
  MapPoint const vertex_A, MapPoint const vertex_B,
  MapPoint const vertex_C, MapRef const tile,
  MapEditChanges *const change_info)
{
  WriteShapeContext context = {
    .map = map,
    .tile_num = tile,
    .change_info = change_info,
    .redraw_area = MapArea_make_invalid(),
  };
  Shapes_tri(plot_shape, &context, vertex_A, vertex_B, vertex_C);
  do_redraw(map, &context.redraw_area);
}

void MapEdit_plot_rect(MapEditContext const *const map,
  MapPoint const vertex_A, MapPoint const vertex_B, MapRef const tile,
  MapEditChanges *const change_info)
{
  WriteShapeContext context = {
    .map = map,
    .tile_num = tile,
    .change_info = change_info,
    .redraw_area = MapArea_make_invalid(),
  };
  Shapes_rect(plot_shape, &context, vertex_A, vertex_B);
  do_redraw(map, &context.redraw_area);
}

void MapEdit_plot_circ(MapEditContext const *const map,
  MapPoint const centre, MapCoord const radius, MapRef const tile,
  MapEditChanges *const change_info)
{
  WriteShapeContext context = {
    .map = map,
    .tile_num = tile,
    .change_info = change_info,
    .redraw_area = MapArea_make_invalid(),
  };
  Shapes_circ(plot_shape, &context, centre, radius);
  do_redraw(map, &context.redraw_area);
}

void MapEdit_plot_line(MapEditContext const *const map, MapPoint const start,
  MapPoint const end, MapRef const tile, MapCoord const thickness,
  MapEditChanges *const change_info)
{
  WriteShapeContext context = {
    .map = map,
    .tile_num = tile,
    .change_info = change_info,
    .redraw_area = MapArea_make_invalid(),
  };
  Shapes_line(plot_shape, &context, start, end, thickness);
  do_redraw(map, &context.redraw_area);
}

void MapEdit_global_replace(MapEditContext const *const map,
  MapRef const find, MapRef const replace, MapEditChanges *const change_info)
{
  assert(map != NULL);
  assert(map->overlay || !map_ref_is_mask(replace));
  DEBUG("Will globally replace tile %zu with %zu", map_ref_to_num(find),
        map_ref_to_num(replace));

  if (map_ref_is_equal(find, replace)) {
    return;
  }

  MapArea redraw_area = MapArea_make_invalid();
  MapData *const write_map = get_write_map(map);

  MapAreaIter iter;
  for (MapPoint p = map_get_first(&iter);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    MapRef const tile = read_tile_core(map, p);
    if (map_ref_is_equal(tile, find)) {
      write_tile_core(write_map, p, replace, change_info, &redraw_area);
    }
  }

  if (map->anims != NULL) {
    /* Now perform equivalent substitution within animations data */
    MapAnimParam param;
    MapAnimsIter iter;
    MapArea const bounds = {{0,0}, {Map_Size - 1, Map_Size - 1}};
    for (MapPoint p = MapAnimsIter_get_first(&iter, map->anims, &bounds, &param);
         !MapAnimsIter_done(&iter);
         p = MapAnimsIter_get_next(&iter, &param))
    {
      if (replace_frame(&param, find, replace)) {
        MapAnimsIter_replace_current(&iter, param);
        MapArea_expand(&redraw_area, p);
        MapEditChanges_change_anim(change_info);
      }
    }
  }

  do_redraw(map, &redraw_area);
}

void MapEdit_flood_fill(MapEditContext const *const map,
  MapRef const replace,  MapPoint const pos,
  MapEditChanges *const change_info)
{
  DEBUG("Will locally replace with %zu (flood at %" PRIMapCoord ",%" PRIMapCoord ")",
        map_ref_to_num(replace), pos.x, pos.y);

  MapRef const find = MapEdit_read_tile(map, pos);
  if (map_ref_is_equal(find, replace)) {
    return;
  }

  WriteShapeContext context = {
    .map = map,
    .tile_num = replace,
    .change_info = change_info,
    .redraw_area = MapArea_make_invalid(),
  };

  hourglass_on();
  bool const success = Shapes_flood(read_shape, plot_shape, &context,
                                    map_ref_to_num(find), pos, Map_Size);
  hourglass_off();

  do_redraw(map, &context.redraw_area);

  if (!success)
  {
    report_error(SFERROR(NoMem), "", "");
  }
}

void MapEdit_fill_area(MapEditContext const *const map,
  MapArea const *const area, MapRef const tile_num,
  MapEditChanges *const change_info)
{
  MapArea redraw_area = MapArea_make_invalid();

  fill_core(map, area, tile_num, change_info, &redraw_area);

  do_redraw(map, &redraw_area);
}

void MapEdit_copy_to_area(MapEditContext const *const map,
  MapArea const *const area, MapEditReadFn *const read, void *const cb_arg,
  MapEditChanges *const change_info)
{
  assert(map != NULL);
  assert(MapArea_is_valid(area));
  assert(read != NULL);

  if (map->prechange_cb) {
    map->prechange_cb(area, map->session);
  }

  wipe_anims(map, area, change_info);

  MapData *const gmap = get_write_map(map);
  MapArea redraw_area = MapArea_make_invalid();

  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, area);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    MapRef const tile = read(cb_arg, MapPoint_sub(p, area->min));
    assert(map->overlay || !map_ref_is_mask(tile));
    write_tile_core(gmap, map_wrap_coords(p), tile, change_info, &redraw_area);
  }

  do_redraw(map, &redraw_area);
}

void MapEdit_write_tile(MapEditContext const *const map, MapPoint const pos,
  MapRef const tile_num, MapEditChanges *const change_info)
{
  DEBUG_VERBOSE("Putting tile no. %zu at map location %" PRIMapCoord ",%" PRIMapCoord,
    map_ref_to_num(tile_num), pos.x, pos.y);

  assert(map != NULL);
  assert(map->overlay || !map_ref_is_mask(tile_num));

  if (map->prechange_cb) {
    map->prechange_cb(&(MapArea){pos,pos}, map->session);
  }

  wipe_anim(map, pos, change_info);
  MapArea redraw_area = MapArea_make_invalid();

  write_tile_core(get_write_map(map), map_wrap_coords(pos), tile_num,
    change_info, &redraw_area);

  do_redraw(map, &redraw_area);
}

MapRef MapEdit_read_tile(MapEditContext const *const map, MapPoint const pos)
{
  DEBUG_VERBOSE("Reading tile at %" PRIMapCoord ",%" PRIMapCoord,
        pos.x, pos.y);

  return read_tile_core(map, map_wrap_coords(pos));
}

MapRef MapEdit_read_overlay(MapEditContext const *const map, MapPoint const pos)
{
  DEBUG_VERBOSE("Reading overlay at %" PRIMapCoord ",%" PRIMapCoord,
        pos.x, pos.y);

  return read_overlay_core(map, map_wrap_coords(pos));
}

bool MapEdit_write_anim(MapEditContext const *const map,
  MapPoint const map_pos, MapAnimParam const param,
  MapEditChanges *const change_info)
{
  assert(map != NULL);
  wipe_anim(map, map_pos, change_info);

  if (map->anims)
  {
    if (report_error(MapAnims_add(map->anims, get_write_map(map), map_pos, param), "", ""))
    {
      return false;
    }
    MapEditChanges_add_anim(change_info);
  }
  return true;
}

void MapEdit_anims_to_map(MapEditContext const *const map,
  MapEditChanges *const change_info)
{
  /* Ensures that the ground map displays the current state of all animations */
  assert(map != NULL);
  if (!map->anims) {
    return;
  }

  MapArea redraw_area = MapArea_make_invalid();

  MapData *const gmap = get_write_map(map);
  MapArea const bounds = {{0,0}, {Map_Size - 1, Map_Size - 1}};
  MapAnimParam param;
  MapAnimsIter iter;
  for (MapPoint p = MapAnimsIter_get_first(&iter, map->anims, &bounds, &param);
       !MapAnimsIter_done(&iter);
       p = MapAnimsIter_get_next(&iter, &param))
  {
    MapRef const tile_num = MapAnimsIter_get_current(&iter);
    if (!map_ref_is_mask(tile_num) &&
        !map_ref_is_equal(tile_num, read_tile_core(map, p))) {
      write_tile_core(gmap, p, tile_num, change_info, &redraw_area);
    }
  }

  do_redraw(map, &redraw_area);
}

void MapEdit_reset_anims(MapEditContext const *const map)
{
  assert(map != NULL);
  if (map->anims) {
    MapAnims_reset(map->anims);
  }
}

SchedulerTime MapEdit_update_anims(MapEditContext const *const map,
  int const steps_to_advance, struct MapAreaColData *const redraw_map)
{
  assert(map != NULL);
  if (!map->anims)
  {
    return INT_MAX;
  }
  return MapAnims_update(map->anims, get_write_map(map), steps_to_advance, redraw_map);
}

size_t MapEdit_count_anims(MapEditContext const *const map)
{
  assert(map != NULL);
  if (!map->anims)
  {
    return INT_MAX;
  }
  return MapAnims_count(map->anims);
}

bool MapEdit_check_tile_range(MapEditContext const *const map,
  size_t const num_tiles)
{
  /* Returns true if the tiles are all valid */
  MapAreaIter iter;
  for (MapPoint p = map_get_first(&iter);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    if (map->base != NULL) {
      MapRef const map_tile = map_get_tile(map->base, p);
      if (map_ref_to_num(map_tile) >= num_tiles) {
        DEBUG("Base tile %zu at location %" PRIMapCoord ",%" PRIMapCoord
              " not in range 0,%zu", map_ref_to_num(map_tile), p.x, p.y,
              num_tiles - 1);
        return false;
      }
    }

    if (map->overlay != NULL) {
      MapRef const map_tile = map_get_tile(map->overlay, p);
      if (!map_ref_is_mask(map_tile) &&
          map_ref_to_num(map_tile) >= num_tiles) {
        DEBUG("Overlay tile %zu at location %" PRIMapCoord ",%" PRIMapCoord
              " not in range 0,%zu", map_ref_to_num(map_tile), p.x, p.y,
              num_tiles - 1);
        return false;
      }
    }
  }
  return true;
}

