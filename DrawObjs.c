/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Plot area of the objects grid
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

#include <math.h>

#include "Macros.h"
#include "err.h"
#include "msgtrans.h"
#include "debug.h"
#include "PalEntry.h"

#include "View.h"
#include "EditWin.h"
#include "MapCoord.h"
#include "SFInit.h"
#include "DrawCloud.h"
#include "DrawTrig.h"
#include "ObjGfxMesh.h"
#include "plot.h"
#include "DrawObjs.h"
#include "ObjEditSel.h"
#include "Vertex.h"
#include "Obj.h"
#include "MapTexBitm.h"
#include "Map.h"
#include "Triggers.h"
#include "Desktop.h"
#include "Hill.h"
#include "Session.h"
#include "ObjGfxData.h"
#include "ObjLayout.h"

enum {
  PaletteIndexWhite = 255,
  PaletteIndexBlack = 0,
  HillColour = 0, // FIXME: invisible against some backgrounds
  UnknownColour = 23,
  DashLen = (MapTexSize << TexelToOSCoordLog2) / 4,
  CameraDistance = 65536 * 4,
  HalveFactorLog2 = 1,
  OverlapSizeOS = 6, /* slightly generous */
  MinScaleTriggerZoom = 0, // multiple rows of icons beyond this (too big)
  MaxDrawTriggerZoom = 1, // icons not drawn at all beyond this (too small)
  MaxDrawObjZoom = 2, // non-hills not drawn at all beyond this (too small)
  DrawCloudSizeOSAtZoom0Log2 = MapTexSizeLog2 + Map_SizeLog2 - Obj_SizeLog2 + TexelToOSCoordLog2, /* OS units per objects grid size */
  DrawCloudSizeOSAtZoom0 = (1 << DrawCloudSizeOSAtZoom0Log2) + OverlapSizeOS, /* 35 px at 8x magnification with ex1,ey1 */
  ArrowLen = MAP_COORDS_LIMIT / (Obj_Size * 2),
};

static inline int calc_grid_size_log2(int const zoom)
{
  int const grid_size_log2 = MapTexSizeLog2 + TexelToOSCoordLog2 + Map_SizeLog2 - Obj_SizeLog2 - zoom;
  DEBUG("Grid size for zoom %d = pow(2,%d)", zoom, grid_size_log2);
  return grid_size_log2;
}

static int get_cloud_zoom(void)
{
  /* Determine the zoom adjustment required to make the sprite fit the objects grid.
     When plotted at zoom 0, the sprite should ideally have a width and height of
     DrawCloudSizeAtZoom0 pixels. */
  static int zoom = INT_MIN;
  if (zoom == INT_MIN) {
    Vertex spr_size_os = DrawCloud_get_size_os();
    zoom = 0;
    DEBUGF("Sprite size is %d,%d at zoom %d\n", spr_size_os.x, spr_size_os.y, zoom);
    if (spr_size_os.x > DrawCloudSizeOSAtZoom0 ||
        spr_size_os.y > DrawCloudSizeOSAtZoom0) {
      while (spr_size_os.x > DrawCloudSizeOSAtZoom0 ||
             spr_size_os.y > DrawCloudSizeOSAtZoom0)
      {
        spr_size_os = Vertex_div_log2(spr_size_os, HalveFactorLog2);
        ++zoom; // bigger zoom means smaller sprite
        DEBUGF("Shrunk sprite size to %d,%d at zoom %d\n", spr_size_os.x, spr_size_os.y, zoom);
      }
    } else {
      while (spr_size_os.x <= (DrawCloudSizeOSAtZoom0 / 2) ||
             spr_size_os.y <= (DrawCloudSizeOSAtZoom0 / 2))
      {
        spr_size_os = Vertex_mul_log2(spr_size_os, HalveFactorLog2);
        --zoom; // smaller zoom means bigger sprite
        DEBUGF("Grew sprite size to %d,%d at zoom %d\n", spr_size_os.x, spr_size_os.y, zoom);
      }
    }
  }
  return zoom;
}

static MapPoint get_cloud_size(void)
{
  static MapPoint cloud_size;
  static bool have_size;
  if (!have_size) {
    have_size = true;

    Vertex const sprite_size_in_os = DrawCloud_get_size_os();
    int const cloud_zoom = get_cloud_zoom();
    int const fine_unit_per_os_log2 = MAP_COORDS_LIMIT_LOG2 - MapTexSizeLog2 - Map_SizeLog2 - TexelToOSCoordLog2 - cloud_zoom;
    DEBUGF("fine_unit_per_os_log2 %d\n", fine_unit_per_os_log2);

    /* Scale the cloud sprite's size to map coordinates.
       A bit arbitrary because these sprites are drawn with whatever zoom we request. */
    cloud_size = MapPoint_mul_log2(MapPoint_from_vertex(sprite_size_in_os), fine_unit_per_os_log2);
    DEBUGF("cloud_size B %" PRIMapCoord ",%" PRIMapCoord "\n", cloud_size.x, cloud_size.y);
  }
  return cloud_size;
}

