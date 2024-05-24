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

/* "Knowing me, knowing you... ah-haaaa." */
#include "stdlib.h"
#include "stdio.h"
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>
#include <ctype.h>

#include "toolbox.h"
#include "event.h"
#include "window.h"
#include "wimp.h"
#include "wimplib.h"
#include "gadgets.h"
#include "menu.h"

#include "err.h"
#include "msgtrans.h"
#include "Macros.h"
#include "SprFormats.h"
#include "TboxBugs.h"
#include "FileUtils.h"
#include "StrExtra.h"
#include "debug.h"
#include "nobudge.h"

#include "OurEvents.h"
#include "Session.h"
#include "utils.h"
#include "MapMode.h"
#include "StatusBar.h"
#include "DrawTiles.h"
#include "Palette.h"
#ifdef FASTPLOT
#include "FastPlot.h"
#endif
#include "debug.h"
#include "plot.h"
#include "EditWin.h"
#include "MPropDbox.h"
#include "NewTransfer.h"
#include "MapAnims.h"
#include "MapEdit.h"
#include "MapEditSel.h"
#include "MSnakes.h"
#include "Smooth.h"
#include "MTransfers.h"
#include "MTransfersPalette.h"
#include "MSnakesPalette.h"
#include "TilesPalette.h"
#include "MapCoord.h"
#include "MapTexData.h"
#include "MapEditChg.h"
#include "EditMode.h"
#include "Desktop.h"
#include "Shapes.h"
#include "Map.h"
#include "DataType.h"
#include "MapEditCtx.h"
#include "EditorData.h"
#include "Filenames.h"
#include "DataType.h"
#include "DFileUtils.h"
#include "IntDict.h"
#include "MapLayout.h"

#define PENDING_IS_SELECTED 0

enum {
  GRID_GAP_SIZE = MapTexSize << TexelToOSCoordLog2,
  MAX_SELECTED = Map_Area,
  DeletedFillRef = 0,
  ScaleFactorNumerator = 1024,
  MaxDrawAnimZoom = 2, // markers not drawn at all beyond this (too small)
};

typedef enum {
  MAPPALETTE_TYPE_NONE = -1,
  MAPPALETTE_TYPE_SNAKES,
  MAPPALETTE_TYPE_TILES,
  MAPPALETTE_TYPE_TRANSFERS
} MapPaletteType;

typedef struct
{
  MapEditSelection selection;
  MapPaletteType palette_type;
  MapArea ghost_bbox, drop_bbox;
  MapPoint drag_start_pos, pending_vert[3];
  MapEditChanges change_info; /* for accumulation */
  PendingShape pending_shape;
  MapTransfer *pending_transfer, *pending_paste, *pending_drop, *dragged;
  bool uk_drop_pending:1, lock_selection:1;
  MapSnakesContext snake_ctx;
  MapPropDboxes prop_dboxes;
}
MapModeData;

static MapTransfer *clipboard;

/* ---------------- Private functions ---------------- */

static inline MapModeData *get_mode_data(Editor const *const editor)
{
  assert(Editor_get_edit_mode(editor) == EDITING_MODE_MAP);
  assert(editor->editingmode_data);
  return editor->editingmode_data;
}

static void redraw_selection(MapArea const *const area, void *const arg)
{
  Editor_redraw_map(arg, area);
}

static bool MapMode_can_select_tool(Editor const *const editor, EditorTool const tool)
{
  EditSession *const session = Editor_get_session(editor);
  MapTex *const textures = Session_get_textures(session);
  bool can_select_tool = true;

  switch (tool)
  {
  case EDITORTOOL_SNAKE:
    can_select_tool = (MapSnakes_get_count(&textures->snakes) > 0);
    break;

  case EDITORTOOL_SMOOTHWAND:
    can_select_tool = (MapTexGroups_get_count(&textures->groups) > 0);
    break;

  case EDITORTOOL_TRANSFER:
    can_select_tool = (MapTransfers_get_count(&textures->transfers) > 0);
    break;

  default:
    can_select_tool = true;
    break;
  }

  return can_select_tool;
}

static bool MapMode_anim_is_selected(Editor const *const editor)
{
  MapModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  MapEditContext const *const map = Session_get_map(session);

  if (!map->anims) {
    return false;
  }

  MapArea sel_area;
  if (!MapEditSelection_get_bounds(&mode_data->selection, &sel_area)) {
    return false;
  }

  if (map->anims) {
    DEBUGF("Searching animations for the first selected\n");
    MapAnimsIter iter;
    for (MapPoint p = MapAnimsIter_get_first(&iter, map->anims, &sel_area, NULL);
         !MapAnimsIter_done(&iter);
         p = MapAnimsIter_get_next(&iter, NULL))
    {
      if (MapEditSelection_is_selected(&mode_data->selection, p)) {
        return true;
      }
    }
  }
  return false;
}

static bool MapMode_can_clip_overlay(Editor const *editor)
{
  EditSession *const session = Editor_get_session(editor);

  return Session_has_data(session, DataType_OverlayMap) &&
         Session_has_data(session, DataType_BaseMap);
}

static void create_trans_msg(Editor *const editor,
                             MapTransfer *const transfer)
{
  MapModeData *const mode_data = get_mode_data(editor);

  char const *const name = get_leaf_name(MapTransfer_get_dfile(transfer));

  size_t const num_tiles = MapEditSelection_size(&mode_data->selection);
  char tiles_count_str[16];
  sprintf(tiles_count_str, "%ld", num_tiles);

  size_t const num_animations = MapTransfers_get_anim_count(transfer);
  if (num_animations > 0) {
    char anim_count_str[16];
    sprintf(anim_count_str, "%zu", num_animations);

    Editor_display_msg(editor,
      msgs_lookup_subn("MStatusCrTr2", 3, tiles_count_str, anim_count_str, name),
      true);
  } else {
    Editor_display_msg(editor,
      msgs_lookup_subn("MStatusCrTr1", 2, tiles_count_str, name),
      true);
  }
}

static void notify_changed(EditSession *const session,
  MapEditChanges *const change_info)
{
  DEBUG("Assimilating change record %p", (void *)change_info);

  if (MapEditChanges_anims_changed(change_info))
  {
    Session_notify_changed(session, DataType_OverlayMapAnimations);
  }

  if (MapEditChanges_map_changed(change_info))
  {
    Session_notify_changed(session, (Session_get_map(session)->overlay !=
                           NULL ? DataType_OverlayMap : DataType_BaseMap));
  }
}

static void display_msg(Editor *const editor,
  MapEditChanges const *const change_info)
{
  char *const msg = MapEditChanges_get_message(change_info);
  if (msg) {
    Editor_display_msg(editor, msg, true);
  }
}

static void changed_with_msg(Editor *const editor)
{
  EditSession *const session = Editor_get_session(editor);
  MapModeData *const mode_data = get_mode_data(editor);

  notify_changed(session, &mode_data->change_info);
  display_msg(editor, &mode_data->change_info);
}

static Vertex calc_grid_size(int const zoom)
{
  /* Calculate the size of each grid square (in OS units) */
  Vertex const grid_size = {
    SIGNED_R_SHIFT(GRID_GAP_SIZE, zoom),
    SIGNED_R_SHIFT(GRID_GAP_SIZE, zoom)
  };
  DEBUG("Grid size for zoom %d = %d, %d", zoom, grid_size.x, grid_size.y);
  assert(grid_size.x > 0);
  assert(grid_size.y > 0);
  return grid_size;
}

static Vertex grid_to_os_coords(Vertex const origin, MapPoint const map_pos,
  Vertex const grid_size)
{
  assert((map_pos.x == Map_Size && map_pos.y == Map_Size) ||
         map_coords_in_range(map_pos));
  assert(grid_size.x > 0);
  assert(grid_size.y > 0);

  Vertex const mpos = {(int)map_pos.x, (int)map_pos.y};
  Vertex const os_coords = Vertex_add(origin, Vertex_mul(mpos, grid_size));
  DEBUG("OS origin = %d,%d Map coords = %" PRIMapCoord ",%" PRIMapCoord
        " OS coords = %d,%d", origin.x, origin.y, map_pos.x, map_pos.y,
        os_coords.x, os_coords.y);
  return os_coords;
}

typedef enum {
  RedrawCheqValue_Skip,
  RedrawCheqValue_Selected,
  RedrawCheqValue_Clear,
} RedrawCheqValue;

typedef struct {
  MapEditSelection const *selection;
  PaletteEntry bg_sel_colour;
  PaletteEntry bg_colour;
  MapEditContext const *map;
  RedrawCheqValue last;
  Vertex min_os;
} RedrawCheqData;

static void draw_chequered_bbox(void *cb_arg, BBox const *bbox, MapRef const value)
{
  RedrawCheqData *const data = cb_arg;
  RedrawCheqValue current = map_ref_to_num(value);
  assert(data);
  assert(bbox);
  DEBUGF("BBox value %zu min %d,%d max %d,%d\n", map_ref_to_num(value),
         bbox->xmin, bbox->ymin, bbox->xmax, bbox->ymax);

  if (current != RedrawCheqValue_Skip) {
    if (current != data->last) {
      plot_set_col(current == RedrawCheqValue_Selected ?
        data->bg_sel_colour : data->bg_colour);
      data->last = current;
    }

    BBox trans_bbox;
    BBox_translate(bbox, data->min_os, &trans_bbox);
    plot_fg_bbox(&trans_bbox);
  }
}

static DrawTilesReadResult draw_chequered_read(void *const cb_arg, MapPoint const map_pos)
{
  assert(cb_arg);
  RedrawCheqData const *const data = cb_arg;

  /* If skip_overlay then skip tiles overridden in the overlay map
     (prevents unsightly overplotting). */
  if (data->map && !map_ref_is_mask(MapEdit_read_overlay(data->map, map_pos)))
  {
    return (DrawTilesReadResult){map_ref_from_num(RedrawCheqValue_Skip)};
  }

  return (DrawTilesReadResult){map_ref_from_num(
    data->selection && MapEditSelection_is_selected(data->selection, map_pos) ?
           RedrawCheqValue_Selected : RedrawCheqValue_Clear)};
}

static void fill_to_infinity(PaletteEntry const fill_col)
{
  /* Draw plain rectangle instead */
  plot_set_col(fill_col);
  plot_fg_rect_2v((Vertex){SHRT_MIN, SHRT_MIN}, (Vertex){SHRT_MAX, SHRT_MAX});
}

static void draw_chequered(Editor *const editor, MapAngle const angle,
  MapArea const *const scr_area,
  Vertex const min_os, EditWin const *const edit_win, bool const skip_overlay)
{
  DEBUGF("Drawing chequered\n");
  EditSession *const session = Editor_get_session(editor);
  MapModeData *const mode_data =
      (Editor_get_edit_mode(editor) == EDITING_MODE_MAP ?
      editor->editingmode_data : NULL);

  PaletteEntry const bg_colour = EditWin_get_bg_colour(edit_win);

  int const zoom = EditWin_get_zoom(edit_win);
  Vertex const tile_size = {
      SIGNED_R_SHIFT(MapTexSize << TexelToOSCoordLog2, zoom),
      SIGNED_R_SHIFT(MapTexSize << TexelToOSCoordLog2, zoom)};

  Vertex const draw_min = Vertex_mul(MapPoint_to_vertex(scr_area->min), tile_size);

  RedrawCheqData data = {
    .selection = mode_data ? &mode_data->selection : NULL,
    .bg_colour = bg_colour,
    .bg_sel_colour = opposite_col(bg_colour),
    .map = skip_overlay ? Session_get_map(session) : NULL,
    .last = RedrawCheqValue_Skip,
    .min_os = Vertex_add(min_os, draw_min),
  };

  DrawTiles_to_bbox(angle, scr_area, draw_chequered_read, &data, draw_chequered_bbox,
    &data, tile_size);
}

static MapEditSelection *get_selection(Editor *const editor)
{
  if (Editor_get_edit_mode(editor) != EDITING_MODE_MAP)
    return NULL;

  MapModeData *const mode_data = get_mode_data(editor);
  return &mode_data->selection;
}

static void draw_no_tiles(Editor *const editor, MapAngle const angle,
  MapArea const *const scr_area, Vertex const scr_orig, EditWin const *const edit_win)
{
  DEBUGF("Draw simple background (no tile graphics)\n");
  PaletteEntry const bg_colour = EditWin_get_bg_colour(edit_win);

  if (get_selection(editor)) {
    DEBUGF("need to show selected tiles\n");
    draw_chequered(editor, angle, scr_area, scr_orig, edit_win, false);
  } else {
    DEBUGF("plain background will suffice\n");
    fill_to_infinity(bg_colour);
  }
}

