/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Editing window
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

#include <ctype.h>
#include <stdbool.h>
#include "stdio.h"
#include "stdlib.h"
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

#include "kernel.h"
#include "event.h"
#include "flex.h"
#include "toolbox.h"
#include "wimp.h"
#include "wimplib.h"
#include "window.h"
#include "menu.h"

#include "err.h"
#include "DeIconise.h"
#include "Macros.h"
#include "msgtrans.h"
#include "ViewsMenu.h"
#include "scheduler.h"
#include "Tboxbugs.h"
#include "PalEntry.h"
#include "OSWord.h"
#include "EventExtra.h"
#include "StackViews.h"
#include "Entity2.h"
#include "WimpExtra.h"
#include "OSReadTime.h"
#include "SprFormats.h"
#include "loader3.h"
#include "saver2.h"

#include "MapTexData.h"
#include "ObjectsEdit.h"
#include "EditWinData.h"
#include "DCS_dialogue.h"
#include "filepaths.h"
#include "MapMode.h"
#include "ObjectsMode.h"
#include "ShipsMode.h"
#include "InfoMode.h"
#include "utils.h"
#include "Session.h"
#include "debug.h"
#include "plot.h"
#include "SFInit.h"
#include "EditWin.h"
#include "ourevents.h"
#include "SaveMiss.h"
#include "SaveMap.h"
#include "EditMenu.h"
#include "EffectMenu.h"
#include "zoommenu.h"
#include "OrientMenu.h"
#include "utilsmenu.h"
#include "layersmenu.h"
#include "mainmenu.h"
#include "utilsmenu.h"
#include "GridCol.h"
#include "BackCol.h"
#include "MapCoord.h"
#include "EditMenu.h"
#include "EffectMenu.h"
#include "Desktop.h"
#include "DataType.h"
#include "MapTexBitm.h"
#include "Drag.h"
#include "Config.h"
#include "ObjGfxData.h"
#include "DrawObjs.h"
#include "DrawTiles.h"
#include "DrawInfos.h"
#include "ObjEditCtx.h"
#include "MapEditCtx.h"
#include "InfoEditCtx.h"
#include "ObjGfxMesh.h"
#include "NewTransfer.h"
#include "MapLayout.h"
#include "ObjLayout.h"
#include "MapAreaCol.h"
#include "Goto.h"

#define DEBUG_REDRAW_AREA 0
#define SHOW_REDRAW_RECT 0
#define DEBUG_REDRAW 0
#define DEBUG_TRACK_PTR 0

#undef CancelDrag /* definition in "wimplib.h" is wrong! */
#define CancelDrag ((WimpDragBox *)-1)

enum {
  MAX_REDRAW_PERIOD = CLOCKS_PER_SEC/10,
  SCROLL_BORDER = 64,
  MAP_HEIGHT = ((MapTexSize << TexelToOSCoordLog2) * Map_Size), /* 8192 */
  MAP_WIDTH = ((MapTexSize << TexelToOSCoordLog2) * Map_Size), /* 8192 */
  FREQUENCY = 10,
  PRIORITY = SchedulerPriority_Min,
  IntKeyNum_Shift = 0,
  IntKeyNum_Ctrl = 1,
  ObjColourWeight = 40,
  SelColourWeight = 60,
  WimpIcon_WorkArea = -1, /* Pseudo icon handle (window's work area) */
};

static EditWin *drag_claim_edit_win, *drag_origin_edit_win;

/* ---------------- Private functions ---------------- */

static void gen_sel_tex_bw_table(EditWin *const edit_win)
{
  EditSession *const session = EditWin_get_session(edit_win);
  if (!Session_has_data(session, DataType_MapTextures)) {
    return;
  }
  MapTex *const textures = Session_get_textures(session);
  size_t const count = MapTexBitmaps_get_count(&textures->tiles);

  for (size_t index = 0; index < count; ++index) {
    int const av = MapTexBitmaps_get_average_colour(&textures->tiles, map_ref_from_num(index));

    unsigned int const bright =
      palette_entry_brightness(edit_win->view.sel_palette[av]);

    size_t const bit = 1u << (index % CHAR_BIT);
    if (bright > MaxBrightness/2)
    {
      SET_BITS(edit_win->sel_tex_bw_table[index / CHAR_BIT], bit);
    }
    else
    {
      CLEAR_BITS(edit_win->sel_tex_bw_table[index / CHAR_BIT], bit);
    }
  }
}

static void set_sel_colour(EditWin *const edit_win)
{
  assert(edit_win != NULL);
  PaletteEntry const colour = edit_win->view.config.sel_colour;

  unsigned int const denom = ObjColourWeight + SelColourWeight;
  for (int i = 0; i < NumColours; ++i)
  {
    unsigned int const r = ((PALETTE_GET_RED((*palette)[i]) * ObjColourWeight) +
                            (PALETTE_GET_RED(colour) * SelColourWeight)) / denom;
    unsigned int const g = ((PALETTE_GET_GREEN((*palette)[i]) * ObjColourWeight) +
                            (PALETTE_GET_GREEN(colour) * SelColourWeight)) / denom;
    unsigned int const b = ((PALETTE_GET_BLUE((*palette)[i]) * ObjColourWeight) +
                            (PALETTE_GET_BLUE(colour) * SelColourWeight)) / denom;
    edit_win->view.sel_palette[i] = make_palette_entry(r,g,b);
    edit_win->view.sel_colours[i] = nearest_palette_entry(*palette, NumColours, edit_win->view.sel_palette[i]);
  }

  gen_sel_tex_bw_table(edit_win);
}

static bool key_pressed(int const key_num)
{
  enum
  {
    OSByteScanKeys        = 129,
    OSByteScanKeysNoLimit = 0xff,
    OSByteScanKeysSingle  = 0xff,
    OSByteR1ResultMask    = 0xff,
  };
  int const key_held = _kernel_osbyte(OSByteScanKeys,
    key_num ^ OSByteScanKeysSingle, OSByteScanKeysNoLimit);

  if (key_held == _kernel_ERROR)
  {
    E(_kernel_last_oserror());
    return false;
  }

  return (key_held & OSByteR1ResultMask) != 0;
}

static Vertex calc_map_size(int const zoom)
{
  Vertex const map_size = Vertex_div_log2((Vertex){MAP_WIDTH, MAP_HEIGHT}, zoom);
  DEBUGF("Map size at zoom %d is %d,%d\n", zoom, map_size.x, map_size.y);
  return map_size;
}

static int map_units_per_os_unit_log2(int const zoom)
{
  assert(zoom >= EditWinZoomMin);
  assert(zoom <= EditWinZoomMax);
  assert(MAP_COORDS_LIMIT_LOG2 >= TexelToOSCoordLog2 + Map_SizeLog2 + MapTexSizeLog2);
  int const map_units_log2 =
    MAP_COORDS_LIMIT_LOG2 - TexelToOSCoordLog2 - Map_SizeLog2 - MapTexSizeLog2 + zoom;
  assert(map_units_log2 >= 0);
  return map_units_log2;
}

static Vertex calc_visible_size(EditWin const *const edit_win, const WimpGetWindowStateBlock *const window_state)
{
  assert(window_state != NULL);
  Vertex size = BBox_size(&window_state->visible_area);

  if (edit_win->view.config.show_status_bar) {
    size.y -=  StatusBar_get_height() + (1 << Desktop_get_eigen_factors().y);
  }

  size = Vertex_min(size, edit_win->view.map_size_in_os_units);

  DEBUG("Size of visible area (screen coords) = %d,%d", size.x, size.y);
  return size;
}

static void scroll_to(EditWin const *const edit_win, MapPoint const grid_pos, WimpGetWindowStateBlock *const window_state)
{
  assert(edit_win);
  assert(window_state);

  Vertex const visible_size = calc_visible_size(edit_win, window_state);
  Vertex const half_vis_size = Vertex_div_log2(visible_size, 1);
  MapPoint const new_map_pos = Editor_grid_to_map_coords(edit_win->editor, grid_pos, edit_win);
  Vertex new_centre = MapPoint_to_vertex(MapPoint_div_log2(new_map_pos, edit_win->view.map_units_per_os_unit_log2));
  new_centre.y -= edit_win->view.map_size_in_os_units.y; // convert to work area coordinates (origin at top left)

  window_state->xscroll = new_centre.x - half_vis_size.x;
  window_state->yscroll = new_centre.y + half_vis_size.y;
}

static void set_extent(EditWin *const edit_win, MapPoint const *const grid_pos)
{
  assert(edit_win);
  DEBUG("Current extent of edit_win %p is %d,%d", (void *)edit_win,
        edit_win->extent.x, edit_win->extent.y);

  // convert to work area coordinates (origin at top left)
  edit_win->extent = (Vertex){edit_win->view.map_size_in_os_units.x, -edit_win->view.map_size_in_os_units.y};

  if (edit_win->view.config.show_status_bar) {
    Vertex const eigen_factors = Desktop_get_eigen_factors();
    edit_win->extent.y -= StatusBar_get_height() + (1 << eigen_factors.y); // increases extent
  }

  DEBUG("Have calculated new extent of edit_win %p as %d,%d", (void *)edit_win,
        edit_win->extent.x, edit_win->extent.y);

  /* Change extent of window work area */
  BBox extent = {
    .xmin = 0,
    .ymin = edit_win->extent.y,
    .xmax = edit_win->extent.x,
    .ymax = 0
  };
  ON_ERR_RPT_RTN(window_set_extent(0, edit_win->window_id, &extent));

  /* Re-open window with new extent */
  WimpGetWindowStateBlock window_state = {edit_win->wimp_id};
  ON_ERR_RPT_RTN(wimp_get_window_state(&window_state));

  if (grid_pos) {
    scroll_to(edit_win, *grid_pos, &window_state);
  }

  ON_ERR_RPT_RTN(toolbox_show_object(0, edit_win->window_id,
                 Toolbox_ShowObject_FullSpec, &window_state.visible_area,
                 NULL_ObjectId, NULL_ComponentId));

  /* We only get open window request events in response to the user dragging or
     resizing the window so ensure that the status bar is reformatted. */
  Editor *const editor = edit_win->editor;
  int const width = window_state.visible_area.xmax - window_state.visible_area.xmin;
  StatusBar_reformat(&edit_win->statusbar_data, width,
    Editor_get_coord_field_width(editor));

  MapAreaCol_init(&edit_win->pending_redraws, MAP_COORDS_LIMIT_LOG2);
  E(window_force_redraw(0, edit_win->window_id, &extent));
}

static void show_or_hide_status_bar(EditWin *const edit_win)
{
  /* Show or hide status bar */
  if (edit_win->view.config.show_status_bar) {
    StatusBar_show(&edit_win->statusbar_data, edit_win->window_id);
  } else {
    StatusBar_hide(&edit_win->statusbar_data);
  }

  /* Change extent of window and force redraw */
  set_extent(edit_win, NULL);
}

static Vertex calc_work_area_origin(const WimpGetWindowStateBlock *const window_state)
{
  assert(window_state != NULL);
  DEBUG("Window visible area: %d,%d,%d,%d scroll offsets: %d,%d",
        window_state->visible_area.xmin, window_state->visible_area.ymin,
        window_state->visible_area.xmax, window_state->visible_area.ymax,
        window_state->xscroll, window_state->yscroll);

  Vertex const origin = {
    window_state->visible_area.xmin - window_state->xscroll,
    window_state->visible_area.ymax - window_state->yscroll
  };
  DEBUG("Origin of work area (screen coords) = %d,%d", origin.x, origin.y);
  return origin;
}

static Vertex calc_window_origin(EditWin const *const edit_win, const WimpGetWindowStateBlock *const window_state)
{
  assert(edit_win);

  /* Calculate the bottom-left corner of the window work area origin in screen coordinates */
  Vertex origin = calc_work_area_origin(window_state);
  origin.y -= edit_win->view.map_size_in_os_units.y; // convert to work area coordinates (origin at top left)
  DEBUG("Origin of map (screen coords) = %d,%d", origin.x, origin.y);
  return origin;
}

static MapPoint scr_to_map_coords(EditWin const *const edit_win, Vertex const origin,
  Vertex const screen_in)
{
  DEBUG("Will convert screen coords %d,%d to map (origin %d,%d)",
        screen_in.x, screen_in.y, origin.x, origin.y);

  /* Calculate coordinates of point relative to bottom left corner of window's work area
     (still in OS units)
     Additional checks to keep mouse coords within bounds seem to be necessary.
     I believe this is because the Wimp counts the pointer as within a window's
     visible area even when actually over the 1 pixel wide border.
   */
  Vertex const rel_coord = Vertex_sub(screen_in, origin);
  Vertex const map_limit = Vertex_sub(edit_win->view.map_size_in_os_units, (Vertex){1, 1});
  Vertex const clamped_coords = Vertex_min(map_limit, Vertex_max((Vertex){0,0}, rel_coord));
  DEBUG("Relative to map origin: %d,%d", clamped_coords.x, clamped_coords.y);

  /* Convert OS units to fixed range 0 to MAP_COORDS_LIMIT (according to zoom).
   */
  MapPoint const map_out = MapPoint_mul_log2(MapPoint_from_vertex(clamped_coords),
                                             edit_win->view.map_units_per_os_unit_log2);

  DEBUG("Scaled to standard range: %" PRIMapCoord ", %" PRIMapCoord,
        map_out.x, map_out.y);

  /* It's tempting to convert to grid coordinates here but the objects editing
     mode can benefit from higher-resolution coordinate information because
     objects are irregularly shaped. */

  return map_out;
}

static Vertex map_to_scr_coords(EditWin const *const edit_win, Vertex const origin, MapPoint const map_pos)
{
  DEBUG("Will convert map coords %" PRIMapCoord ",%" PRIMapCoord " to screen",
        map_pos.x, map_pos.y);

  /* Converted fixed range 0 to MAP_COORDS_LIMIT to OS units
   * (according to zoom).
   */
  Vertex screen_pos = MapPoint_to_vertex(MapPoint_div_log2(map_pos, edit_win->view.map_units_per_os_unit_log2));
  DEBUG("Relative screen coords: %d, %d", screen_pos.x, screen_pos.y);

  /* Translate relative to bottom left corner of window's work area into absolute screen coordinates */
  screen_pos = Vertex_add(screen_pos, origin);
  DEBUG("Absolute screen coords: %d, %d", screen_pos.x, screen_pos.y);

  return screen_pos;
}

static void redraw_loop(EditWin *const edit_win, WimpRedrawWindowBlock *const block)
{
  /* Separate from redraw handler so that it can also be called after
     wimp_update_window */
  DEBUG("Entering redraw loop for edit_win %p, redraw block is %p", (void *)edit_win,
        (void *)block);

  Editor *const editor = edit_win->editor;
  EditSession *const session = EditWin_get_session(edit_win);

  /* We turn off compaction on flex_free() to speed up deallocation of
     render buffer */
  int const compact_state = flex_set_deferred_compaction(1);

  int more;
  do {
    /* Convert OS screen coordinates of redraw rectangle to map coordinates
       Note that Wimp redraw rectangle maximum coordinates are exclusive */
    Vertex const redraw_min = {block->redraw_area.xmin, block->redraw_area.ymin};
    Vertex const redraw_max = {block->redraw_area.xmax - 1, block->redraw_area.ymax - 1};
    Vertex const window_origin = calc_window_origin(edit_win, (WimpGetWindowStateBlock *)block);
    MapArea const area = {
      .min = scr_to_map_coords(edit_win, window_origin, redraw_min),
      .max = scr_to_map_coords(edit_win, window_origin, redraw_max)
    };

    if ((edit_win->view.config.flags.MAP && Session_has_data(session, DataType_BaseMap)) ||
        (edit_win->view.config.flags.MAP_OVERLAY && Session_has_data(session, DataType_OverlayMap)) ||
        Editor_get_edit_mode(editor) == EDITING_MODE_MAP) {
      /* Draw tiled ground map (or chequerboard if graphics turned off) */
      MapMode_draw(editor, window_origin, &area, edit_win);
    } else {
      /* Draw plain background colour */
      plot_set_col(edit_win->view.config.back_colour);
      plot_fg_rect_2v(redraw_min, redraw_max);
    }

    if (edit_win->view.config.flags.GRID && Editor_can_draw_grid(editor, edit_win)) {
      Editor_draw_grid(editor, window_origin, &area, edit_win);
    }

    if (Editor_get_edit_mode(editor) == EDITING_MODE_MAP &&
        edit_win->view.config.flags.NUMBERS && Editor_can_draw_numbers(editor, edit_win)) {
      /* Draw tile numbers */
      Editor_draw_numbers(editor, window_origin, &area, edit_win);
    }

    if ((edit_win->view.config.flags.OBJECTS && Session_has_data(session, DataType_BaseObjects)) ||
        (edit_win->view.config.flags.OBJECTS_OVERLAY && Session_has_data(session, DataType_OverlayObjects))) {
      /* Draw polygonal ground objects */
      ObjectsMode_draw(editor, window_origin, &area, edit_win);
    }

    if (Editor_get_edit_mode(editor) == EDITING_MODE_OBJECTS &&
        edit_win->view.config.flags.NUMBERS &&
        Editor_can_draw_numbers(editor, edit_win)) {
      /* Draw object numbers */
      Editor_draw_numbers(editor, window_origin, &area, edit_win);
    }

    if (Session_has_data(session, DataType_Mission)) {
      if (edit_win->view.config.flags.SHIPS) {
        /* Draw ships and flightpaths */
        ShipsMode_draw(editor, window_origin, &area, edit_win);
      }

      if (edit_win->view.config.flags.INFO) {
        /* Draw strategic target information */
        InfoMode_draw(editor, window_origin, &area, edit_win);
      }
    }

#if SHOW_REDRAW_RECT
    plot_inv_dot_rect_2v(redraw_min, redraw_max);
#endif
    /* Get next redraw rectangle */
    if (E(wimp_get_rectangle(block, &more))) {
      more = 0;
    }
  } while (more);

  /* Restore immediate heap compaction  */
  flex_set_deferred_compaction(compact_state);
  while (flex_compact() != 0) {};
}

