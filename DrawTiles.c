/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Plot area of the ground map to a specified sprite
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
#include "stdio.h"
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "OSVDU.h"

#include "hourglass.h"
#include "Macros.h"
#include "err.h"
#include "msgtrans.h"
#include "SprFormats.h"
#include "OSSpriteOp.h"
#include "EditWin.h"

#ifdef FASTPLOT
#include "FastPlot.h"
#endif
#include "debug.h"
#include "DrawTiles.h"
#include "Plot.h"
#include "MapTexBDat.h"
#include "MapTexBitm.h"
#include "Map.h"
#include "SprMem.h"
#include "MapLayout.h"

#ifndef FASTPLOT

enum {
  ScaleFactorNumerator = 1024,
  DrawSmallMinZoom = MapTexSizeLog2,
};

static bool draw_bitmap_big(MapTexBitmaps *const textures,
  MapAngle const angle, MapArea const *const scr_area,
  DrawTilesReadFn *const read, void *const cb_arg, int const zoom,
  SprMem const *const sprites,
  unsigned char const (*const sel_colours)[NumColours])
{
  assert(textures != NULL);
  assert(read != NULL);

  /* White out the sprite (needed for EOR plot if tiles inverted) */

  /* Set up some static OS_SpriteOp parameters */
  ScaleFactors *pscale = NULL;
#if 0
  ScaleFactors scale_factors;
  if (zoom != 0) {
    /* Create scale factors. The source and destination sprites must have
       the same eigen values, so ignore them here. */
    scale_factors = (ScaleFactors){
      .xmul = ScaleFactorNumerator,
      .ymul = ScaleFactorNumerator,
      .xdiv = SIGNED_L_SHIFT(ScaleFactorNumerator, zoom),
      .ydiv = SIGNED_L_SHIFT(ScaleFactorNumerator, zoom)
    };
    DEBUGF("Sprite scale %d/%d, %d/%d\n", scale_factors.xmul, scale_factors.xdiv,
           scale_factors.ymul, scale_factors.ydiv);
    pscale = &scale_factors;
  }
#endif
  bool needs_mask = false;
  Vertex draw_pos = {0,0};
  Vertex const tile_size = (Vertex){
    SIGNED_R_SHIFT(MapTexSize << DrawTilesModeXEig, zoom),
    SIGNED_R_SHIFT(MapTexSize << DrawTilesModeYEig, zoom)};

  hourglass_on();
  MapCoord const nrows = scr_area->max.y - scr_area->min.y + 1;
  size_t const count = MapTexBitmaps_get_count(textures);

  for (MapPoint scr_pos = {.y = scr_area->min.y};
       scr_pos.y <= scr_area->max.y;
       scr_pos.y++, draw_pos.y += tile_size.y) {

    hourglass_percentage(((scr_pos.y - scr_area->min.y) * 100) / nrows);

    for (scr_pos.x = scr_area->min.x, draw_pos.x = 0;
         scr_pos.x <= scr_area->max.x;
         scr_pos.x++, draw_pos.x += tile_size.x) {
      MapPoint const map_pos = MapLayout_derotate_scr_coords_to_map(angle, scr_pos);
      DrawTilesReadResult value = read(cb_arg, map_pos);
      MapRef tile_ref = value.tile_ref;

      if (!map_ref_is_mask(tile_ref)) {
        if (map_ref_to_num(tile_ref) >= count) {
          tile_ref = map_ref_from_num(0); /* FIXME: substitute a placeholder sprite? */
        }

        char tile_name[12];
        sprintf(tile_name, "%zu", map_ref_to_num(tile_ref));

        int action = SPRITE_ACTION_OVERWRITE;
        void const *colours = NULL;
        if (value.is_selected) {
          //action = SPRITE_ACTION_EOR;
          colours = sel_colours;
        }
        SprMem_plot_scaled_sprite(sprites, tile_name,
                                  draw_pos, action, pscale, colours);
      } else {
        needs_mask = true;
      }
    }
  }

  hourglass_off();

  return needs_mask;
}