static PaletteEntry get_contrasting(EditWin const *const edit_win, const MapTexBitmaps *tiles,
                                    MapRef const tile_num, bool const is_selected)
{
  /* Use table of bright/dark tiles to determine whether white or black is higher contrast */
  bool const is_bright = is_selected ? EditWin_get_sel_tex_is_bright(edit_win, tile_num) :
                                       MapTexBitmaps_is_bright(tiles, tile_num);
  return is_bright ? PAL_BLACK : PAL_WHITE;
}

static void draw_anims(Editor *const editor,
  MapArea const *const grid_area, Vertex const scr_orig, EditWin *const edit_win)
{
  EditSession *const session = Editor_get_session(editor);
  MapEditContext const *const map = Session_get_map(session);
  ConvAnimations *const anims = map->anims;
  MapEditSelection const *const selection = get_selection(editor);

  if (!anims) {
    return;
  }

  MapEditContext const *const read_map_data = EditWin_get_read_map_ctx(edit_win);

  int const zoom = EditWin_get_zoom(edit_win);
  if (zoom > MaxDrawAnimZoom) {
    return;
  }

  Vertex const tile_size = {
      SIGNED_R_SHIFT(MapTexSize << TexelToOSCoordLog2, zoom),
      SIGNED_R_SHIFT(MapTexSize << TexelToOSCoordLog2, zoom)};

  MapAngle const angle = EditWin_get_angle(edit_win);

  MapTex *const textures = Session_get_textures(session);

  size_t const tile_count = MapTexBitmaps_get_count(&textures->tiles);
  PaletteEntry last_colour = 1; /* impossible? */
  PaletteEntry const bg_colour = EditWin_get_bg_colour(edit_win);
  PaletteEntry const bg_sel_colour = opposite_col(bg_colour);
  unsigned int const bg_brightness = palette_entry_brightness(bg_colour);
  unsigned int const bg_sel_brightness = palette_entry_brightness(bg_sel_colour);

  MapAnimsIter iter;
  for (MapPoint p = MapAnimsIter_get_first(&iter, anims, grid_area, NULL);
       !MapAnimsIter_done(&iter);
       p = MapAnimsIter_get_next(&iter, NULL))
  {
    MapPoint const scr_pos = MapLayout_rotate_map_coords_to_scr(angle, p);
    Vertex const draw_min = Vertex_add(scr_orig, Vertex_mul(MapPoint_to_vertex(scr_pos), tile_size));

    bool const inv_tile =
      (selection ? MapEditSelection_is_selected(selection, p) : false);

    PaletteEntry colour;
    MapRef tile_no = MapEdit_read_tile(read_map_data, p);
    if (map_ref_is_mask(tile_no)) {
      /* Use pre-calculated brightness of background colour (or inverted
         background) to determine whether to do white or black marker here */
      colour = (inv_tile ? bg_sel_brightness : bg_brightness) >
               MaxBrightness/2 ? PAL_BLACK : PAL_WHITE;
    } else {
      if (map_ref_to_num(tile_no) >= tile_count) {
        tile_no = map_ref_from_num(0); /* FIXME: substitute a placeholder sprite? */
      }
      colour = get_contrasting(edit_win, &textures->tiles, tile_no, inv_tile);
    }
    if (colour != last_colour) {
      plot_set_col(colour);
      last_colour = colour;
    }
    plot_move(draw_min);
    plot_fg_line_ex_end(Vertex_add(draw_min, tile_size));
    plot_move((Vertex){draw_min.x + tile_size.x - 1, draw_min.y});
    plot_fg_line((Vertex){draw_min.x, draw_min.y + tile_size.y - 1});
  }
}


typedef struct {
  MapEditContext read_map_data;
  MapEditSelection *selection;
} RedrawToSpriteData;

static DrawTilesReadResult read_map(void *const cb_arg, MapPoint const map_pos)
{
  assert(cb_arg);
  RedrawToSpriteData const *const data = cb_arg;
  return (DrawTilesReadResult){
    MapEdit_read_tile(&data->read_map_data, map_pos),
    data->selection ? MapEditSelection_is_selected(data->selection, map_pos) : false,
  };
}

static DrawTilesReadResult read_overlay(void *const cb_arg, MapPoint const map_pos)
{
  assert(cb_arg);
  RedrawToSpriteData const *const data = cb_arg;
  return (DrawTilesReadResult){
    MapEdit_read_overlay(&data->read_map_data, map_pos),
    data->selection ? MapEditSelection_is_selected(data->selection, map_pos) : false};
}

static bool draw_to_sprite(Editor *const editor,
  SprMem *const sm, Vertex const sprite_dims, int zoom,
  MapArea const *const rot_area, EditWin *const edit_win, RedrawToSpriteData *const data)
{
  zoom = HIGHEST(zoom, 0);
  MapAngle const angle = EditWin_get_angle(edit_win);

  if (!SprMem_create_sprite(sm, "RenderBuffer", false, sprite_dims, DrawTilesModeNumber))
    return false;

  EditSession *const session = Editor_get_session(editor);
  MapTex *const textures = Session_get_textures(session);

  bool const needs_mask = DrawTiles_to_sprite(&textures->tiles, sm, "RenderBuffer",
    angle, rot_area, data->read_map_data.base ? read_map : read_overlay, data,
    zoom, EditWin_get_sel_colours(edit_win));

  if (needs_mask) {
    DEBUG("Creating render buffer mask");
    if (!SprMem_create_mask(sm, "RenderBuffer"))
      return false;

    DrawTiles_to_mask(sm, "RenderBuffer", angle, rot_area, read_overlay, data, zoom);
  }

#ifndef NDEBUG
  SprMem_verify(sm);
  SprMem_save(sm, "<Wimp$ScrapDir>.RenderBuffer");
#endif

  return true;
}

static void draw_with_tiles(Editor *const editor, MapAngle const angle,
  MapArea const *const scr_area, Vertex const scr_orig, EditWin *const edit_win)
{
  assert(MapArea_is_valid(scr_area));

  /* OS_SpriteOp factors to scale sprite for current desktop mode.
     Bigger eigen values mean lower resolution, e.g. if the current screen
     mode is half the resolution of the render buffer then xdiv and ydiv are
     bigger, therefore the sprite is scaled down to use fewer pixels. */
  Vertex eigen_factors = Desktop_get_eigen_factors();
  int zoom = EditWin_get_zoom(edit_win);
  MapPoint const rot_size = MapArea_size(scr_area);
  Vertex sprite_dims = MapPoint_to_vertex(rot_size);

  int diff = 0;
  if (eigen_factors.x > TexelToOSCoordLog2 ||
      eigen_factors.y > TexelToOSCoordLog2) {
    // Desktop is lower resolution than map: decrease map resolution
    diff = LOWEST(eigen_factors.x - TexelToOSCoordLog2, eigen_factors.y - TexelToOSCoordLog2);
  } else {
    // Desktop is same or higher resolution than map: increase map resolution
    diff = -HIGHEST(TexelToOSCoordLog2 - eigen_factors.x, TexelToOSCoordLog2 - eigen_factors.y);
  }

  zoom += diff;

  ScaleFactors scale_factors = {
    .xmul = SIGNED_L_SHIFT(ScaleFactorNumerator, TexelToOSCoordLog2 + diff),
    .ymul = SIGNED_L_SHIFT(ScaleFactorNumerator, TexelToOSCoordLog2 + diff), /* render buffer */
    .xdiv = ScaleFactorNumerator << eigen_factors.x,
    .ydiv = ScaleFactorNumerator << eigen_factors.y }; /* screen */

  DEBUG("Dimensions of render buffer (in tiles) : %d,%d",
        sprite_dims.x, sprite_dims.y);

  if (/*zoom > 2*/0) {
    /* At smaller scales (including 1:8) a 1:16 bitmap (i.e. 1 pixel per tile)
    is scaled up or down as necessary */
    if (zoom > 3) {
      scale_factors.xdiv <<= zoom - EditWinZoomMax;
      scale_factors.ydiv <<= zoom - EditWinZoomMax;
    } else {
      scale_factors.xmul <<= EditWinZoomMax - zoom;
      scale_factors.ymul <<= EditWinZoomMax - zoom;
    }
  } else {
    /* Calculate dimensions of render buffer according to tile size at this scale */
    int const scaled_tile_size = MapTexSize >> HIGHEST(zoom, 0);
    if (scaled_tile_size >= 1) {
      sprite_dims.x *= scaled_tile_size;
      sprite_dims.y *= scaled_tile_size;
    }

    if (zoom < 0) {
      /* At larger scales a 1:1 bitmap (i.e. 16 pixels per tile) is scaled up
      as necessary */
      scale_factors.xmul <<= -zoom;
      scale_factors.ymul <<= -zoom;
    }
  }

  PaletteEntry const bg_colour = EditWin_get_bg_colour(edit_win);

  SprMem sm;
  if (!SprMem_init(&sm, 0)) {
    fill_to_infinity(bg_colour);
    return;
  }

  RedrawToSpriteData data = {
    .read_map_data = *EditWin_get_read_map_ctx(edit_win),
    .selection = get_selection(editor),
  };

  /* Draw textured ground map in the temporary sprite */
  if (!draw_to_sprite(editor, &sm, sprite_dims, zoom, scr_area, edit_win, &data)) {
    /* Fallback to plain colour fill */
    fill_to_infinity(bg_colour);
  } else {
    ViewDisplayFlags const display_flags = EditWin_get_display_flags(edit_win);
    if (!display_flags.MAP || !Session_has_data(Editor_get_session(editor), DataType_BaseMap)) {
      /* Draw chequered background to the screen (parts will show through) */
      assert(display_flags.MAP_OVERLAY);
      assert(Session_has_data(Editor_get_session(editor), DataType_OverlayMap));
      draw_chequered(editor, angle, scr_area, scr_orig, edit_win, true);
    }

    void *const transtable = Desktop_get_trans_table();
    Vertex const grid_size = calc_grid_size(EditWin_get_zoom(edit_win));
    Vertex const min_os = grid_to_os_coords(scr_orig, scr_area->min, grid_size);
    SprMem_plot_scaled_sprite(&sm, "RenderBuffer", min_os,
      SPRITE_ACTION_OVERWRITE|SPRITE_ACTION_USE_MASK, &scale_factors,
      transtable);

    Desktop_put_trans_table(transtable);
  }
  SprMem_destroy(&sm); /* no longer need render buffer */
}

static void MapMode_wipe_ghost(Editor *const editor)
{
  MapModeData *const mode_data = get_mode_data(editor);

  if (mode_data->pending_shape == Pending_None) {
    return;
  }

  DEBUGF("Wiping ghost tile(s)\n");
#if PENDING_IS_SELECTED
  MapEditSelection_clear(&mode_data->selection);
#else
  Editor_redraw_ghost(editor); // undraw
  Editor_clear_ghost_bbox(editor);
#endif

  mode_data->pending_shape = Pending_None;
  mode_data->pending_transfer = NULL;
}

static void update_transfer_ghost(Editor *const editor,
  MapTransfer *const transfer, MapPoint const map_pos)
{
  MapModeData *const mode_data = get_mode_data(editor);
  MapMode_wipe_ghost(editor);

  MapPoint const t_dims = MapTransfers_get_dims(transfer);
  MapPoint const t_pos_on_map = MapPoint_sub(map_pos, MapPoint_div_log2(t_dims, 1));
  mode_data->ghost_bbox = MapTransfers_get_bbox(t_pos_on_map, transfer);

#if PENDING_IS_SELECTED
  MapTransfers_select(&mode_data->selection, t_pos_on_map, transfer);
#else
  Editor_set_ghost_map_bbox(editor, &mode_data->ghost_bbox);
#endif

  mode_data->pending_shape = Pending_Transfer;
  mode_data->pending_transfer = transfer;
}

static bool paste_generic(Editor *const editor,
  MapTransfer *const transfer, MapPoint const map_pos, MapEditSelection *const selection)
{
  MapModeData *const mode_data = get_mode_data(editor);

  MapMode_wipe_ghost(editor);

  MapEditChanges_init(&mode_data->change_info);

  if (selection) {
    MapEditSelection_clear(selection);
  }

  /* Plot transfer at mouse pointer */
  EditSession *const session = Editor_get_session(editor);
  MapPoint const t_dims = MapTransfers_get_dims(transfer);
  MapPoint const t_pos_on_map = MapPoint_sub(map_pos, MapPoint_div_log2(t_dims, 1));
  bool success = MapTransfers_plot_to_map(Session_get_map(session), t_pos_on_map, transfer,
                           selection, &mode_data->change_info);

  changed_with_msg(editor);
  return success;
}