static MapPoint get_trig_size(void)
{
  static MapPoint trigger_size;
  static bool have_size;
  if (!have_size) {
    have_size = true;

    Vertex const sprite_size_in_os = DrawTrig_get_max_size_os();
    int const fine_unit_per_os_log2 = MAP_COORDS_LIMIT_LOG2 - MapTexSizeLog2 - Map_SizeLog2 - TexelToOSCoordLog2;
    DEBUGF("fine_unit_per_os_log2 %d\n", fine_unit_per_os_log2);

    /* Scale the trigger sprites' (maximum) size to map coordinates.
       A bit arbitrary because these sprites are drawn with whatever zoom we request. */
    trigger_size = MapPoint_mul_log2(MapPoint_from_vertex(sprite_size_in_os), fine_unit_per_os_log2);
    DEBUGF("trigger_size B %" PRIMapCoord ",%" PRIMapCoord "\n", trigger_size.x, trigger_size.y);
  }
  return trigger_size;
}

static MapArea get_mesh_bbox(ObjGfxMeshes *const meshes, View const *const view, ObjRef const obj_ref)
{
  assert(view);
  MapArea bbox = {{0}};

  if (objects_ref_is_none(obj_ref)) {
    // FIXME: 3D?
    static MapPoint const half_size = {MAP_COORDS_LIMIT / (Obj_Size * 2), MAP_COORDS_LIMIT / (Obj_Size * 2)};
    bbox = (MapArea){{-half_size.x, -half_size.y}, {half_size.x, half_size.y}};
  } else if (objects_ref_is_object(obj_ref) &&
             objects_ref_to_num(obj_ref) < ObjGfxMeshes_get_ground_count(meshes)) {
    bbox = ObjGfxMeshes_get_ground_bbox(meshes, obj_ref, view->config.angle);
  } else if (objects_ref_is_cloud(obj_ref)) {
    MapPoint const cloud_size = get_cloud_size();
    MapPoint const half_size = MapPoint_div_log2(cloud_size, HalveFactorLog2);
    DEBUGF("half_size %" PRIMapCoord ",%" PRIMapCoord "\n", half_size.x, half_size.y);

    /* Centre the bounding box in the objects grid location */
    bbox = (MapArea){
      .min = {-half_size.x, -half_size.y},
      .max = {-half_size.x + cloud_size.x, -half_size.y + cloud_size.y}
    };
    DEBUGF("cloud_bbox B %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord "\n",
           bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y);
  } else {
    DEBUGF("%zu is not a polygonal object or bad object reference\n", objects_ref_to_num(obj_ref));
  }

  /* Some views might have a number centred on the grid location; also used for bad refs. */
  static MapPoint const quarter_size = {MAP_COORDS_LIMIT / (Obj_Size * 4), MAP_COORDS_LIMIT / (Obj_Size * 4)};
  MapArea const numbers_area = {{-quarter_size.x, -quarter_size.y}, {quarter_size.x, quarter_size.y}};
  MapArea_expand_for_area(&bbox, &numbers_area);

  return bbox;
}

static void get_fine_collision_coords(View const *const view, MapPoint const coll_size,
  MapPoint (*const coords)[4])
{
  MapPoint const bbox_size = MapPoint_add((MapPoint){1,1}, MapPoint_mul_log2(coll_size, 1));
  MapPoint const centre = ObjLayout_map_coords_to_centre(view, coll_size);

  assert(coords);
  size_t n = 0;
  (*coords)[n++] = MapPoint_sub(ObjLayout_map_coords_to_fine(view, (MapPoint){0, 0}), centre);
  (*coords)[n++] = MapPoint_sub(ObjLayout_map_coords_to_fine(view, (MapPoint){0, bbox_size.y}), centre);
  (*coords)[n++] = MapPoint_sub(ObjLayout_map_coords_to_fine(view, (MapPoint){bbox_size.x, bbox_size.y}), centre);
  (*coords)[n++] = MapPoint_sub(ObjLayout_map_coords_to_fine(view, (MapPoint){bbox_size.x, 0}), centre);
}

static void get_fine_collision_coords_for_obj(ObjGfxMeshes *const meshes, View const *const view, ObjRef const obj_ref,
  MapPoint (*const coords)[4])
{
  MapPoint coll_size = {0,0};

  if (objects_ref_is_object(obj_ref) &&
      objects_ref_to_num(obj_ref) < ObjGfxMeshes_get_ground_count(meshes)) {
    coll_size = ObjGfxMeshes_get_collision_size(meshes, obj_ref);
  }

  get_fine_collision_coords(view, coll_size, coords);
}

static MapArea get_collision_bbox(ObjGfxMeshes *const meshes, View const *const view, ObjRef const obj_ref)
{
  MapPoint coords[4];
  get_fine_collision_coords_for_obj(meshes, view, obj_ref, &coords);

  MapArea bbox = {{0}};
  for (size_t n = 0; n < ARRAY_SIZE(coords); ++n) {
    MapArea_expand(&bbox, coords[n]);
  }
  return bbox;
}