static bool draw_bitmap_small(MapTexBitmaps *const textures,
  MapAngle const angle, MapArea const *const scr_area,
  DrawTilesReadFn *const read, void *const cb_arg,
  unsigned char const (*const sel_colours)[NumColours])
{
  assert(textures != NULL);
  assert(read != NULL);

  bool needs_mask = false;
  Vertex draw_pos = {0,0};

  hourglass_on();
  MapCoord const nrows = scr_area->max.y - scr_area->min.y + 1;
  size_t const count = MapTexBitmaps_get_count(textures);

  for (MapPoint scr_pos = {.y = scr_area->min.y};
       scr_pos.y <= scr_area->max.y;
       scr_pos.y++, draw_pos.y += 1 << DrawTilesModeYEig) {

    hourglass_percentage(((scr_pos.y - scr_area->min.y) * 100) / nrows);

    int current_colour = -1;
    plot_move(draw_pos);

    for (scr_pos.x = scr_area->min.x, draw_pos.x = 0;
         scr_pos.x <= scr_area->max.x;
         scr_pos.x++, draw_pos.x += 1 << DrawTilesModeXEig) {
      MapPoint const map_pos = MapLayout_derotate_scr_coords_to_map(angle, scr_pos);
      DrawTilesReadResult value = read(cb_arg, map_pos);
      MapRef tile_ref = value.tile_ref;

      if (!map_ref_is_mask(tile_ref)) {
        if (map_ref_to_num(tile_ref) >= count) {
          tile_ref = map_ref_from_num(0); /* FIXME: substitute a placeholder sprite? */
        }

        /* Plot average colour of tile */
        int new_col = MapTexBitmaps_get_average_colour(textures, tile_ref);
        if (value.is_selected) {
          //new_col ^= UINT8_MAX; /* invert colour */
          assert(new_col >= 0);
          assert(new_col < NumColours);
          new_col = (*sel_colours)[new_col];
        }

        if (new_col != current_colour) {
          if (current_colour >= 0) {
            plot_fg_line_ex_end(draw_pos);
          }

          plot_move(draw_pos);
          current_colour = new_col;
          os_set_colour(0, GCOLAction_Overwrite, current_colour);
        }
      } else {
        if (current_colour >= 0) {
          plot_fg_line_ex_end(draw_pos);
          current_colour = -1;
        }

        needs_mask = true;
      }
    }

    if (current_colour >= 0) {
      plot_fg_line_ex_end(draw_pos);
    }
  }
  hourglass_off();

  return needs_mask;
}

#endif