static void clear_selection_and_redraw(Editor *const editor)
{
  MapModeData *const mode_data = get_mode_data(editor);
  MapEditSelection_clear(&mode_data->selection);
}

static void set_selected_tile(Editor *const editor, MapRef tile)
{
  assert(editor);
  Palette_set_selection(&editor->palette_data, map_ref_to_num(tile));
}

static void MapMode_sample_tile(Editor *const editor, MapPoint fine_pos, MapPoint const map_pos,
  EditWin const *const edit_win)
{
  /* Sample ground map at this location */
  NOT_USED(fine_pos);
  MapMode_wipe_ghost(editor);

  MapEditContext const *const read_map_data = EditWin_get_read_map_ctx(edit_win);

  MapRef const tile = MapEdit_read_tile(read_map_data, map_pos);
  set_selected_tile(editor, tile);
}

static MapRef get_selected_tile(Editor *const editor)
{
  assert(editor);
  size_t const pal_index = Palette_get_selection(&editor->palette_data);
  return map_ref_from_num(pal_index != NULL_DATA_INDEX ? pal_index : 0);
}

static MapTransfer *get_selected_transfer(Editor *const editor)
{
  assert(editor != NULL);
  size_t const sel_index = Palette_get_selection(&editor->palette_data);
  if (sel_index == NULL_DATA_INDEX)
    return NULL;

  EditSession *const session = Editor_get_session(editor);
  MapTex *const textures = Session_get_textures(session);
  return MapTransfers_find_by_index(&textures->transfers, sel_index);
}

static void MapMode_flood_fill(Editor *const editor, MapPoint const fine_pos, MapPoint const map_pos,
  EditWin const *const edit_win)
{
  NOT_USED(fine_pos);
  NOT_USED(edit_win);
  MapModeData *const mode_data = get_mode_data(editor);

  MapMode_wipe_ghost(editor);

  /* Get tile to use as replacement from palette */
  MapRef const replace = get_selected_tile(editor);

  /* Read current tile at pointer (tile to replace) */
  EditSession *const session = Editor_get_session(editor);
  MapEditContext const *const map = Session_get_map(session);

  MapEditChanges_init(&mode_data->change_info);
  MapEdit_flood_fill(map, replace, map_pos, &mode_data->change_info);

  changed_with_msg(editor);
}

static void MapMode_global_replace(Editor *const editor, MapPoint const fine_pos,
  MapPoint const map_pos, EditWin const *const edit_win)
{
  NOT_USED(fine_pos);
  NOT_USED(edit_win);
  MapModeData *const mode_data = get_mode_data(editor);
  MapMode_wipe_ghost(editor);

  /* Get tile to use as replacement from palette */
  MapRef const replace = get_selected_tile(editor);

  /* Read current tile at pointer (tile to replace) */
  EditSession *const session = Editor_get_session(editor);
  MapEditContext const *const map = Session_get_map(session);
  MapRef const find = MapEdit_read_tile(map, map_pos);

  MapEditChanges_init(&mode_data->change_info);
  MapEdit_global_replace(map, find, replace, &mode_data->change_info);

  changed_with_msg(editor);
}

static void MapMode_start_brush(Editor *const editor, int const brush_size,
  MapPoint const map_pos)
{
  MapModeData *const mode_data = get_mode_data(editor);

  MapRef const tile = get_selected_tile(editor);

  MapEditChanges_init(&mode_data->change_info);

  EditSession *const session = Editor_get_session(editor);
  MapEditContext const *const map = Session_get_map(session);

  MapEdit_plot_circ(map, map_pos, brush_size,
    tile, &mode_data->change_info);

  changed_with_msg(editor);
}

static void MapMode_pending_brush(Editor *const editor, int const brush_size,
   MapPoint const map_pos)
{
  MapModeData *const mode_data = get_mode_data(editor);

  MapMode_wipe_ghost(editor);

#if PENDING_IS_SELECTED
  MapEditSelection_select_circ(&mode_data->selection,
    map_pos, brush_size);
#else
  mode_data->pending_vert[0] = map_pos;
  mode_data->pending_vert[1] = (MapPoint){map_pos.x, map_pos.y + brush_size};
  MapPoint const r = {brush_size, brush_size};
  mode_data->ghost_bbox = (MapArea){MapPoint_sub(map_pos, r), MapPoint_add(map_pos, r)};
  Editor_set_ghost_map_bbox(editor, &mode_data->ghost_bbox);
#endif

  mode_data->pending_shape = Pending_Circle;
}

static void MapMode_draw_brush(Editor *const editor, int const brush_size,
  MapPoint const last_map_pos, MapPoint const map_pos)
{
  MapModeData *const mode_data = get_mode_data(editor);

  EditSession *const session = Editor_get_session(editor);
  MapEditContext const *const map = Session_get_map(session);

  MapMode_wipe_ghost(editor);

  MapRef const tile = get_selected_tile(editor);

  MapEdit_plot_line(map, last_map_pos, map_pos, tile,
    brush_size, &mode_data->change_info);

  changed_with_msg(editor);
}

static void MapMode_start_snake(Editor *const editor, MapPoint const map_pos, bool const inside)
{
  MapModeData *const mode_data = get_mode_data(editor);

  size_t const snake = Palette_get_selection(&editor->palette_data);
  if (snake == NULL_DATA_INDEX) {
    return;
  }

  MapEditChanges_init(&mode_data->change_info);
  EditSession *const session = Editor_get_session(editor);
  MapEditContext const *const map = Session_get_map(session);
  MapSnakes *const snakes_data = &Session_get_textures(session)->snakes;

  MapSnakes_begin_line(&mode_data->snake_ctx, map,
    //&mode_data->selection.selection_map,
    snakes_data, map_pos, snake, inside,
    &mode_data->change_info);

  changed_with_msg(editor);
}

static void MapMode_draw_snake(Editor *const editor, MapPoint const map_pos)
{
  MapModeData *const mode_data = get_mode_data(editor);

  MapMode_wipe_ghost(editor);

  size_t const snake = Palette_get_selection(&editor->palette_data);
  if (snake == NULL_DATA_INDEX) {
    return;
  }

  MapSnakes_plot_line(&mode_data->snake_ctx, map_pos,
    &mode_data->change_info);

  changed_with_msg(editor);
}

static void smooth_selection(Editor *const editor)
{
  MapModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  MapEditContext const *const map = Session_get_map(session);
  MapTexGroups *const groups_data = &Session_get_textures(session)->groups;

  MapEditChanges_init(&mode_data->change_info);

  mode_data->lock_selection = true;
  MapEdit_smooth_selection(map, &mode_data->selection, groups_data,
                           &mode_data->change_info);
  mode_data->lock_selection = false;

  Session_redraw_pending(session, false);
  changed_with_msg(editor);
}

static void reverse_selection(Editor *const editor)
{
  MapModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  MapEditContext const *const map = Session_get_map(session);

  MapEditChanges_init(&mode_data->change_info);

  MapEdit_reverse_selected(map, &mode_data->selection, &mode_data->change_info);
  MapEdit_anims_to_map(map, &mode_data->change_info);

  Session_redraw_pending(session, false);
  changed_with_msg(editor);
}

static void MapMode_clip_overlay(Editor *const editor)
{
  assert(MapMode_can_clip_overlay(editor));
  MapModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  MapEditContext const *const map = Session_get_map(session);

  MapEditChanges_init(&mode_data->change_info);
  MapEdit_crop_overlay(map, &mode_data->change_info);
  changed_with_msg(editor);
}

typedef struct {
  MapEditContext const *map;
  MapTexGroups *groups_data;
  MapEditChanges *change_info;
} SmoothData;

static void smooth_line_area_cb(MapArea const *const area,
  void *const cb_arg)
{
  SmoothData *const data = cb_arg;

  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, area);
      !MapAreaIter_done(&iter);
      p = MapAreaIter_get_next(&iter))
  {
    MapTexGroups_smooth(data->map, data->groups_data, p, data->change_info);
  }
}

static void MapMode_start_smooth(Editor *const editor, int const wand_size,
  MapPoint const map_pos)
{
  MapModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);

  MapEditChanges_init(&mode_data->change_info);

  MapEditContext const *const map = Session_get_map(session);
  MapTexGroups *const groups_data = &Session_get_textures(session)->groups;
  SmoothData data = {map, groups_data, &mode_data->change_info};

  Shapes_circ(smooth_line_area_cb, &data, map_pos, wand_size);

  changed_with_msg(editor);
}

static void MapMode_draw_smooth(Editor *const editor, int const wand_size,
  MapPoint const last_map_pos, MapPoint const map_pos)
{
  MapModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);

  MapEditContext const *const map = Session_get_map(session);
  MapTexGroups *const groups_data = &Session_get_textures(session)->groups;
  SmoothData data = {map, groups_data, &mode_data->change_info};

  MapMode_wipe_ghost(editor);

  Shapes_line(smooth_line_area_cb, &data, last_map_pos, map_pos,
    wand_size);

  changed_with_msg(editor);
}

static void free_pending_paste(MapModeData *const mode_data)
{
  assert(mode_data);
  if (mode_data->pending_paste) {
    assert(mode_data->pending_paste != mode_data->pending_transfer);
    dfile_release(MapTransfer_get_dfile(mode_data->pending_paste));
    mode_data->pending_paste = NULL;
  }
}

static void free_dragged(MapModeData *const mode_data)
{
  assert(mode_data);
  if (mode_data->dragged) {
    assert(mode_data->dragged != mode_data->pending_transfer);
    dfile_release(MapTransfer_get_dfile(mode_data->dragged));
    mode_data->dragged = NULL;
  }
}

static void free_pending_drop(MapModeData *const mode_data)
{
  assert(mode_data);
  if (mode_data->pending_drop) {
    assert(mode_data->pending_drop != mode_data->pending_transfer);
    dfile_release(MapTransfer_get_dfile(mode_data->pending_drop));
    mode_data->pending_drop = NULL;
  }
}

static bool MapMode_start_select(Editor *const editor, bool const only_inside,
  MapPoint const fine_pos, EditWin *const edit_win)
{
  MapModeData *const mode_data = get_mode_data(editor);
  MapPoint const map_pos = MapLayout_map_coords_from_fine(EditWin_get_view(edit_win), fine_pos);

  if (only_inside) {
    return true;
  }

  MapEditSelection_invert(&mode_data->selection, map_pos);
  return true;
}

static bool MapMode_start_exclusive_select(Editor *const editor, bool const only_inside,
  MapPoint const fine_pos, EditWin *const edit_win)
{
  MapModeData *const mode_data = get_mode_data(editor);
  MapPoint const map_pos = MapLayout_map_coords_from_fine(EditWin_get_view(edit_win), fine_pos);

  if (MapEditSelection_is_selected(&mode_data->selection, map_pos)) {
    return false;
  }

  MapEditSelection_clear(&mode_data->selection);

  if (only_inside) {
    return true;
  }

  MapEditSelection_invert(&mode_data->selection, map_pos);

  return true;
}

static void MapMode_pending_transfer(Editor *const editor, MapPoint const map_pos)
{
  MapTransfer *const transfer = get_selected_transfer(editor);
  if (transfer == NULL) {
    return;
  }

  update_transfer_ghost(editor, transfer, map_pos);
}

static bool MapMode_has_selection(Editor const *const editor)
{
  MapModeData *const mode_data = get_mode_data(editor);

  return !MapEditSelection_is_none(&mode_data->selection);
}

static bool MapMode_can_edit_properties(Editor const *const editor)
{
  MapModeData *const mode_data = get_mode_data(editor);
  return MapEditSelection_size(&mode_data->selection) == 1;
}

static void MapMode_edit_properties(Editor *const editor, EditWin *const edit_win)
{
  assert(MapMode_can_edit_properties(editor));
  MapModeData *const mode_data = get_mode_data(editor);
  MapEditSelIter iter;
  MapPoint const pos = MapEditSelIter_get_first(&iter, &mode_data->selection);
  assert(!MapEditSelIter_done(&iter));
  MapPropDboxes_open(&mode_data->prop_dboxes, pos, edit_win);
}

static bool MapMode_can_smooth(Editor const *const editor)
{
  MapModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);

  return !MapEditSelection_is_none(&mode_data->selection) &&
         MapTexGroups_get_count(&Session_get_textures(session)->groups) != 0;
}