static MapArea get_max_collision_bbox(ObjGfxMeshes *const meshes, View const *const view)
{
  MapPoint coords[4];
  get_fine_collision_coords(view, ObjGfxMeshes_get_max_collision_size(meshes), &coords);

  MapArea bbox = {{0}};
  for (size_t n = 0; n < ARRAY_SIZE(coords); ++n) {
    MapArea_expand(&bbox, coords[n]);
  }
  return bbox;
}

MapArea DrawObjs_get_bbox(ObjGfxMeshes *const meshes, View const *const view, ObjRef const obj_ref)
{
  /* Some objects have vertices outside their collision box and
     we have no idea whether a given object currently has a visible collision box. */
  MapArea bbox = get_collision_bbox(meshes, view, obj_ref);
  MapArea const mesh_bbox = get_mesh_bbox(meshes, view, obj_ref);
  MapArea_expand_for_area(&bbox, &mesh_bbox);
  return bbox;
}

MapArea DrawObjs_get_select_bbox(ObjGfxMeshes *const meshes, View const *const view, ObjRef const obj_ref)
{
#if COLLISION_BBOX_IS_SELECTION_BBOX
  return get_collision_bbox(meshes, view, obj_ref);
#else
  return get_mesh_bbox(meshes, view, obj_ref);
#endif
}

static void expand_for_triggers(MapArea *const bbox)
{
  MapPoint const trig_size = get_trig_size();
  bbox->min.y -= trig_size.y;
  bbox->max.x = HIGHEST(bbox->max.x, bbox->min.x + trig_size.x);
  DEBUGF("%s %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord "\n",
         __func__, bbox->min.x, bbox->min.y, bbox->max.x, bbox->max.y);
}

MapArea DrawObjs_get_bbox_with_triggers(ObjGfxMeshes *const meshes, View const *const view, ObjRef const obj_ref)
{
  MapArea bbox = get_mesh_bbox(meshes, view, obj_ref);
  expand_for_triggers(&bbox);

  /* Some objects have vertices outside their collision box and
     we have no idea whether a given object currently has a visible collision box. */
  MapArea const coll_bbox = get_collision_bbox(meshes, view, obj_ref);
  MapArea_expand_for_area(&bbox, &coll_bbox);
  return bbox;
}

MapArea DrawObjs_get_auto_bbox(ObjGfxMeshes *const meshes, View const *const view, ObjRef const obj_ref)
{
  if (objects_ref_is_defence(obj_ref)) {
    return DrawObjs_get_bbox_with_triggers(meshes, view, obj_ref);
  } else {
    return DrawObjs_get_bbox(meshes, view, obj_ref);
  }
}

static void get_arrow_ends(MapPoint const line_vec, MapCoord const arrow_len,
                           MapPoint *const a, MapPoint *const b)
{
  assert(a);
  assert(b);
  double const angle = atan2(line_vec.y, line_vec.x);
  MapPoint const arrow_vec = {cos(angle) * arrow_len, sin(angle) * arrow_len};
  MapPoint const arrow_base = MapPoint_sub(line_vec, arrow_vec);
  *a = MapPoint_add(arrow_base, (MapPoint){-arrow_vec.y / 2, arrow_vec.x / 2});
  *b = MapPoint_add(arrow_base, (MapPoint){arrow_vec.y / 2, -arrow_vec.x / 2});
}

MapArea DrawObjs_get_trigger_bbox(ObjGfxMeshes *meshes, View const *const view, ObjRef const obj_ref, MapPoint const pos,
  TriggerFullParam const fparam)
{
  MapArea bbox = get_mesh_bbox(meshes, view, obj_ref);

  MapPoint const trig_size = get_trig_size();
  bbox.max.y = bbox.min.y - 1;
  bbox.min.y -= trig_size.y;
  bbox.max.x = HIGHEST(bbox.max.x, bbox.min.x + trig_size.x);

  /* Some views might have a number centred on the grid location which is underlined for triggers. */
  static MapPoint const quarter_size = {MAP_COORDS_LIMIT / (Obj_Size * 4), MAP_COORDS_LIMIT / (Obj_Size * 4)};
  MapArea const numbers_area = {{-quarter_size.x, -quarter_size.y}, {quarter_size.x, quarter_size.y}};
  MapArea_expand_for_area(&bbox, &numbers_area);

  if (fparam.param.action == TriggerAction_ChainReaction) {
    MapPoint const start = ObjLayout_map_coords_to_centre(view, pos);
    MapPoint const end = ObjLayout_map_coords_to_centre(view, fparam.next_coords);
    MapPoint const line_vec = MapPoint_sub(end, start);

    MapArea_expand(&bbox, (MapPoint){0,0});
    MapArea_expand(&bbox, line_vec);

    MapPoint arrow_a, arrow_b;
    get_arrow_ends(line_vec, ArrowLen, &arrow_a, &arrow_b);
    MapArea_expand(&bbox, arrow_a);
    MapArea_expand(&bbox, arrow_b);
  }

  return bbox;
}