static void redraw_area(EditWin *const edit_win, MapArea const *const area,
  bool const immediate)
{
  /* Force redraw of specified area of map (taking account of zoom level) */
  assert(area != NULL);
  DEBUG("Forcing redraw of map area x %" PRIMapCoord ",%" PRIMapCoord
        ", y %" PRIMapCoord ",%" PRIMapCoord " (%s)",
        area->min.x, area->max.x, area->min.y,
        area->max.y, immediate ? "immediate" : "deferred");

  /* Converted fixed range 0 to MAP_COORDS_LIMIT to window work area coordinates
     (according to current zoom factor). */
  assert(edit_win != NULL);

  MapArea redraw_area;
  MapArea_div_log2(area, edit_win->view.map_units_per_os_unit_log2, &redraw_area);

  Vertex const eig = Desktop_get_eigen_factors();
  WimpRedrawWindowBlock block = {
    .window_handle = edit_win->wimp_id,
    .visible_area = { /* actually redraw area not visible area! */
      .xmin = (int)redraw_area.min.x,
      .ymin = (int)redraw_area.min.y,
      /* Redraw bounding boxes have exclusive maximum coordinates */
      .xmax = (int)redraw_area.max.x + (1 << eig.x),
      .ymax = (int)redraw_area.max.y + (1 << eig.y)
    }
  };

  // convert to work area coordinates (origin at top left)
  BBox_translate(&block.visible_area, (Vertex){0, -edit_win->view.map_size_in_os_units.y},
    &block.visible_area);

  DEBUG("Window area at current scale is x %d,%d, y %d,%d",
        block.visible_area.xmin, block.visible_area.xmax,
        block.visible_area.ymin, block.visible_area.ymax);

  if (immediate) {
    /* Update window contents immediately */
#ifdef DEBUG_OUTPUT
    clock_t const start = clock();
#endif
    int more;
    E(wimp_update_window(&block, &more));
    if (more) {
      redraw_loop(edit_win, &block);
    }

#ifdef DEBUG_OUTPUT
    clock_t const period = clock() - start;
    if (period > MAX_REDRAW_PERIOD) {
      DEBUGF("Immediate redraw period: %g\n", (double)period / CLOCKS_PER_SEC);
    }
#endif
  } else {
    E(window_force_redraw(0, edit_win->window_id, &block.visible_area));
  }
}

static void redraw_all(EditWin *const edit_win)
{
  static MapArea const area = {{0, 0}, {MAP_COORDS_LIMIT, MAP_COORDS_LIMIT}};
  redraw_area(edit_win, &area, false);
}

static bool auto_scroll(EditWin *const edit_win, WimpGetWindowStateBlock *const window_state,
                        Vertex const ptr, const SchedulerTime const new_time)
{
  Vertex scroll = {0,0};
  Vertex border = {0,0};

  Vertex const visible_size = calc_visible_size(edit_win, window_state);

  /* Cope with very narrow edit_wins where borders would overlap */
  if (visible_size.x < SCROLL_BORDER * 2)
    border.x = (window_state->visible_area.xmax - window_state->visible_area.xmin) / 2;
  else
    border.x = SCROLL_BORDER;

  if (visible_size.y < SCROLL_BORDER * 2)
    border.y = (window_state->visible_area.ymax - window_state->visible_area.ymin) / 2;
  else
    border.y = SCROLL_BORDER;

  /* Auto-scroll window if pointer is at edge */
  if (!edit_win->snap_vert) {
    if (ptr.x >= window_state->visible_area.xmin &&
        ptr.x < window_state->visible_area.xmin + border.x) {
      DEBUG("Will scroll west");
      scroll.x = ptr.x - (window_state->visible_area.xmin + border.x);
    } else {
      if (ptr.x > window_state->visible_area.xmax - border.x &&
          ptr.x <= window_state->visible_area.xmax)
      {
        DEBUG("Will scroll east");
        scroll.x = ptr.x - (window_state->visible_area.xmax - border.x);
      }
    }
  }

  if (!edit_win->snap_horiz) {
    int const ymin = window_state->visible_area.ymin +
                     (edit_win->view.config.show_status_bar ?
                       StatusBar_get_height() : 0);
    if (ptr.y >= ymin && ptr.y < ymin + SCROLL_BORDER) {
      DEBUG("Will scroll south");
      scroll.y = ptr.y - (ymin + border.y);
    } else {
      if (ptr.y > window_state->visible_area.ymax - border.y &&
          ptr.y <= window_state->visible_area.ymax)
      {
        DEBUG("Will scroll north");
        scroll.y = ptr.y - (window_state->visible_area.ymax - border.y);
      }
    }
  }

  if (scroll.y != 0 || scroll.x != 0) {
    if (edit_win->auto_scrolling) {
      /* Scroll window by amount based on elapsed time */
      SchedulerTime time_diff;

      /* should handle timer wrap-around correctly */
      time_diff = new_time - edit_win->last_scroll;
      DEBUG("Time since last scroll update: %d", time_diff);

      /* Put a cap on enormous time intervals */
      if (time_diff > 25) {
        time_diff = 25;
        DEBUG("Time difference capped");
      }

      window_state->xscroll += (scroll.x * (int)time_diff * 10) / border.x;
      window_state->yscroll += (scroll.y * (int)time_diff * 10) / border.y;
      DEBUG("New scroll offsets: x %d  y %d", window_state->xscroll, window_state->yscroll);

      /* Re-open window with modified scroll offsets */
      E(toolbox_show_object(0, edit_win->window_id,
          Toolbox_ShowObject_FullSpec, &window_state->visible_area, NULL_ObjectId,
          NULL_ComponentId));
    } else
      DEBUG("Can't scroll until next time");

    /* Store new time */
    edit_win->last_scroll = new_time;
    edit_win->auto_scrolling = true;
    return true;
  }

  //DEBUG("Pointer outside scroll borders");
  edit_win->auto_scrolling = false; /* have gone outside scroll area */
  return false;
}

static void restrict_ptr(EditWin *const edit_win, int const x, int const y)
{
  WimpGetWindowStateBlock window_state = {
    .window_handle = edit_win->wimp_id,
  };
  ON_ERR_RPT_RTN(wimp_get_window_state(&window_state));

  if (edit_win->view.config.show_status_bar) {
    Vertex const eigen_factors = Desktop_get_eigen_factors();
    window_state.visible_area.ymin += StatusBar_get_height() + (1 << eigen_factors.y);
  }

  if (y != INT_MIN) {
    window_state.visible_area.ymin = window_state.visible_area.ymax = y;
  }

  if (x != INT_MIN) {
    window_state.visible_area.xmin = window_state.visible_area.xmax = x;
  }

  edit_win->pointer_trapped = !E(os_word_set_pointer_bbox(&window_state.visible_area));
  if (edit_win->pointer_trapped) {
    edit_win->snap_horiz = (y != INT_MIN);
    edit_win->snap_vert = (x != INT_MIN);
  }
}

static void close(EditWin *const edit_win, bool const open_parent)
{
  /* Attempt to close window */
  int const count = Session_try_delete_edit_win(
    EditWin_get_session(edit_win), edit_win, open_parent);

  if (count > 0) {
    DCS_queryunsaved(edit_win->window_id, count, open_parent);
  }
}

/*
 * Start, hide, or cancel a Wimp drag operation. Typically Wimp_DragBox with drag
 * type 5 is used for DragBoxOp_Start, drag type 7 for DragBoxOp_Hide and
 * Wimp_DragBox -1 for DragBoxOp_Cancel.
 */
static const _kernel_oserror *drag_box_method(DragBoxOp action, bool solid_drags, int mouse_x,
                                              int mouse_y, void *client_handle)
{
  NOT_USED(solid_drags);
  EditWin *const edit_win = client_handle;
  Vertex const mouse_pos = {mouse_x, mouse_y};

  if (action == DragBoxOp_Cancel) {
    DEBUGF("Calling Wimp_DragBox to cancel drag\n");
    ON_ERR_RTN_E(wimp_drag_box(CancelDrag));
    edit_win->obj_drag_box = false;
  } else {
    assert(action == DragBoxOp_Hide || action == DragBoxOp_Start);
    WimpGetWindowStateBlock getwincoords = {
      .window_handle = edit_win->wimp_id,
    };
    ON_ERR_RTN_E(wimp_get_window_state(&getwincoords));

    /* Set up initial position of drag box */
    Vertex const min = map_to_scr_coords(edit_win, mouse_pos, edit_win->shown_drag_bbox.min);

    /* Drag bounding boxes have exclusive maximum coordinates */
    Vertex const eig = Desktop_get_eigen_factors();
    Vertex const pix = {1 << eig.x, 1 << eig.y};
    Vertex const max = Vertex_add(pix,
                         map_to_scr_coords(edit_win, mouse_pos, edit_win->shown_drag_bbox.max));

    /* Allow drag anywhere on the screen */
    Vertex const desktop_size = Desktop_get_size_os();
    Vertex const parent_min = Vertex_sub(min, mouse_pos);
    Vertex const parent_max = Vertex_sub(Vertex_add(max, desktop_size), mouse_pos);

    WimpDragBox drag_box = {
      .drag_type = (action == DragBoxOp_Hide ? Wimp_DragBox_DragPoint : Wimp_DragBox_DragFixedDash),
      .dragging_box = {.xmin = min.x, .ymin = min.y, .xmax = max.x, .ymax = max.y},
      .parent_box = {.xmin = parent_min.x, .ymin = parent_min.y,
                     .xmax = parent_max.x, .ymax = parent_max.y}
    };

    DEBUGF("Calling Wimp_DragBox to start drag of type %d\n",
           drag_box.drag_type);

    ON_ERR_RTN_E(wimp_drag_box(&drag_box));
    edit_win->obj_drag_box = true;
  }

  return NULL;
}

static void update_projection(EditWin *const edit_win)
{
  assert(edit_win);
  int const map_scaler = SIGNED_R_SHIFT(256 << TexelToOSCoordLog2, edit_win->view.config.zoom_factor); /* min. 32 (at ½× zoom) */
  ObjGfxMeshes_set_direction(&edit_win->view.plot_ctx,
    (ObjGfxDirection){ObjGfxAngle_from_map(edit_win->view.config.angle), {-OBJGFXMESH_ANGLE_QUART}, {0}},
    map_scaler);
}

static void change_zoom_recentre(EditWin *const edit_win, int const zoom_factor, MapPoint const grid_pos)
{
  assert(edit_win != NULL);
  assert(edit_win->view.config.zoom_factor >= EditWinZoomMin);
  assert(edit_win->view.config.zoom_factor <= EditWinZoomMax);
  assert(zoom_factor >= EditWinZoomMin);
  assert(zoom_factor <= EditWinZoomMax);
  // Any drag box is invalidated by zoom
  assert(!edit_win->wimp_drag_box);
  assert(!edit_win->dragging_obj);
  assert(!edit_win->pointer_trapped);

  if (edit_win->view.config.zoom_factor == zoom_factor)
    return;

  DEBUG("Will change zoom from %d to %d", edit_win->view.config.zoom_factor, zoom_factor);
  edit_win->view.config.zoom_factor = zoom_factor;
  update_projection(edit_win);

  edit_win->view.map_size_in_os_units = calc_map_size(zoom_factor);
  edit_win->view.map_units_per_os_unit_log2 = map_units_per_os_unit_log2(zoom_factor);

  StatusBar_show_zoom(&edit_win->statusbar_data, edit_win->view.config.zoom_factor);

  /* Reopen window with new scroll offsets and
     correct extent for new zoom */
  set_extent(edit_win, &grid_pos);
}

static MapPoint get_scroll_pos(EditWin const *const edit_win, WimpGetWindowStateBlock *const window_state)
{
  assert(edit_win);
  assert(window_state);

  Vertex const visible_size = calc_visible_size(edit_win, window_state);
  Vertex const half_vis_size = Vertex_div_log2(visible_size, 1);
  Vertex const centre = Vertex_sub(BBox_get_max(&window_state->visible_area), half_vis_size);
  Vertex const window_origin = calc_window_origin(edit_win, window_state);
  MapPoint const map_pos = scr_to_map_coords(edit_win, window_origin, centre);
  return Editor_map_to_grid_coords(edit_win->editor, map_pos, edit_win);
}

static void change_zoom(EditWin *const edit_win, int const zoom_factor)
{
  assert(edit_win != NULL);
  assert(edit_win->view.config.zoom_factor >= EditWinZoomMin);
  assert(edit_win->view.config.zoom_factor <= EditWinZoomMax);
  assert(zoom_factor >= EditWinZoomMin);
  assert(zoom_factor <= EditWinZoomMax);

  if (edit_win->view.config.zoom_factor == zoom_factor)
    return;

  WimpGetWindowStateBlock window_state = {
    .window_handle = edit_win->wimp_id,
  };

  ON_ERR_RPT_RTN(wimp_get_window_state(&window_state));
  change_zoom_recentre(edit_win, zoom_factor, get_scroll_pos(edit_win, &window_state));
}

static void change_angle(EditWin *const edit_win, MapAngle const angle)
{
  assert(edit_win != NULL);
  assert(edit_win->view.config.angle >= MapAngle_North);
  assert(edit_win->view.config.angle <= MapAngle_West);
  assert(angle >= MapAngle_North);
  assert(angle <= MapAngle_West);
  // Any drag box is invalidated by rotation
  assert(!edit_win->wimp_drag_box);
  assert(!edit_win->dragging_obj);
  assert(!edit_win->pointer_trapped);

  // Any stored rectangle to be used when undrawing ghost objects is invalidated by rotation
  Editor_wipe_ghost(edit_win->editor);
  Editor_hide_ghost_drop(edit_win->editor);

  WimpGetWindowStateBlock window_state = {
    .window_handle = edit_win->wimp_id,
  };

  ON_ERR_RPT_RTN(wimp_get_window_state(&window_state));

  MapPoint const grid_pos = get_scroll_pos(edit_win, &window_state);

  edit_win->view.config.angle = angle;
  update_projection(edit_win);
  StatusBar_show_angle(&edit_win->statusbar_data, edit_win->view.config.angle);

  scroll_to(edit_win, grid_pos, &window_state);

  E(toolbox_show_object(0, edit_win->window_id,
                        Toolbox_ShowObject_FullSpec, &window_state.visible_area,
                        NULL_ObjectId, NULL_ComponentId));

  BBox extent = {
    .xmin = 0,
    .ymin = edit_win->extent.y,
    .xmax = edit_win->extent.x,
    .ymax = 0
  };

  MapAreaCol_init(&edit_win->pending_redraws, MAP_COORDS_LIMIT_LOG2);
  E(window_force_redraw(0, edit_win->window_id, &extent));
}