static void edit_properties_at_pos(Editor *const editor, MapPoint const fine_pos, EditWin *const edit_win)
{
  MapModeData *const mode_data = get_mode_data(editor);
  MapPoint const map_pos = MapLayout_map_coords_from_fine(EditWin_get_view(edit_win), fine_pos);
  MapPropDboxes_open(&mode_data->prop_dboxes, map_pos, edit_win);
}

static void MapMode_pending_point(Editor *const editor, MapPoint const map_pos)
{
  MapModeData *const mode_data = get_mode_data(editor);

  MapMode_wipe_ghost(editor);

  mode_data->pending_vert[0] = map_pos;
  mode_data->ghost_bbox = (MapArea){map_pos, map_pos};

#if PENDING_IS_SELECTED
  MapEditSelection_select(&mode_data->selection, map_pos);
#else
  Editor_set_ghost_map_bbox(editor, &mode_data->ghost_bbox);
#endif

  mode_data->pending_shape = Pending_Point;
}

static void MapMode_pending_fill(Editor *const editor, MapPoint const fine_pos,
  MapPoint const map_pos, EditWin const *const edit_win)
{
  NOT_USED(edit_win);
  NOT_USED(fine_pos);
  MapMode_pending_point(editor, map_pos);
}

static void MapMode_pending_line(Editor *const editor, MapPoint const a, MapPoint const b)
{
  MapModeData *const mode_data = get_mode_data(editor);
  assert(Editor_get_tool(editor) == EDITORTOOL_PLOTSHAPES);

  MapMode_wipe_ghost(editor);

#if PENDING_IS_SELECTED
  MapEditSelection_select_line(&mode_data->selection, a, b, 0);
#else
  mode_data->pending_vert[0] = a;
  mode_data->pending_vert[1] = b;
  MapArea_from_points(&mode_data->ghost_bbox, a, b);
  Editor_set_ghost_map_bbox(editor, &mode_data->ghost_bbox);
#endif

  mode_data->pending_shape = Pending_Line;
}

static void MapMode_plot_line(Editor *const editor, MapPoint const a, MapPoint const b)
{
  MapModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  assert(Editor_get_plot_shape(editor) == PLOTSHAPE_LINE);

  MapEditChanges_init(&mode_data->change_info);

  MapMode_wipe_ghost(editor);

  MapRef const tile = get_selected_tile(editor);
  MapEditContext const *const map = Session_get_map(session);
  MapEdit_plot_line(map, a, b, tile, 0, &mode_data->change_info);
  changed_with_msg(editor);
}

static void MapMode_pending_tri(Editor *const editor,
  MapPoint const a, MapPoint const b, MapPoint const c)
{
  MapModeData *const mode_data = get_mode_data(editor);
  assert(Editor_get_tool(editor) == EDITORTOOL_PLOTSHAPES);

  MapMode_wipe_ghost(editor);

#if PENDING_IS_SELECTED
  MapEditSelection_select_tri(&mode_data->selection, a, b, c);
#else
  mode_data->pending_vert[0] = a;
  mode_data->pending_vert[1] = b;
  mode_data->pending_vert[2] = c;
  MapArea_from_points(&mode_data->ghost_bbox, a, b);
  MapArea_expand(&mode_data->ghost_bbox, c);
  Editor_set_ghost_map_bbox(editor, &mode_data->ghost_bbox);
#endif

  mode_data->pending_shape = Pending_Triangle;
}

static void MapMode_plot_tri(Editor *const editor,
  MapPoint const a, MapPoint const b, MapPoint const c)
{
  MapModeData *const mode_data = get_mode_data(editor);
  assert(Editor_get_plot_shape(editor) == PLOTSHAPE_TRIANGLE);
  EditSession *const session = Editor_get_session(editor);

  MapMode_wipe_ghost(editor);

  MapRef const tile = get_selected_tile(editor);
  MapEditContext const *const map = Session_get_map(session);
  MapEditChanges_init(&mode_data->change_info);
  MapEdit_plot_tri(map, a, b, c, tile,
                   &mode_data->change_info);
  changed_with_msg(editor);
}

static void MapMode_pending_rect(Editor *const editor, MapPoint const a, MapPoint const b)
{
  MapModeData *const mode_data = get_mode_data(editor);
  assert(Editor_get_tool(editor) == EDITORTOOL_PLOTSHAPES);

  MapMode_wipe_ghost(editor);

#if PENDING_IS_SELECTED
  MapEditSelection_select_rect(&mode_data->selection, a, b);
#else
  mode_data->pending_vert[0] = a;
  mode_data->pending_vert[1] = b;
  MapArea_from_points(&mode_data->ghost_bbox, a, b);
  Editor_set_ghost_map_bbox(editor, &mode_data->ghost_bbox);
#endif
  mode_data->pending_shape = Pending_Rectangle;
}

static void MapMode_plot_rect(Editor *const editor, MapPoint const a, MapPoint const b)
{
  MapModeData *const mode_data = get_mode_data(editor);
  assert(Editor_get_plot_shape(editor) == PLOTSHAPE_RECTANGLE);
  EditSession *const session = Editor_get_session(editor);

  MapMode_wipe_ghost(editor);

  MapRef const tile = get_selected_tile(editor);
  MapEditContext const *const map = Session_get_map(session);
  MapEditChanges_init(&mode_data->change_info);
  MapEdit_plot_rect(map, a, b,
                    tile, &mode_data->change_info);
  changed_with_msg(editor);
}

static void MapMode_pending_circ(Editor *const editor, MapPoint const a, MapPoint const b)
{
  MapModeData *const mode_data = get_mode_data(editor);
  assert(Editor_get_plot_shape(editor) == PLOTSHAPE_CIRCLE);

  MapMode_wipe_ghost(editor);

#if PENDING_IS_SELECTED
  MapEditSelection_select_circ(&mode_data->selection,
    a, MapPoint_dist(a, b));
#else
  mode_data->pending_vert[0] = a;
  mode_data->pending_vert[1] = b;

  MapCoord const radius = MapPoint_dist(a, b);

  MapArea_from_points(&mode_data->ghost_bbox, MapPoint_sub(a, (MapPoint){radius,radius}),
     MapPoint_add(a, (MapPoint){radius,radius}));
  Editor_set_ghost_map_bbox(editor, &mode_data->ghost_bbox);
#endif

  mode_data->pending_shape = Pending_Circle;
}

static void MapMode_plot_circ(Editor *const editor, MapPoint const a, MapPoint const b)
{
  MapModeData *const mode_data = get_mode_data(editor);
  assert(Editor_get_plot_shape(editor) == PLOTSHAPE_CIRCLE);
  EditSession *const session = Editor_get_session(editor);

  MapMode_wipe_ghost(editor);

  MapRef const tile = get_selected_tile(editor);
  MapEditContext const *const map = Session_get_map(session);
  MapEditChanges_init(&mode_data->change_info);
  MapEdit_plot_circ(map, a, MapPoint_dist(a, b),
    tile, &mode_data->change_info);

  changed_with_msg(editor);
}

static MapTransfer *clipboard;

static bool cb_copy_core(Editor *const editor)
{
  MapModeData *const mode_data = get_mode_data(editor);
  assert(!MapEditSelection_is_none(&mode_data->selection));

  EditSession *const session = Editor_get_session(editor);
  assert(!clipboard);
  clipboard = MapTransfers_grab_selection(
    Session_get_map(session), &mode_data->selection);

  return clipboard != NULL;
}

static void cb_status(Editor *const editor, bool const copy)
{
  MapModeData *const mode_data = get_mode_data(editor);

  size_t const tiles_count = MapEditSelection_size(&mode_data->selection);
  char tiles_count_str[16];
  sprintf(tiles_count_str, "%ld",
           tiles_count);

  size_t const anim_count = MapTransfers_get_anim_count(clipboard);

  if (anim_count > 0) {
    char anim_count_str[16];
    sprintf(anim_count_str, "%zu", anim_count);

    Editor_display_msg(editor, msgs_lookup_subn(copy ? "MStatusCopy2" :
                       "MStatusCut2", 2, tiles_count_str, anim_count_str), true);
  } else {
    Editor_display_msg(editor, msgs_lookup_subn(copy ? "MStatusCopy1" :
                       "MStatusCut1", 1, tiles_count_str), true);
  }
}

static void delete_selected_anims(Editor *const editor)
{
  MapModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);

  MapEditChanges_init(&mode_data->change_info);
  MapEditContext const *const map = Session_get_map(session);

  MapEdit_delete_selected(map, &mode_data->selection,
    &mode_data->change_info);

  changed_with_msg(editor);
  Session_redraw_pending(session, false);
}

static void MapMode_paint_selected(Editor *const editor)
{
  MapModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);

  MapRef const tile = get_selected_tile(editor);

  MapEditChanges_init(&mode_data->change_info);
  MapEditContext const *const map = Session_get_map(session);

  mode_data->lock_selection = true;
  MapEdit_fill_selection(map, &mode_data->selection, tile,
    &mode_data->change_info);
  mode_data->lock_selection = false;

  changed_with_msg(editor);
}

static void MapMode_draw_transfer(Editor *const editor, MapPoint const map_pos)
{
  MapTransfer *const s = get_selected_transfer(editor);
  if (s != NULL) {
    paste_generic(editor, s, map_pos, NULL);
  }
}

static MapPoint MapMode_map_to_grid_coords(MapPoint const pos, EditWin const *const edit_win)
{
  return MapLayout_map_coords_from_fine(EditWin_get_view(edit_win), pos);
}

MapArea MapMode_map_to_grid_area(MapArea const *const map_area, EditWin const *const edit_win)
{
  return MapLayout_map_area_from_fine(EditWin_get_view(edit_win), map_area);
}

static MapPoint MapMode_grid_to_map_coords(MapPoint const pos, EditWin const *const edit_win)
{
  return MapLayout_map_coords_to_centre(EditWin_get_view(edit_win), pos);
}

/* ----------------- Public functions ---------------- */

bool MapMode_set_properties(Editor *const editor, MapPoint const pos, MapAnimParam const anim)
{
  size_t nm_count = 0;
  MapRef tile = map_ref_mask();
  for (size_t frame = 0; frame < ARRAY_SIZE(anim.tiles); ++frame) {
    if (!map_ref_is_mask(anim.tiles[frame])) {
      tile = anim.tiles[frame];
      ++nm_count;
    }
  }
  DEBUG("%zu non-skipped animation frames", nm_count);
  if (nm_count == 0) {
    return false;
  }

  MapModeData *const mode_data = get_mode_data(editor);
  MapEditChanges_init(&mode_data->change_info);

  EditSession *const session = Editor_get_session(editor);
  MapEditContext const *const map = Session_get_map(session);

  mode_data->lock_selection = true;
  if (map->anims && nm_count > 1) {
    if (!MapEdit_write_anim(map, pos, anim, &mode_data->change_info)) {
      return false;
    }
    MapEdit_anims_to_map(map, &mode_data->change_info);
  } else {
    MapEdit_write_tile(map, pos, tile, &mode_data->change_info);
  }
  mode_data->lock_selection = false;

  changed_with_msg(editor);
  Session_redraw_pending(session, false);
  return true;
}

static void MapMode_create_transfer(Editor *const editor, char const *const name)
{
  MapModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);

  /* Check for an existing transfer with the same name */
  MapTex *const textures = Session_get_textures(session);

  size_t replace_index;
  MapTransfer * const replace_transfer = MapTransfers_find_by_name(
    &textures->transfers, name, &replace_index);

  if (replace_transfer != NULL) {
    if (!dialogue_confirm(msgs_lookup_subn("DupTransferName", 1,
         get_leaf_name(MapTransfer_get_dfile(replace_transfer))), "OvBut"))
      return; /* no user confirmation */
  }

  /* Grab transfer from selected tiles on map */
  MapTransfer *const transfer = MapTransfers_grab_selection(
    Session_get_map(session), &mode_data->selection);

  if (!transfer) {
    return; /* failed  */
  }

  /* Add the new transfer to the linked list for this tiles set */
  size_t new_index;
  if (!MapTransfers_add(&textures->transfers, transfer, name,
                     &new_index, &textures->tiles)) {
    dfile_release(MapTransfer_get_dfile(transfer));
    return; /* failed */
  }

  /* Update the palettes */
  if (replace_transfer == NULL) {
    Session_all_textures_changed(textures, EDITOR_CHANGE_TEX_TRANSFER_ADDED,
      &(EditorChangeParams){.transfer_added.index = new_index});
  } else {
    assert(new_index == replace_index);
    Session_all_textures_changed(textures, EDITOR_CHANGE_TEX_TRANSFER_REPLACED,
      &(EditorChangeParams){.transfer_replaced.index = replace_index});
  }

  create_trans_msg(editor, transfer);
}