void DrawObjs_unknown_to_screen(
  View const *const view,
  MapArea const *const scr_area,
  Vertex const scr_orig)
{
  assert(MapArea_is_valid(scr_area));

  DEBUGF("Plot unknown objects for grid %" PRIMapCoord ", %" PRIMapCoord
         ", %" PRIMapCoord ", %" PRIMapCoord "\n", scr_area->min.x, scr_area->min.y,
         scr_area->max.x, scr_area->max.y);

  MapPoint scr_grid_pos = {.y = scr_area->min.y};
  int const grid_size_log2 = calc_grid_size_log2(view->config.zoom_factor);
  int const grid_size = 1 << grid_size_log2;
  Vertex const offset_orig = Vertex_add(scr_orig, (Vertex){grid_size / 2, grid_size / 2});

  for (Vertex screen_pos = {.y = offset_orig.y + (scr_grid_pos.y * grid_size)};
       scr_grid_pos.y <= scr_area->max.y;
       scr_grid_pos.y++) {
    scr_grid_pos.x = scr_area->min.x;
    screen_pos.x = offset_orig.x + (scr_grid_pos.x * grid_size);

    for (; scr_grid_pos.x <= scr_area->max.x; scr_grid_pos.x++) {
      ObjGfxMeshes_plot_unknown(&view->plot_ctx, screen_pos, CameraDistance, (Vertex3D){0, 0, 0});
      screen_pos.x += grid_size;
    } /* next scr_grid_pos.x */

    screen_pos.y += grid_size;
  } /* next scr_grid_pos.y */
}