static void free_pointer(EditWin *const edit_win)
{
  assert(edit_win != NULL);
  if (!edit_win->pointer_trapped) {
    return;
  }

  DEBUG("Freeing mouse pointer from bounding box");
  Vertex const desktop_size = Desktop_get_size_os();
  BBox const bbox = {0, 0, desktop_size.x, desktop_size.y};
  E(os_word_set_pointer_bbox(&bbox));

  edit_win->snap_horiz = edit_win->snap_vert = edit_win->pointer_trapped = false;
}

static SchedulerTime track_pointer(void *const handle, SchedulerTime const new_time,
  const volatile bool *const time_up)
{
  /* Null event handler for updating things based on pointer position */
  NOT_USED(time_up);
  EditWin *const edit_win = handle;

#if !DEBUG_TRACK_PTR
  DebugOutput old = DEBUG_SET_OUTPUT(DebugOutput_None, "");
#endif

  /* Get mouse status */
  Vertex pointer;
  int buttons;
  ObjectId window;
  ON_ERR_RPT_RTN_V(window_get_pointer_info(0, &pointer.x, &pointer.y, &buttons,
                   &window, NULL), new_time + FREQUENCY);

  /*DEBUG("Null event - pointer:%d,%d buttons:%d window:%d", pointer.x,
  pointer.y, buttons, window);*/

  /* Check the status of the mouse buttons */
  if ((TEST_BITS(edit_win->button_held, BUTTONS_DRAG(Wimp_MouseButtonSelect)) &&
      !TEST_BITS(buttons, Wimp_MouseButtonSelect)) ||
      (TEST_BITS(edit_win->button_held, BUTTONS_DRAG(Wimp_MouseButtonAdjust)) &&
      !TEST_BITS(buttons, Wimp_MouseButtonAdjust))) {
    /* Drag ended */
    DEBUG("Buttons status indicates drag finished");
    edit_win->button_held = 0;
    free_pointer(edit_win);
  }

  if (TEST_BITS(buttons, Window_GetPointerNotToolboxWindow) ||
      window != edit_win->window_id) {
    /* pointer is outside our window */
    //DEBUG("Pointer is outside edit_win %d", edit_win->window_id);
    if (edit_win->mouse_in) {
      edit_win->mouse_in = false;
      StatusBar_show_pos(&edit_win->statusbar_data, true, (MapPoint){0, 0});
    }
  }
  else
  {
    WimpGetWindowStateBlock window_state = {
      .window_handle = edit_win->wimp_id,
    };
    ON_ERR_RPT_RTN_V(wimp_get_window_state(&window_state), new_time + FREQUENCY);
    /*DEBUG("visible area: x %d,%d  y %d,%d", window_state.visible_area.xmin,
    window_state.visible_area.xmax, window_state.visible_area.ymin, window_state.visible_area.ymax);*/
    //DEBUG("x scroll: %d  y scroll: %d", window_state.xscroll, window_state.yscroll);

    /* Convert pointer coordinates to map coordinate system */
    Vertex const window_origin = calc_window_origin(edit_win, &window_state);
    MapPoint map_pos = scr_to_map_coords(edit_win, window_origin, pointer);
    Editor *const editor = edit_win->editor;
    MapPoint grid_pos = Editor_map_to_grid_coords(editor, map_pos, edit_win);

    if (edit_win->pointer_trapped) {
      /* Clamp horizontal or vertical coordinates if requested */
      if (key_pressed(IntKeyNum_Ctrl)) {
        MapPoint const start_pos = Editor_grid_to_map_coords(editor, edit_win->start_drag_pos, edit_win);

        if (!edit_win->snap_horiz && !edit_win->snap_vert) {
          Vertex const window_origin = calc_window_origin(edit_win, &window_state);
          Vertex const start_scr = map_to_scr_coords(edit_win, window_origin, start_pos);
          MapPoint diff = MapPoint_abs_diff(grid_pos, edit_win->start_drag_pos);
          if (diff.x || diff.y) {
            if (edit_win->view.config.angle == MapAngle_East ||
                edit_win->view.config.angle == MapAngle_West) {
              diff = MapPoint_swap(diff);
            }
            if (diff.x >= diff.y) {
              DEBUGF("Enable clamp horizontally\n");
              restrict_ptr(edit_win, INT_MIN, start_scr.y);
            } else {
              DEBUGF("Enable clamp vertically\n");
              restrict_ptr(edit_win, start_scr.x, INT_MIN);
            }
          }
        }

        if (edit_win->snap_vert) {
          DEBUGF("Clamp vertically\n");
          map_pos.x = start_pos.x;
          grid_pos.x = edit_win->start_drag_pos.x;
        } else if (edit_win->snap_horiz) {
          DEBUGF("Clamp horizontally\n");
          map_pos.y = start_pos.y;
          grid_pos.y = edit_win->start_drag_pos.y;
        }

      } else {
        DEBUGF("Update clamp start pos\n");
        edit_win->start_drag_pos = grid_pos;

        if (edit_win->snap_horiz || edit_win->snap_vert) {
          DEBUGF("Free pointer from clamp\n");
          restrict_ptr(edit_win, INT_MIN, INT_MIN);
        }
      }
    }

    if (!edit_win->mouse_in || !MapPoint_compare(grid_pos, edit_win->old_grid_pos)) {
      StatusBar_show_pos(&edit_win->statusbar_data, false, grid_pos);
      edit_win->old_grid_pos = grid_pos;
      edit_win->mouse_in = true;
    }

    bool const scroll = Editor_pointer_update(editor, map_pos, edit_win->button_held, edit_win);

    /* Auto-scroll if necessary */
    //DEBUG("Auto-scroll %sallowed", scroll ? "" : "not ");
    if (scroll)
      auto_scroll(edit_win, &window_state, pointer, new_time);
    else
      edit_win->auto_scrolling = false; /* reset scroll interval timer */
  }
#if !DEBUG_TRACK_PTR
  DEBUG_SET_OUTPUT(old, "");
#endif

  return new_time + FREQUENCY;
}

static int pointer_leave(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  /* The mouse pointer has left our edit_win window */
  NOT_USED(event);
  NOT_USED(event_code);
  NOT_USED(id_block);
  EditWin *const edit_win = handle;
  DEBUG("EditWin %p received pointer leaving window event", (void *)edit_win);

  /* the Wimp unhelpfully sends a bogus pointer-leaving-window event when
  Wimp_DragBox is called */
  if (edit_win->wimp_drag_box)
  {
    return 1; /* claim event */
  }

  if (edit_win->null_poller)
  {
    scheduler_deregister(track_pointer, edit_win);
    edit_win->null_poller = false;
  }

  if (edit_win->mouse_in)
  {
    edit_win->mouse_in = false;
    StatusBar_show_pos(&edit_win->statusbar_data, true, (MapPoint){0, 0});
  }

  return 1; /* claim event */
}

static int select_drag_complete(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Called when a Wimp_DragBox operation is terminated by the user */
  NOT_USED(id_block);
  NOT_USED(event_code);
  WimpUserDragBoxEvent *wudbe = (WimpUserDragBoxEvent *)event;
  EditWin *const edit_win = handle;

  if (!edit_win->wimp_drag_box)
    return 0; /* unaware of drag - assume belongs to another edit_win */

  DEBUG("Wimp_DragBox terminated - bounds %d,%d,%d,%d",
  wudbe->bbox.xmin, wudbe->bbox.ymin, wudbe->bbox.xmax, wudbe->bbox.ymax);

  WimpGetWindowStateBlock window_state = {
    .window_handle = edit_win->wimp_id,
  };
  ON_ERR_RPT_RTN_V(wimp_get_window_state(&window_state), 0);

  /* Convert OS screen coordinates of drag box to map coordinate system
     Note that Wimp drag box maximum coordinates are exclusive */
  Vertex drag_box_min = {wudbe->bbox.xmin, wudbe->bbox.ymin};
  Vertex drag_box_max = {wudbe->bbox.xmax - 1, wudbe->bbox.ymax - 1};
  if (drag_box_min.x > drag_box_max.x) {
    SWAP(drag_box_max.x, drag_box_min.x);
  }
  if (drag_box_min.y > drag_box_max.y) {
    SWAP(drag_box_max.y, drag_box_min.y);
  }
  Vertex const window_origin = calc_window_origin(edit_win, &window_state);

  MapArea const map_bbox = {
    .min = scr_to_map_coords(edit_win, window_origin, drag_box_min),
    .max = scr_to_map_coords(edit_win, window_origin, drag_box_max)
  };

  Editor *const editor = edit_win->editor;
  Editor_drag_select_ended(editor, &map_bbox, edit_win);
  E(event_deregister_wimp_handler(-1, Wimp_EUserDrag, select_drag_complete, edit_win));
  edit_win->wimp_drag_box = false;

  /* Fake the pointer-leaving-window event we ignored (we will receive a
  pointer-entering-window event shortly if appropriate) */
  pointer_leave(Wimp_EPointerLeavingWindow, NULL, NULL, edit_win);

  return 1; /* claim event */
}

static void stop_drag(EditWin *const edit_win)
{
  assert(edit_win != NULL);
  if (edit_win->wimp_drag_box)
  {
    E(wimp_drag_box(CancelDrag));
    E(event_deregister_wimp_handler(-1, Wimp_EUserDrag, select_drag_complete, edit_win));
    edit_win->wimp_drag_box = false;

    /* Fake the pointer-leaving-window event we ignored (we will receive a
       pointer-entering-window event shortly if appropriate) */
    pointer_leave(Wimp_EPointerLeavingWindow, NULL, NULL, edit_win);
  }
}

static bool menu_is_open(EditSession *const session)
{
  assert(session != NULL);
  EditSession *const menu_session = MainMenu_get_session();
  return session == menu_session;
}

static int scroll_request(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Respond to scroll request events */
  NOT_USED(event_code);
  EditWin *const edit_win = handle;
  WimpScrollRequestEvent *const wsre = (WimpScrollRequestEvent *)event;

  DEBUG("Scroll request for window %d: x change %d, y change %d",
        wsre->open.window_handle, wsre->xscroll, wsre->yscroll);

  DEBUG("Current scroll offsets: %d,%d", wsre->open.xscroll,
        wsre->open.yscroll);

  Vertex const visible_size = calc_visible_size(edit_win, (WimpGetWindowStateBlock *)&wsre->open);

  switch (wsre->yscroll) {
    case -4: /* N.B. +/-3 are used by the Ursula Wimp */
      wsre->open.yscroll = edit_win->extent.y + visible_size.y;
      break;
    case -2:
      wsre->open.yscroll -= visible_size.y;
      break;
    case -1:
      wsre->open.yscroll -= 32;
      break;
    case 1:
      wsre->open.yscroll += 32;
      break;
    case 2:
      wsre->open.yscroll += visible_size.y;
      break;
    case 4:
      wsre->open.yscroll = 0;
      break;
  }

  switch (wsre->xscroll) {
    case -4: /* N.B. +/-3 are used by the Ursula Wimp */
      wsre->open.xscroll = 0;
      break;
    case -2:
      wsre->open.xscroll -= visible_size.x;
      break;
    case -1:
      wsre->open.xscroll -= 32;
      break;
    case 1:
      wsre->open.xscroll += 32;
      break;
    case 2:
      wsre->open.xscroll += visible_size.x;
      break;
    case 4:
      wsre->open.xscroll = edit_win->extent.x - visible_size.x;
      break;
  }

  DEBUG("Adjusted scroll offsets: %d,%d", wsre->open.xscroll, wsre->
  open.yscroll);

  E(toolbox_show_object(0, id_block->self_id,
    Toolbox_ShowObject_FullSpec, &wsre->open.visible_area, id_block->parent_id,
    id_block->parent_component));

  return 1; /* claim event */
}

static void show_perf(EditSession *const session, int const event_code)
{
  static const struct {
    int event_code;
    ShipType ship_type;
  } map[] = {
    {EVENT_MISSION_PERF1, ShipType_Fighter1},
    {EVENT_MISSION_PERF2, ShipType_Fighter2},
    {EVENT_MISSION_PERF3, ShipType_Fighter3},
    {EVENT_MISSION_PERF4, ShipType_Fighter4},
    {EVENT_MISSION_PERF13, ShipType_Big1},
    {EVENT_MISSION_PERF14, ShipType_Big2},
    {EVENT_MISSION_PERF15, ShipType_Big3},
  };
  for (size_t i = 0; i < ARRAY_SIZE(map); ++i)
  {
    if (map[i].event_code == event_code)
    {
      Session_show_performance(session, map[i].ship_type);
      break;
    }
  }
}

/* Clipboard code is here despite the clipboard not being specific to any one
   edit_win because this file contains most of the OS-specific code for editors.
   There's also the strange need for a window handle in data requests. */

static int estimate_cb(int const file_type, void *const client_handle)
{
  /* This function is called to estimate the size of the current clipboard
     contents, e.g. before pasting them into a document. */
  NOT_USED(client_handle);
  return Editor_estimate_clipboard(file_type_to_data_type(file_type, ""));
}

static bool cb_write(Writer *const writer, int const file_type,
  char const *const filename, void *const client_handle)
{
  /* This function is called to get the current clipboard contents, e.g.
     to paste them into a document. */
  NOT_USED(client_handle);
  return Editor_write_clipboard(writer, file_type_to_data_type(file_type, ""), filename);
}

static void cb_lost(void *const client_handle)
{
  /* This function is called to free any data held on the clipboard, for
     example if another application claims the global clipboard. */
  NOT_USED(client_handle);
  Editor_free_clipboard();
}

static void data_types_to_file_types(DataType const *const data_types, int *const file_types, size_t const n)
{
  assert(data_types);
  assert(file_types);

  size_t count;
  for (count = 0; data_types[count] != DataType_Count && count < n; ++count) {
    file_types[count] = data_type_to_file_type(data_types[count]);
  }

  file_types[count] = FileType_Null;
}

static bool claim_clipboard(Editor *const editor)
{
  DataType const *const export_data_types = Editor_get_export_data_types(editor);
  int export_file_types[10];
  data_types_to_file_types(export_data_types, export_file_types,
                           ARRAY_SIZE(export_file_types)-1);

  /* Claim the global clipboard
     (a side-effect is to free any clipboard data held by us) */
  return !E(entity2_claim(Wimp_MClaimEntity_Clipboard, export_file_types,
                          estimate_cb, cb_write, cb_lost, NULL));
}

static MapArea drag_bbox_to_grid2(EditWin const *const edit_win, MapPoint const map_pos,
  MapArea const *const drag_bbox)
{
  assert(edit_win);

  MapArea map_bbox = {{0}};
  MapArea_rotate(edit_win->view.config.angle, drag_bbox, &map_bbox);
  MapArea_translate(&map_bbox, map_pos, &map_bbox);

  Editor *const editor = edit_win->editor;
  return Editor_map_to_grid_area(editor, &map_bbox, edit_win);
}

static MapArea drag_bbox_to_grid(EditWin const *const edit_win,
  const WimpGetWindowStateBlock *const window_state,
  MapArea const *const drag_bbox, Vertex const pointer)
{
  Vertex const window_origin = calc_window_origin(edit_win, window_state);
  MapPoint const map_pos = scr_to_map_coords(edit_win, window_origin, pointer);
  return drag_bbox_to_grid2(edit_win, map_pos, drag_bbox);
}

static bool drop_read_cb(Reader *const reader, int const estimated_size,
  int const file_type, char const *const leaf_name, void *const client_handle)
{
  EditWin *const edit_win = client_handle;
  Editor *const editor = edit_win->editor;

  DataType const data_type = file_type_to_data_type(file_type, "");
  assert(data_type != DataType_Count);

  WimpGetWindowStateBlock window_state = {
    .window_handle = edit_win->wimp_id,
  };

  if (E(wimp_get_window_state(&window_state))) {
    return false;
  }

  MapArea const grid_bbox = drag_bbox_to_grid(edit_win, &window_state,
                                              &edit_win->drop_bbox, edit_win->drop_pos);

  return Editor_drop(editor, &grid_bbox, reader, estimated_size, data_type, leaf_name);
}

static bool paste_read_cb(Reader *const reader, int const estimated_size,
  int const file_type, char const *const leaf_name, void *const client_handle)
{
  EditWin *const edit_win = client_handle;
  Editor *const editor = edit_win->editor;

  /* file_type may be none of those in the requester's list of preferred types. */
  DataType const data_type = file_type_to_data_type(file_type, "");
  if (data_type == DataType_Count) {
    report_error(SFERROR(CBWrong), leaf_name, "");
    return false;
  }

  return Editor_start_pending_paste(editor, reader, estimated_size, data_type, leaf_name);
}