static bool MapMode_auto_select(Editor *const editor, MapPoint const fine_pos, EditWin *const edit_win)
{
  NOT_USED(edit_win);
  MapModeData *const mode_data = get_mode_data(editor);
  MapPoint const map_pos = MapLayout_map_coords_from_fine(EditWin_get_view(edit_win), fine_pos);

#if PENDING_IS_SELECTED
  /* Map mode uses the selection bitmap to show a pending paste. */
  if (mode_data->pending_paste) {
    return false;
  }
#endif

  if (!MapEditSelection_is_none(&mode_data->selection) ||
      Editor_get_tool(editor) != EDITORTOOL_SELECT)
    return false; /* already have a selection or not using that tool */

  MapEditSelection_select(&mode_data->selection, map_pos);
  return true;
}

static void MapMode_auto_deselect(Editor *const editor)
{
#if PENDING_IS_SELECTED
  /* Map mode uses the selection bitmap to show a pending paste. */
  MapModeData *const mode_data = get_mode_data(editor);
  if (mode_data->pending_paste) {
    return;
  }
#endif
  clear_selection_and_redraw(editor);
}

static MapArea select_box_to_map_area(MapArea const *const select_box,
  bool const only_inside, EditWin const *const edit_win)
{
  View const *const view = EditWin_get_view(edit_win);
  return only_inside ? MapLayout_map_area_inside_from_fine(view, select_box):
                       MapLayout_map_area_from_fine(view, select_box);
}

static void MapMode_update_select(Editor *const editor, bool const only_inside,
  MapArea const *const last_select_box, MapArea const *const select_box,
  EditWin const *const edit_win)
{
  MapModeData *const mode_data = get_mode_data(editor);

  // Undo the current selection bounding box by inverting the state of tiles within it
  MapArea last_map_area = select_box_to_map_area(last_select_box, only_inside, edit_win);
  bool const last_is_valid = MapArea_is_valid(&last_map_area);
  if (last_is_valid) {
    MapEditSelection_invert_rect(&mode_data->selection,
      last_map_area.min, last_map_area.max, false);
  }

  // Apply the new selection bounding box by inverting the state of tiles within it
  MapArea map_area = select_box_to_map_area(select_box, only_inside, edit_win);
  bool const new_is_valid = MapArea_is_valid(&map_area);
  if (new_is_valid) {
    MapEditSelection_invert_rect(&mode_data->selection,
      map_area.min, map_area.max, false);
  }

  if (!last_is_valid) {
    if (new_is_valid) {
      redraw_selection(&map_area, editor);
    }
  } else if (!new_is_valid) {
    if (last_is_valid) {
      redraw_selection(&last_map_area, editor);
    }
  } else {
    // Redraw only any changes in selection state at the borders
    MapArea_split_diff(&last_map_area, &map_area, redraw_selection, editor);
  }
}

static void MapMode_cancel_select(Editor *const editor, bool const only_inside,
  MapArea const *const last_select_box, EditWin *const edit_win)
{
  /* Abort selection drag by undoing effect of last rectangle */
  MapModeData *const mode_data = get_mode_data(editor);

  MapArea const map_area = select_box_to_map_area(last_select_box, only_inside, edit_win);
  if (MapArea_is_valid(&map_area)) {
    MapEditSelection_invert_rect(&mode_data->selection,
      map_area.min, map_area.max, true);
  }
}

static int MapMode_misc_event(Editor *const editor, int const event_code)
{
  switch (event_code)
  {
    case EVENT_DELETE_SEL_ANIMS:
      if (!MapMode_anim_is_selected(editor)) {
        putchar('\a'); /* no map area selected */
      } else {
        delete_selected_anims(editor);
      }
      return 1; /* claim event */

    case EVENT_SMOOTH_SEL:
      if (!MapMode_can_smooth(editor)) {
        putchar('\a'); /* no map area selected */
      } else {
        smooth_selection(editor);
      }
      return 1; /* claim event */

    case EVENT_REVERSE_ANIMS:
      if (!MapMode_anim_is_selected(editor)) {
        putchar('\a'); /* no map area selected */
      } else {
        reverse_selection(editor);
      }
      return 1; /* claim event */

    default:
      break;
  }

  return 0; /* not interested */
}

static char *MapMode_get_help_msg(Editor const *const editor)
{
  char *msg = NULL; // remove help
  char size_string[4] = "";
  MapModeData *const mode_data = editor->editingmode_data;

  switch (Editor_get_tool(editor)) {
    case EDITORTOOL_BRUSH:
      sprintf(size_string, "%d",
        (Editor_get_brush_size(editor) * 2) + 1);

      msg = msgs_lookup_subn("MapTexBrush", 1, size_string);
      break;

    case EDITORTOOL_SNAKE:
      msg = msgs_lookup("MapTexSnake");
      break;

    case EDITORTOOL_SMOOTHWAND:
      sprintf(size_string, "%d",
        (Editor_get_wand_size(editor) * 2) + 1);

      msg = msgs_lookup_subn("MapTexWand", 1, size_string);
      break;

    case EDITORTOOL_TRANSFER:
      msg = msgs_lookup("MapTransfer");
      break;

    case EDITORTOOL_SELECT:
      msg = msgs_lookup(mode_data->pending_paste ? "MapTexPaste" : "MapTexSelect");
      break;

    case EDITORTOOL_SAMPLER:
      msg = msgs_lookup("MapTexSample");
      break;

    default:
      break;
  }
  return msg;
}

static bool MapMode_start_pending_paste(Editor *const editor, Reader *const reader,
                   int const estimated_size,
                   DataType const data_type, char const *const filename)
{
  NOT_USED(estimated_size);
  NOT_USED(data_type);
  MapModeData *const mode_data = get_mode_data(editor);

  free_pending_paste(mode_data);
  mode_data->pending_paste = MapTransfer_create();
  if (mode_data->pending_paste == NULL) {
    return false;
  }

  SFError err = read_compressed(MapTransfer_get_dfile(mode_data->pending_paste),
                                reader);
  if (err.type == SFErrorType_TransferNot) {
    err = SFERROR(CBWrong);
  }

  if (report_error(err, filename, "")) {
    free_pending_paste(mode_data);
    return false;
  }

  return true;
}

static void MapMode_pending_paste(Editor *const editor, MapPoint const map_pos)
{
  MapModeData *const mode_data = get_mode_data(editor);
  assert(mode_data->pending_paste);

  update_transfer_ghost(editor, mode_data->pending_paste, map_pos);
}

static bool MapMode_draw_paste(Editor *const editor, MapPoint const map_pos)
{
  MapModeData *const mode_data = get_mode_data(editor);
  assert(mode_data->pending_paste);

  if (!paste_generic(editor, mode_data->pending_paste, map_pos, &mode_data->selection)) {
    return false;
  }
  free_pending_paste(mode_data);
  return true;
}

static void MapMode_cancel_paste(Editor *const editor)
{
  /* Abort pending paste from clipboard */
  MapModeData *const mode_data = get_mode_data(editor);
  if (!mode_data->pending_paste) {
    return;
  }

  MapMode_wipe_ghost(editor);
  free_pending_paste(mode_data);
}

static void MapMode_tool_selected(Editor *const editor)
{
  assert(Editor_get_edit_mode(editor) == EDITING_MODE_MAP);

  assert(editor);
  MapModeData *const mode_data = editor->editingmode_data;

  MapMode_wipe_ghost(editor);

  switch (Editor_get_tool(editor))
  {
    case EDITORTOOL_SNAKE:
      /* Configure palette to display snakes */
      if (mode_data->palette_type != MAPPALETTE_TYPE_SNAKES)
      {
        MapSnakesPalette_register(&editor->palette_data);
        mode_data->palette_type = MAPPALETTE_TYPE_SNAKES;
      }
      break;

    case EDITORTOOL_TRANSFER:
      /* Configure palette for transfers */
      if (mode_data->palette_type != MAPPALETTE_TYPE_TRANSFERS)
      {
        MapTransfersPalette_register(&editor->palette_data);
        mode_data->palette_type = MAPPALETTE_TYPE_TRANSFERS;
      }
      break;

    default:
      /* Configure palette to display map tiles */
      if (mode_data->palette_type != MAPPALETTE_TYPE_TILES)
      {
        TilesPalette_register(&editor->palette_data);
        mode_data->palette_type = MAPPALETTE_TYPE_TILES;
      }
      break;
  }
}

static void MapMode_leave(Editor *const editor)
{
  /* Marines! - WE ARE LEAVING!!! */
  DEBUG("leaving map mode");
  MapModeData *const mode_data = get_mode_data(editor);

  MapPropDboxes_destroy(&mode_data->prop_dboxes);

  MapEditSelection_destroy(&mode_data->selection);
  free_dragged(mode_data);
  free_pending_drop(mode_data);
  free_pending_paste(mode_data);
  free(mode_data);
}

static void MapMode_resource_change(Editor *const editor, EditorChange const event,
  EditorChangeParams const *const params)
{
  MapModeData *const mode_data = get_mode_data(editor);

  switch (event) {
  case EDITOR_CHANGE_TEX_ALL_RELOADED:
    Palette_reinit(&editor->palette_data);
    break;

  case EDITOR_CHANGE_TEX_TRANSFERS_RELOADED:
    if (mode_data->palette_type == MAPPALETTE_TYPE_TRANSFERS) {
      if (mode_data->pending_shape == Pending_Transfer) {
        MapMode_wipe_ghost(editor);
      }
      Palette_reinit(&editor->palette_data);
    }
    break;

  case EDITOR_CHANGE_TEX_SNAKES_RELOADED:
    if (mode_data->palette_type == MAPPALETTE_TYPE_SNAKES) {
      Palette_reinit(&editor->palette_data);
    }
    break;

  case EDITOR_CHANGE_TEX_GROUPS_RELOADED:
    if (mode_data->palette_type == MAPPALETTE_TYPE_TILES) {
      Palette_reinit(&editor->palette_data);
    }
    break;

  case EDITOR_CHANGE_TEX_TRANSFER_ADDED:
    assert(params);
    if (mode_data->palette_type == MAPPALETTE_TYPE_TRANSFERS) {
      Palette_object_added(&editor->palette_data, params->transfer_added.index);
    }
    break;

  case EDITOR_CHANGE_TEX_TRANSFER_DELETED:
    assert(params);
    if (mode_data->palette_type == MAPPALETTE_TYPE_TRANSFERS) {
      if (Palette_get_selection(&editor->palette_data) == params->transfer_deleted.index) {
        if (mode_data->pending_shape == Pending_Transfer) {
          MapMode_wipe_ghost(editor);
        }
      }
      Palette_object_deleted(&editor->palette_data, params->transfer_deleted.index);
    }
    break;

  case EDITOR_CHANGE_TEX_TRANSFER_ALL_DELETED:
    if (mode_data->palette_type == MAPPALETTE_TYPE_TRANSFERS) {
      if (mode_data->pending_shape == Pending_Transfer) {
        MapMode_wipe_ghost(editor);
      }
      Palette_object_deleted(&editor->palette_data, NULL_DATA_INDEX);
    }
    break;

  case EDITOR_CHANGE_TEX_TRANSFER_REPLACED:
    assert(params);
    if (mode_data->palette_type == MAPPALETTE_TYPE_TRANSFERS) {
      if (Palette_get_selection(&editor->palette_data) == params->transfer_replaced.index) {
        if (mode_data->pending_shape == Pending_Transfer) {
          MapMode_wipe_ghost(editor);
        }
      }
      Palette_redraw_name(&editor->palette_data, params->transfer_replaced.index);
      Palette_redraw_object(&editor->palette_data, params->transfer_replaced.index);
    }
    break;

  case EDITOR_CHANGE_TEX_TRANSFER_RENAMED:
    assert(params);
    if (mode_data->palette_type == MAPPALETTE_TYPE_TRANSFERS) {
      if (params->transfer_renamed.index == params->transfer_renamed.new_index) {
        Palette_redraw_name(&editor->palette_data, params->transfer_renamed.index);
      } else {
        Palette_object_moved(&editor->palette_data, params->transfer_renamed.index,
                             params->transfer_renamed.new_index);
      }
    }
    break;

  case EDITOR_CHANGE_MAP_ALL_REPLACED:
    MapEditSelection_clear(&mode_data->selection);
    MapPropDboxes_destroy(&mode_data->prop_dboxes);
    MapPropDboxes_init(&mode_data->prop_dboxes, editor);
    break;

  case EDITOR_CHANGE_MAP_PRECHANGE:
    assert(params);
    if (!mode_data->lock_selection) {
      MapEditSelection_deselect_area(&mode_data->selection,
        &params->map_prechange.bbox);
    }

    MapPropDboxes_update_for_del(&mode_data->prop_dboxes, &params->map_prechange.bbox);
    break;

  case EDITOR_CHANGE_MAP_PREMOVE:
    assert(params);
    if (MapEditSelection_is_selected(&mode_data->selection, params->map_premove.old_pos))
    {
      MapEditSelection_deselect(&mode_data->selection, params->map_premove.old_pos);
      MapEditSelection_select(&mode_data->selection, params->map_premove.new_pos);
    }

    MapPropDboxes_update_for_del(&mode_data->prop_dboxes,
                                 &(MapArea){params->map_premove.new_pos,
                                            params->map_premove.new_pos});

    MapPropDboxes_update_for_move(&mode_data->prop_dboxes, params->map_premove.old_pos,
                                  params->map_premove.new_pos);
    break;

  default:
    break;
  }
}