void DrawObjs_to_screen(
  PolyColData const *const poly_colours,
  HillColData const *const hill_colours,
  CloudColData const *const clouds,
  ObjGfxMeshes *const meshes,
  View const *view,
  MapArea const *const scr_area,
  DrawObjsReadObjFn *const read_obj,
  DrawObjsReadHillFn *const read_hill, void *const cb_arg,
  TriggersData *const triggers,
  ObjEditSelection const *const selection,
  Vertex const scr_orig,
  bool const is_ghost, ObjEditSelection const *const occluded)
{
  assert(read_obj != NULL);
  assert(read_hill != NULL);
  assert(MapArea_is_valid(scr_area));

  DEBUGF("Plot objects for grid %" PRIMapCoord ", %" PRIMapCoord
         ", %" PRIMapCoord ", %" PRIMapCoord "\n", scr_area->min.x, scr_area->min.y,
         scr_area->max.x, scr_area->max.y);

  int const zoom = view->config.zoom_factor;
  int const grid_size_log2 = calc_grid_size_log2(zoom);
  int const grid_size = 1 << grid_size_log2;
  Vertex const offset_orig = Vertex_add(scr_orig, (Vertex){grid_size / 2, grid_size / 2});

  struct {
    bool cloud:1;
    bool trigger:1;
    bool defence:1;
    bool hill:1;
  } found = {false, false, false, false};

  static Vertex3D const world_pos = {0, 0, 0};

  if (is_ghost) {
    plot_set_col(view->config.ghost_colour);
  }

  Vertex screen_pos = {.y = offset_orig.y + SIGNED_L_SHIFT(scr_area->min.y, grid_size_log2)};

  for (MapPoint scr_grid_pos = {.y = scr_area->min.y};
       scr_grid_pos.y <= scr_area->max.y;
       scr_grid_pos.y++, screen_pos.y += grid_size)
  {
    scr_grid_pos.x = scr_area->min.x;
    screen_pos.x = offset_orig.x + SIGNED_L_SHIFT(scr_grid_pos.x, grid_size_log2);

    for (; scr_grid_pos.x <= scr_area->max.x;
         scr_grid_pos.x++, screen_pos.x += grid_size)
    {
      MapPoint const map_pos = ObjLayout_derotate_scr_coords_to_map(view->config.angle, scr_grid_pos);

      bool const is_selected = selection && ObjEditSelection_is_selected(selection, map_pos);

      if ((map_pos.x % Hill_ObjPerHill) == 0 && (map_pos.y % Hill_ObjPerHill) == 0)
      {
        unsigned char colours[Hill_MaxPolygons];
        unsigned char heights[HillCorner_Count];
        HillType const hill_type = read_hill(cb_arg,
                                             MapPoint_div_log2(map_pos, Hill_ObjPerHillLog2),
                                             &colours, &heights);

        if (hill_type != HillType_None && hill_colours)
        {
          /* Hills are drawn relative to the centre of a grid square, like any other object.
             Unlike most other objects (which are centred), the origin of a hill ('o') could
             be one corner or even entirely outside a one-polygon hill:
           .   .   .   .
             B_______C
           . |\.   . | .
             |  \    |
           . | .  \. | .
             o______\|
           . A .   . D .
           (Even if points A, B and D have height 0, triangle BCD is plotted relative to 'o'.)
          */
          ObjGfxMeshes_plot_poly_hill(&view->plot_ctx, hill_colours, hill_type, &colours, &heights,
            screen_pos, CameraDistance, world_pos, palette, NULL,
            is_ghost ? ObjGfxMeshStyle_Wireframe : ObjGfxMeshStyle_Filled);
        }
        DEBUGF("DrawObjs read hill type %d at %" PRIMapCoord ",%" PRIMapCoord "\n",
               hill_type, map_pos.x, map_pos.y);
      }

      if (zoom > MaxDrawObjZoom) {
        continue;
      }

      ObjRef const obj_ref = read_obj(cb_arg, map_pos);
      DEBUGF("DrawObjs read object type %zu at %" PRIMapCoord ",%" PRIMapCoord "\n",
             objects_ref_to_num(obj_ref), map_pos.x, map_pos.y);

      if (objects_ref_is_none(obj_ref))
      {
      }
      else if (objects_ref_is_object(obj_ref))
      {
        /* Check for bad object references */
        if (objects_ref_to_num(obj_ref) < ObjGfxMeshes_get_ground_count(meshes)) {
          ObjGfxMeshes_plot(meshes, &view->plot_ctx, poly_colours, obj_ref,
                            screen_pos, CameraDistance, world_pos,
                            is_selected ? &view->sel_palette : palette, NULL,
                            is_ghost ? ObjGfxMeshStyle_Wireframe : ObjGfxMeshStyle_Filled);
        } else {
          DEBUGF("Bad object reference %zu at %" PRIMapCoord ",%" PRIMapCoord "\n",
                 objects_ref_to_num(obj_ref), map_pos.x, map_pos.y);

          if (!is_ghost) {
            plot_set_col(is_selected ? view->sel_palette[UnknownColour] : (*palette)[UnknownColour]);
          }
          ObjGfxMeshes_plot_unknown(&view->plot_ctx, screen_pos, CameraDistance, world_pos);
        }
      }
      else if (objects_ref_is_hill(obj_ref))
      {
        found.hill = true;
      }
      else if (objects_ref_is_cloud(obj_ref))
      {
        found.cloud = true;
      }

      if (!is_ghost)
      {
        found.defence = found.defence || objects_ref_is_defence(obj_ref);
      }

      if (triggers)
      {
        found.trigger = found.trigger || triggers_check_locn(triggers, map_pos);
      }
    } /* next scr_grid_pos.x */
  } /* next scr_grid_pos.y */

  if (zoom > MaxDrawObjZoom) {
    return;
  }

  DEBUGF("Found %s %s %s %s\n", found.cloud ? "cloud" : "",
         found.trigger ? "trigger" : "", found.defence ? "defence" : "",
         found.hill ? "hill" : "");

  if ((selection && !ObjEditSelection_is_none(selection)) ||
      (occluded && !ObjEditSelection_is_none(occluded)) ||
       found.cloud || found.trigger || found.defence || found.hill || is_ghost)
  {
    DrawCloudContext clouds_ctx;
    Vertex plot_cloud_offset = {0,0};

    if (found.cloud)
    {
      int const cloud_zoom = zoom + get_cloud_zoom();
      if (!DrawCloud_init(&clouds_ctx, clouds, palette, &view->sel_palette, cloud_zoom, is_ghost))
      {
        return;
      }

      Vertex const scaled_cloud_size = Vertex_div_log2(DrawCloud_get_size_os(), cloud_zoom);
      DEBUGF("scaled_cloud_size %d,%d\n", scaled_cloud_size.x, scaled_cloud_size.y);
      plot_cloud_offset = Vertex_div_log2(scaled_cloud_size, HalveFactorLog2);
    }

    /* Limit the amount by which icons can be scaled up to allow room for more icons
       to be displayed in the same map area at higher zoom levels. */
    int const trigger_zoom = (zoom < MinScaleTriggerZoom ? MinScaleTriggerZoom : zoom);
    DrawTrigContext triggers_ctx;
    if (found.trigger || found.defence)
    {
      PaletteEntry colours[] = {
        is_ghost ? view->config.ghost_colour : (*palette)[PaletteIndexWhite],
        (*palette)[PaletteIndexBlack],
      };

      PaletteEntry sel_colours[] = {
        view->sel_palette[PaletteIndexWhite],
        view->sel_palette[PaletteIndexBlack],
      };

      if (!DrawTrig_init(&triggers_ctx, &colours, &sel_colours, trigger_zoom))
      {
        return;
      }
    }

    screen_pos.y = offset_orig.y + SIGNED_L_SHIFT(scr_area->min.y, grid_size_log2);

    for (MapPoint scr_grid_pos = {.y = scr_area->min.y};
         scr_grid_pos.y <= scr_area->max.y;
         scr_grid_pos.y++, screen_pos.y += grid_size)
    {
      scr_grid_pos.x = scr_area->min.x;
      screen_pos.x = offset_orig.x + SIGNED_L_SHIFT(scr_grid_pos.x, grid_size_log2);

      for (; scr_grid_pos.x <= scr_area->max.x;
           scr_grid_pos.x++, screen_pos.x += grid_size)
      {
        MapPoint const map_pos = ObjLayout_derotate_scr_coords_to_map(view->config.angle, scr_grid_pos);
        ObjRef const obj_ref = read_obj(cb_arg, map_pos);
        bool const is_occluded = occluded && ObjEditSelection_is_selected(occluded, map_pos);
        bool const is_selected = selection && ObjEditSelection_is_selected(selection, map_pos);
        Vertex scr_min, scr_max;

        if ((!objects_ref_is_none(obj_ref) && is_selected) || found.trigger || found.defence || is_ghost) {
          MapArea object_bbox = get_mesh_bbox(meshes, view, obj_ref);
          MapArea_div_log2(&object_bbox, view->map_units_per_os_unit_log2, &object_bbox);

          scr_min = Vertex_add(screen_pos, MapPoint_to_vertex(object_bbox.min));
          scr_max = Vertex_add(screen_pos, MapPoint_to_vertex(object_bbox.max));
        }

        if (!objects_ref_is_none(obj_ref))
        {
          if (found.hill && objects_ref_is_hill(obj_ref)) {
            if (!is_ghost) {
              plot_set_col(is_selected ? view->sel_palette[HillColour] : (*palette)[HillColour]);
            }
            ObjGfxMeshes_plot_hill(&view->plot_ctx, screen_pos, CameraDistance, world_pos);
          }

          /* Draw clouds in the second pass because they are highest */
          if (found.cloud && objects_ref_is_cloud(obj_ref)) {
            Vertex const plot_cloud_min = Vertex_sub(screen_pos, plot_cloud_offset);
            DrawCloud_plot(&clouds_ctx, plot_cloud_min, is_selected,
              objects_ref_get_cloud_tint(obj_ref, map_pos));
          }

          /* Draw a rectangle around selected objects */
          if (is_selected) {
            plot_set_col(view->config.sel_colour);
            plot_fg_ol_rect_2v(scr_min, scr_max);
          }
        }

        /* Draw a rectangle around ghost or occluded objects */
        if (!objects_ref_is_mask(obj_ref) && (is_ghost || is_occluded)) {
          plot_set_col(view->config.ghost_colour);

          MapPoint coords[4];
          get_fine_collision_coords_for_obj(meshes, view, obj_ref, &coords);

          Vertex scr_coords[ARRAY_SIZE(coords)];
          for (size_t n = 0; n < ARRAY_SIZE(coords); ++n) {
            scr_coords[n] = Vertex_add(screen_pos,
              MapPoint_to_vertex(MapPoint_div_log2(coords[n], view->map_units_per_os_unit_log2)));
          }

          plot_set_dot_pattern_len(0);
          plot_move(scr_coords[0]);
          for (size_t n = 0; n < ARRAY_SIZE(coords); ++n) {
            plot_fg_dot_line(scr_coords[(n + 1) % ARRAY_SIZE(coords)]);
          }

          /* Cross out an occluded object */
          if (is_occluded) {
            plot_fg_dot_line(scr_coords[2]);
            plot_move(scr_coords[1]);
            plot_fg_dot_line(scr_coords[3]);
          }
        }

        /* If we are zoomed too far out then icons are too small to bother drawing at all */
        if (zoom > MaxDrawTriggerZoom) {
          continue;
        }

        /* If we have zoomed in far enough that icons are not scaled up any further then
           we may be able to fit more than one row of icons into the available space. */
        assert(trigger_zoom >= zoom);
        int const zoom_diff = trigger_zoom - zoom;
        int const max_rows = 1 << zoom_diff;
        int row = 0;

        Vertex const icon_size = Vertex_div_log2(DrawTrig_get_max_size_os(), trigger_zoom);
        DEBUGF("icon_size %d,%d\n", icon_size.x, icon_size.y);
        Vertex trig_scr_pos = {scr_min.x, scr_min.y - icon_size.y};

        /* Check again for 'none' because it's also used when an object (that may have triggers) is
           outside the redraw rectangle. We must not draw triggers relative to such a 'none' object. */
        if (found.trigger && !objects_ref_is_none(obj_ref)) {
          TriggersIter iter;
          TriggerFullParam fparam;
          MapArea const this_pos = {map_pos, map_pos};
          for (TriggersIter_get_first(&iter, triggers, &this_pos, &fparam);
               !TriggersIter_done(&iter);
               TriggersIter_get_next(&iter, &fparam))
          {
            if (trig_scr_pos.x > scr_min.x && trig_scr_pos.x + icon_size.x > scr_max.x) {
              if (++row >= max_rows) {
                break;
              }
              trig_scr_pos.y -= icon_size.y; /* next row */
              trig_scr_pos.x = scr_min.x;
            }
            assert(fparam.param.action != TriggerAction_Dummy);
            DrawTrig_plot(&triggers_ctx, fparam.param, trig_scr_pos, is_selected);
            trig_scr_pos.x += icon_size.x;
          }
        }

        if (found.defence && objects_ref_is_defence(obj_ref)) {
          do {
            if (trig_scr_pos.x > scr_min.x && trig_scr_pos.x + icon_size.x > scr_max.x) {
              if (++row >= max_rows) {
                break;
              }
              trig_scr_pos.y -= icon_size.y; /* next row */
              trig_scr_pos.x = scr_min.x;
            }
            DrawTrig_plot_defence(&triggers_ctx, obj_ref, trig_scr_pos, is_selected);
          } while(0);
        }
      } /* next scr_grid_pos.x */
    } /* next scr_grid_pos.y */
  }

  if (triggers) {
    plot_set_col(view->config.sel_colour);

    // Convert dash length from OS units to pixels in the current screen mode
    Vertex const eig = Desktop_get_eigen_factors();
    int const dash_len = DashLen >> LOWEST(eig.x, eig.y);
    plot_set_dash_pattern(SIGNED_R_SHIFT(dash_len, zoom));

    TriggersChainIter chain_iter;
    TriggerFullParam fparam = {{0}};
    static MapArea const all = {{0,0}, {Obj_Size - 1, Obj_Size - 1}};
    for (MapPoint p = TriggersChainIter_get_first(&chain_iter, triggers, &all, &fparam);
         !TriggersChainIter_done(&chain_iter);
         p = TriggersChainIter_get_next(&chain_iter, &fparam))
    {
      assert(fparam.param.action == TriggerAction_ChainReaction);
      MapPoint const start = ObjLayout_map_coords_to_centre(view, p);
      MapPoint const end = ObjLayout_map_coords_to_centre(view, fparam.next_coords);
      MapPoint const line_vec = MapPoint_sub(end, start);
      MapPoint arrow_a, arrow_b;
      get_arrow_ends(line_vec, ArrowLen, &arrow_a, &arrow_b);

      Vertex const scr_start = Vertex_add(scr_orig,
        MapPoint_to_vertex(MapPoint_div_log2(start, view->map_units_per_os_unit_log2)));

      Vertex const scr_end = Vertex_add(scr_orig,
        MapPoint_to_vertex(MapPoint_div_log2(end, view->map_units_per_os_unit_log2)));

      Vertex const scr_arrow_a = MapPoint_to_vertex(MapPoint_div_log2(arrow_a, view->map_units_per_os_unit_log2));
      Vertex const scr_arrow_b = MapPoint_to_vertex(MapPoint_div_log2(arrow_b, view->map_units_per_os_unit_log2));

      plot_move(scr_start);
      plot_fg_dot_line(scr_end);

      plot_move(Vertex_add(scr_start, scr_arrow_a));
      plot_fg_line(scr_end);
      plot_fg_line_ex_start(Vertex_add(scr_start, scr_arrow_b));
    }
  }
}