static void paste_failed_cb(const _kernel_oserror *const e, void *const client_handle)
{
  NOT_USED(client_handle);
  E(e);
}

static void init_data_request(EditWin const *const edit_win, WimpDataRequestMessage *const data_request)
{
  assert(edit_win);
  assert(data_request);

  *data_request = (WimpDataRequestMessage)
  {
    .destination_window = edit_win->wimp_id,
    .destination_icon = WimpIcon_WorkArea,
    .destination_x = 0,
    .destination_y = 0,
    .flags = Wimp_MDataRequest_Clipboard,
  };

  Editor *const editor = edit_win->editor;
  DataType const *const import_data_types = Editor_get_import_data_types(editor);
  data_types_to_file_types(import_data_types, data_request->file_types,
                           ARRAY_SIZE(data_request->file_types)-1);
}

static void begin_paste(EditWin *const edit_win)
{
  WimpDataRequestMessage data_request;
  init_data_request(edit_win, &data_request);
  entity2_cancel_requests(edit_win);
  E(entity2_request_data(&data_request, paste_read_cb, paste_failed_cb, edit_win));
}

static int useracthandler(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event);
  assert(id_block);
  EditWin *const edit_win = handle;
  assert(edit_win);
  EditSession *const session = EditWin_get_session(edit_win);
  Editor *const editor = edit_win->editor;

  /* Careful - handler is called for unclaimed toolbox events on any object */
  if (id_block->self_id != edit_win->window_id &&
      id_block->ancestor_id != edit_win->window_id)
    return 0; /* event not for us - pass it on */

  /* Handle hotkey/menu selection events */
  switch (event_code) {
    case EVENT_MISSION_BRIEF: /* Show mission briefing */
      if (!Session_has_data(session, DataType_Mission)) {
        putchar('\a'); /* no mission data loaded */
        return 1; /* claim event */
      }
      Session_show_briefing(session);
      return 1; /* claim event */

    case EVENT_MISSION_PERF1:
    case EVENT_MISSION_PERF2:
    case EVENT_MISSION_PERF3:
    case EVENT_MISSION_PERF4:
    case EVENT_MISSION_PERF13:
    case EVENT_MISSION_PERF14:
    case EVENT_MISSION_PERF15:
      show_perf(session, event_code);
      return 1; /* claim event */

    case EVENT_SPECIAL_SHIP: /* Show player's special ship dbox */
      if (!Session_has_data(session, DataType_Mission)) {
        putchar('\a'); /* no mission data loaded */
        return 1; /* claim event */
      }
      Session_show_special(session);
      return 1; /* claim event */

    case EVENT_QUICKSAVE:
      if (Session_can_quick_save(session)) {
        /* Can save immediately to existing path  */
        Session_quick_save(session);
        return 1; /* claim event */
      }
      /* otherwise continue as for normal save... */

    case EVENT_STD_SAVE:
      EditWin_show_dbox(edit_win, Toolbox_ShowObject_AsMenu, Session_get_ui_type(session) ==
                        UI_TYPE_MISSION ? SaveMiss_sharedid : SaveMap_sharedid);
      return 1; /* claim event */

    case EVENT_STD_CLOSE:
      close(edit_win, false);
      return 1; /* claim event */

    case EVENT_NEWVIEW:
      Session_new_edit_win(session, NULL);
      return 1; /* claim event */

    case EVENT_COPY_VIEW:
      Session_new_edit_win(session, edit_win);
      return 1; /* claim event */

    case EVENT_TOGGLE_GRID:
      edit_win->view.config.flags.GRID = !edit_win->view.config.flags.GRID;
      if (Editor_can_draw_grid(editor, edit_win)) {
        redraw_all(edit_win);
      }
      UtilsMenu_update(edit_win);
      return 1; /* claim event */

    case EVENT_BACK_COLOUR:
      BackCol_show(edit_win);
      return 1; /* claim event */

    case EVENT_TOGGLE_NUMBERS:
      edit_win->view.config.flags.NUMBERS = !edit_win->view.config.flags.NUMBERS;
      if (Editor_can_draw_numbers(editor, edit_win)) {
        redraw_all(edit_win);
      }
      UtilsMenu_update(edit_win);
      return 1; /* claim event */

    case EVENT_TOGGLE_STATUS:
      edit_win->view.config.show_status_bar = !edit_win->view.config.show_status_bar;
      show_or_hide_status_bar(edit_win);
      UtilsMenu_update(edit_win);
      return 1; /* claim event */

    case EVENT_TOGGLE_TBOX:
      Editor_set_tools_shown(editor, !Editor_get_tools_shown(editor), edit_win);
      UtilsMenu_update(edit_win);
      return 1; /* claim event */

    case EVENT_TOGGLE_PALETTE:
      Editor_set_pal_shown(editor, !Editor_get_pal_shown(editor), edit_win);
      UtilsMenu_update(edit_win);
      return 1; /* claim event */

    case EVENT_TOGGLE_ANIMS:
      if (!Session_has_data(session, DataType_OverlayMapAnimations)) {
        putchar('\a'); /* no animations loaded */
        return 1; /* claim event */
      }
      Session_set_anims_shown(session, !Session_get_anims_shown(session));
      UtilsMenu_update(edit_win);
      return 1; /* claim event */

    case EVENT_REVEAL_PALETTE:
      Editor_reveal_palette(editor);
      UtilsMenu_update(edit_win);
      return 1; /* claim event */

    case EVENT_STD_ZOOM:
      ZoomMenu_show(edit_win);
      return 1; /* claim event */

    case EVENT_STD_ORIENTATION:
      OrientMenu_show(edit_win);
      return 1; /* claim event */

    case EVENT_STD_GOTO:
      Goto_show(edit_win);
      return 1; /* claim event */

    case EVENT_ZOOM_IN:
      if (edit_win->wimp_drag_box || edit_win->dragging_obj || edit_win->pointer_trapped) {
        return 1;
      }
      if (edit_win->view.config.zoom_factor > EditWinZoomMin) {
        StatusBar_show_hint(&edit_win->statusbar_data, msgs_lookup("StatusZoomIn"));
        change_zoom(edit_win, edit_win->view.config.zoom_factor - 1);
      }
      if (menu_is_open(session)) {
        /* Close menu tree in case outdated */
        MainMenu_hide();
      }
      return 1; /* claim event */

    case EVENT_ZOOM_OUT:
      if (edit_win->wimp_drag_box || edit_win->dragging_obj || edit_win->pointer_trapped) {
        return 1;
      }
      if (edit_win->view.config.zoom_factor < EditWinZoomMax) {
        StatusBar_show_hint(&edit_win->statusbar_data, msgs_lookup("StatusZoomOut"));
        change_zoom(edit_win, edit_win->view.config.zoom_factor + 1);
      }
      if (menu_is_open(session)) {
        /* Close menu tree in case outdated */
        MainMenu_hide();
      }
      return 1; /* claim event */

    case EVENT_ROTATE_ANTICLOCKWISE:
      if (edit_win->wimp_drag_box || edit_win->dragging_obj || edit_win->pointer_trapped) {
        return 1;
      }
      StatusBar_show_hint(&edit_win->statusbar_data, msgs_lookup("StatusRotACW"));
      if (edit_win->view.config.angle > MapAngle_First) {
        change_angle(edit_win, edit_win->view.config.angle - 1);
      } else {
        change_angle(edit_win, MapAngle_Count - 1);
      }
      if (menu_is_open(session)) {
        /* Close menu tree in case outdated */
        MainMenu_hide();
      }
      return 1; /* claim event */

    case EVENT_ROTATE_CLOCKWISE:
      if (edit_win->wimp_drag_box || edit_win->dragging_obj || edit_win->pointer_trapped) {
        return 1;
      }
      StatusBar_show_hint(&edit_win->statusbar_data, msgs_lookup("StatusRotCW"));
      if (edit_win->view.config.angle + 1 < MapAngle_Count) {
        change_angle(edit_win, edit_win->view.config.angle + 1);
      } else {
        change_angle(edit_win, MapAngle_First);
      }
      if (menu_is_open(session)) {
        /* Close menu tree in case outdated */
        MainMenu_hide();
      }
      return 1; /* claim event */

    case EVENT_SCROLL_TOP:
    case EVENT_PAGE_UP:
    case EVENT_SCROLL_UP:
    case EVENT_SCROLL_BOT:
    case EVENT_PAGE_DOWN:
    case EVENT_SCROLL_DOWN:
    case EVENT_SCROLL_LHS:
    case EVENT_PAGE_LEFT:
    case EVENT_SCROLL_LEFT:
    case EVENT_SCROLL_RHS:
    case EVENT_PAGE_RIGHT:
    case EVENT_SCROLL_RIGHT:
      { /* To avoid duplication of scrollbar handling code
           we fake a Wimp scroll request event */
        WimpScrollRequestEvent wsre = {{edit_win->wimp_id}};
        ON_ERR_RPT_RTN_V(wimp_get_window_state((WimpGetWindowStateBlock *)
                         &wsre.open), 1);
        /* The above call overwrites wsre.xscroll with window flags,
        but that doesn't matter because... */

        switch (event_code) {
          case EVENT_SCROLL_RHS:
            wsre.xscroll = +4; /* N.B. +/-3 are used by the Ursula Wimp */
            break;
          case EVENT_PAGE_RIGHT:
            wsre.xscroll = +2;
            break;
          case EVENT_SCROLL_RIGHT:
            wsre.xscroll = +1;
            break;
          default:
            wsre.xscroll = 0;
            break;
          case EVENT_SCROLL_LEFT:
            wsre.xscroll = -1;
            break;
          case EVENT_PAGE_LEFT:
            wsre.xscroll = -2;
            break;
          case EVENT_SCROLL_LHS:
            wsre.xscroll = -4;
            break;
        }

        switch (event_code) {
          case EVENT_SCROLL_TOP:
            wsre.yscroll = +4; /* N.B. +/-3 are used by the Ursula Wimp */
            break;
          case EVENT_PAGE_UP:
            wsre.yscroll = +2;
            break;
          case EVENT_SCROLL_UP:
            wsre.yscroll = +1;
            break;
          default:
            wsre.yscroll = 0;
            break;
          case EVENT_SCROLL_DOWN:
            wsre.yscroll = -1;
            break;
          case EVENT_PAGE_DOWN:
            wsre.yscroll = -2;
            break;
          case EVENT_SCROLL_BOT:
            wsre.yscroll = -4;
            break;
        }

        /* Strictly the 'wsre' block is too small for a WimpPollBlock but we
        know our function doesn't use beyond sizeof(WimpScrollRequestEvent) */
        scroll_request(Wimp_EScrollRequest, (WimpPollBlock *)&wsre,
                             id_block, handle);
      }
      return 1; /* claim event */

    case EVENT_STD_SEL_ALL:
      Editor_select_all(editor);
      EditMenu_update(editor);
      EffectMenu_update(editor);
      return 1; /* claim event */

    case EVENT_STD_CLEAR_SEL:
      Editor_clear_selection(editor);
      EditMenu_update(editor);
      EffectMenu_update(editor);
      return 1; /* claim event */

    case EVENT_DELETE:
      if (Editor_can_delete(editor)) {
        Editor_delete(editor);
        EditMenu_update(editor);
        EffectMenu_update(editor);
      }
      return 1; /* claim event */

    case EVENT_STD_CUT:
      if (Editor_can_delete(editor) && claim_clipboard(editor) &&
          Editor_cut(editor)) {
        EditMenu_update(editor);
        EffectMenu_update(editor);
      }
      return 1; /* claim event */

    case EVENT_STD_COPY:
      if (Editor_num_selected(editor) && claim_clipboard(editor) &&
          Editor_copy(editor)) {
        EditMenu_update(editor);
        EffectMenu_update(editor);
      }
      return 1; /* claim event */

    case EVENT_STD_PASTE:
      begin_paste(edit_win);
      return 1; /* claim event */

    case EVENT_SET_DEFAULT_DISPLAY_CHOICES:
      Config_set_default_view(&edit_win->view.config);
      Config_set_default_animate_enabled(Session_get_anims_shown(session));
      Config_set_default_palette_enabled(Editor_get_pal_shown(editor));
      Config_set_default_tool_bar_enabled(Editor_get_tools_shown(editor));
      return 1; /* claim event */

    case EVENT_SET_DEFAULT_MODE_CHOICES:
      Config_set_default_edit_mode(Editor_get_edit_mode(editor));
      return 1; /* claim event */

    case EVENT_SET_DEFAULT_TOOL_CHOICES:
      Config_set_default_edit_tool(Editor_get_tool(editor));
      Config_set_default_fill_is_global(Editor_get_fill_is_global(editor));
      Config_set_default_plot_shape(Editor_get_plot_shape(editor));
      Config_set_default_brush_size(Editor_get_brush_size(editor));
      Config_set_default_wand_size(Editor_get_wand_size(editor));
      return 1; /* claim event */

    case EVENT_SET_DEFAULT_EDITOR_CHOICES:
      useracthandler(EVENT_SET_DEFAULT_MODE_CHOICES, event, id_block, handle);
      useracthandler(EVENT_SET_DEFAULT_TOOL_CHOICES, event, id_block, handle);
      return 1; /* claim event */

    case EVENT_SET_DEFAULT_ALL_CHOICES:
      useracthandler(EVENT_SET_DEFAULT_EDITOR_CHOICES, event, id_block, handle);
      useracthandler(EVENT_SET_DEFAULT_DISPLAY_CHOICES, event, id_block, handle);
      return 1; /* claim event */

    case EVENT_STD_EDIT:
      if (!Editor_can_edit_properties(editor)) {
        putchar('\a');
      } else {
        Editor_edit_properties(editor, edit_win);
      }
      return 1; /* claim event */

    case EVENT_PAINT_SEL:
      if (!Editor_num_selected(editor)) {
        putchar('\a');
      } else {
        Editor_paint_selected(editor);
      }
      return 1; /* claim event */

    case EVENT_CLIP_OVERLAY:
      if (!Editor_can_clip_overlay(editor)) {
        putchar('\a');
      } else {
        Editor_clip_overlay(editor);
      }
      return 1; /* claim event */

    case EVENT_CREATE_TRANSFER:
      if (!Editor_can_create_transfer(editor)) {
        putchar('\a'); /* no map area selected */
      } else {
        NewTransfer_show(edit_win);
      }
      return 1; /* claim event */

    case EVENT_ESCAPE:
      Editor_cancel(editor, edit_win);
      break;

    default:
      Editor_misc_event(editor, event_code);
      return 0;
  }

  return 1; /* claim event */
}

static int open_window(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  /* We only get these events in response to the user dragging or resizing
     the window. */
  NOT_USED(event_code);
  WimpOpenWindowRequestEvent *const wowre =
    (WimpOpenWindowRequestEvent *)event;
  EditWin *const edit_win = handle;

  E(toolbox_show_object(0, id_block->self_id,
             Toolbox_ShowObject_FullSpec, &wowre->visible_area,
             id_block->parent_id, id_block->parent_component));

  int const width = wowre->visible_area.xmax -
                    wowre->visible_area.xmin;

  Editor *const editor = edit_win->editor;
  StatusBar_reformat(&edit_win->statusbar_data, width,
    Editor_get_coord_field_width(editor));

  return 1; /* claim event */
}

