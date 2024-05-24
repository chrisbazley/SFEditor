/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Plot strategic target information
 *  Copyright (C) 2022 Christopher Bazley
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
#include "plot.h"
#include "DrawInfos.h"
#include "DrawInfo.h"
#include "SelBitmask.h"
#include "Vertex.h"
#include "Map.h"
#include "MapTexBitm.h"
#include "Desktop.h"
#include "Session.h"
#include "MapLayout.h"
#include "Infos.h"
#include "ObjGfxMesh.h"

enum {
  PaletteIndexWhite = 255,
  PaletteIndexBlack = 0,
  HalveFactorLog2 = 1,
  CameraDistance = 65536 * 4,
};

static inline int calc_grid_size_log2(int const zoom)
{
  int const grid_size_log2 = MapTexSizeLog2 + TexelToOSCoordLog2 - zoom;
  DEBUG("Grid size for zoom %d = pow(2,%d)", zoom, grid_size_log2);
  return grid_size_log2;
}

static MapPoint get_info_size(View const *const view)
{
  static MapPoint info_size;
  static int zoom_factor = INT_MAX;
  if (zoom_factor != view->config.zoom_factor) {
    zoom_factor = view->config.zoom_factor;

    Vertex sprite_size_in_os = DrawInfo_get_size_os(false);
    sprite_size_in_os = Vertex_max(sprite_size_in_os, DrawInfo_get_size_os(true));
    int const over_zoom = zoom_factor < 0 ? zoom_factor : 0;
    int const fine_unit_per_os_log2 = MAP_COORDS_LIMIT_LOG2 - MapTexSizeLog2 - Map_SizeLog2 - TexelToOSCoordLog2 + over_zoom;
    DEBUGF("fine_unit_per_os_log2 %d\n", fine_unit_per_os_log2);

    /* Scale the info sprite's size to map coordinates.
       A bit arbitrary because these sprites are drawn with whatever zoom we request. */
    info_size = MapPoint_mul_log2(MapPoint_from_vertex(sprite_size_in_os), fine_unit_per_os_log2);
    DEBUGF("info_size B %" PRIMapCoord ",%" PRIMapCoord "\n", info_size.x, info_size.y);
  }
  return info_size;
}

static MapArea get_info_bbox(View const *const view)
{
  assert(view);
  MapArea bbox = {{0}};

  MapPoint const info_size = get_info_size(view);
  MapPoint const half_size = MapPoint_div_log2(info_size, HalveFactorLog2);
  DEBUGF("half_size %" PRIMapCoord ",%" PRIMapCoord "\n", half_size.x, half_size.y);

  /* Centre the bounding box in the tiles grid location */
  bbox = (MapArea){
    .min = {-half_size.x, -half_size.y},
    .max = {-half_size.x + info_size.x, -half_size.y + info_size.y}
  };
  DEBUGF("info_bbox B %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord "\n",
         bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y);

  return bbox;
}

static void get_fine_clickable_coords(View const *const view,
  MapPoint (*const coords)[4])
{
  MapPoint const click_dist = {InfoMaxClickDist, InfoMaxClickDist};
  MapPoint const bbox_size = MapPoint_add((MapPoint){1,1}, MapPoint_mul_log2(click_dist, 1));
  MapPoint const centre = MapLayout_map_coords_to_centre(view, click_dist);

  assert(coords);
  size_t n = 0;
  (*coords)[n++] = MapPoint_sub(MapLayout_map_coords_to_fine(view, (MapPoint){0, 0}), centre);
  (*coords)[n++] = MapPoint_sub(MapLayout_map_coords_to_fine(view, (MapPoint){0, bbox_size.y}), centre);
  (*coords)[n++] = MapPoint_sub(MapLayout_map_coords_to_fine(view, (MapPoint){bbox_size.x, bbox_size.y}), centre);
  (*coords)[n++] = MapPoint_sub(MapLayout_map_coords_to_fine(view, (MapPoint){bbox_size.x, 0}), centre);
}

static MapArea get_clickable_bbox(View const *const view)
{
  MapPoint coords[4];
  get_fine_clickable_coords(view, &coords);

  MapArea bbox = {{0}};
  for (size_t n = 0; n < ARRAY_SIZE(coords); ++n) {
    MapArea_expand(&bbox, coords[n]);
  }
  return bbox;
}