static void get_overlapping_area(View const *const view,
  MapArea const *const fine_area, MapArea *const overlapping_area, MapArea const *const max_obj_bbox)
{
  /* Some objects overlap a bigger area than just their grid location. This function
     calculates the required search area from a fine-scale bounding box, in grid
     coordinates. */
  assert(view);
  assert(MapArea_is_valid(fine_area));
  assert(MapArea_is_valid(max_obj_bbox));
  assert(overlapping_area);

  MapArea const overlapping_fine_area = {
    .min = MapPoint_sub(fine_area->min, max_obj_bbox->max),
    .max = MapPoint_sub(fine_area->max, max_obj_bbox->min),
  };

  *overlapping_area = ObjLayout_map_area_from_fine(view, &overlapping_fine_area);
}

void DrawObjs_get_overlapping_draw_area(ObjGfxMeshes *const meshes, View const *const view,
  MapArea const *const fine_area, MapArea *const overlapping_area)
{
  MapArea bbox = ObjGfxMeshes_get_max_ground_bbox(meshes, view->config.angle);
  expand_for_triggers(&bbox);

  /* Some objects have vertices outside their collision box and
     we have no idea whether a given object currently has a visible collision box. */
  MapArea const coll_bbox = get_max_collision_bbox(meshes, view);
  MapArea_expand_for_area(&bbox, &coll_bbox);
  get_overlapping_area(view, fine_area, overlapping_area, &bbox);
}