static int mouse_click(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  /* The user has clicked mouse button on our window */
  NOT_USED(event_code);
  NOT_USED(id_block);
  EditWin *const edit_win = handle;
  WimpMouseClickEvent const *const mouse_click = (WimpMouseClickEvent *)event;
  WimpGetWindowStateBlock window_state = {
    .window_handle = edit_win->wimp_id,
  };

  DEBUG("Mouse click on edit_win %p at %d,%d (buttons %d)", (void *)edit_win,
  mouse_click->mouse_x, mouse_click->mouse_y, mouse_click->buttons);

  if (TEST_BITS(mouse_click->buttons,
                BUTTONS_DRAG(Wimp_MouseButtonSelect | Wimp_MouseButtonAdjust)))
    edit_win->button_held = mouse_click->buttons & BUTTONS_DRAG(
                             Wimp_MouseButtonSelect | Wimp_MouseButtonAdjust);

  ON_ERR_RPT_RTN_V(wimp_get_window_state(&window_state), 1);

  Vertex const mouse_pos = {mouse_click->mouse_x, mouse_click->mouse_y};

  if (TEST_BITS(mouse_click->buttons,
                BUTTONS_CLICK(Wimp_MouseButtonSelect | Wimp_MouseButtonAdjust))) {
    /* Claim the input focus unless we already have it */
    WimpGetCaretPositionBlock caret;
    if (!E(wimp_get_caret_position(&caret)) && caret.window_handle != edit_win->wimp_id)
      E(wimp_set_caret_position(edit_win->wimp_id, -1, 0, 0, -1, -1));
  }

  Editor *const editor = edit_win->editor;
  Vertex const window_origin = calc_window_origin(edit_win, &window_state);
  MapPoint const map_pos = scr_to_map_coords(edit_win, window_origin, mouse_pos);

  if (Editor_get_tool(editor) == EDITORTOOL_MAGNIFIER) {
    /* Mode independent code for mouse magnifier */
    if (TEST_BITS(mouse_click->buttons,
                  BUTTONS_CLICK(Wimp_MouseButtonSelect | Wimp_MouseButtonAdjust))) {
      /* Calculate future centre of edit_win (in work area coordinates) */
      int zoom_factor = edit_win->view.config.zoom_factor;

      /* SELECT or ADJUST click - zoom map in to or out from pointer position */
      if (TEST_BITS(mouse_click->buttons, BUTTONS_CLICK(Wimp_MouseButtonSelect))) {
        if (edit_win->view.config.zoom_factor > EditWinZoomMin) {
          /* Magnification of map is doubled */
          --zoom_factor;
          StatusBar_show_hint(&edit_win->statusbar_data,
                                msgs_lookup("StatusZoomIn"));
        }
      } else if (TEST_BITS(mouse_click->buttons, BUTTONS_CLICK(Wimp_MouseButtonAdjust))) {
        if (edit_win->view.config.zoom_factor < EditWinZoomMax) {
          /* Magnification of map is halved */
          ++zoom_factor;
          StatusBar_show_hint(&edit_win->statusbar_data,
                                msgs_lookup("StatusZoomOut"));
        }
      }

      Vertex const work_area_origin = calc_work_area_origin(&window_state);
      change_zoom_recentre(edit_win, zoom_factor, Editor_map_to_grid_coords(editor, map_pos, edit_win));
    }
  } else {
    /* Convert OS screen coordinates of mouse click to map coordinates */
    bool const trap = Editor_mouse_click(editor, map_pos, mouse_click->buttons,
                                         key_pressed(IntKeyNum_Shift), edit_win);
    if (trap) {
      if (!edit_win->pointer_trapped) {
        edit_win->start_drag_pos = Editor_map_to_grid_coords(editor, map_pos, edit_win);
        restrict_ptr(edit_win, INT_MIN, INT_MIN);
      }
    } else {
      free_pointer(edit_win);
    }
  }
  MapPoint const grid_pos = get_scroll_pos(edit_win, &window_state);

  return 1; /* claim event */
}

static void caret_lost(void *const client_handle)
{
  EditWin *const edit_win = client_handle;

  DEBUGF("Notified that input focus lost from edit_win %p\n", (void *)edit_win);
  assert(edit_win != NULL);

  if (edit_win->has_input_focus)
  {
    // FIXME redraw_current_select(edit_win);
    edit_win->has_input_focus = false;
  }
}

static int gain_caret(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  /* The user has clicked mouse button on our window */
  NOT_USED(event);
  NOT_USED(event_code);
  NOT_USED(id_block);
  EditWin *const edit_win = handle;

  if (!edit_win->has_input_focus &&
      !E(entity2_claim(Wimp_MClaimEntity_CaretOrSelection, NULL, NULL, NULL,
                       caret_lost, edit_win)))
  {
    edit_win->has_input_focus = true;
    // FIXME redraw_current_select(edit_win);
  }

  return 1; /* claim event */
}

static int pointer_enter(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  /* The mouse pointer has entered our map window */
  NOT_USED(event);
  NOT_USED(event_code);
  NOT_USED(id_block);
  EditWin *const edit_win = handle;
  DEBUG("EditWin %p received pointer entering window event", (void *)edit_win);

  ON_ERR_RPT_RTN_V(scheduler_register_delay(track_pointer, edit_win, 0, PRIORITY), 1);

  edit_win->null_poller = true;
  edit_win->auto_scrolling = false; /* reset scroll interval timer */

  return 1; /* claim event */
}

static int redraw_window(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Process redraw events */
  NOT_USED(event_code);
  NOT_USED(id_block);
  const WimpRedrawWindowRequestEvent *const wrwre =
    (WimpRedrawWindowRequestEvent *)event;
#ifdef DEBUG_OUTPUT
  clock_t const start = clock();
#endif

#if !DEBUG_REDRAW
  DebugOutput old = DEBUG_SET_OUTPUT(DebugOutput_None, "");
#endif

  WimpRedrawWindowBlock block = {wrwre->window_handle};
  int more;
  if (!E(wimp_redraw_window(&block, &more)) && more) {
    redraw_loop(handle, &block);
  }

#ifdef DEBUG_OUTPUT
  clock_t const period = clock() - start;
  if (period > MAX_REDRAW_PERIOD) {
    DEBUGF("Redraw event period: %g\n", (double)period / CLOCKS_PER_SEC);
  }
#endif

#if !DEBUG_REDRAW
  DEBUG_SET_OUTPUT(old, "");
#endif

  return 1; /* claim event */
}

static int close_window(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  /* User has clicked main window close button */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(id_block);
  EditWin *const edit_win = handle;
  WimpGetPointerInfoBlock ptr;
  bool open_parent;

  /* Check for ADJUST-click on close icon */
  if (!E(wimp_get_pointer_info(&ptr)) && TEST_BITS(ptr.button_state, Wimp_MouseButtonAdjust)) {
    if (key_pressed(IntKeyNum_Shift)) {
      /* Shift-ADJUST: open parent directory, don't attempt to close window */
      Session_openparentdir(EditWin_get_session(edit_win));
      return 1;
    }
    /* ADJUST click with no shift: Open parent and attempt to close window */
    open_parent = true;
  } else
    open_parent = false; /* no ADJUST click */

  close(edit_win, open_parent);

  return 1; /* claim event */
}

/* ----------------------------------------------------------------------- */

static void relinquish_drag(void)
{
  if (drag_claim_edit_win != NULL)
  {
    DEBUGF("EditWin %p relinquishing drag\n", (void *)drag_claim_edit_win);

    /* Undraw the ghost caret, if any */
    Editor *const drag_claim_editor = drag_claim_edit_win->editor;
    Editor_hide_ghost_drop(drag_claim_editor);

    if (drag_claim_edit_win->mouse_in)
    {
      drag_claim_edit_win->mouse_in = false;
      StatusBar_show_pos(&drag_claim_edit_win->statusbar_data, true, (MapPoint){0, 0});
    }

    drag_claim_edit_win->dragclaim_msg_ref = 0;
    drag_claim_edit_win = NULL;
  }
}

static void maybe_relinquish_drag(const WimpDraggingMessage *const dragging)
{
  /* If this Dragging message is not for the window that previously claimed
     the drag then undraw its ghost caret and stop auto-scrolling. */
  assert(dragging != NULL);
  if (drag_claim_edit_win != NULL &&
      (dragging->window_handle != drag_claim_edit_win->wimp_id ||
       dragging->icon_handle < WimpIcon_WorkArea))
  {
    relinquish_drag();
  }
}

static int dragging_msg_handler(WimpMessage *const message, void *const handle)
{
  assert(message);
  assert(message->hdr.action_code == Wimp_MDragging);
  EditWin *const edit_win = handle;
  const WimpDraggingMessage *const dragging =
    (WimpDraggingMessage *)&message->data;

  DEBUGF("Received a Dragging message (ref. %d) for icon %d in window &%x\n"
        " (coordinates %d,%d)\n", message->hdr.my_ref, dragging->icon_handle,
        dragging->window_handle, dragging->x, dragging->y);

  DEBUGF("Bounding box of data is %d,%d,%d,%d\n", dragging->bbox.xmin,
        dragging->bbox.ymin, dragging->bbox.xmax, dragging->bbox.ymax);

  assert(edit_win != NULL);

  maybe_relinquish_drag(dragging);

  /* Check whether the pointer is within our window (excluding borders) */
  if (dragging->window_handle != edit_win->wimp_id ||
      dragging->icon_handle < WimpIcon_WorkArea)
  {
    return 0; /* No - do not claim message */
  }

  assert(drag_claim_edit_win == NULL || drag_claim_edit_win == edit_win);

  Editor *const editor = edit_win->editor;
  if (!Editor_allow_drop(editor)) {
    return 1;
  }

  bool const is_local = (message->hdr.sender == task_handle && drag_origin_edit_win);
  Editor *const drag_origin_editor = is_local ? drag_origin_edit_win->editor : NULL;

  if (drag_origin_editor &&
      Editor_get_edit_mode(drag_origin_editor) != Editor_get_edit_mode(editor)) {
    DEBUGF("Editing mode mismatch\n");
    relinquish_drag();
    return 1;
  }

  DataType const *const import_data_types = Editor_get_dragged_data_types(editor);
  int import_file_types[10];
  data_types_to_file_types(import_data_types, import_file_types,
                           ARRAY_SIZE(import_file_types)-1);

  /* The sender can set a flag to prevent us from claiming the drag again
     (i.e. force us to relinquish it if we had claimed it) */
  if (TEST_BITS(dragging->flags, Wimp_MDragging_DoNotClaimMessage))
  {
    DEBUGF("Forbidden from claiming this drag\n");
    relinquish_drag();
    return 1;
  }

  if (common_file_type(import_file_types, dragging->file_types) == FileType_Null)
  {
    DEBUGF("We don't like any of their export file types\n");
    relinquish_drag();
    return 1;
  }

  DEBUGF("We can handle one of the file types offered\n");

  WimpGetWindowStateBlock window_state = {
    .window_handle = edit_win->wimp_id,
  };

  SchedulerTime time;

  if (E(window_get_pointer_info(0, &edit_win->drop_pos.x, &edit_win->drop_pos.y, NULL, NULL, NULL)) ||
      E(wimp_get_window_state(&window_state)) ||
      E(os_read_monotonic_time(&time)))
  {
    relinquish_drag();
    return 1;
  }

  /* Convert pointer coordinates to map coordinate system */
  Vertex const window_origin = calc_window_origin(edit_win, &window_state);
  MapPoint const map_pos = scr_to_map_coords(edit_win, window_origin, edit_win->drop_pos);
  MapPoint const grid_pos = Editor_map_to_grid_coords(editor, map_pos, edit_win);

  if (!edit_win->mouse_in || !MapPoint_compare(grid_pos, edit_win->old_grid_pos)) {
    StatusBar_show_pos(&edit_win->statusbar_data, false, grid_pos);
    edit_win->old_grid_pos = grid_pos;
    edit_win->mouse_in = true;
  }

  auto_scroll(edit_win, &window_state, edit_win->drop_pos, time);

  /* Update the ghost objects position so that it follows the mouse
     pointer whilst this editing window is claiming the drag */

  /* x0 > x1 indicates no bounding box */

  if (dragging->bbox.xmin <= dragging->bbox.xmax) {
    edit_win->drop_bbox = (MapArea){
      .min = {dragging->bbox.xmin, dragging->bbox.ymin},
      .max = {dragging->bbox.xmax - 1, dragging->bbox.ymax - 1}
    };
  } else {
    edit_win->drop_bbox = (MapArea){{0,0},{0,0}};
  }

  unsigned int flags = 0;

  SpriteParams param = {.r3 = 2, .r4 = 0, .r5 = 16};
  // Sprite may not be in the global pool, depending on OS version
  if (wimp_sprite_op(SPRITEOP_SETPTRSHAPE, "ptr_drop", &param) == NULL) {
    flags |= Wimp_MDragClaim_PtrShapeChanged;
  }

  MapArea const grid_bbox = drag_bbox_to_grid2(edit_win, map_pos, &edit_win->drop_bbox);

  if (Editor_show_ghost_drop(editor, &grid_bbox, drag_origin_editor) ||
      (is_local &&
        (EditWin_get_zoom(drag_origin_edit_win) != EditWin_get_zoom(edit_win) ||
         EditWin_get_angle(drag_origin_edit_win) != EditWin_get_angle(edit_win)))) {
    flags |= Wimp_MDragClaim_RemoveDragBox;
  }

  drag_claim_edit_win = edit_win; // already showed the ghost

  if (claim_drag(message, import_file_types, flags, &edit_win->dragclaim_msg_ref)) {
    DEBUGF("Drag claimed by edit_win %p\n", (void *)edit_win);
  } else {
    relinquish_drag();
  }

  return 1; /* claim message */
}

static int datasave_msg_handler(WimpMessage *const message, void *const handle)
{
  /* This handler should receive DataSave messages before CBLibrary's Loader
     component. We need to intercept replies to a DragClaim message. */
  EditWin *edit_win = handle;

  assert(edit_win != NULL);
  assert(message != NULL);
  assert(message->hdr.action_code == Wimp_MDataSave);

  DEBUGF("EditWin %p evaluating a DataSave message (ref. %d in reply to %d)\n",
        (void *)edit_win, message->hdr.my_ref, message->hdr.your_ref);

  if (message->hdr.your_ref != 0) {
    if (!drag_claim_edit_win || drag_claim_edit_win->dragclaim_msg_ref != message->hdr.your_ref)
    {
      DEBUGF("Could be a reply to a DataRequest message\n");
      return 0;
    }

    edit_win = drag_claim_edit_win;
    relinquish_drag();
  }

  assert(edit_win);
  if (edit_win->wimp_id != message->data.data_save.destination_window)
  {
    DEBUGF("Destination is not in edit_win %p\n", (void *)edit_win);
    return 0; /* message is not intended for this editing window */
  }

  Editor *const editor = edit_win->editor;
  if (!Editor_allow_drop(editor)) {
    return 1;
  }

  if (message->hdr.your_ref != 0) {
    /* It's a reply to our drag claim message from a task about to send dragged data. */
    relinquish_drag();
  } else {
    edit_win->drop_bbox = (MapArea){{0,0},{0,0}};
  }

  edit_win->drop_pos = (Vertex){message->data.data_save.destination_x,
                                message->data.data_save.destination_y};

  DataType const *const import_data_types = Editor_get_dragged_data_types(editor);
  int import_file_types[10];
  data_types_to_file_types(import_data_types, import_file_types,
                           ARRAY_SIZE(import_file_types)-1);

  if (!in_file_types(message->data.data_save.file_type, import_file_types))
  {
    report_error(SFERROR(BadFileType), message->data.data_save.leaf_name, "");
    return 1;
  }

  E(loader3_receive_data(message, drop_read_cb, paste_failed_cb, edit_win));

  return 1; /* claim message */
}