static void MapMode_palette_selection(Editor *const editor, size_t const object)
{
  MapModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  MapTex *const textures = Session_get_textures(session);
  char const *msg = "";


  switch (mode_data->palette_type) {
    case MAPPALETTE_TYPE_SNAKES:
    {
      char snake_name[16];
      MapSnakes_get_name(&textures->snakes, object, snake_name, sizeof(snake_name));
      msg = msgs_lookup_subn("StatusSnSel", 1, snake_name);
      break;
    }

    case MAPPALETTE_TYPE_TILES:
    {
      char tile_num_as_string[12];
      sprintf(tile_num_as_string, "%zu", object);

      assert(editor != NULL);
      msg = msgs_lookup_subn("StatusTiSel", 1, tile_num_as_string);
      break;
    }

    default:
    {
      assert(mode_data->palette_type == MAPPALETTE_TYPE_TRANSFERS);
      MapTransfer *const transfer = MapTransfers_find_by_index(
        &textures->transfers, object);

      assert(transfer != NULL);
      msg = msgs_lookup_subn("StatusTrSel", 1,
        get_leaf_name(MapTransfer_get_dfile(transfer)));
      break;
    }
  }

  Editor_display_msg(editor, msg, true);

}

static bool MapMode_can_draw_numbers(Editor *editor, EditWin const *const edit_win)
{
  NOT_USED(editor);
  return EditWin_get_zoom(edit_win) <= 0;
}

static void MapMode_draw_numbers(Editor *const editor,
  Vertex const scr_orig, MapArea const *const redraw_area, EditWin const *const edit_win)
{
  assert(MapMode_can_draw_numbers(editor, edit_win));
  EditSession *const session = Editor_get_session(editor);

  if (!Session_has_data(session, DataType_BaseMap) &&
      !Session_has_data(session, DataType_OverlayMap))
    return; /* nothing to plot */

  int const zoom = EditWin_get_zoom(edit_win);
  Vertex const grid_size = calc_grid_size(zoom);

  MapEditSelection const *const selection = get_selection(editor);
  bool const may_blend_to_bg = plot_can_blend_font();

  int handle;
  Vertex const font_size = {
    SIGNED_R_SHIFT(6, zoom),
    SIGNED_R_SHIFT(12, zoom)
  };
  if (!plot_find_font(font_size, &handle)) {
    return;
  }

  BBox char_bbox;
  plot_get_char_bbox(handle, &char_bbox);
  Vertex string_size = {.y = char_bbox.ymax - char_bbox.ymin};
  DEBUG("Max height of font is %d", string_size.y);

  /* Calculate which rows and columns to redraw */
  MapArea const scr_area = MapLayout_scr_area_from_fine(EditWin_get_view(edit_win), redraw_area);

  Vertex coord = {
    .y = scr_orig.y + (scr_area.min.y * grid_size.y) + (grid_size.y / 2l)
  };
  size_t last_tile = SIZE_MAX;
  bool blend = false;
  PaletteEntry const bg_colour = EditWin_get_bg_colour(edit_win);
  PaletteEntry const bg_sel_colour = opposite_col(bg_colour);
  unsigned int const bg_brightness = palette_entry_brightness(bg_colour);
  unsigned int const bg_sel_brightness = palette_entry_brightness(bg_sel_colour);

  MapTex *const textures = Session_get_textures(session);

  PaletteEntry last_bg_colour = 1, last_fg_colour = 1; /* impossible? */

  MapEditContext const *const map = Session_get_map(session);
  MapEditContext const *const read_map_data = EditWin_get_read_map_ctx(edit_win);

  char string[4];
  char underline[sizeof(string)] = "";
  size_t last_ulen = 0;

  MapAngle const angle = EditWin_get_angle(edit_win);
  size_t const tile_count = MapTexBitmaps_get_count(&textures->tiles);

  for (MapPoint scr_pos = {.y = scr_area.min.y};
       scr_pos.y <= scr_area.max.y;
       scr_pos.y++, coord.y += grid_size.y)
  {
    coord.x = scr_orig.x + (scr_area.min.x * grid_size.x) + (grid_size.x / 2l);

    for (scr_pos.x = scr_area.min.x;
         scr_pos.x <= scr_area.max.x;
         scr_pos.x++, coord.x += grid_size.x)
    {
      MapPoint const map_pos = MapLayout_derotate_scr_coords_to_map(angle, scr_pos);
      PaletteEntry font_fg_colour, font_bg_colour;
      MapRef tile_no = MapEdit_read_tile(read_map_data, map_pos);

      bool const inv_tile =
        (selection ? MapEditSelection_is_selected(selection, map_pos) : false);
#if 0
      if (map_ref_is_mask(tile_no)) {
        continue; /* no base map loaded - skip this grid location */
      }
#endif

      /* Only generate text string if different from last map location */
      size_t const this_tile = map_ref_to_num(tile_no);
      if (last_tile != this_tile) {
        /* Generate string and calculate width */
        sprintf(string, "%zu", this_tile);
        string_size.x = plot_get_font_width(handle, string);
        last_tile = this_tile;
      }

      tile_no = MapEdit_read_tile(read_map_data, map_pos);

      if (map_ref_is_mask(tile_no)) {
        /* Safe to do normal anti-aliasing to plain background
        (map loc'n is mask value) */
        blend = false;

        /* Use pre-calculated brightness of background colour (or inverted
           background) to determine whether to do white or black number here */
        font_bg_colour = inv_tile ? bg_sel_colour : bg_colour;
        font_fg_colour = (inv_tile ? bg_sel_brightness : bg_brightness) >
                         MaxBrightness/2 ? PAL_BLACK : PAL_WHITE;
      } else {
        /* Check for tile out of range for this graphics set */
        assert(textures != NULL);
        if (map_ref_to_num(tile_no) >= tile_count) {
          tile_no = map_ref_from_num(0); /* FIXME: substitute a placeholder sprite? */
        }

        /* Need background blending to paint over tile graphic */
        if (may_blend_to_bg) {
          blend = true;
        }

        font_fg_colour = font_bg_colour = get_contrasting(edit_win, &textures->tiles, tile_no, inv_tile);
      }

      /* Only set font colours if different from last map location */
      if (font_bg_colour != last_bg_colour ||
          font_fg_colour != last_fg_colour) {
        plot_set_font_col(handle, font_bg_colour, font_fg_colour);
        last_bg_colour = font_bg_colour;
        last_fg_colour = font_fg_colour;
      }

      /* Calculate coordinates at which to plot numbers
         (centred within the corresponding grid location) */
      Vertex const font_coord = {
        coord.x - string_size.x / 2,
        coord.y - string_size.y / 4
      };

      plot_font(handle, string, NULL, font_coord, blend);

      if (map->anims && MapAnims_check_locn(map->anims, map_pos)) {
        size_t const ulen = strlen(string);
        if (ulen != last_ulen) {
          memset(underline, '_', ulen);
          underline[ulen] = '\0';
          last_ulen = ulen;
        }
        plot_font(handle, underline, NULL, font_coord, blend);
      }
    } /* next scr_pos.x */
  } /* next scr_pos.y */

  plot_lose_font(handle);
}

static bool MapMode_can_draw_grid(Editor *editor, EditWin const *const edit_win)
{
  NOT_USED(editor);
  return EditWin_get_zoom(edit_win) <= 1;
}

static void MapMode_draw_grid(Vertex const scr_orig,
  MapArea const *const redraw_area, EditWin const *edit_win)
{
  assert(MapMode_can_draw_grid(EditWin_get_editor(edit_win), edit_win));
  PaletteEntry const colour = EditWin_get_grid_colour(edit_win);
  int const zoom = EditWin_get_zoom(edit_win);

  Vertex const grid_size = calc_grid_size(zoom);

  /* Calculate which rows and columns to redraw */
  MapArea const scr_area = MapLayout_scr_area_from_fine(EditWin_get_view(edit_win), redraw_area);

  plot_set_col(colour);

  Vertex const min_os = grid_to_os_coords(scr_orig, scr_area.min, grid_size);

  Vertex line_start = {
    min_os.x,
    SHRT_MIN
  };

  Vertex line_end = {
    min_os.x,
    SHRT_MAX
  };

  for (MapCoord x_grid = scr_area.min.x; x_grid <= scr_area.max.x; x_grid++) {
    plot_move(line_start);
    plot_fg_line(line_end);

    line_start.x += grid_size.x;
    line_end.x += grid_size.x;
  } /* next x_grid */

  line_start.x = SHRT_MIN;
  line_start.y = line_end.y = min_os.y;
  line_end.x = SHRT_MAX;

  for (MapCoord y_grid = scr_area.min.y; y_grid <= scr_area.max.y; y_grid++) {
    plot_move(line_start);
    plot_fg_line(line_end);

    line_start.y += grid_size.y;
    line_end.y += grid_size.y;
  } /* next y_grid */
}

#if !PENDING_IS_SELECTED
typedef struct {
  MapTransfer *transfer;
  MapArea transfer_area;
  Vertex min_os;
} DrawTransferShadow;

static DrawTilesReadResult ghost_paste_read(void *const cb_arg, MapPoint map_pos)
{
  assert(cb_arg);
  const DrawTransferShadow *const args = cb_arg;

  if (!map_bbox_contains(&args->transfer_area, map_pos)) {
    return (DrawTilesReadResult){map_ref_mask()};
  }

  map_pos = map_wrap_coords(map_pos);
  MapPoint const min = map_wrap_coords(args->transfer_area.min);

  if (min.x > map_pos.x) {
    map_pos.x += Map_Size;
  }

  if (min.y > map_pos.y) {
    map_pos.y += Map_Size;
  }
  MapRef tile = MapTransfers_read_ref(args->transfer, MapPoint_sub(map_pos, min));

  // Treat all non-mask values as equivalent for efficient bbox generation
  if (!map_ref_is_mask(tile)) {
    tile = map_ref_from_num(0);
  }
  return (DrawTilesReadResult){tile};
}

static void write_ghost(BBox const *const bbox, Vertex const min_os)
{
  // Just draw horizontal lines instead of trying to represent individual tiles
  BBox trans_bbox;
  BBox_translate(bbox, min_os, &trans_bbox);

  int const step = 2 << Desktop_get_eigen_factors().y;
  for (int y = trans_bbox.ymin; y < trans_bbox.ymax; y += step) {
    assert(step > 0);
    plot_move((Vertex){trans_bbox.xmin, y});
    plot_fg_line_ex_end((Vertex){trans_bbox.xmax, y});
  }
}

typedef struct {
  Vertex min_os, tile_size;
  MapArea const *grid_area;
  MapAngle angle;
} DrawShapeShadow;

static void draw_shape_ghost(MapArea const *const bbox, void *const cb_arg)
{
  assert(MapArea_is_valid(bbox));
  assert(cb_arg);
  DrawShapeShadow const *const args = cb_arg;

  MapArea intersect;
  MapArea_intersection(bbox, args->grid_area, &intersect);
  if (!MapArea_is_valid(&intersect)) {
    return;
  }

  MapArea const scr_area = MapLayout_rotate_map_area_to_scr(args->angle, &intersect);

  BBox const screen_bbox = {
    scr_area.min.x * args->tile_size.x,
    scr_area.min.y * args->tile_size.y,
    (scr_area.max.x + 1) * args->tile_size.x,
    (scr_area.max.y + 1) * args->tile_size.y,
  };
  write_ghost(&screen_bbox, args->min_os);
}