void DrawObjs_get_overlapping_select_area(ObjGfxMeshes *const meshes, View const *const view,
  MapArea const *const fine_area, MapArea *const overlapping_area)
{
#if COLLISION_BBOX_IS_SELECTION_BBOX
  MapArea const max_coll_bbox = get_max_collision_bbox(meshes, view);
  get_overlapping_area(view, fine_area, overlapping_area, &max_coll_bbox);
#else
  MapArea const max_obj_bbox = ObjGfxMeshes_get_max_ground_bbox(meshes, view->config.angle);
  get_overlapping_area(view, fine_area, overlapping_area, &max_obj_bbox);
#endif
}

static bool split_obj_bbox(View const *const view,
  MapPoint const grid_pos, MapArea *const object_bbox,
  bool (*const callback)(MapArea const *, void *), MapArea const *const map_area)
{
  DEBUG("Object's bounding box is %ld <= x <= %ld, %ld <= y <= %ld",
        object_bbox->min.x, object_bbox->max.x,
        object_bbox->min.y, object_bbox->max.y);

  /* Calculate the centre of the grid location in map coordinates */
  MapPoint const object_centre = ObjLayout_map_coords_to_centre(view,
                                    objects_wrap_coords(grid_pos));
  MapArea_translate(object_bbox, object_centre, object_bbox);

  /* Check whether the object's bounding box (relative to the centre of
     its grid location) overlaps the specified rectangle. */
  return MapArea_split(object_bbox, MAP_COORDS_LIMIT_LOG2, callback, (void *)map_area);
}