MapArea DrawInfos_get_bbox(View const *const view)
{
  /* Info icon may exceed bounds of the collision box and we have
     no idea whether a given info currently has a visible collision box. */
  MapArea bbox = get_info_bbox(view);
  MapArea const clickable_bbox = get_clickable_bbox(view);
  MapArea_expand_for_area(&bbox, &clickable_bbox);
  return bbox;
}

MapArea DrawInfos_get_select_bbox(View const *const view)
{
#if COLLISION_BBOX_IS_SELECTION_BBOX
  return get_clickable_bbox(view);
#else
  return get_info_bbox(view);
#endif
}

typedef struct {
  DrawInfoContext info_ctx;
  MapArea overlapping_area;
  Vertex offset_orig;
  int grid_size_log2, id;
  bool is_selected:1, is_ghost:1, is_occluded:1;
  Vertex scr_bbox_coords[4];
  PaletteEntry sel_colour, ghost_colour;
  Vertex sel_min, sel_max;
} PlotInfoArgs;

static void plot_info_at_y(PlotInfoArgs const *const args, MapPoint scr_tile_pos)
{
  Vertex const scr_pos = Vertex_add(args->offset_orig,
                            Vertex_mul_log2(MapPoint_to_vertex(scr_tile_pos), args->grid_size_log2));

  DrawInfo_plot(&args->info_ctx, scr_pos, args->is_selected, args->id);

  /* Draw a rectangle around selected infos */
  if (args->is_selected) {
    plot_set_col(args->sel_colour);
    plot_fg_ol_rect_2v(Vertex_add(scr_pos, args->sel_min), Vertex_add(scr_pos, args->sel_max));
  }


  /* Draw a rectangle around ghost or occluded infos */
  bool const is_occluded = args->is_occluded;
  if (args->is_ghost || is_occluded) {
    Vertex scr_coords[ARRAY_SIZE(args->scr_bbox_coords)];
    for (size_t n = 0; n < ARRAY_SIZE(scr_coords); ++n) {
      scr_coords[n] = Vertex_add(scr_pos, (args->scr_bbox_coords)[n]);
    }

    plot_set_col(args->ghost_colour);
    plot_set_dot_pattern_len(0);
    plot_move(scr_coords[0]);
    for (size_t n = 0; n < ARRAY_SIZE(args->scr_bbox_coords); ++n) {
      plot_fg_dot_line(scr_coords[(n + 1) % ARRAY_SIZE(args->scr_bbox_coords)]);
    }
  }
}

static void plot_info_at_x(PlotInfoArgs const *const args, MapPoint scr_tile_pos)
{
  plot_info_at_y(args, scr_tile_pos);

  MapArea const *const overlapping_area = &args->overlapping_area;

  if (scr_tile_pos.y >= overlapping_area->min.y) {
    scr_tile_pos.y -= Map_Size;
    plot_info_at_y(args, scr_tile_pos);
  } else if (scr_tile_pos.y <= overlapping_area->max.y) {
    scr_tile_pos.y += Map_Size;
    plot_info_at_y(args, scr_tile_pos);
  }
}

static void plot_info(PlotInfoArgs const *const args, MapPoint scr_tile_pos)
{
  plot_info_at_x(args, scr_tile_pos);

  MapArea const *const overlapping_area = &args->overlapping_area;
  assert(overlapping_area->min.x >= overlapping_area->max.x);
  assert(overlapping_area->min.y >= overlapping_area->max.y); // because wrapped

  // Plot duplicates if near the edges of the map, to handle coordinate wrap-around
  // Assumes symmetrical objects wrt the calculated overlapping_area
  if (scr_tile_pos.x >= overlapping_area->min.x) {
    scr_tile_pos.x -= Map_Size;
    plot_info_at_x(args, scr_tile_pos);
  } else if (scr_tile_pos.x <= overlapping_area->max.x) {
    scr_tile_pos.x += Map_Size;
    plot_info_at_x(args, scr_tile_pos);
  }
}