static void ghost_paste_bbox(void *const cb_arg, BBox const *const bbox, MapRef const value)
{
  const DrawTransferShadow *const args = cb_arg;
  DEBUGF("Drawing ghost value %zu with bbox {%d,%d,%d,%d}\n", map_ref_to_num(value),
         bbox->xmin, bbox->ymin, bbox->xmax, bbox->ymax);

  if (!map_ref_is_mask(value)) {
    write_ghost(bbox, args->min_os);
  }
}

static void draw_ghost_paste(MapTransfer *const transfer,
  MapPoint const bl, EditWin const *const edit_win, Vertex const scr_orig,
  MapArea const *const grid_area)
{
  DEBUGF("Drawing ghost of transfer %p at %" PRIMapCoord ",%" PRIMapCoord "\n",
         (void *)transfer, bl.x, bl.y);

  int const zoom = EditWin_get_zoom(edit_win);

  Vertex const tile_size = {
      SIGNED_R_SHIFT(MapTexSize << TexelToOSCoordLog2, zoom),
      SIGNED_R_SHIFT(MapTexSize << TexelToOSCoordLog2, zoom)};

  MapAngle const angle = EditWin_get_angle(edit_win);
  MapArea const scr_area = MapLayout_rotate_map_area_to_scr(angle, grid_area);

  Vertex const draw_min = Vertex_mul(MapPoint_to_vertex(scr_area.min), tile_size);

  MapPoint const transfer_dims = MapTransfers_get_dims(transfer);
  DrawTransferShadow data = {
    .transfer = transfer,
    .transfer_area = {
      .min = bl,
      .max = MapPoint_add(bl, MapPoint_sub(transfer_dims, (MapPoint){1,1}))
    },
    .min_os = Vertex_add(scr_orig, draw_min),
  };

  DrawTiles_to_bbox(angle, &scr_area, ghost_paste_read, &data, ghost_paste_bbox,
    &data, tile_size);
}

static void draw_pending(MapModeData const *const mode_data, Vertex const scr_orig,
  MapArea const *grid_area, EditWin const *const edit_win)
{
  if (!map_overlap(grid_area, &mode_data->ghost_bbox)) {
    return;
  }

  int const zoom = EditWin_get_zoom(edit_win);

  Vertex const tile_size = {
      SIGNED_R_SHIFT(MapTexSize << TexelToOSCoordLog2, zoom),
      SIGNED_R_SHIFT(MapTexSize << TexelToOSCoordLog2, zoom)};

  DrawShapeShadow data = {
    .min_os = scr_orig,
    .tile_size = tile_size,
    .grid_area = grid_area,
    .angle = EditWin_get_angle(edit_win),
  };

  switch (mode_data->pending_shape) {
    case Pending_Point:
      draw_shape_ghost(&(MapArea){mode_data->pending_vert[0], mode_data->pending_vert[0]}, &data);
      break;

    case Pending_Line:
      Shapes_line(draw_shape_ghost, &data, mode_data->pending_vert[0],
        mode_data->pending_vert[1], 0);
      break;

    case Pending_Triangle:
      Shapes_tri(draw_shape_ghost, &data, mode_data->pending_vert[0],
        mode_data->pending_vert[1], mode_data->pending_vert[2]);
      break;

    case Pending_Rectangle:
      Shapes_rect(draw_shape_ghost, &data, mode_data->pending_vert[0],
        mode_data->pending_vert[1]);
      break;

    case Pending_Circle:
      Shapes_circ(draw_shape_ghost, &data, mode_data->pending_vert[0],
        MapPoint_dist(mode_data->pending_vert[0], mode_data->pending_vert[1]));
      break;

    case Pending_Transfer:
      draw_ghost_paste(mode_data->pending_transfer,
                           mode_data->ghost_bbox.min, edit_win,
                           scr_orig, grid_area);
      break;

    default:
      return; /* unknown plot type */
  }
}

static void draw_unknown_drop(MapArea const *const drop_bbox, EditWin const *const edit_win,
  Vertex const scr_orig, MapArea const *const grid_area)
{
  int const zoom = EditWin_get_zoom(edit_win);

  Vertex const tile_size = {
      SIGNED_R_SHIFT(MapTexSize << TexelToOSCoordLog2, zoom),
      SIGNED_R_SHIFT(MapTexSize << TexelToOSCoordLog2, zoom)};

  DrawShapeShadow data = {
    .min_os = scr_orig,
    .tile_size = tile_size,
    .grid_area = grid_area,
    .angle = EditWin_get_angle(edit_win),
  };

  draw_shape_ghost(drop_bbox, &data);
}

#endif /* !PENDING_IS_SELECTED */

void MapMode_draw(Editor *const editor, Vertex const scr_orig,
  MapArea const *const redraw_area, EditWin *const edit_win)
{
  /* Process redraw rectangle */
  DEBUG("Request to redraw map area %" PRIMapCoord " <= x <= %" PRIMapCoord
        ", %" PRIMapCoord " <= y <= %" PRIMapCoord,
        redraw_area->min.x, redraw_area->max.x,
        redraw_area->min.y, redraw_area->max.y);

  assert(redraw_area->max.x >= redraw_area->min.x);
  assert(redraw_area->max.y >= redraw_area->min.y);

  EditSession *const session = Editor_get_session(editor);

  if (!Session_has_data(session, DataType_BaseMap) &&
      !Session_has_data(session, DataType_OverlayMap))
  {
    fill_to_infinity(EditWin_get_bg_colour(edit_win));
    return;
  }

  /* Calculate which rows and columns to redraw */
  MapArea const grid_area = MapLayout_map_area_from_fine(EditWin_get_view(edit_win), redraw_area);
  MapAngle const angle = EditWin_get_angle(edit_win);
  MapArea const scr_area = MapLayout_rotate_map_area_to_scr(angle, &grid_area);

  ViewDisplayFlags const display_flags = EditWin_get_display_flags(edit_win);
  if ((!display_flags.MAP || !Session_has_data(Editor_get_session(editor), DataType_BaseMap)) &&
      (!display_flags.MAP_OVERLAY || !Session_has_data(Editor_get_session(editor), DataType_OverlayMap))) {
    draw_no_tiles(editor, angle, &scr_area, scr_orig, edit_win);
  } else {
    draw_with_tiles(editor, angle, &scr_area, scr_orig, edit_win);
  }

  if (display_flags.MAP_ANIMS && (!display_flags.NUMBERS || !MapMode_can_draw_numbers(editor, edit_win))) {
    draw_anims(editor, &grid_area, scr_orig, edit_win);
  }

#if !PENDING_IS_SELECTED
  MapModeData *const mode_data = Editor_get_edit_mode(editor) == EDITING_MODE_MAP ?
                                 editor->editingmode_data : NULL;

  if (mode_data && mode_data->pending_shape != Pending_None) {
    plot_set_col(EditWin_get_ghost_colour(edit_win));
    draw_pending(mode_data, scr_orig, &grid_area, edit_win);
  }

  if (mode_data && mode_data->pending_drop &&
    map_overlap(&grid_area, &mode_data->drop_bbox))
  {
    plot_set_col(EditWin_get_ghost_colour(edit_win));
    draw_ghost_paste(mode_data->pending_drop,
                     mode_data->drop_bbox.min, edit_win, scr_orig, &grid_area);
  }

  if (mode_data && mode_data->uk_drop_pending &&
    map_overlap(&grid_area, &mode_data->drop_bbox))
  {
    plot_set_col(EditWin_get_ghost_colour(edit_win));
    draw_unknown_drop(&mode_data->drop_bbox, edit_win, scr_orig, &grid_area);
  }
#endif /* !PENDING_IS_SELECTED */
}

static size_t MapMode_num_selected(Editor const *const editor)
{
  MapModeData *const mode_data = get_mode_data(editor);

#if PENDING_IS_SELECTED
  if (mode_data->pending_shape != Pending_None) {
    return 0;
  }
#endif

  return MapEditSelection_size(&mode_data->selection);
}

static size_t MapMode_max_selected(Editor const *const editor)
{
  NOT_USED(editor);
  assert(Editor_get_edit_mode(editor) == EDITING_MODE_MAP);
  return MAX_SELECTED;
}

static void MapMode_select_all(Editor *const editor)
{
  MapModeData *const mode_data = get_mode_data(editor);
  MapEditSelection_select_all(&mode_data->selection);
}

static void MapMode_clear_selection(Editor *const editor)
{
  clear_selection_and_redraw(editor);
}

static bool MapMode_copy(Editor *const editor)
{
  if (!cb_copy_core(editor)) {
    return false;
  }
  cb_status(editor, true);
  return true;
}

static void MapMode_delete_core(Editor *const editor, MapEditContext const *const map,
  MapEditChanges *const change_info)
{
  MapModeData *const mode_data = get_mode_data(editor);
  mode_data->lock_selection = true; // strictly redundant
  MapEdit_fill_selection(map, &mode_data->selection, map_ref_from_num(DeletedFillRef),
                         change_info);
  mode_data->lock_selection = false;
  MapEditSelection_clear(&mode_data->selection);
}

static void MapMode_delete(Editor *const editor)
{
  MapModeData *const mode_data = get_mode_data(editor);

  MapEditChanges_init(&mode_data->change_info);
  EditSession *const session = Editor_get_session(editor);
  MapMode_delete_core(editor, Session_get_map(session), &mode_data->change_info);
  changed_with_msg(editor);
}

static bool MapMode_cut(Editor *const editor)
{
  if (!cb_copy_core(editor)) {
    return false;
  }

  cb_status(editor, false);

  EditSession *const session = Editor_get_session(editor);
  MapMode_delete_core(editor, Session_get_map(session), NULL);

  return true;
}

static bool MapMode_start_drag_obj(Editor *const editor,
  MapPoint const fine_pos, EditWin *const edit_win)
{
  MapModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  MapEditContext const *const map = Session_get_map(session);

  MapArea sel_box;
  if (!MapEditSelection_get_bounds(&mode_data->selection, &sel_box)) {
    return false;
  }
  mode_data->drag_start_pos = sel_box.min;

  free_dragged(mode_data);
  mode_data->dragged = MapTransfers_grab_selection(map, &mode_data->selection);
  if (!mode_data->dragged) {
    return false;
  }

  View const *const view = EditWin_get_view(edit_win);
  MapArea sent_bbox = MapLayout_map_area_to_centre(view, &sel_box);
  MapArea_translate(&sent_bbox, (MapPoint){-fine_pos.x, -fine_pos.y}, &sent_bbox);

  MapArea shown_bbox = MapLayout_map_area_to_fine(view, &sel_box);
  MapArea_translate(&shown_bbox, (MapPoint){-fine_pos.x, -fine_pos.y}, &shown_bbox);

  return EditWin_start_drag_obj(edit_win, &sent_bbox, &shown_bbox);
}

static bool MapMode_drag_obj_remote(Editor *const editor, struct Writer *const writer,
    DataType const data_type, char const *const filename)
{
  MapModeData *const mode_data = get_mode_data(editor);
  NOT_USED(data_type);

  if (!mode_data->dragged) {
    return false;
  }

  bool success = !report_error(write_compressed(MapTransfer_get_dfile(mode_data->dragged),
                                 writer), filename, "");

  free_dragged(mode_data);
  return success;
}

static bool MapMode_show_ghost_drop(Editor *const editor,
                                   MapArea const *const bbox,
                                   Editor const *const drag_origin)
{
  bool const hide_origin_bbox = true;
  MapModeData *const mode_data = get_mode_data(editor);
  MapModeData *const origin_data = drag_origin ? get_mode_data(drag_origin) : NULL;
  assert(MapArea_is_valid(bbox));

  if (origin_data) {
    // Dragging from a window belonging to this task
    assert(origin_data->dragged);
    assert(!mode_data->uk_drop_pending);

    if (mode_data->pending_drop) {
      if (MapArea_compare(&mode_data->drop_bbox, bbox) &&
          mode_data->pending_drop == origin_data->dragged) {
        DEBUGF("Drop pos unchanged\n");
        return hide_origin_bbox;
      }

#if PENDING_IS_SELECTED
      MapEditSelection_clear(&mode_data->selection);
#else
      Editor_redraw_ghost(editor); // undraw
#endif
      free_pending_drop(mode_data);
    }

#if PENDING_IS_SELECTED
    MapTransfers_select(&mode_data->selection, bbox->min, origin_data->dragged);
#else
    Editor_set_ghost_map_bbox(editor, bbox);
#endif

    mode_data->pending_drop = origin_data->dragged;
    dfile_claim(MapTransfer_get_dfile(origin_data->dragged));

  } else {
    // Dragging from a window belonging to another task
    assert(!mode_data->pending_drop);

    if (mode_data->uk_drop_pending) {
      if (MapArea_compare(&mode_data->drop_bbox, bbox)) {
        DEBUGF("Drop pos unchanged\n");
        return hide_origin_bbox;
      }
#if PENDING_IS_SELECTED
      MapEditSelection_clear(&mode_data->selection);
#else
      Editor_redraw_ghost(editor); // undraw
#endif
    }

#if PENDING_IS_SELECTED
    MapEditSelection_select_area(&mode_data->selection, bbox);
#else
    Editor_set_ghost_map_bbox(editor, bbox);
#endif

    mode_data->uk_drop_pending = true;
  }

  mode_data->drop_bbox = *bbox;
  return hide_origin_bbox;
}