static bool filter_area_touches(MapArea const *const object_bbox, void *const arg)
{
  MapArea const *const map_area = arg;
  return MapArea_overlaps(object_bbox, map_area);
}

static bool filter_area_contains(MapArea const *const object_bbox, void *const arg)
{
  MapArea const *const map_area = arg;
  return MapArea_contains_area(map_area, object_bbox);
}

static MapArea get_obj_draw_bbox(ObjGfxMeshes *const meshes, View const *const view, bool const triggers,
  ObjRef const obj_ref)
{
  if (triggers) {
    DEBUGF("BBox with triggers\n");
    return DrawObjs_get_bbox_with_triggers(meshes, view, obj_ref);
  } else {
    DEBUGF("BBox without triggers\n");
    return DrawObjs_get_auto_bbox(meshes, view, obj_ref);
  }
}

bool DrawObjs_touch_select_bbox(ObjGfxMeshes *const meshes, View const *const view, ObjRef const obj_ref,
  MapPoint const grid_pos, MapArea const *const map_area)
{
  DEBUGF("Checking whether object at %" PRIMapCoord ",%" PRIMapCoord
         " touches click box %" PRIMapCoord " <= x <= %" PRIMapCoord ", "
         "%" PRIMapCoord " <= y <= %" PRIMapCoord "\n",
         grid_pos.x, grid_pos.y,
         map_area->min.x, map_area->max.x, map_area->min.y, map_area->max.y);

  if (objects_ref_is_none(obj_ref) || objects_ref_is_mask(obj_ref)) {
    DEBUG("No object at this grid location");
    return false;
  }

  /* Retrieve the object's bounding box, if it is a valid object reference */
  MapArea object_bbox = DrawObjs_get_select_bbox(meshes, view, obj_ref);
  return split_obj_bbox(view, grid_pos, &object_bbox,
                        filter_area_touches, map_area);
}

bool DrawObjs_touch_ghost_bbox(ObjGfxMeshes *const meshes, View const *const view, bool const triggers,
  ObjRef const obj_ref, MapPoint const grid_pos, MapArea const *const map_area)
{
  DEBUGF("Checking whether object at %" PRIMapCoord ",%" PRIMapCoord
         " touches draw box %" PRIMapCoord " <= x <= %" PRIMapCoord ", "
         "%" PRIMapCoord " <= y <= %" PRIMapCoord "\n",
         grid_pos.x, grid_pos.y,
         map_area->min.x, map_area->max.x, map_area->min.y, map_area->max.y);

  if (objects_ref_is_mask(obj_ref)) {
    DEBUG("No object at this grid location");
    return false;
  }

  /* Retrieve the object's bounding box, if it is a valid object reference */
  MapArea object_bbox = get_obj_draw_bbox(meshes, view, triggers, obj_ref);
  return split_obj_bbox(view, grid_pos, &object_bbox,
                        filter_area_touches, map_area);
}

bool DrawObjs_in_select_bbox(ObjGfxMeshes *const meshes, View const *const view, ObjRef const obj_ref,
  MapPoint const grid_pos, MapArea const *const map_area)
{
  DEBUGF("Checking whether object at %" PRIMapCoord ",%" PRIMapCoord
         " is in draw box %" PRIMapCoord " <= x <= %" PRIMapCoord ", "
         "%" PRIMapCoord " <= y <= %" PRIMapCoord "\n",
         grid_pos.x, grid_pos.y,
         map_area->min.x, map_area->max.x, map_area->min.y, map_area->max.y);

  if (objects_ref_is_none(obj_ref) || objects_ref_is_mask(obj_ref)) {
    DEBUG("No object at this grid location");
    return false;
  }

  /* Retrieve the object's bounding box, if it is a valid object reference */
  MapArea object_bbox = DrawObjs_get_select_bbox(meshes, view, obj_ref);
  return split_obj_bbox(view, grid_pos, &object_bbox,
                        filter_area_contains, map_area);
}