static int dataload_msg_handler(WimpMessage *const message, void *const handle)
{
  EditWin *const edit_win = handle;

  assert(message);
  assert(message->hdr.action_code == Wimp_MDataLoad);
  DEBUGF("Received a DataLoad message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  if (message->hdr.your_ref)
  {
    DEBUGF("EditWin %p ignoring a reply\n", (void *)edit_win);
    return 0; /* message is a reply (should be dealt with by Loader3 module) */
  }

  assert(edit_win != NULL);

  if (edit_win->wimp_id != message->data.data_load.destination_window)
  {
    DEBUGF("Destination is not in edit_win %p\n", (void *)edit_win);
    return 0; /* message is not intended for this editing window */
  }

  Editor *const editor = edit_win->editor;
  if (!Editor_allow_drop(editor)) {
    return 1;
  }

  DataType const *const import_data_types = Editor_get_dragged_data_types(editor);
  int import_file_types[10];
  data_types_to_file_types(import_data_types, import_file_types,
                           ARRAY_SIZE(import_file_types)-1);

  if (!in_file_types(message->data.data_load.file_type, import_file_types))
  {
    report_error(SFERROR(BadFileType), message->data.data_load.leaf_name, "");
    return 1;
  }

  edit_win->drop_bbox = (MapArea){{0,0},{0,0}};
  edit_win->drop_pos = (Vertex){message->data.data_load.destination_x,
                                message->data.data_load.destination_y};

  bool success = false;
  success = loader3_load_file(message->data.data_load.leaf_name,
    message->data.data_load.file_type, drop_read_cb, paste_failed_cb, edit_win);

  if (success)
  {
    /* Acknowledge that the file was loaded successfully
     (just a courtesy message, we don't expect a reply) */
    message->hdr.your_ref = message->hdr.my_ref;
    message->hdr.action_code = Wimp_MDataLoadAck;

    if (!E(wimp_send_message(Wimp_EUserMessage,
                message, message->hdr.sender, 0, NULL)))
    {
      DEBUGF("Sent DataLoadAck message (ref. %d)\n", message->hdr.my_ref);
    }
  }

  return 1; /* claim message */
}

static const struct
{
  int                 msg_no;
  WimpMessageHandler *handler;
}
message_handlers[] =
{
  {
    Wimp_MDragging,
    dragging_msg_handler
  },
  {
    Wimp_MDataSave,
    datasave_msg_handler
  },
  {
    Wimp_MDataLoad,
    dataload_msg_handler
  }
};

static void deregister_msg_partial(EditWin *const edit_win, size_t i)
{
    while (i-- > 0)
    {
      event_deregister_message_handler(message_handlers[i].msg_no,
                                       message_handlers[i].handler,
                                       edit_win);
    }
}

static bool register_msg(EditWin *const edit_win)
{
  assert(edit_win != NULL);

  for (size_t i = 0; i < ARRAY_SIZE(message_handlers); i++)
  {
    if (E(event_register_message_handler(message_handlers[i].msg_no,
                                         message_handlers[i].handler,
                                         edit_win)))
    {
      deregister_msg_partial(edit_win, i);
      return false;
    }
  }

  return true;
}

static void deregister_msg(EditWin *const edit_win)
{
  deregister_msg_partial(edit_win, ARRAY_SIZE(message_handlers));
}

static bool register_wimp_handlers(EditWin *const edit_win)
{
  assert(edit_win != NULL);

  static const struct {
    int event_code;
    WimpEventHandler *handler;
  } handlers[] = {
    { Wimp_EOpenWindow, open_window },
    { Wimp_ECloseWindow, close_window },
    { Wimp_EScrollRequest, scroll_request },
    { Wimp_ERedrawWindow, redraw_window },
    { Wimp_EMouseClick, mouse_click },
    { Wimp_EGainCaret, gain_caret },
    { Wimp_EPointerEnteringWindow, pointer_enter },
    { Wimp_EPointerLeavingWindow, pointer_leave }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    if (E(event_register_wimp_handler(edit_win->window_id,
                                      handlers[i].event_code,
                                      handlers[i].handler, edit_win)))
    {
      return false;
    }
  }
  return true;
}

static bool drag_write(Writer *const writer, int const file_type,
  char const *const filename, void *const client_handle)
{
  /* This function is called to send the selected data when one of our drags
     terminates. We could predict the file type but don't bother. */
  EditWin *const src_edit_win = client_handle;
  assert(src_edit_win != NULL);

  DataType const data_type = file_type_to_data_type(file_type, "");
  return Editor_drag_obj_remote(src_edit_win->editor, writer, data_type, filename);
}

static void drag_failed(const _kernel_oserror *const error,
  void *const client_handle)
{
  EditWin *const src_edit_win = client_handle;
  assert(src_edit_win != NULL);
  if (error != NULL)
  {
    err_report(error->errnum, msgs_lookup_subn("SaveFail", 1, error->errmess));
  }
  Editor_cancel_drag_obj(src_edit_win->editor);
}

static void drag_moved(int const file_type, char const *const file_path,
  int const datasave_ref, void *const client_handle)
{
  EditWin *const src_edit_win = client_handle;
  assert(src_edit_win != NULL);
  NOT_USED(datasave_ref);
  NOT_USED(file_type);
  NOT_USED(file_path);

  DEBUGF("Selection saved to %s with DataSave message %d\n",
    file_path != NULL ? file_path : "unsafe destination", datasave_ref);

  Editor_delete(src_edit_win->editor);
}

static void local_drop(EditWin const *const dest_edit_win, EditWin const *const src_edit_win,
  bool const shift_held, Vertex const mouse_pos)
{
  Editor *const dest_editor = dest_edit_win->editor;
  Editor *const src_editor = src_edit_win->editor;

  WimpGetWindowStateBlock window_state = {
    .window_handle = dest_edit_win->wimp_id,
  };
  if (E(wimp_get_window_state(&window_state))) {
    Editor_cancel_drag_obj(src_editor);
    return;
  }

  MapArea const grid_bbox = drag_bbox_to_grid(dest_edit_win, &window_state,
                                              &src_edit_win->sent_drag_bbox, mouse_pos);

  if (dest_edit_win != src_edit_win)
  {
    if (shift_held)
    {
      if (Editor_get_session(dest_editor) == Editor_get_session(src_editor)) {
        Editor_drag_obj_move(dest_editor, &grid_bbox, src_editor);
      } else {
        if (Editor_drag_obj_copy(dest_editor, &grid_bbox, src_editor)) {
          Editor_delete(src_edit_win->editor);
        }
      }
    }
    else
    {
      Editor_drag_obj_copy(dest_editor, &grid_bbox, src_editor);
    }
  }
  else if (shift_held)
  {
    Editor_drag_obj_copy(dest_editor, &grid_bbox, src_editor);
  }
  else
  {
    Editor_drag_obj_move(dest_editor, &grid_bbox, src_editor);
  }
}

/* A drag terminates because the user released all mouse buttons.
 * The drop location can be used to construct a DataSave message.
 * If the drag was being claimed by a task then 'claimant_task' will be its handle
 * and 'claimant_ref' will be the ID of its last DragClaim message; otherwise
 * 'claimant_task' will be 0. The file type negotiated with the drag claimant
 * (if any) is one of those on the list passed to drag_start.
 * Return true if a DataSave message was sent to the drag claimant.
 */
static bool drop_method(bool const shift_held, int const window, int const icon,
                        int const mouse_x, int const mouse_y,
                        int const file_type, int const claimant_task, int const claimant_ref,
                        void *const client_handle)
{
  bool saved = true;
  EditWin *const src_edit_win = client_handle;
  EditWin *dest_edit_win = src_edit_win;

  assert(src_edit_win->dragging_obj);
  src_edit_win->dragging_obj = false;

  if (src_edit_win->wimp_id == window) {
    DEBUGF("Drag terminated within source window\n");
    dest_edit_win = src_edit_win;
  } else {
    DEBUGF("Drag terminated in another window\n");
    dest_edit_win = Session_edit_win_from_wimp_handle(window);
  }

  if (dest_edit_win != NULL) {
    /* It's more robust to stop the drag now instead of returning false
       and waiting for a final Dragging message. */
    if (drag_claim_edit_win == dest_edit_win)
    {
      relinquish_drag();
    }

    local_drop(dest_edit_win, src_edit_win, shift_held, (Vertex){mouse_x, mouse_y});

  } else if (!Session_drag_obj_link(EditWin_get_session(src_edit_win), window, icon, src_edit_win->editor)) {
    DEBUGF("Drag destination is remote\n");
    WimpMessage msg = {
      .hdr.your_ref = claimant_ref,
      /* action code and message size are filled out automatically */
      .data.data_save = {
        .destination_window = window,
        .destination_icon = icon,
        .destination_x = mouse_x,
        .destination_y = mouse_y,
        .estimated_size = 0,//estimate_size(file_type, source_size),
        .file_type = file_type,
      }
    };

    STRCPY_SAFE(msg.data.data_save.leaf_name, msgs_lookup("LeafName"));

    if (E(saver2_send_data(claimant_task, &msg, drag_write,
                           shift_held ? drag_moved : NULL, drag_failed, src_edit_win)))
    {
      Editor_cancel_drag_obj(src_edit_win->editor);
      saved = false;
    }
  }

  return saved;
}

static bool split_redraw_area_cb(MapArea const *area, void *arg)
{
  redraw_area(arg, area, false);
  return false;
}

static bool split_redraw_area_imm_cb(MapArea const *area, void *arg)
{
  redraw_area(arg, area, true);
  return false;
}

static bool read_hill(EditWin const *const edit_win, MapPoint pos)
{
  return objects_ref_is_hill(ObjectsEdit_read_ref(&edit_win->read_obj_ctx, pos));
}

static void redraw_object(MapPoint const centre, MapArea *const area, EditWin *const edit_win)
{
  assert(edit_win);
  DEBUGF("redraw_object %" PRIMapCoord ",%" PRIMapCoord "\n", centre.x, centre.y);
  MapArea_translate(area, centre, area);
  MapAreaCol_add(&edit_win->pending_redraws, area);
}

static void redraw_hill(EditWin *const edit_win, MapPoint const pos,
  HillType const old_type, unsigned char (*const old_heights)[HillCorner_Count],
  HillType const new_type, unsigned char (*const new_heights)[HillCorner_Count])
{
  DEBUGF("redraw_hill %" PRIMapCoord ",%" PRIMapCoord "\n", pos.x, pos.y);
  MapPoint const centre = ObjLayout_map_coords_to_centre(&edit_win->view,
                             MapPoint_mul_log2(pos, Hill_ObjPerHillLog2));

  if (old_type != HillType_None) {
    MapArea old_area = ObjGfxMeshes_get_hill_bbox(old_type, old_heights, edit_win->view.config.angle);
    redraw_object(centre, &old_area, edit_win);
  }

  if (new_type != HillType_None) {
    MapArea new_area = ObjGfxMeshes_get_hill_bbox(new_type, new_heights, edit_win->view.config.angle);
    redraw_object(centre, &new_area, edit_win);
  }
}

static bool hills_need_update(MapPoint const pos)
{
  return (pos.x % Hill_ObjPerHill) == 0 &&
         (pos.y % Hill_ObjPerHill) == 0;
}

static void paste_probe_cb(int const file_type, void *const client_handle)
{
  EditWin *const edit_win = client_handle;
  assert(edit_win != NULL);

  Editor *const editor = edit_win->editor;
  DataType const *const import_data_types = Editor_get_import_data_types(editor);
  int import_file_types[10];
  data_types_to_file_types(import_data_types, import_file_types,
                           ARRAY_SIZE(import_file_types)-1);

  Editor_set_paste_enabled(editor, in_file_types(file_type, import_file_types));
}

static void probe_failed_cb(const _kernel_oserror *const e, void *const client_handle)
{
  EditWin *const edit_win = client_handle;
  assert(edit_win != NULL);
  NOT_USED(e);
  Editor_set_paste_enabled(edit_win->editor, false);
}

static void update_read_obj_ctx(EditWin *const edit_win)
{
  struct ObjEditContext const *const objects = Session_get_objects(EditWin_get_session(edit_win));
  ViewDisplayFlags const display_flags = EditWin_get_display_flags(edit_win);

  edit_win->read_obj_ctx = (struct ObjEditContext){
    .base = !display_flags.OBJECTS ? NULL : objects->base,
    .overlay = !display_flags.OBJECTS_OVERLAY ? NULL : objects->overlay,
    .triggers = objects->triggers};
}

static void update_read_map_ctx(EditWin *const edit_win)
{
  struct MapEditContext const *const map = Session_get_map(EditWin_get_session(edit_win));
  ViewDisplayFlags const display_flags = EditWin_get_display_flags(edit_win);

  edit_win->read_map_ctx = (struct MapEditContext){
    .base = !display_flags.MAP ? NULL : map->base,
    .overlay = !display_flags.MAP_OVERLAY ? NULL : map->overlay,
    .anims = map->anims};
}

static void update_read_info_ctx(EditWin *const edit_win)
{
  struct InfoEditContext const *const infos = Session_get_infos(EditWin_get_session(edit_win));
  ViewDisplayFlags const display_flags = EditWin_get_display_flags(edit_win);

  static struct InfoEditContext const dummy = {.data = NULL};
  edit_win->read_info_ctx = !display_flags.INFO ? &dummy : infos;
}

/* ---------------- Public functions ---------------- */

bool EditWin_init(EditWin *const edit_win, Editor *const editor,
  EditWin const *const edit_win_to_copy)
{
  ObjectId status_bar_id = NULL_ObjectId;
  DEBUG("Creating new edit_win (cloned from %p) on editor %p",
        (void *)edit_win_to_copy, (void *)editor);

  EditSession *const session = Editor_get_session(editor);

  /* Set default values or else copy from existing edit_win */
  *edit_win = (EditWin){
    .view.config = edit_win_to_copy ? edit_win_to_copy->view.config : *Config_get_default_view(),
    .editor = editor,
    .session = Editor_get_session(editor),
    .button_held = 0,
    .old_grid_pos = (MapPoint){0,0},
    .extent = (Vertex){0,0},
    .null_poller = false,
    .pointer_trapped = false,
    .pointer = Pointer_Standard,
    .auto_scrolling = false,
    .mouse_in = false,
    .wimp_drag_box = false,
    .obj_drag_box = false,
    .pending_hills_update = MapArea_make_invalid(),
  };
  MapAreaCol_init(&edit_win->pending_redraws, MAP_COORDS_LIMIT_LOG2);
  MapAreaCol_init(&edit_win->ghost_bboxes, MAP_COORDS_LIMIT_LOG2);

  edit_win->view.map_size_in_os_units = calc_map_size(edit_win->view.config.zoom_factor);
  edit_win->view.map_units_per_os_unit_log2 = map_units_per_os_unit_log2(edit_win->view.config.zoom_factor);

  set_sel_colour(edit_win);

  update_read_obj_ctx(edit_win);
  update_read_map_ctx(edit_win);
  update_read_info_ctx(edit_win);
  update_projection(edit_win);

  if (Session_has_data(session, DataType_BaseObjects) ||
      Session_has_data(session, DataType_OverlayObjects)) {
    if (report_error(hills_init(&edit_win->hills, read_hill, redraw_hill, edit_win), "", "")) {
      return false;
    }
    edit_win->has_hills = true;
    hills_make(&edit_win->hills);
  }

  /* Create new map edit_win window and associate with our data block */
  if (!E(toolbox_create_object(0, "EditWin", &edit_win->window_id))) {
    DEBUG("Main window for new edit_win is 0x%x", edit_win->window_id);

    /* Add this edit_win to the iconbar menu (real title will be set later) */
    if (!E(ViewsMenu_add(edit_win->window_id, "Bridget", Session_get_filename(session))))
    {
      if (!E(event_register_toolbox_handler(-1, -1, useracthandler, edit_win)))
      {
        if (register_msg(edit_win))
        {
          do
          {
            if (E(toolbox_set_client_handle(0, edit_win->window_id, edit_win))) {
              break;
            }

            if (E(window_get_wimp_handle(0, edit_win->window_id, &edit_win->wimp_id))) {
              break;
            }

            if (E(window_get_tool_bars(Window_InternalBottomLeftToolbar,
                                       edit_win->window_id, &status_bar_id,
                                       NULL, NULL, NULL))) {
              break;
            }

            if (!register_wimp_handlers(edit_win)) {
              break;
            }

            EditWin_set_help_and_ptr(edit_win, Editor_get_help_msg(editor), Editor_get_ptr_type(editor));

            /* Fill in generic field(s) on status bar */
            StatusBar_init(&edit_win->statusbar_data, status_bar_id);
            StatusBar_show_zoom(&edit_win->statusbar_data, edit_win->view.config.zoom_factor);
            StatusBar_show_angle(&edit_win->statusbar_data, edit_win->view.config.angle);
            StatusBar_show_mode(&edit_win->statusbar_data, Editor_get_mode_name(editor));
            StatusBar_show_hint(&edit_win->statusbar_data, msgs_lookup("StatusNewEditWin"));

            /* Ensure correct window extent for this zoom */
            show_or_hide_status_bar(edit_win);

            BBox visible_area;
            if (E(StackViews_open_get_bbox(edit_win->window_id, NULL_ObjectId, NULL_ComponentId,
                      &visible_area))) {
              break;
            }

            /* We only get open window request events in response to the user dragging or
               resizing the window so ensure that the status bar is reformatted. */
            int const width = visible_area.xmax - visible_area.xmin;
            StatusBar_reformat(&edit_win->statusbar_data, width, -1);

            return true;

          } while(0);
          deregister_msg(edit_win);
        }
        (void)event_deregister_toolbox_handler(-1, -1, useracthandler, edit_win);
      }
      (void)ViewsMenu_remove(edit_win->window_id);
    }
    (void)remove_event_handlers_delete(edit_win->window_id);
  }
  if (edit_win->has_hills) {
    hills_destroy(&edit_win->hills);
  }
  return false;
}

void EditWin_destroy(EditWin *const edit_win)
{
  assert(edit_win);
  DEBUG("EditWin object %p deleted", (void *)edit_win);

  drag_abort();

  if (edit_win->has_input_focus) {
    entity2_release(Wimp_MClaimEntity_CaretOrSelection);
  }

  entity2_cancel_requests(edit_win);
  loader3_cancel_receives(edit_win);
  saver2_cancel_sends(edit_win);

  stop_drag(edit_win);

  E(ViewsMenu_remove(edit_win->window_id));

  E(event_deregister_toolbox_handler(-1, -1, useracthandler, edit_win));

  if (edit_win->null_poller) {
    scheduler_deregister(track_pointer, edit_win);
  }

  free_pointer(edit_win);

  // Prevent the toolbar being deleted with the window
  E(window_set_tool_bars(Window_ExternalTopLeftToolbar,
     edit_win->window_id, NULL_ObjectId, NULL_ObjectId, NULL_ObjectId, NULL_ObjectId));

  E(remove_event_handlers_delete(edit_win->window_id));

  deregister_msg(edit_win);

  MainMenu_hide();

  if (edit_win->has_hills) {
    hills_destroy(&edit_win->hills);
  }
}

void EditWin_show(EditWin const *const edit_win)
{
  assert(edit_win);
  E(DeIconise_show_object(0,
                          edit_win->window_id,
                          Toolbox_ShowObject_Default,
                          NULL,
                          NULL_ObjectId,
                          NULL_ComponentId));
}

EditSession *EditWin_get_session(EditWin const *const edit_win)
{
  assert(edit_win != NULL);
  return edit_win->session;
}

void EditWin_update_can_paste(EditWin *const edit_win)
{
  assert(edit_win);

  WimpDataRequestMessage data_request;
  init_data_request(edit_win, &data_request);

  entity2_cancel_requests(edit_win);
  if (E(entity2_probe_data(&data_request, paste_probe_cb, probe_failed_cb, edit_win)))
  {
    Editor_set_paste_enabled(edit_win->editor, false);
  }
}

void EditWin_stop_drag_select(EditWin *const edit_win)
{
  stop_drag(edit_win);
}

int EditWin_get_wimp_handle(EditWin const *edit_win)
{
  assert(edit_win);
  return edit_win->wimp_id;
}


bool EditWin_start_drag_obj(EditWin *const edit_win, MapArea const *sent_bbox, MapArea const *shown_bbox)
{
  assert(edit_win);
  assert(!edit_win->dragging_obj);
  assert(sent_bbox);
  assert(shown_bbox);
  Editor *const editor = edit_win->editor;
  DataType const *const export_data_types = Editor_get_export_data_types(editor);
  int export_file_types[10];
  data_types_to_file_types(export_data_types, export_file_types,
                           ARRAY_SIZE(export_file_types)-1);

  MapArea_derotate(edit_win->view.config.angle, sent_bbox, &edit_win->sent_drag_bbox);
  edit_win->shown_drag_bbox = *shown_bbox;
  drag_origin_edit_win = edit_win;

  BBox const data_bbox = {.xmin = edit_win->sent_drag_bbox.min.x,
                          .ymin = edit_win->sent_drag_bbox.min.y,
                          .xmax = edit_win->sent_drag_bbox.max.x + 1,
                          .ymax = edit_win->sent_drag_bbox.max.y + 1};

  if (E(drag_start(export_file_types, &data_bbox, drag_box_method, drop_method, edit_win))) {
    return false;
  }

  edit_win->dragging_obj = true;
  return true;
}

void EditWin_stop_drag_obj(EditWin *const edit_win)
{
  if (edit_win->dragging_obj) {
    edit_win->dragging_obj = false;
    drag_abort();
  }
}

bool EditWin_start_drag_select(EditWin *const edit_win, int const drag_type,
                               MapArea const *const initial_box, bool const local)
{
  assert(edit_win);

  WimpGetWindowStateBlock getwincoords = {
    .window_handle = edit_win->wimp_id,
  };

  if (E(wimp_get_window_state(&getwincoords))) {
    return false;
  }

  WimpDragBox dragbox = {
    .wimp_window = edit_win->wimp_id,
    .drag_type = drag_type,
  };

  Vertex const eig = Desktop_get_eigen_factors();

  if (drag_type != Wimp_DragBox_DragPoint)
  {
    /* Set up initial position of drag box */
    Vertex const window_origin = calc_window_origin(edit_win, &getwincoords);
    assert(initial_box != NULL);

    Vertex const min = map_to_scr_coords(edit_win, window_origin, initial_box->min);
    Vertex const max = map_to_scr_coords(edit_win, window_origin, initial_box->max);

    dragbox.dragging_box = (BBox){
      .xmin = min.x,
      .ymin = min.y,
      /* Drag bounding boxes have exclusive maximum coordinates */
      .xmax = max.x + (1 << eig.x),
      .ymax = max.y + (1 << eig.y),
    };
  } else {
    assert(local);
  }

  if (local)
  {
    /* Restrict pointer to window */
    dragbox.parent_box = (BBox){
      .xmin = getwincoords.visible_area.xmin,
      .ymin = getwincoords.visible_area.ymin,
      .xmax = getwincoords.visible_area.xmax - (1 << eig.x),
      .ymax = getwincoords.visible_area.ymax - (1 << eig.y),
    };

    if (edit_win->view.config.show_status_bar)
    {
      dragbox.parent_box.ymin += StatusBar_get_height() + (1 << eig.y);
    }

  }
  else
  {
    /* Allow drag anywhere on the screen */
    WimpGetPointerInfoBlock ptr_info;
    if (E(wimp_get_pointer_info(&ptr_info))) {
      return false;
    }

    assert(drag_type != Wimp_DragBox_DragPoint);
    Vertex const desktop_size = Desktop_get_size_os();

    dragbox.parent_box = (BBox){
      .xmin = - (ptr_info.x - dragbox.dragging_box.xmin),
      .ymin = - (ptr_info.y - dragbox.dragging_box.ymin),
      .xmax = desktop_size.x + (dragbox.dragging_box.xmax - ptr_info.x),
      .ymax = desktop_size.y + (dragbox.dragging_box.ymax - ptr_info.y),
    };
  }

  if (E(wimp_drag_box2(&dragbox,
                   Wimp_DragBox_FixToWorkArea | Wimp_DragBox_ClipToWindow))) {
    return false;
  }

  if (E(event_register_wimp_handler(-1, Wimp_EUserDrag, select_drag_complete, edit_win))) {
    E(wimp_drag_box(CancelDrag));
    return false;

  }

  edit_win->wimp_drag_box = true;
  return true;
}

void EditWin_redraw_area(EditWin *const edit_win, MapArea const *area, bool immediate)
{
  DEBUGF("Redrawing %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord " (%s)\n",
         area->min.x, area->min.y, area->max.x, area->max.y, immediate ? "immediate" : "deferred" );
#if DEBUG_REDRAW_AREA
  area = &{{0, 0}, {MAP_COORDS_LIMIT, MAP_COORDS_LIMIT}};
#endif
  MapArea_split(area, MAP_COORDS_LIMIT_LOG2,
                immediate ? split_redraw_area_imm_cb : split_redraw_area_cb, edit_win);
}

PaletteEntry EditWin_get_bg_colour(EditWin const *const edit_win)
{
  assert(edit_win != NULL);
  return edit_win->view.config.back_colour;
}

void EditWin_set_bg_colour(EditWin *const edit_win, PaletteEntry const colour)
{
  assert(edit_win != NULL);
  if (edit_win->view.config.back_colour != colour) {
    edit_win->view.config.back_colour = colour;
    redraw_all(edit_win);
  }
}

PaletteEntry EditWin_get_grid_colour(EditWin const *const edit_win)
{
  assert(edit_win != NULL);
  return edit_win->view.config.grid_colour;
}

void EditWin_set_grid_colour(EditWin *const edit_win, PaletteEntry const colour)
{
  assert(edit_win != NULL);
  if (edit_win->view.config.grid_colour != colour) {
    edit_win->view.config.grid_colour = colour;
    if (edit_win->view.config.flags.GRID &&
        Editor_can_draw_grid(edit_win->editor, edit_win)) {
      redraw_all(edit_win);
    }
  }
}

PaletteEntry EditWin_get_sel_colour(EditWin const *const edit_win)
{
  assert(edit_win != NULL);
  return edit_win->view.config.sel_colour;
}

void EditWin_set_sel_colour(EditWin *const edit_win, PaletteEntry const colour)
{
  assert(edit_win != NULL);
  if (edit_win->view.config.sel_colour != colour) {
    edit_win->view.config.sel_colour = colour;
    set_sel_colour(edit_win);
    redraw_all(edit_win);
  }
}

PaletteEntry const (*EditWin_get_sel_palette(EditWin const *const edit_win))[NumColours]
{
  return &edit_win->view.sel_palette;
}

unsigned char const (*EditWin_get_sel_colours(EditWin const *const edit_win))[NumColours]
{
  return &edit_win->view.sel_colours;
}

bool EditWin_get_sel_tex_is_bright(EditWin const *const edit_win, MapRef const tile_num)
{
  assert(edit_win != NULL);
  size_t const index = map_ref_to_num(tile_num);
  assert(index < MapTexBitmaps_get_count(&Session_get_textures(EditWin_get_session(edit_win))->tiles));

  return TEST_BITS(edit_win->sel_tex_bw_table[index / CHAR_BIT], 1u << (index % CHAR_BIT));
}

PaletteEntry EditWin_get_ghost_colour(EditWin const *const edit_win)
{
  assert(edit_win != NULL);
  return edit_win->view.config.ghost_colour;
}

void EditWin_set_ghost_colour(EditWin *const edit_win, PaletteEntry const colour)
{
  assert(edit_win != NULL);
  if (edit_win->view.config.ghost_colour != colour) {
    edit_win->view.config.ghost_colour = colour;
    redraw_all(edit_win);
  }
}

HillsData const *EditWin_get_hills(EditWin const *const edit_win)
{
  assert(edit_win != NULL);
  return edit_win->has_hills ? &edit_win->hills : NULL;
}

struct ObjEditContext const *EditWin_get_read_obj_ctx(EditWin const *const edit_win)
{
  assert(edit_win);
  return &edit_win->read_obj_ctx;
}

struct MapEditContext const *EditWin_get_read_map_ctx(EditWin const *const edit_win)
{
  assert(edit_win);
  return &edit_win->read_map_ctx;
}

struct InfoEditContext const *EditWin_get_read_info_ctx(EditWin const *const edit_win)
{
  assert(edit_win);
  return edit_win->read_info_ctx;
}

ObjGfxMeshesView const *EditWin_get_plot_ctx(EditWin const *const edit_win)
{
  assert(edit_win);
  return &edit_win->view.plot_ctx;
}

View const *EditWin_get_view(EditWin const *edit_win)
{
  assert(edit_win);
  return &edit_win->view;
}

void EditWin_redraw_map(EditWin *const edit_win, MapArea const *const area)
{
  DEBUGF("%s\n", __func__);
  assert(edit_win != NULL);
  assert(MapArea_is_valid(area));
#if 0 // Can't optimise because we can select mask tiles
  EditSession *const session = EditWin_get_session(edit_win);
  if (Session_has_data(session, DataType_OverlayMap)) {
    if (!edit_win->view.config.flags.MAP_OVERLAY && !edit_win->view.config.flags.MAP) {
      return;
    }
  } else if (!edit_win->view.config.flags.MAP) {
    return;
  }
#endif
  DEBUGF("Redraw map at {%" PRIMapCoord ", %" PRIMapCoord ",%" PRIMapCoord ", %" PRIMapCoord "}\n",
          area->min.x, area->min.y, area->max.x, area->max.y);

  MapArea const map_area = MapLayout_map_area_to_fine(&edit_win->view, area);
  MapAreaCol_add(&edit_win->pending_redraws, &map_area);
}

void EditWin_redraw_object(EditWin *const edit_win, MapPoint const pos,
  ObjRef const base_ref, ObjRef const old_ref, ObjRef const new_ref, bool const has_triggers)
{
  EditSession *const session = EditWin_get_session(edit_win);
  if (Session_has_data(session, DataType_OverlayObjects)) {
    if (!edit_win->view.config.flags.OBJECTS_OVERLAY && !edit_win->view.config.flags.OBJECTS) {
      return;
    }
  } else if (!edit_win->view.config.flags.OBJECTS) {
    return;
  }

  DEBUGF("Redraw object %zu to %zu (base %zu) at %" PRIMapCoord ", %" PRIMapCoord "\n",
          objects_ref_to_num(old_ref), objects_ref_to_num(new_ref), objects_ref_to_num(base_ref),
          pos.x, pos.y);

  ObjGfx *const graphics = Session_get_graphics(EditWin_get_session(edit_win));
  ObjGfxMeshes *const meshes = &graphics->meshes;

  MapPoint const centre = ObjLayout_map_coords_to_centre(&edit_win->view, pos);

  ObjRef const old_disp_ref = (objects_ref_is_mask(old_ref) ? base_ref : old_ref);

  /* If we read from the base grid but wrote to the overlay grid then the
     previously-visible object ref MAY not be the same as old_ref. It's wrong
     to rely on which layers are visible because that varies per-edit_win.*/
  if (!objects_ref_is_equal(old_disp_ref, new_ref) && !objects_ref_is_none(old_disp_ref))
  {
    MapArea old_area = has_triggers ?
                       DrawObjs_get_bbox_with_triggers(meshes, &edit_win->view, old_disp_ref) :
                       DrawObjs_get_auto_bbox(meshes, &edit_win->view, old_disp_ref);
    redraw_object(centre, &old_area, edit_win);
  }

  ObjRef new_disp_ref = new_ref;
  if (!objects_ref_is_none(new_ref))
  {
    new_disp_ref = (objects_ref_is_mask(new_ref) ? base_ref : new_ref);
    MapArea new_area = has_triggers ?
                       DrawObjs_get_bbox_with_triggers(meshes, &edit_win->view, new_disp_ref) :
                       DrawObjs_get_auto_bbox(meshes, &edit_win->view, new_disp_ref);
    redraw_object(centre, &new_area, edit_win);
  }

  if (hills_need_update(pos) && !objects_ref_is_equal(old_disp_ref, new_disp_ref) &&
      (objects_ref_is_hill(old_disp_ref) || objects_ref_is_hill(new_disp_ref)))
  {
    MapArea_expand(&edit_win->pending_hills_update, MapPoint_div_log2(pos, Hill_ObjPerHillLog2));
  }
}

void EditWin_redraw_info(EditWin *const edit_win, MapPoint const pos)
{
  if (!edit_win->view.config.flags.INFO) {
    return;
  }

  DEBUGF("Redraw info at %" PRIMapCoord ", %" PRIMapCoord "\n", pos.x, pos.y);
  MapArea info_bbox = DrawInfos_get_bbox(&edit_win->view);
  MapPoint const info_centre = MapLayout_map_coords_to_centre(&edit_win->view, pos);
  redraw_object(info_centre, &info_bbox, edit_win);
}

void EditWin_occluded_obj_changed(EditWin *const edit_win, MapPoint const pos, ObjRef const obj_ref)
{
  DEBUGF("Redraw occluded obj %zu at %" PRIMapCoord ",%" PRIMapCoord "\n",
         objects_ref_to_num(obj_ref), pos.x, pos.y);
  MapArea const obj_bbox = EditWin_get_ghost_obj_bbox(edit_win, pos, obj_ref);
  MapAreaCol_add(&edit_win->pending_redraws, &obj_bbox);
}

void EditWin_occluded_info_changed(EditWin *const edit_win, MapPoint const pos)
{
  DEBUGF("Redraw occluded info at %" PRIMapCoord ",%" PRIMapCoord "\n", pos.x, pos.y);
  MapArea const info_bbox = EditWin_get_ghost_info_bbox(edit_win, pos);
  MapAreaCol_add(&edit_win->pending_redraws, &info_bbox);
}

void EditWin_trig_changed(EditWin *const edit_win, MapPoint const pos,
  ObjRef const obj_ref, TriggerFullParam const fparam)
{
  assert(edit_win != NULL);
  if (!edit_win->view.config.flags.OBJECTS && !edit_win->view.config.flags.OBJECTS_OVERLAY) {
    return;
  }

  EditSession *const session = EditWin_get_session(edit_win);
  if (!Session_has_data(session, DataType_Mission)) {
    return;
  }

  DEBUGF("Redraw trigger for object %zu at %" PRIMapCoord ", %" PRIMapCoord "\n",
          objects_ref_to_num(obj_ref), pos.x, pos.y);

  MapPoint const centre = ObjLayout_map_coords_to_centre(&edit_win->view, pos);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;
  MapArea new_area = DrawObjs_get_trigger_bbox(meshes, &edit_win->view, obj_ref, pos, fparam);
  redraw_object(centre, &new_area, edit_win);
}

void EditWin_redraw_ghost(EditWin *const edit_win)
{
  assert(edit_win != NULL);
  MapAreaColIter iter;
  for (MapArea const *ghost_bbox = MapAreaColIter_get_first(&iter, &edit_win->ghost_bboxes);
       ghost_bbox != NULL;
       ghost_bbox = MapAreaColIter_get_next(&iter)) {
    DEBUGF("Redrawing ghost bbox %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord "\n",
           ghost_bbox->min.x, ghost_bbox->min.y, ghost_bbox->max.x, ghost_bbox->max.y);
    MapAreaCol_add(&edit_win->pending_redraws, ghost_bbox);
  }
}

void EditWin_clear_ghost_bbox(EditWin *const edit_win)
{
  assert(edit_win != NULL);
  MapAreaCol_init(&edit_win->ghost_bboxes, MAP_COORDS_LIMIT_LOG2);
  DEBUGF("Cleared ghost bbox\n");
}

void EditWin_set_ghost_map_bbox(EditWin *const edit_win, MapArea const *const area)
{
  assert(edit_win != NULL);
  assert(MapArea_is_valid(area));
  DEBUGF("Set ghost bbox %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord "\n",
         area->min.x, area->min.y, area->max.x, area->max.y);
  // FIXME: is it really worth handling this differently from other modes?
  MapArea const map_bbox = MapLayout_map_area_to_fine(&edit_win->view, area);
  MapAreaCol_init(&edit_win->ghost_bboxes, MAP_COORDS_LIMIT_LOG2);
  MapAreaCol_add(&edit_win->ghost_bboxes, &map_bbox);
  MapAreaCol_add(&edit_win->pending_redraws, &map_bbox);
}

MapArea EditWin_get_ghost_obj_bbox(EditWin *const edit_win, MapPoint const pos, ObjRef const obj_ref)
{
  ObjGfx *const graphics = Session_get_graphics(EditWin_get_session(edit_win));
  ObjGfxMeshes *const meshes = &graphics->meshes;
  MapArea obj_bbox = DrawObjs_get_bbox(meshes, &edit_win->view, obj_ref);
  MapPoint const obj_centre = ObjLayout_map_coords_to_centre(&edit_win->view, pos);
  MapArea_translate(&obj_bbox, obj_centre, &obj_bbox);
  return obj_bbox;
}

void EditWin_add_ghost_obj(EditWin *const edit_win, MapPoint const pos, ObjRef const obj_ref)
{
  DEBUGF("Extend ghost bbox for obj %zu at %" PRIMapCoord ",%" PRIMapCoord "\n",
         objects_ref_to_num(obj_ref), pos.x, pos.y);
  MapArea const obj_bbox = EditWin_get_ghost_obj_bbox(edit_win, pos, obj_ref);
  MapAreaCol_add(&edit_win->ghost_bboxes, &obj_bbox);
}

void EditWin_add_ghost_unknown_obj(EditWin *const edit_win, MapArea const *const bbox)
{
  assert(MapArea_is_valid(bbox));
  DEBUGF("Extending ghost bbox by %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord "\n",
         bbox->min.x, bbox->min.y, bbox->max.x, bbox->max.y);
  MapArea const unknown_bbox = ObjLayout_map_area_to_fine(&edit_win->view, bbox);
  MapAreaCol_add(&edit_win->ghost_bboxes, &unknown_bbox);
}

MapArea EditWin_get_ghost_info_bbox(EditWin *const edit_win, MapPoint const pos)
{
  MapArea info_bbox = DrawInfos_get_bbox(&edit_win->view);
  MapPoint const info_centre = MapLayout_map_coords_to_centre(&edit_win->view, pos);
  MapArea_translate(&info_bbox, info_centre, &info_bbox);
  return info_bbox;
}

void EditWin_add_ghost_info(EditWin *const edit_win, MapPoint const pos)
{
  DEBUGF("Extend ghost bbox for info at %" PRIMapCoord ",%" PRIMapCoord "\n",
         pos.x, pos.y);
  MapArea const info_bbox = EditWin_get_ghost_info_bbox(edit_win, pos);
  MapAreaCol_add(&edit_win->ghost_bboxes, &info_bbox);
}

void EditWin_add_ghost_unknown_info(EditWin *const edit_win, MapArea const *const bbox)
{
  assert(MapArea_is_valid(bbox));
  DEBUGF("Extending ghost bbox by %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord "\n",
         bbox->min.x, bbox->min.y, bbox->max.x, bbox->max.y);
  MapArea const unknown_bbox = MapLayout_map_area_to_fine(&edit_win->view, bbox);
  MapAreaCol_add(&edit_win->ghost_bboxes, &unknown_bbox);
}

void EditWin_redraw_pending(EditWin *const edit_win, bool const immediate)
{
  assert(edit_win);

  if (MapArea_is_valid(&edit_win->pending_hills_update)) {
    hills_update(&edit_win->hills, &edit_win->pending_hills_update);
    edit_win->pending_hills_update = MapArea_make_invalid();
    DEBUGF("Cleared hills rect\n");
  }

  DEBUGF("Doing pending redraws\n");
  MapAreaColIter iter;
  for (MapArea const *area = MapAreaColIter_get_first(&iter, &edit_win->pending_redraws);
       area != NULL;
       area = MapAreaColIter_get_next(&iter)) {
    EditWin_redraw_area(edit_win, area, immediate);
  }
  MapAreaCol_init(&edit_win->pending_redraws, MAP_COORDS_LIMIT_LOG2);
  DEBUGF("Cleared redraw rect\n");
}

void EditWin_display_mode(EditWin *const edit_win)
{
  Editor *const editor = edit_win->editor;

  StatusBar_show_mode(&edit_win->statusbar_data,
    Editor_get_mode_name(editor));

  StatusBar_reformat(&edit_win->statusbar_data, -1,
    Editor_get_coord_field_width(editor));

  StatusBar_show_pos(&edit_win->statusbar_data, !edit_win->mouse_in,
                            edit_win->old_grid_pos);

  redraw_all(edit_win);
}

void EditWin_set_help_and_ptr(EditWin *const edit_win, char *const help,
  PointerType const ptr)
{
  if (help == NULL || *help != '\0') {
    if (help != NULL) {
      DEBUG("Setting help message '%s'", help);
    } else {
      DEBUG("Removing help message");
    }

    E(window_set_help_message2(0, edit_win->window_id, help));
  }

  if (ptr != edit_win->pointer) {
    edit_win->pointer = ptr;

    static const struct {
      char *sprite_name;
      Vertex hot_spot;
    } pointers[] = {
      [Pointer_Standard] = { NULL, {8, 8} },
      [Pointer_Brush] = { "ptrbrush", {0, 17} },
      [Pointer_Fill] = { "ptrfill", {2, 16} },
      [Pointer_Snake] = { "ptrsnake", {0, 0} },
      [Pointer_Wand] = { "ptrwand", {5, 5} },
      [Pointer_Paste] = { "ptrpaste", {11, 11} },
      [Pointer_Sample] = { "ptrsample", {0, 0} },
      [Pointer_Zoom] = { "ptrzoom", {10, 10} },
      [Pointer_Crosshair] = { "ptrcrosshair", {8, 8} },
    };
    assert(ptr >= 0);
    assert(ptr < ARRAY_SIZE(pointers));

    if (pointers[ptr].sprite_name != NULL) {
      DEBUG("Setting pointer shape '%s' (hot spot %d, %d)",
            pointers[ptr].sprite_name,
            pointers[ptr].hot_spot.x, pointers[ptr].hot_spot.y);
    } else {
      DEBUG("Removing special pointer shape");
    }

    E(window_set_pointer(0, edit_win->window_id, pointers[ptr].sprite_name,
      pointers[ptr].hot_spot.x, pointers[ptr].hot_spot.y));
  }
}

void EditWin_display_hint(EditWin *const edit_win, char const *hint)
{
  StatusBar_show_hint(&edit_win->statusbar_data, hint);
}

void EditWin_close(EditWin *const edit_win)
{
  /* Attempt to close the specified edit_win (prompting for discard if necessary) */
  close(edit_win, false);
}

int EditWin_get_zoom(EditWin const *const edit_win)
{
  assert(edit_win != NULL);
  assert(edit_win->view.config.zoom_factor >= EditWinZoomMin);
  assert(edit_win->view.config.zoom_factor <= EditWinZoomMax);
  return edit_win->view.config.zoom_factor;
}

void EditWin_set_zoom(EditWin *const edit_win, int const zoom_factor)
{
  if (edit_win->wimp_drag_box || edit_win->dragging_obj || edit_win->pointer_trapped) {
    return;
  }
  change_zoom(edit_win, zoom_factor);
}

MapAngle EditWin_get_angle(EditWin const *const edit_win)
{
  assert(edit_win != NULL);
  return edit_win->view.config.angle;
}

void EditWin_set_angle(EditWin *const edit_win, MapAngle const angle)
{
  if (edit_win->wimp_drag_box || edit_win->dragging_obj || edit_win->pointer_trapped) {
    return;
  }
  change_angle(edit_win, angle);
}

ViewDisplayFlags EditWin_get_display_flags(EditWin const *const edit_win)
{
  assert(edit_win);
  return edit_win->view.config.flags;
}

void EditWin_set_display_flags(EditWin *const edit_win, ViewDisplayFlags const flags)
{
  assert(edit_win);

  ViewDisplayFlags const old_flags = edit_win->view.config.flags;
  if (!ViewDisplayFlags_equal(flags, old_flags)) {
    edit_win->view.config.flags = flags;

    if (old_flags.MAP != flags.MAP ||
        old_flags.MAP_OVERLAY != flags.MAP_OVERLAY) {
      update_read_map_ctx(edit_win);
    }

    if (old_flags.OBJECTS != flags.OBJECTS ||
        old_flags.OBJECTS_OVERLAY != flags.OBJECTS_OVERLAY) {
      update_read_obj_ctx(edit_win);
      hills_make(&edit_win->hills);
    }

    if (old_flags.INFO != flags.INFO) {
      update_read_info_ctx(edit_win);
    }

    LayersMenu_update(edit_win);
    UtilsMenu_update(edit_win);
    redraw_all(edit_win);
  }
}

bool EditWin_get_status_shown(EditWin const *const edit_win)
{
  assert(edit_win);
  return edit_win->view.config.show_status_bar;
}

void EditWin_set_title(EditWin *const edit_win, char const *new_title)
{
  char const *const file_path = Session_get_filename(EditWin_get_session(edit_win));
  DEBUG("Setting title of edit_win %d to '%s' (path '%s')", edit_win->window_id, new_title,
    file_path);

  /* cast is only required because veneer is crap */
  E(window_set_title(0, edit_win->window_id, (char *)new_title));

  E(ViewsMenu_setname(edit_win->window_id, new_title, file_path));

#if PER_VIEW_SELECT
  Editor_update_title(edit_win->editor);
#endif
}

void EditWin_show_dbox(EditWin const *const edit_win, unsigned int const flags, ObjectId const dbox_id)
{
  assert(edit_win != NULL);
  assert(dbox_id != NULL_ObjectId);

  DEBUG("Showing object 0x%x relative to edit_win %p", dbox_id, (void *)edit_win);

  open_topleftofwin(flags, dbox_id, edit_win->window_id,
                    edit_win->window_id, NULL_ComponentId);
}

void EditWin_show_dbox_at_ptr(EditWin const *const edit_win,
  ObjectId const dbox_id)
{
  assert(edit_win != NULL);
  assert(dbox_id != NULL_ObjectId);
  DEBUG("Showing object 0x%x at pointer for edit_win %p", dbox_id, (void *)edit_win);

  E(toolbox_show_object(Toolbox_ShowObject_AsMenu, dbox_id,
                        Toolbox_ShowObject_AtPointer, NULL,
                        edit_win->window_id,
                        NULL_ComponentId));
}

void EditWin_show_window_aligned_right(EditWin const *const edit_win,
  ObjectId const win_id, int const width)
{
  assert(edit_win != NULL);
  assert(win_id != NULL_ObjectId);
  DEBUG("Showing object 0x%x aligned to the right of edit_win %p", win_id, (void *)edit_win);

  /* Enabling about-to-be-shown events for the edit_win object breaks this because its
     Wimp window state is not up-to-date during creation of a new edit_win. Don't! */
  WimpGetWindowStateBlock wgwsb = {edit_win->wimp_id};
  ON_ERR_RPT_RTN(wimp_get_window_state(&wgwsb));

  int sbar_width = 0, sbar_height = 0;
  get_scrollbar_sizes(&sbar_width, &sbar_height);

  Vertex const eigen_factors = Desktop_get_eigen_factors();

  WindowShowObjectBlock showblock = {
    .visible_area = {
      .xmin = wgwsb.visible_area.xmax + sbar_width,
      .ymin = wgwsb.visible_area.ymin - (sbar_height - (1 << eigen_factors.y)),
      .xmax = wgwsb.visible_area.xmax + sbar_width + width,
      .ymax = wgwsb.visible_area.ymax,
    },
    .xscroll = 0,
    .yscroll = 0,
    .behind = -1 };

  DEBUG("Showing window at coordinates %d,%d,%d,%d",
        showblock.visible_area.xmin, showblock.visible_area.ymin,
        showblock.visible_area.xmax, showblock.visible_area.ymax);

  E(DeIconise_show_object(0, win_id,
                          Toolbox_ShowObject_FullSpec, &showblock,
                          edit_win->window_id, NULL_ComponentId));
}

void EditWin_show_toolbar(EditWin const *const edit_win, ObjectId const tools_id)
{
  assert(edit_win != NULL);
  assert(tools_id != NULL_ObjectId);
  DEBUG("Showing object 0x%x as toolbar of edit_win %p", tools_id, (void *)edit_win);

  ON_ERR_RPT_RTN(window_set_tool_bars(Window_ExternalTopLeftToolbar,
     edit_win->window_id, NULL_ObjectId, NULL_ObjectId, NULL_ObjectId, tools_id));

  E(toolbox_show_object(0, tools_id,
    Toolbox_ShowObject_Default, NULL, edit_win->window_id, NULL_ComponentId));
}

Editor *EditWin_get_editor(EditWin const *const edit_win)
{
  assert(edit_win != NULL);
  return edit_win->editor;
}

void EditWin_resource_change(EditWin *const edit_win, EditorChange const event,
  EditorChangeParams const *params)
{
  NOT_USED(params);
  switch (event)
  {
  case EDITOR_CHANGE_MISSION_REPLACED:
    update_read_info_ctx(edit_win);
    break;
  case EDITOR_CHANGE_OBJ_ALL_REPLACED:
    update_read_obj_ctx(edit_win);
    hills_make(&edit_win->hills);
    break;
  case EDITOR_CHANGE_MAP_ALL_REPLACED:
    update_read_map_ctx(edit_win);
    break;
  case EDITOR_CHANGE_TEX_ALL_RELOADED:
    gen_sel_tex_bw_table(edit_win);
    break;
  default:
    break;
  }
}

void EditWin_set_scroll_pos(EditWin const *const edit_win, MapPoint const pos)
{
  assert(edit_win);
  WimpGetWindowStateBlock window_state = {
    .window_handle = edit_win->wimp_id,
  };
  if (!E(wimp_get_window_state(&window_state))) {
    scroll_to(edit_win, pos, &window_state);

    E(toolbox_show_object(0, edit_win->window_id,
                          Toolbox_ShowObject_FullSpec, &window_state.visible_area,
                          NULL_ObjectId, NULL_ComponentId));
  }
}

MapPoint EditWin_get_scroll_pos(EditWin const *const edit_win)
{
  assert(edit_win);
  WimpGetWindowStateBlock window_state = {
    .window_handle = edit_win->wimp_id,
  };
  if (!E(wimp_get_window_state(&window_state))) {
    return get_scroll_pos(edit_win, &window_state);
  }
  return (MapPoint){0,0};
}