static void MapMode_hide_ghost_drop(Editor *const editor)
{
  MapModeData *const mode_data = get_mode_data(editor);

  if (mode_data->pending_drop) {
#if PENDING_IS_SELECTED
    MapEditSelection_clear(&mode_data->selection);
#else
    Editor_redraw_ghost(editor); // undraw
    Editor_clear_ghost_bbox(editor);
#endif
    free_pending_drop(mode_data);
  }

  if (mode_data->uk_drop_pending) {
#if PENDING_IS_SELECTED
    MapEditSelection_clear(&mode_data->selection);
#else
    Editor_redraw_ghost(editor); // undraw
    Editor_clear_ghost_bbox(editor);
#endif
    mode_data->uk_drop_pending = false;
  }

}

static void drag_obj_copy_core(Editor *const editor,
                           MapArea const *const bbox,
                           MapTransfer *const dropped,
                           MapEditContext const *const objects)
{
  assert(MapArea_is_valid(bbox));
  MapModeData *const mode_data = get_mode_data(editor);

  MapEditSelection_clear(&mode_data->selection);
  MapTransfers_plot_to_map(objects, bbox->min, dropped,
                           &mode_data->selection, &mode_data->change_info);
}

static bool MapMode_drag_obj_copy(Editor *const editor,
                                  MapArea const *const bbox,
                                  Editor const *const drag_origin)
{
  MapModeData *const dst_data = get_mode_data(editor);
  MapModeData *const origin_data = get_mode_data(drag_origin);
  EditSession *const session = Editor_get_session(editor);

  MapEditChanges_init(&dst_data->change_info);

  drag_obj_copy_core(editor, bbox, origin_data->dragged, Session_get_map(session));

  changed_with_msg(editor);
  free_dragged(origin_data);

  return true;
}

static void MapMode_cancel_drag_obj(Editor *const editor)
{
  MapModeData *const mode_data = get_mode_data(editor);
  free_dragged(mode_data);
}

static void gen_premove_msgs(EditSession *const session, MapModeData *const mode_data,
                           MapArea const *const bbox)
{
  assert(mode_data);
  assert(MapArea_is_valid(bbox));

  /* Take into account the direction of the move to avoid issues when part of the
     source data is overwritten by the moved data. */
  MapTransfer *const transfer = mode_data->dragged;
  MapPoint const dims = MapTransfers_get_dims(transfer);
  MapPoint dir = {1, 1}, start = {0, 0}, stop = dims;

  if (mode_data->drag_start_pos.x < bbox->min.x) {
    start.x = dims.x - 1;
    stop.x = -1;
    dir.x = -1;
  }

  if (mode_data->drag_start_pos.y < bbox->min.y) {
    start.y = dims.y - 1;
    stop.y = -1;
    dir.y = -1;
  }

  for (MapPoint p = {.x = start.x}; p.x != stop.x; p.x += dir.x) {
    for (p.y = start.y; p.y != stop.y; p.y += dir.y) {
      DEBUGF("%" PRIMapCoord ",%" PRIMapCoord " in source area\n", p.x, p.y);
      MapRef const map_ref = MapTransfers_read_ref(transfer, p);

      if (!map_ref_is_mask(map_ref)) {
        Session_map_premove(session, MapPoint_add(mode_data->drag_start_pos, p),
                            MapPoint_add(bbox->min, p));
      }
    }
  }
}

static MapEditContext get_no_prechange_cb_ctx(MapEditContext const *const map)
{
  assert(map);

  MapEditContext no_prechange_cb_ctx = *map;
  // Suppress EDITOR_CHANGE_MAP_PRECHANGE messages
  no_prechange_cb_ctx.prechange_cb = NULL;
  return no_prechange_cb_ctx;
}

static void MapMode_drag_obj_move(Editor *const editor,
                                  MapArea const *const bbox,
                                  Editor *const drag_origin)
{
  MapModeData *const dst_data = get_mode_data(editor);
  MapModeData *const origin_data = get_mode_data(drag_origin);
  EditSession *const session = Editor_get_session(editor);
  assert(session == Editor_get_session(drag_origin));
  MapEditContext const no_prechange_cb_ctx = get_no_prechange_cb_ctx(Session_get_map(session));

  MapEditChanges_init(&dst_data->change_info);
  MapEditChanges_init(&origin_data->change_info);

  // Moves the selection: take care if reordering these calls
  gen_premove_msgs(session, origin_data, bbox);

  // FIXME: single move call?
  MapTransfers_fill_map(&no_prechange_cb_ctx, origin_data->drag_start_pos,
                           origin_data->dragged, map_ref_from_num(DeletedFillRef),
                           &origin_data->change_info);

  MapEditSelection_clear(&dst_data->selection);
  MapTransfers_plot_to_map(&no_prechange_cb_ctx, bbox->min, origin_data->dragged,
                           &dst_data->selection, &dst_data->change_info);

  changed_with_msg(editor);
  if (editor != drag_origin) {
    changed_with_msg(drag_origin);
  }
  free_dragged(origin_data);
}

static bool MapMode_drop(Editor *const editor, MapArea const *const bbox,
                             Reader *const reader, int const estimated_size,
                             DataType const data_type, char const *const filename)
{
  NOT_USED(estimated_size);
  NOT_USED(data_type);
  MapModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);

  MapTransfer *const dropped = MapTransfer_create();
  if (dropped == NULL) {
    return false;
  }

  SFError err = read_compressed(MapTransfer_get_dfile(dropped), reader);
  bool success = !report_error(err, filename, "");
  if (success) {
    MapEditChanges_init(&mode_data->change_info);

    drag_obj_copy_core(editor, bbox, dropped, Session_get_map(session));

    changed_with_msg(editor);
  }

  dfile_release(MapTransfer_get_dfile(dropped));
  return success;
}

static void MapMode_update_title(Editor *const editor)
{
  MapModeData *const mode_data = get_mode_data(editor);
  MapPropDboxes_update_title(&mode_data->prop_dboxes);
}

bool MapMode_can_enter(Editor *const editor)
{
  EditSession *const session = Editor_get_session(editor);

  return Session_has_data(session, DataType_BaseMap) ||
         Session_has_data(session, DataType_OverlayMap);
}

bool MapMode_enter(Editor *const editor)
{
  DEBUG("Entering map mode");
  assert(MapMode_can_enter(editor));

  MapModeData *const mode_data = malloc(sizeof(MapModeData));
  if (mode_data == NULL) {
    report_error(SFERROR(NoMem), "", "");
    return false;
  }

  *mode_data = (MapModeData){
    .palette_type = MAPPALETTE_TYPE_NONE,
    .pending_shape = Pending_None,
  };

  editor->editingmode_data = mode_data;

  static DataType const type_list[] = {DataType_MapTransfer, DataType_Count};

  static EditModeFuncts const map_mode_fns = {
    .coord_limit = {Map_Size, Map_Size},
    .dragged_data_types = type_list,
    .import_data_types = type_list,
    .export_data_types = type_list,
    .auto_select = MapMode_auto_select,
    .auto_deselect = MapMode_auto_deselect,
    .misc_event = MapMode_misc_event,
    .can_draw_grid = MapMode_can_draw_grid,
    .draw_grid = MapMode_draw_grid,
    .leave = MapMode_leave,
    .can_draw_numbers = MapMode_can_draw_numbers,
    .draw_numbers = MapMode_draw_numbers,
    .map_to_grid_coords = MapMode_map_to_grid_coords,
    .map_to_grid_area = MapMode_map_to_grid_area,
    .grid_to_map_coords = MapMode_grid_to_map_coords,
    .num_selected = MapMode_num_selected,
    .max_selected = MapMode_max_selected,

    .resource_change = MapMode_resource_change,
    .palette_selection = MapMode_palette_selection,

    .can_clip_overlay = MapMode_can_clip_overlay,
    .clip_overlay = MapMode_clip_overlay,
    .can_create_transfer = MapMode_has_selection,
    .can_smooth = MapMode_can_smooth,
    .anim_is_selected = MapMode_anim_is_selected,
    .can_replace = MapMode_has_selection,
    .can_delete = MapMode_has_selection,
    .can_edit_properties = MapMode_can_edit_properties,
    .edit_properties = MapMode_edit_properties,
    .can_select_tool = MapMode_can_select_tool,
    .tool_selected = MapMode_tool_selected,
    .select_all = MapMode_select_all,
    .clear_selection = MapMode_clear_selection,
    .delete = MapMode_delete,
    .cut = MapMode_cut,
    .copy = MapMode_copy,
    .update_title = MapMode_update_title,
    .get_help_msg = MapMode_get_help_msg,

    .pending_sample_obj = MapMode_pending_point,
    .sample_obj = MapMode_sample_tile,

    .pending_plot = MapMode_pending_point,

    .pending_line = MapMode_pending_line,
    .plot_line = MapMode_plot_line,

    .pending_rect = MapMode_pending_rect,
    .plot_rect = MapMode_plot_rect,

    .pending_circ = MapMode_pending_circ,
    .plot_circ = MapMode_plot_circ,

    .pending_tri = MapMode_pending_tri,
    .plot_tri = MapMode_plot_tri,

    .cancel_plot = MapMode_wipe_ghost,

    .pending_smooth = MapMode_pending_brush,
    .start_smooth = MapMode_start_smooth,
    .draw_smooth = MapMode_draw_smooth,

    .pending_transfer = MapMode_pending_transfer,
    .draw_transfer = MapMode_draw_transfer,

    .pending_flood_fill = MapMode_pending_fill,
    .flood_fill = MapMode_flood_fill,

    .pending_global_replace = MapMode_pending_fill,
    .global_replace = MapMode_global_replace,

    .start_select = MapMode_start_select,
    .start_exclusive_select = MapMode_start_exclusive_select,
    .update_select = MapMode_update_select,
    .cancel_select = MapMode_cancel_select,

    .start_drag_obj = MapMode_start_drag_obj,
    .drag_obj_remote = MapMode_drag_obj_remote,
    .drag_obj_copy = MapMode_drag_obj_copy,
    .drag_obj_move = MapMode_drag_obj_move,
    .cancel_drag_obj = MapMode_cancel_drag_obj,

    .show_ghost_drop = MapMode_show_ghost_drop,
    .hide_ghost_drop = MapMode_hide_ghost_drop,
    .drop = MapMode_drop,

    .edit_properties_at_pos = edit_properties_at_pos,

    .start_pending_paste = MapMode_start_pending_paste,
    .pending_paste = MapMode_pending_paste,
    .draw_paste = MapMode_draw_paste,
    .cancel_paste = MapMode_cancel_paste,

    .pending_brush = MapMode_pending_brush,
    .start_brush = MapMode_start_brush,
    .draw_brush = MapMode_draw_brush,

    .pending_snake = MapMode_pending_point,
    .start_snake = MapMode_start_snake,
    .draw_snake = MapMode_draw_snake,

    .paint_selected = MapMode_paint_selected,

    .create_transfer = MapMode_create_transfer,

    .wipe_ghost = MapMode_wipe_ghost,
  };
  editor->mode_functions = &map_mode_fns;

  MapPropDboxes_init(&mode_data->prop_dboxes, editor);

  if (!report_error(MapEditSelection_init(&mode_data->selection, redraw_selection, editor), "", ""))
  {
    Editor_display_msg(editor, msgs_lookup("StatusMapMode"), false);
    return true;
  }
  free(mode_data);
  editor->editingmode_data = NULL;
  return false;
}

void MapMode_free_clipboard(void)
{
  if (clipboard) {
    dfile_release(MapTransfer_get_dfile(clipboard));
    clipboard = NULL;
  }
}

bool MapMode_write_clipboard(struct Writer *const writer,
  DataType const data_type, char const *const filename)
{
  NOT_USED(data_type);
  return !report_error(write_compressed(MapTransfer_get_dfile(clipboard), writer), filename, "");
}

int MapMode_estimate_clipboard(DataType const data_type)
{
  NOT_USED(data_type);
  return worst_compressed_size(MapTransfer_get_dfile(clipboard));
}