void DrawInfos_unknown_to_screen(
  struct View const *const view,
  MapArea const *const scr_area,
  Vertex const scr_orig)
{
  assert(MapArea_is_valid(scr_area));

  DEBUGF("Plot unknown infos for grid %" PRIMapCoord ", %" PRIMapCoord
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

void DrawInfos_to_screen(
  View const *const view,
  DrawInfosReadInfoFn *const read_info, void *const cb_arg,
  SelectionBitmask const *const selection,
  Vertex const scr_orig,
  bool const is_ghost,
  SelectionBitmask const *const occluded)
{
  assert(read_info);
  PaletteEntry colours[TargetInfoMax][DrawInfoPaletteSize], sel_colours[TargetInfoMax][DrawInfoPaletteSize];
  static unsigned char const id_cols[TargetInfoMax] = {255, 23, 119, 99,  203, 159};
  int maxp = is_ghost ? 1 : TargetInfoMax;
  for (int p = 0; p < maxp; ++p) {
    sel_colours[p][0] = view->sel_palette[id_cols[p]],
    sel_colours[p][1] = view->sel_palette[PaletteIndexBlack];

    colours[p][0] = (*palette)[id_cols[p]];
    colours[p][1] = is_ghost ? view->config.ghost_colour : (*palette)[PaletteIndexBlack];
  }

  int const zoom = view->config.zoom_factor >= 0 ? view->config.zoom_factor : 0;

  MapPoint fine_bbox_coords[4];
  get_fine_clickable_coords(view, &fine_bbox_coords);

  Vertex const scaled_info_size = Vertex_div_log2(DrawInfo_get_size_os(is_ghost), zoom);
  DEBUGF("scaled_info_size %d,%d\n", scaled_info_size.x, scaled_info_size.y);
  Vertex const plot_info_offset = Vertex_div_log2(scaled_info_size, HalveFactorLog2);

  int const grid_size_log2 = calc_grid_size_log2(view->config.zoom_factor);
  int const grid_size = 1 << grid_size_log2;
  Vertex const offset_orig = Vertex_add(scr_orig, (Vertex){grid_size / 2, grid_size / 2});

  PlotInfoArgs plot_info_args = {.offset_orig = offset_orig,
                                 .grid_size_log2 = grid_size_log2,
                                 .is_ghost = is_ghost,
                                 .sel_colour = view->config.sel_colour,
                                 .ghost_colour = view->config.ghost_colour,
                                 .sel_min = Vertex_sub((Vertex){0,0}, plot_info_offset),
                                 .sel_max = Vertex_sub(scaled_info_size, plot_info_offset)};

  for (size_t n = 0; n < ARRAY_SIZE(fine_bbox_coords); ++n) {
    plot_info_args.scr_bbox_coords[n] = MapPoint_to_vertex(MapPoint_div_log2(fine_bbox_coords[n], view->map_units_per_os_unit_log2));
  }

  // Calculate the tiles grid area potentially overlapped by infos which are offscreen
  MapArea const full_map = MapArea_make_max();
  DrawInfos_get_overlapping_draw_area(view, &full_map, &plot_info_args.overlapping_area);
  // The bounding box is expected to be invalid after wrapping offscreen coordinates to the opposite side
  plot_info_args.overlapping_area.min = map_wrap_coords(plot_info_args.overlapping_area.min);
  plot_info_args.overlapping_area.max = map_wrap_coords(plot_info_args.overlapping_area.max);

  if (!DrawInfo_init(&plot_info_args.info_ctx, &colours, &sel_colours, zoom, is_ghost)) {
    return;
  }

  MapPoint map_pos;
  int id;
  for (size_t index = read_info(cb_arg, &map_pos, &id);
       index != SIZE_MAX;
       index = read_info(cb_arg, &map_pos, &id)) {

    plot_info_args.is_selected = selection && SelectionBitmask_is_selected(selection, index);
    plot_info_args.is_occluded = occluded && SelectionBitmask_is_selected(occluded, index);
    plot_info_args.id = is_ghost ? 0 : id;

    MapPoint scr_tile_pos = MapLayout_rotate_map_coords_to_scr(view->config.angle, map_pos);
    plot_info(&plot_info_args, scr_tile_pos);
  }
}

static bool split_info_bbox(View const *const view,
  MapPoint const grid_pos, MapArea const *const fine_area,
  bool (*const callback)(MapArea const *, void *),
  MapArea *const info_bbox)
{
  /* Retrieve the info's bounding box */
  DEBUG("Info's bounding box is %ld <= x <= %ld, %ld <= y <= %ld",
        info_bbox->min.x, info_bbox->max.x,
        info_bbox->min.y, info_bbox->max.y);

  /* Calculate the centre of the grid location in map coordinates */
  MapPoint const info_centre = MapLayout_map_coords_to_centre(view, map_wrap_coords(grid_pos));
  MapArea_translate(info_bbox, info_centre, info_bbox);

  /* Check whether the info's bounding box (relative to the centre of
     its grid location) overlaps the specified rectangle. */
  return MapArea_split(info_bbox, MAP_COORDS_LIMIT_LOG2,
                       callback, (void *)fine_area);
}

static bool filter_area_touches(MapArea const *const object_bbox, void *const arg)
{
  MapArea const *const fine_area = arg;
  return MapArea_overlaps(object_bbox, fine_area);
}

static bool filter_area_contains(MapArea const *const object_bbox, void *const arg)
{
  MapArea const *const fine_area = arg;
  return MapArea_contains_area(fine_area, object_bbox);
}

bool DrawInfos_touch_select_bbox(View const *const view,
  MapPoint const grid_pos, MapArea const *const fine_area)
{
  DEBUGF("Checking whether info at %" PRIMapCoord ",%" PRIMapCoord
         " touches click box %" PRIMapCoord " <= x <= %" PRIMapCoord ", "
         "%" PRIMapCoord " <= y <= %" PRIMapCoord "\n",
         grid_pos.x, grid_pos.y,
         fine_area->min.x, fine_area->max.x, fine_area->min.y, fine_area->max.y);

  MapArea info_bbox = DrawInfos_get_select_bbox(view);
  return split_info_bbox(view, grid_pos, fine_area,
                         filter_area_touches, &info_bbox);
}

bool DrawInfos_touch_ghost_bbox(View const *const view,
  MapPoint const grid_pos, MapArea const *const fine_area)
{
  DEBUGF("Checking whether info at %" PRIMapCoord ",%" PRIMapCoord
         " touches draw box %" PRIMapCoord " <= x <= %" PRIMapCoord ", "
         "%" PRIMapCoord " <= y <= %" PRIMapCoord "\n",
         grid_pos.x, grid_pos.y,
         fine_area->min.x, fine_area->max.x, fine_area->min.y, fine_area->max.y);

  MapArea info_bbox = DrawInfos_get_bbox(view);
  return split_info_bbox(view, grid_pos, fine_area,
                         filter_area_touches, &info_bbox);
}

bool DrawInfos_in_select_bbox(View const *const view,
  MapPoint const grid_pos, MapArea const *const fine_area)
{
  DEBUGF("Checking whether info at %" PRIMapCoord ",%" PRIMapCoord
         " is in draw box %" PRIMapCoord " <= x <= %" PRIMapCoord ", "
         "%" PRIMapCoord " <= y <= %" PRIMapCoord "\n",
         grid_pos.x, grid_pos.y,
         fine_area->min.x, fine_area->max.x, fine_area->min.y, fine_area->max.y);

  MapArea info_bbox = DrawInfos_get_select_bbox(view);
  return split_info_bbox(view, grid_pos, fine_area,
                         filter_area_contains, &info_bbox);
}

static void get_overlapping_grid_area(View const *const view,
  MapArea const *const fine_area, MapArea const *const info_bbox, MapArea *const overlapping_area)
{
  /* Info points may overlap a bigger area than just their tile location. This function
     calculates the required search area from a fine-scale bounding box, in tile map
     coordinates. */
  assert(view);
  assert(MapArea_is_valid(fine_area));
  assert(MapArea_is_valid(info_bbox));
  assert(overlapping_area);

  MapArea const overlapping_fine_area = {
    .min = MapPoint_sub(fine_area->min, info_bbox->max),
    .max = MapPoint_sub(fine_area->max, info_bbox->min),
  };

  *overlapping_area = MapLayout_map_area_from_fine(view, &overlapping_fine_area);
}

void DrawInfos_get_select_area(View const *const view,
  MapArea const *const fine_area, MapArea *const overlapping_area)
{
  MapArea const info_bbox = DrawInfos_get_select_bbox(view);
  get_overlapping_grid_area(view, fine_area, &info_bbox, overlapping_area);
}

void DrawInfos_get_overlapping_draw_area(View const *const view,
  MapArea const *const fine_area, MapArea *const overlapping_area)
{
  MapArea const info_bbox = DrawInfos_get_bbox(view);
  get_overlapping_grid_area(view, fine_area, &info_bbox, overlapping_area);
}