bool DrawTiles_to_sprite(MapTexBitmaps *const textures,
  SprMem *const sm, char const *const name, MapAngle const angle, MapArea const *const scr_area,
  DrawTilesReadFn *const read, void *const cb_arg,
  int const zoom, unsigned char const (*const sel_colours)[NumColours])
{
  assert(textures != NULL);
  assert(read != NULL);

  DEBUGF("Plot bitmap for tiles x %" PRIMapCoord "..%" PRIMapCoord " y %" PRIMapCoord "..%" PRIMapCoord
         " at zoom level %d\n",
        scr_area->min.x, scr_area->max.x, scr_area->min.y, scr_area->max.y, zoom);

  /* Veneer onto FastPlot_plotarea, to take sprite area+name rather than
     raw bitmap pointer */
  bool needs_mask = false;
#ifdef FASTPLOT
  /* Use optimised direct plot routine */
  SpriteHeader *const sprite = SprMem_get_sprite_address(sm, name);
  if (sprite == NULL)
    return false;

  switch (zoom) {
    case -2:
    case -1:
    case 0: /*  1:1 or 2:1 (16x16) */
      needs_mask = FastPlot_plotarea(textures->sprites, sprite, angle, scr_area, columns,
        base, overlay);
      break;

    case 1: /*  1:2 (8x8) */
      needs_mask = FastPlot_plotareaB(textures->sprites, sprite, angle, scr_area, columns,
        base, overlay);
      break;

    case 2: /*  1:4 (4x4) */
      needs_mask = FastPlot_plotareaC(textures->sprites, sprite, angle, scr_area, columns,
        base, overlay);
      break;

    case 3: /*  1:16 (1x1) */
    case 4:
      needs_mask = FastPlot_plotareaD(textures->count,
        textures->avcols_table, sprite, angle, scr_area, columns,
        base, overlay);
      break;
  }
  SprMem_put_sprite_address(sm, sprite);
#else
  SprMem const *sprites = NULL;
  if (zoom < DrawSmallMinZoom) {
    sprites = MapTexBitmaps_get_sprites(textures, angle, zoom);
    if (!sprites) {
      os_set_colour(OS_SetColour_Background, GCOLAction_Overwrite, UINT8_MAX);
      plot_clear_window();
      return false;
    }
  }

  /* Use OS calls to plot to sprite (slow!) */
  if (!SprMem_output_to_sprite(sm, name))
    return false;

  if (zoom < DrawSmallMinZoom) {
    needs_mask = draw_bitmap_big(textures, angle, scr_area, read, cb_arg, zoom, sprites, sel_colours);
  } else {
    needs_mask = draw_bitmap_small(textures, angle, scr_area, read, cb_arg, sel_colours);
  }

  SprMem_restore_output(sm);
#endif
  return needs_mask;
}

static void draw_mask_bbox(void *cb_arg, BBox const *bbox, MapRef const value)
{
  NOT_USED(cb_arg);
  if (map_ref_is_mask(value)) {
    plot_inv_bbox(bbox);
  }
}

void DrawTiles_to_mask(SprMem *const sm, char const *const name,
  MapAngle const angle, MapArea const *const scr_area, DrawTilesReadFn *const read, void *const cb_arg,
  int const zoom)
{
  /* Veneer onto FastPlot_plotmask, to take sprite area+name rather than raw bitmap pointer */
  DEBUGF("Plot mask for tiles x %" PRIMapCoord "..%" PRIMapCoord " y %" PRIMapCoord "..%" PRIMapCoord "\n",
         scr_area->min.x, scr_area->max.x, scr_area->min.y, scr_area->max.y);

#ifdef FASTPLOT
  /* Use optimised direct plot routine */
  SpriteHeader *const sprite = SprMem_get_sprite_address(sm, name);
  if (sprite == NULL)
    return;

  switch (zoom) {
    case -2:
    case -1:
    case 0:
      FastPlot_plotmask(sprite, scr_area, columns, overlay);
      break;
    case 1:
      FastPlot_plotmaskB(sprite, scr_area, columns, overlay);
      break;
    case 2:
      FastPlot_plotmaskC(sprite, scr_area, columns, overlay);
      break;
    case 3:
    case 4:
      FastPlot_plotmaskD(sprite, scr_area, columns, overlay);
      break;
  }
  SprMem_put_sprite_address(sm, sprite);
#else
  /* Use OS calls to plot to sprite (slow!) */
  if (!SprMem_output_to_mask(sm, name))
    return;

  /* Plot holes in the thumbnail mask */
  Vertex tile_size = {1 << DrawTilesModeXEig, 1 << DrawTilesModeYEig};

  if (zoom < DrawSmallMinZoom) {
    tile_size = (Vertex){
      SIGNED_R_SHIFT(MapTexSize << DrawTilesModeXEig, zoom),
      SIGNED_R_SHIFT(MapTexSize << DrawTilesModeYEig, zoom)};
  }

  DrawTiles_to_bbox(angle, scr_area, read, cb_arg, draw_mask_bbox, NULL, tile_size);

  SprMem_restore_output(sm);
#endif
}

typedef enum {
  RectState_None,
  RectState_XSpan,
  RectState_YSpanPending,
  RectState_YSpan,
} RectState;

void DrawTiles_to_bbox(
  MapAngle const angle, MapArea const *const scr_area, DrawTilesReadFn *const read, void *const read_arg,
  DrawTilesBBoxFn *const give_bbox, void *const give_bbox_arg, Vertex const tile_size)
{
  assert(read != NULL);
  assert(give_bbox != NULL);
  DEBUGF("Plot bboxes for tiles %" PRIMapCoord ", %" PRIMapCoord "%" PRIMapCoord ", %" PRIMapCoord "\n",
         scr_area->min.x, scr_area->min.y, scr_area->max.x, scr_area->max.y);

  Vertex const draw_max = Vertex_mul(MapPoint_to_vertex(MapArea_size(scr_area)), tile_size);

  Vertex draw_pos = {0,0};
  MapRef span_value = map_ref_from_num(0);
  RectState state = RectState_None;
  BBox bbox = {0, 0, 0, 0};

  hourglass_on();
  MapCoord const nrows = scr_area->max.y - scr_area->min.y + 1;

  for (MapPoint scr_pos = {.y = scr_area->min.y};
       scr_pos.y <= scr_area->max.y;
       ++scr_pos.y, draw_pos.y += tile_size.y) {

    hourglass_percentage(((scr_pos.y - scr_area->min.y) * 100) / nrows);

    for (scr_pos.x = scr_area->min.x, draw_pos.x = 0;
         scr_pos.x <= scr_area->max.x;
         ++scr_pos.x, draw_pos.x += tile_size.x) {

      MapPoint const map_pos = MapLayout_derotate_scr_coords_to_map(angle, scr_pos);
      DrawTilesReadResult const value = read(read_arg, map_pos);
      if (state != RectState_None && map_ref_is_equal(value.tile_ref, span_value)) {
        continue;
      }

      if (state == RectState_YSpan) {
        DEBUG("Closing y span at %" PRIMapCoord ",%" PRIMapCoord "", scr_pos.x, scr_pos.y);
        bbox.xmax = draw_max.x;
        bbox.ymax = draw_pos.y;
        give_bbox(give_bbox_arg, &bbox, span_value);

        if (scr_pos.x != 0) {
          DEBUG("Start x span at start of row %" PRIMapCoord "", scr_pos.y);
          bbox.xmin = 0;
          bbox.ymin = draw_pos.y;
          state = RectState_XSpan;
        }
      }

      if (state == RectState_XSpan || state == RectState_YSpanPending) {
        DEBUG("Closing x span at %" PRIMapCoord ",%" PRIMapCoord "",
              scr_pos.x, scr_pos.y);
        bbox.xmax = draw_pos.x;
        bbox.ymax = draw_pos.y + tile_size.y;
        give_bbox(give_bbox_arg, &bbox, span_value);
      }

      span_value = value.tile_ref;
      DEBUG("Span of %zu starts at %" PRIMapCoord ",%" PRIMapCoord "",
            map_ref_to_num(value.tile_ref), scr_pos.x, scr_pos.y);

      BBox_set_min(&bbox, draw_pos);

      if (scr_pos.x == 0) {
        state = RectState_YSpanPending;
      } else {
        state = RectState_XSpan;
      }
    } /* next col */

    if (state == RectState_XSpan) {
      DEBUG("Closing final x span on row %" PRIMapCoord, scr_pos.y);
      bbox.xmax = draw_max.x;
      bbox.ymax = draw_pos.y + tile_size.y;
      give_bbox(give_bbox_arg, &bbox, span_value);
      state = RectState_None;
    } else if (state == RectState_YSpanPending) {
      DEBUG("Start y span on row %" PRIMapCoord, scr_pos.y);
      state = RectState_YSpan;
    }
  } /* next row */

  if (state == RectState_YSpan) {
    DEBUGF("Closing final y span\n");
    BBox_set_max(&bbox, draw_max);
    give_bbox(give_bbox_arg, &bbox, span_value);
  }

  hourglass_off();
}
