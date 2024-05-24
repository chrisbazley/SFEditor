/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Strategic target information editing mode
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

#include "stdlib.h"
#include <assert.h>

#include "toolbox.h"

#include "Macros.h"
#include "err.h"
#include "msgtrans.h"
#include "debug.h"

#include "DFileUtils.h"
#include "MapTexBitm.h"
#include "Plot.h"
#include "MapCoord.h"
#include "MapLayout.h"
#include "EditorData.h"
#include "EditMode.h"
#include "Session.h"
#include "Mission.h"
#include "Infos.h"
#include "Utils.h"
#include "DataType.h"
#include "EditWin.h"
#include "SelBitmask.h"
#include "IPropDbox.h"
#include "ITransfers.h"
#include "IPalette.h"
#include "InfoEdit.h"
#include "InfoEditCtx.h"
#include "InfoEditChg.h"
#include "InfoMode.h"
#include "DrawInfos.h"
#include "infos.h"

typedef struct
{
  SelectionBitmask selection, occluded, tmp;
  InfoPropDboxes prop_dboxes;
  MapArea ghost_bbox, drop_bbox;
  MapPoint drag_start_pos, pending_vert;
  bool uk_drop_pending:1;
  InfoTransfer *pending_transfer, *pending_paste, *pending_drop, *dragged;
  PendingShape pending_shape;
  InfoEditChanges change_info;
}
InfoModeData;

enum {
  GRID_GAP_SIZE = MapTexSize << TexelToOSCoordLog2,
};

static inline InfoModeData *get_mode_data(Editor const *const editor)
{
  assert(Editor_get_edit_mode(editor) == EDITING_MODE_INFO);
  assert(editor->editingmode_data);
  return editor->editingmode_data;
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

static void InfoMode_leave(Editor *const editor)
{
  InfoModeData *const mode_data = get_mode_data(editor);
  InfoPropDboxes_destroy(&mode_data->prop_dboxes);
  free(editor->editingmode_data);
}

typedef struct {
  InfoTransfer *transfer;
  MapPoint bl;
  View const *view;
  MapArea const *const redraw_area;
  size_t index, count;
} TransferReadArgs;

static size_t read_info_transfer(void *const cb_arg, MapPoint *const map_pos, int *const id)
{
  assert(map_pos);
  TransferReadArgs *const args = cb_arg;
  assert(args);

  size_t index;
  do {
    if (args->index >= args->count) {
      return SIZE_MAX;
    }
    index = args->index++;
    *map_pos = MapPoint_add(args->bl, InfoTransfers_get_pos(args->transfer, index));
    *id = 0;
  } while (!DrawInfos_touch_ghost_bbox(args->view, *map_pos, args->redraw_area));
  return index;
}

typedef struct {
  bool done;
  MapPoint pos;
} ReadGhostInfoData;

static size_t read_ghost(void *const cb_arg, MapPoint *const map_pos, int *const id)
{
  assert(map_pos);
  ReadGhostInfoData *const args = cb_arg;
  assert(args);

  if (args->done) {
    return SIZE_MAX;
  }
  args->done = true;

  *map_pos = args->pos;
  *id = 0;
  return 0; // meaningless as ghost cannot be selected
}

static void draw_unknown_drop(EditWin const *const edit_win, MapArea const *const drop_bbox,
  Vertex const scr_orig, MapArea const *const redraw_area, MapArea const *const overlapping_area)
{
  MapArea intersect;
  MapArea_intersection(drop_bbox, overlapping_area, &intersect);
  if (MapArea_is_valid(&intersect)) {
    View const *const view = EditWin_get_view(edit_win);

    if (MapPoint_compare(drop_bbox->min, drop_bbox->max)) {
      if (DrawInfos_touch_ghost_bbox(view, drop_bbox->min, redraw_area)) {
        ReadGhostInfoData data = { .done = false, .pos = drop_bbox->min };
        DrawInfos_to_screen(view, read_ghost, &data, NULL, scr_orig, true, NULL);
      }
    } else {
      MapArea const scr_area = MapLayout_rotate_map_area_to_scr(view->config.angle, &intersect);
      DrawInfos_unknown_to_screen(view, &scr_area, scr_orig);
    }
  }
}

static void draw_ghost_paste(InfoTransfer *const transfer,
  MapPoint const bl, EditWin const *const edit_win, Vertex const scr_orig,
  MapArea const *const redraw_area)
{
  DEBUGF("Drawing ghost of transfer %p at %" PRIMapCoord ",%" PRIMapCoord "\n",
         (void *)transfer, bl.x, bl.y);
  View const *const view = EditWin_get_view(edit_win);

  TransferReadArgs transfer_args = {
    .view = view,
    .bl = bl,
    .transfer = transfer,
    .redraw_area = redraw_area,
    .index = 0,
    .count = InfoTransfers_get_info_count(transfer),
  };

  DrawInfos_to_screen(view, read_info_transfer, &transfer_args, NULL, scr_orig, true, NULL);
}

static void draw_pending(InfoModeData const *const mode_data,
  EditWin const *const edit_win, Vertex const scr_orig,
  MapArea const *const redraw_area, MapArea const *overlapping_area)
{
  DEBUGF("Drawing pending shape type %d\n", mode_data->pending_shape);
  if (mode_data->pending_shape == Pending_Transfer) {
    draw_ghost_paste(mode_data->pending_transfer, mode_data->pending_vert, edit_win,
                     scr_orig, redraw_area);
  } else {
    switch (mode_data->pending_shape) {
      case Pending_Point:
        if (map_bbox_contains(overlapping_area, mode_data->pending_vert) &&
            DrawInfos_touch_ghost_bbox(EditWin_get_view(edit_win), mode_data->pending_vert, redraw_area)) {
          ReadGhostInfoData data = { .done = false, .pos = mode_data->pending_vert };
          DrawInfos_to_screen(EditWin_get_view(edit_win), read_ghost, &data,
                              NULL, scr_orig, true, NULL);
        }
        break;

      default:
        return; /* unknown plot type */
    }
  }
}

typedef struct {
  View const *view;
  InfoEditContext const *infos;
  size_t index;
  InfoEditIter iter;
  MapArea const *const redraw_area;
} DrawInfoData;

static size_t read_info_from_map(void *const cb_arg, MapPoint *const map_pos, int *const id)
{
  assert(map_pos);
  DrawInfoData *const data = cb_arg;
  assert(data);

  size_t index;
  do {
    if (InfoEditIter_done(&data->iter)) {
      return SIZE_MAX;
    }
    index = data->index;
    data->index = InfoEditIter_get_next(&data->iter);
    TargetInfo const *const info = InfoEdit_get(data->infos, index);
    *map_pos = target_info_get_pos(info);
    *id = target_info_get_id(info);
  } while (!DrawInfos_touch_ghost_bbox(data->view, *map_pos, data->redraw_area));

  return index;
}

void InfoMode_draw(Editor *const editor,
  Vertex const scr_orig, MapArea const *const redraw_area,
  EditWin const *const edit_win)
{
  View const *const view = EditWin_get_view(edit_win);
  int const zoom = EditWin_get_zoom(edit_win);

  /* Process redraw rectangle */
  DEBUG("Request to redraw infos for area %" PRIMapCoord " <= x <= %" PRIMapCoord ", %" PRIMapCoord " <= y <= %" PRIMapCoord,
        redraw_area->min.x, redraw_area->max.x, redraw_area->min.y, redraw_area->max.y);
  assert(redraw_area->max.x >= redraw_area->min.x);
  assert(redraw_area->max.y >= redraw_area->min.y);

  EditSession *const session = Editor_get_session(editor);

  if (!Session_has_data(session, DataType_Mission))
  {
    DEBUGF("Nothing to plot\n");
    return;
  }

  if (zoom > 2)
  {
    DEBUGF("Zoomed too far out to draw infos sensibly\n");
    return;
  }

  MapArea overlapping_area;
  DrawInfos_get_overlapping_draw_area(view, redraw_area, &overlapping_area);

  InfoEditContext const *const infos = EditWin_get_read_info_ctx(edit_win);

  InfoModeData *const mode_data =
      Editor_get_edit_mode(editor) == EDITING_MODE_INFO ?
      editor->editingmode_data : NULL;

  SelectionBitmask const *const selection = mode_data ? &mode_data->selection : NULL;
  SelectionBitmask const *const occluded = mode_data && (mode_data->pending_drop ||
                                                         mode_data->pending_shape != Pending_None) ?
                                           &mode_data->occluded : NULL;

  DrawInfoData data = {.view = view, .infos = infos, .redraw_area = redraw_area};
  data.index = InfoEdit_get_first_idx(&data.iter, infos, &overlapping_area);
  DrawInfos_to_screen(view, read_info_from_map, &data,
                      selection, scr_orig, false, occluded);

  if (mode_data && mode_data->pending_shape != Pending_None) {
    plot_set_col(EditWin_get_ghost_colour(edit_win));
    draw_pending(mode_data, edit_win, scr_orig, redraw_area, &overlapping_area);
  }

  if (mode_data && mode_data->pending_drop) {
    draw_ghost_paste(mode_data->pending_drop, mode_data->drop_bbox.min, edit_win,
                     scr_orig, redraw_area);
  }

  if (mode_data && mode_data->uk_drop_pending) {
    plot_set_col(EditWin_get_ghost_colour(edit_win));

    draw_unknown_drop(edit_win, &mode_data->drop_bbox, scr_orig, redraw_area, &overlapping_area);
  }
}

typedef struct {
  Editor *editor;
  InfoEditContext const *infos;
} OccludedData;

static void occluded_changed(size_t const index, void *const arg)
{
  OccludedData const *const data = arg;
  assert(data);
  MapPoint const pos = target_info_get_pos(InfoEdit_get(data->infos, index));
  Editor_occluded_info_changed(data->editor, pos);
}

static void InfoMode_wipe_ghost(Editor *const editor)
{
  assert(editor);
  InfoModeData *const mode_data = get_mode_data(editor);

  if (mode_data->pending_shape == Pending_None) {
    return;
  }

  DEBUGF("Wiping ghost info(s)\n");

  EditSession *const session = Editor_get_session(editor);
  InfoEditContext const *const infos = Session_get_infos(session);
  OccludedData data = {.editor = editor, .infos = infos};
  SelectionBitmask_for_each(&mode_data->occluded, occluded_changed, &data);
  SelectionBitmask_clear(&mode_data->occluded);

  Editor_redraw_ghost(editor); // undraw
  Editor_clear_ghost_bbox(editor);

  mode_data->pending_shape = Pending_None;
  mode_data->pending_transfer = NULL;
}

static void InfoMode_add_ghost_bbox_for_transfer(Editor *const editor,
  InfoEditContext const *const infos, MapPoint const bl, InfoTransfer *const transfer,
  SelectionBitmask *const occluded)
{
  DEBUGF("Ghost of transfer %p at grid coordinates %" PRIMapCoord ",%" PRIMapCoord "\n",
         (void *)transfer, bl.x, bl.y);

  size_t const count = InfoTransfers_get_info_count(transfer);
  for (size_t index = 0; index < count; ++index) {
    MapPoint const map_pos = MapPoint_add(bl, InfoTransfers_get_pos(transfer, index));
    InfoEdit_find_occluded(infos, map_pos, occluded);
    Editor_add_ghost_info(editor, map_pos);
  }
}

static void InfoMode_set_pending(Editor *const editor, PendingShape const pending_shape,
  InfoTransfer *const pending_transfer, MapPoint const pos)
{
  assert(editor);
  InfoModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  InfoEditContext const *const infos = Session_get_infos(session);

  if (mode_data->pending_shape != Pending_None) {
    Editor_redraw_ghost(editor); // undraw
  }

  Editor_clear_ghost_bbox(editor);

  SelectionBitmask_copy(&mode_data->tmp, &mode_data->occluded);
  SelectionBitmask_clear(&mode_data->occluded);

  bool any = false;

  switch (pending_shape) {
    case Pending_Point:
      InfoEdit_find_occluded(infos, pos, &mode_data->occluded);
      Editor_add_ghost_info(editor, pos);
      any = true;
      break;

    case Pending_Transfer:
      InfoTransfers_find_occluded(infos, pos, pending_transfer, &mode_data->occluded);
      InfoMode_add_ghost_bbox_for_transfer(editor, infos,
        pos, pending_transfer, &mode_data->occluded);
      any = true;
      break;

    default:
      break; /* unknown plot type */
  }

  OccludedData data = {.editor = editor, .infos = infos};
  if (!any) {
    mode_data->pending_shape = Pending_None;
    mode_data->pending_transfer = NULL;
    SelectionBitmask_for_each(&mode_data->tmp /* previously occluded */, occluded_changed, &data);
  } else {
    mode_data->pending_shape = pending_shape;
    mode_data->pending_transfer = pending_transfer;
    mode_data->pending_vert = pos;
    SelectionBitmask_for_each_changed(&mode_data->occluded, &mode_data->tmp, // previously occluded
       occluded_changed, &data);
  }

  Editor_redraw_ghost(editor); // draw
}

static bool InfoMode_can_select_tool(Editor const *const editor, EditorTool const tool)
{
  NOT_USED(editor);
  bool can_select_tool = false;

  switch (tool)
  {
  case EDITORTOOL_BRUSH:
  case EDITORTOOL_SELECT:
  case EDITORTOOL_MAGNIFIER:
    can_select_tool = true;
    break;
  default:
    break;
  }

  return can_select_tool;
}

static bool InfoMode_has_selection(Editor const *const editor)
{
  InfoModeData *const mode_data = get_mode_data(editor);

  return !SelectionBitmask_is_none(&mode_data->selection);
}

static bool InfoMode_can_edit_properties(Editor const *const editor)
{
  InfoModeData *const mode_data = get_mode_data(editor);
  return SelectionBitmask_size(&mode_data->selection) == 1;
}

static void InfoMode_edit_properties(Editor *const editor, EditWin *const edit_win)
{
  assert(InfoMode_can_edit_properties(editor));
  InfoModeData *const mode_data = get_mode_data(editor);
  SelectionBitmaskIter iter;
  size_t const index = SelectionBitmaskIter_get_first(&iter, &mode_data->selection);
  assert(!SelectionBitmaskIter_done(&iter));

  EditSession *const session = Editor_get_session(editor);
  InfoEditContext const *const infos = Session_get_infos(session);
  TargetInfo *info = InfoEdit_get(infos, index);
  InfoPropDboxes_open(&mode_data->prop_dboxes, info, edit_win);
}

static void InfoMode_update_title(Editor *const editor)
{
  InfoModeData *const mode_data = get_mode_data(editor);
  InfoPropDboxes_update_title(&mode_data->prop_dboxes);
}

static void notify_changed(EditSession *const session,
  InfoEditChanges *const change_info)
{
  DEBUG("Assimilating change record %p", (void *)change_info);

  if (InfoEditChanges_is_changed(change_info))
  {
    Session_notify_changed(session, DataType_Mission);
  }
}

static void display_msg(Editor *const editor,
  InfoEditChanges const *const change_info)
{
  char *const msg = InfoEditChanges_get_message(change_info);
  if (msg) {
    Editor_display_msg(editor, msg, true);
  }
}

static void free_pending_paste(InfoModeData *const mode_data)
{
  assert(mode_data);
  if (mode_data->pending_paste) {
    assert(mode_data->pending_paste != mode_data->pending_transfer);
    dfile_release(InfoTransfer_get_dfile(mode_data->pending_paste));
    mode_data->pending_paste = NULL;
  }
}

static void free_dragged(InfoModeData *const mode_data)
{
  assert(mode_data);
  if (mode_data->dragged) {
    assert(mode_data->dragged != mode_data->pending_transfer);
    dfile_release(InfoTransfer_get_dfile(mode_data->dragged));
    mode_data->dragged = NULL;
  }
}

static void free_pending_drop(InfoModeData *const mode_data)
{
  assert(mode_data);
  if (mode_data->pending_drop) {
    assert(mode_data->pending_drop != mode_data->pending_transfer);
    dfile_release(InfoTransfer_get_dfile(mode_data->pending_drop));
    mode_data->pending_drop = NULL;
  }
}

static TargetInfo *get_info_at_point(View const *const view,
  InfoEditContext const *const infos,
  MapPoint const fine_pos, size_t *const index_out)
{
  /* If there is an info at the specified grid location then return its
     address. Otherwise, search for any nearby infos that overlap the specified
     location. If one is found then return its address and array index. */
  assert(index_out);
  DEBUG("Will search for an info overlapping point %" PRIMapCoord ",%" PRIMapCoord,
        fine_pos.x, fine_pos.y);

  TargetInfo *info = NULL;
  MapArea const sample_point = {.min = fine_pos, .max = fine_pos};
  MapPoint const search_centre = MapLayout_map_coords_from_fine(view, fine_pos);

  /* First, check the info at the grid location within which the specified
     map coordinates lie. */
  MapArea overlapping_area = {search_centre, search_centre};
  InfoEditIter iter;
  size_t index = InfoEdit_get_first_idx(&iter, infos, &overlapping_area);
  if (!InfoEditIter_done(&iter)) {
     TargetInfo *const candidate = InfoEdit_get(infos, index);
      MapPoint const grid_pos = target_info_get_pos(candidate);

     if (DrawInfos_touch_select_bbox(view, grid_pos, &sample_point)) {
       DEBUG("Found info %p at exact location", (void *)candidate);
       *index_out = index;
       info = candidate;
     }
  }

  if (!info) {
    /* Nothing at the specified grid location, so search outwards  */
    DrawInfos_get_select_area(view, &sample_point, &overlapping_area);

    for (index = InfoEdit_get_first_idx(&iter, infos, &overlapping_area);
         !InfoEditIter_done(&iter);
         index = InfoEditIter_get_next(&iter))
    {
      TargetInfo *const candidate = InfoEdit_get(infos, index);
      MapPoint const grid_pos = target_info_get_pos(candidate);

      if (DrawInfos_touch_select_bbox(view, grid_pos, &sample_point)) {
        *index_out = index;
        info = candidate;
        break;
      }
    }
  }

  if (info) {
    DEBUG("Found overlapping info at %zu", index);
  } else {
    DEBUG("No overlapping info found");
  }

  return info;
}

static bool drag_select_core(View const *const view, SelectionBitmask *const selected,
  InfoEditContext const *const infos,
  bool const only_inside, MapArea const *select_box, bool const do_redraw)
{
  bool is_changed = false;
  MapArea overlapping_area;
  DrawInfos_get_select_area(view, select_box, &overlapping_area);

  InfoEditIter iter;
  for (size_t index = InfoEdit_get_first_idx(&iter, infos, &overlapping_area);
       !InfoEditIter_done(&iter);
       index = InfoEditIter_get_next(&iter))
  {
    MapPoint const grid_pos = target_info_get_pos(InfoEdit_get(infos, index));
    bool invert = only_inside ?
      DrawInfos_in_select_bbox(view, grid_pos, select_box) :
      DrawInfos_touch_select_bbox(view, grid_pos, select_box);

    if (invert) {
      SelectionBitmask_invert(selected, index, do_redraw);
      is_changed = true;
    }
  }
  return is_changed;
}

static void redraw_selection(size_t const index, void *const arg)
{
  DEBUGF("%s\n", __func__);
  Editor *const editor = arg;
  EditSession *const session = Editor_get_session(editor);
  InfoEditContext const *const infos = Session_get_infos(session);
  TargetInfo *const info = InfoEdit_get(infos, index);
  Editor_redraw_info(editor, target_info_get_pos(info));
}

static void InfoMode_update_select(Editor *const editor, bool const only_inside,
  MapArea const *const last_select_box, MapArea const *const select_box,
  EditWin const *const edit_win)
{
  InfoModeData *const mode_data = get_mode_data(editor);
  View const *const view = EditWin_get_view(edit_win);
  InfoEditContext const *const infos = EditWin_get_read_info_ctx(edit_win);

  SelectionBitmask_copy(&mode_data->tmp, &mode_data->selection);

  bool changed = drag_select_core(view, &mode_data->selection, infos,
                                  only_inside, last_select_box, false);

  if (!drag_select_core(view, &mode_data->selection, infos, only_inside,
                        select_box, false) &&
      !changed) {
    return;
  }

  SelectionBitmask_for_each_changed(&mode_data->selection, &mode_data->tmp,
                                    redraw_selection, editor);
}

static void InfoMode_cancel_select(Editor *const editor,
  bool const only_inside, MapArea const *const last_select_box, EditWin *const edit_win)
{
  /* Abort selection drag by undoing effect of last rectangle */
  InfoModeData *const mode_data = get_mode_data(editor);
  InfoEditContext const *const infos = EditWin_get_read_info_ctx(edit_win);
  View const *const view = EditWin_get_view(edit_win);

  drag_select_core(view, &mode_data->selection, infos,
                   only_inside, last_select_box, true);
}

static void changed_with_msg(Editor *const editor)
{
  EditSession *const session = Editor_get_session(editor);
  InfoModeData *const mode_data = get_mode_data(editor);
  assert(mode_data);

  notify_changed(session, &mode_data->change_info);
  display_msg(editor, &mode_data->change_info);
}

static bool paste_generic(Editor *const editor,
  InfoTransfer *const transfer, MapPoint map_pos)
{
  InfoModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  InfoEditContext const *const infos = Session_get_infos(session);

  InfoMode_wipe_ghost(editor);

  /* Plot transfer at mouse pointer */
  MapPoint const t_dims = InfoTransfers_get_dims(mode_data->pending_paste);
  map_pos = MapPoint_sub(map_pos, MapPoint_div_log2(t_dims, 1));

  InfoEditChanges_init(&mode_data->change_info);

  SelectionBitmask_clear(&mode_data->selection);
  InfoTransfers_plot_to_map(infos, map_pos, transfer, &mode_data->selection,
                                  &mode_data->change_info);
  changed_with_msg(editor);
  return true;
}

static bool InfoMode_start_select(Editor *const editor, bool const only_inside,
  MapPoint const fine_pos, EditWin *const edit_win)
{
  NOT_USED(only_inside);
  InfoModeData *const mode_data = get_mode_data(editor);
  InfoEditContext const *const infos = EditWin_get_read_info_ctx(edit_win);
  View const *const view = EditWin_get_view(edit_win);

  size_t index;
  TargetInfo *const info = get_info_at_point(view, infos, fine_pos, &index);

  if (info) {
    SelectionBitmask_invert(&mode_data->selection, index, true);
  }

  return !info;
}

static bool InfoMode_start_exclusive_select(Editor *const editor, bool const only_inside,
  MapPoint const fine_pos, EditWin *const edit_win)
{
  NOT_USED(only_inside);
  InfoModeData *const mode_data = get_mode_data(editor);
  InfoEditContext const *const infos = EditWin_get_read_info_ctx(edit_win);
  View const *const view = EditWin_get_view(edit_win);
  size_t index;
  TargetInfo *const info = get_info_at_point(view, infos, fine_pos, &index);

  if (info) {
    if (!SelectionBitmask_is_selected(&mode_data->selection, index)) {
      SelectionBitmask_clear(&mode_data->selection);
      SelectionBitmask_invert(&mode_data->selection, index, true);
    }
  } else {
    SelectionBitmask_clear(&mode_data->selection);
  }

  return !info;
}

static void InfoMode_edit_properties_at_pos(Editor *const editor, MapPoint const fine_pos, EditWin *const edit_win)
{
  InfoModeData *const mode_data = get_mode_data(editor);
  InfoEditContext const *const infos = EditWin_get_read_info_ctx(edit_win);
  View const *const view = EditWin_get_view(edit_win);
  size_t index;
  TargetInfo *const info = get_info_at_point(view, infos, fine_pos, &index);

  if (info) {
    InfoPropDboxes_open(&mode_data->prop_dboxes, info, edit_win);
  }
}

static void InfoMode_pending_brush(Editor *const editor, int brush_size,
   MapPoint const map_pos)
{
  NOT_USED(brush_size);
  InfoMode_set_pending(editor, Pending_Point, NULL, map_pos);
}

static void InfoMode_start_brush(Editor *const editor, int brush_size,
  MapPoint const map_pos)
{
  NOT_USED(brush_size);
  InfoModeData *const mode_data = get_mode_data(editor);

  EditSession *const session = Editor_get_session(editor);
  InfoEditContext const *const infos = Session_get_infos(session);

  InfoEditChanges_init(&mode_data->change_info);

  InfoMode_wipe_ghost(editor);
  report_error(InfoEdit_add(infos, map_pos, NULL, &mode_data->change_info, NULL), "", "");
  changed_with_msg(editor);
}

static bool InfoMode_start_pending_paste(Editor *const editor, Reader *const reader,
                       int const estimated_size,
                       DataType const data_type, char const *const filename)
{
  NOT_USED(estimated_size);
  NOT_USED(data_type);
  InfoModeData *const mode_data = get_mode_data(editor);

  free_pending_paste(mode_data);
  mode_data->pending_paste = InfoTransfer_create();
  if (mode_data->pending_paste == NULL) {
    return false;
  }

  SFError err = read_compressed(InfoTransfer_get_dfile(mode_data->pending_paste),
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

static void InfoMode_pending_paste(Editor *const editor, MapPoint const map_pos)
{
  InfoModeData *const mode_data = get_mode_data(editor);
  assert(mode_data->pending_paste);

  MapPoint const t_dims = InfoTransfers_get_dims(mode_data->pending_paste);

  InfoMode_set_pending(editor, Pending_Transfer, mode_data->pending_paste,
                          MapPoint_sub(map_pos, MapPoint_div_log2(t_dims, 1)));
}

static bool InfoMode_draw_paste(Editor *const editor, MapPoint const map_pos)
{
  InfoModeData *const mode_data = get_mode_data(editor);
  assert(mode_data->pending_paste);

  if (!paste_generic(editor, mode_data->pending_paste, map_pos)) {
    return false;
  }
  free_pending_paste(mode_data);
  return true;
}

static void InfoMode_cancel_paste(Editor *const editor)
{
  InfoModeData *const mode_data = get_mode_data(editor);
  if (!mode_data->pending_paste) {
    return;
  }

  InfoMode_wipe_ghost(editor);
  free_pending_paste(mode_data);
}

static bool InfoMode_can_draw_grid(Editor *editor, EditWin const *const edit_win)
{
  NOT_USED(editor);
  return EditWin_get_zoom(edit_win) <= 1;
}

static void InfoMode_draw_grid(Vertex const scr_orig,
  MapArea const *const redraw_area, EditWin const *edit_win)
{
  assert(InfoMode_can_draw_grid(EditWin_get_editor(edit_win), edit_win));
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

static void delete_core(Editor *const editor, InfoEditContext const *const infos,
  InfoEditChanges *const change_info)
{
  InfoModeData *const mode_data = get_mode_data(editor);
  InfoEdit_delete(infos, &mode_data->selection, change_info);
}

static InfoTransfer *clipboard;

static bool cb_copy_core(Editor *const editor)
{
  InfoModeData *const mode_data = get_mode_data(editor);
  assert(!SelectionBitmask_is_none(&mode_data->selection));

  EditSession *const session = Editor_get_session(editor);
  assert(!clipboard);
  clipboard = InfoTransfers_grab_selection(
    Session_get_infos(session), &mode_data->selection);

  return clipboard != NULL;
}

static void cb_status(Editor *const editor, bool const copy)
{
  InfoModeData *const mode_data = get_mode_data(editor);
  size_t const infos_count = SelectionBitmask_size(&mode_data->selection);
  char infos_count_str[16];
  sprintf(infos_count_str, "%ld", infos_count);

  Editor_display_msg(editor, msgs_lookup_subn(copy ? "IStatusCopy1" :
                     "IStatusCut1", 1, infos_count_str), true);
}

static void clear_selection_and_redraw(Editor *const editor)
{
  /* Deselect all infos on the map */
  InfoModeData *const mode_data = get_mode_data(editor);
  SelectionBitmask_clear(&mode_data->selection);
}

static size_t InfoMode_num_selected(Editor const *const editor)
{
  InfoModeData *const mode_data = get_mode_data(editor);
  return SelectionBitmask_size(&mode_data->selection);
}

static size_t InfoMode_max_selected(Editor const *const editor)
{
  return InfoEdit_count(Session_get_infos(Editor_get_session(editor)));
}

static bool InfoMode_auto_select(Editor *const editor, MapPoint const fine_pos, EditWin *const edit_win)
{
  InfoModeData *const mode_data = get_mode_data(editor);
  if (!SelectionBitmask_is_none(&mode_data->selection) ||
      Editor_get_tool(editor) != EDITORTOOL_SELECT) {
    return false; /* already have a selection or not using that tool */
  }

  InfoEditContext const *const infos = EditWin_get_read_info_ctx(edit_win);
  View const *const view = EditWin_get_view(edit_win);
  size_t index;
  TargetInfo *const info = get_info_at_point(view, infos, fine_pos, &index);

  if (!info) {
    return false;
  }

  SelectionBitmask_select(&mode_data->selection, index);
  return true;
}

static void InfoMode_auto_deselect(Editor *const editor)
{
  clear_selection_and_redraw(editor);
}

static void info_deleted(InfoModeData *const mode_data, size_t const index)
{
    SelectionBitmask_obj_deleted(&mode_data->selection, index);
    SelectionBitmask_obj_deleted(&mode_data->occluded, index);
}

static void info_inserted(InfoModeData *const mode_data, size_t const index)
{
    SelectionBitmask_obj_inserted(&mode_data->selection, index);
    SelectionBitmask_obj_inserted(&mode_data->occluded, index);
}

static void InfoMode_resource_change(Editor *const editor, EditorChange const event,
  EditorChangeParams const *const params)
{
  InfoModeData *const mode_data = get_mode_data(editor);
  bool is_selected = false, is_occluded = false;

  switch (event) {
  case EDITOR_CHANGE_INFO_ADDED:
    assert(params);
    info_inserted(mode_data, params->info_added.index);
    break;

  case EDITOR_CHANGE_INFO_PREDELETE:
    assert(params);
    info_deleted(mode_data, params->info_predelete.index);
    InfoPropDboxes_update_for_del(&mode_data->prop_dboxes, params->info_predelete.info);
    break;

  case EDITOR_CHANGE_INFO_MOVED:
    assert(params);
    is_selected = SelectionBitmask_is_selected(&mode_data->selection, params->info_moved.old_index);
    is_occluded = SelectionBitmask_is_selected(&mode_data->occluded, params->info_moved.old_index);
    info_deleted(mode_data, params->info_moved.old_index);
    info_inserted(mode_data, params->info_moved.new_index);
    if (is_selected) {
      SelectionBitmask_select(&mode_data->selection, params->info_moved.new_index);
    }
    if (is_occluded) {
      SelectionBitmask_select(&mode_data->occluded, params->info_moved.new_index);
    }
    InfoPropDboxes_update_for_move(&mode_data->prop_dboxes, params->info_moved.info,
                                   params->info_moved.old_pos);
    break;

  case EDITOR_CHANGE_MISSION_REPLACED:
    InfoPropDboxes_destroy(&mode_data->prop_dboxes);
    InfoPropDboxes_init(&mode_data->prop_dboxes, editor);
    {
      size_t const count = InfoEdit_count(Session_get_infos(Editor_get_session(editor)));
      SelectionBitmask_init(&mode_data->selection, count, redraw_selection, editor);
    }
    break;

  default:
    break;
  }
}

static void InfoMode_select_all(Editor *const editor)
{
  /* Select all infos on the map */
  InfoModeData *const mode_data = get_mode_data(editor);
  SelectionBitmask_select_all(&mode_data->selection);
}

static void InfoMode_clear_selection(Editor *const editor)
{
  clear_selection_and_redraw(editor);
}

static void InfoMode_delete(Editor *const editor)
{
  InfoModeData *const mode_data = get_mode_data(editor);

  InfoEditChanges_init(&mode_data->change_info);
  EditSession *const session = Editor_get_session(editor);
  delete_core(editor, Session_get_infos(session), &mode_data->change_info);
  changed_with_msg(editor);
}

static bool InfoMode_cut(Editor *editor)
{
  if (!cb_copy_core(editor)) {
    return false;
  }

  cb_status(editor, false);

  EditSession *const session = Editor_get_session(editor);
  delete_core(editor, Session_get_infos(session), NULL);

  return true;
}

static bool InfoMode_copy(Editor *editor)
{
  if (!cb_copy_core(editor)) {
    return false;
  }

  cb_status(editor, true);
  return true;
}

static bool InfoMode_start_drag_obj(Editor *const editor,
  MapPoint const fine_pos, EditWin *const edit_win)
{
  InfoModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  InfoEditContext const *const infos = Session_get_infos(session);

  if (SelectionBitmask_is_none(&mode_data->selection)) {
    return false;
  }

  InfoTransfer *const transfer = InfoTransfers_grab_selection(infos, &mode_data->selection);
  if (!transfer) {
    return false;
  }

  free_dragged(mode_data);
  mode_data->dragged = transfer;

  MapArea sent_bbox = MapArea_make_invalid(), shown_bbox = MapArea_make_invalid();
  MapPoint bl = InfoTransfers_get_origin(transfer);
  MapPoint const t_dims = InfoTransfers_get_dims(transfer);

  /* Although the transfer origin may happen to be relative to the drag start
     position, it is not guaranteed (e.g. click on far left, drag on far right). */
  View const *const view = EditWin_get_view(edit_win);
  MapPoint const map_pos = MapLayout_map_coords_from_fine(view, fine_pos);

  MapPoint const t_max = MapPoint_add(bl, t_dims);
  if (map_pos.x + (Map_Size / 2) < bl.x) {
    bl.x -= Map_Size;
  } else if (map_pos.x - (Map_Size / 2) >= t_max.x) {
    bl.x += Map_Size;
  }

  if (map_pos.y + (Map_Size / 2) < bl.y) {
    bl.y -= Map_Size;
  } else if (map_pos.y - (Map_Size / 2) >= t_max.y) {
    bl.y += Map_Size;
  }

  size_t const count = InfoTransfers_get_info_count(transfer);

  for (size_t index = 0; index < count; ++index) {
    MapPoint const info_pos = MapPoint_add(bl, InfoTransfers_get_pos(transfer, index));
    MapArea_expand(&sent_bbox, MapLayout_map_coords_to_centre(EditWin_get_view(edit_win), info_pos));

    MapArea const info_bbox = EditWin_get_ghost_info_bbox(edit_win, info_pos);
    MapArea_expand_for_area(&shown_bbox, &info_bbox);
  }

  mode_data->drag_start_pos = bl;

  MapArea_translate(&sent_bbox, (MapPoint){-fine_pos.x, -fine_pos.y}, &sent_bbox);
  MapArea_translate(&shown_bbox, (MapPoint){-fine_pos.x, -fine_pos.y}, &shown_bbox);
  return EditWin_start_drag_obj(edit_win, &sent_bbox, &shown_bbox);
}

static bool InfoMode_drag_obj_remote(Editor *const editor, struct Writer *const writer,
    DataType const data_type, char const *const filename)
{
  NOT_USED(data_type);
  InfoModeData *const mode_data = get_mode_data(editor);

  if (!mode_data->dragged) {
    return false;
  }

  bool success = !report_error(write_compressed(InfoTransfer_get_dfile(mode_data->dragged),
                                 writer), filename, "");

  free_dragged(mode_data);
  return success;
}

static bool InfoMode_show_ghost_drop(Editor *const editor,
                                        MapArea const *const bbox,
                                        Editor const *const drag_origin)
{
  bool hide_origin_bbox = true;
  InfoModeData *const mode_data = get_mode_data(editor);
  InfoModeData *const origin_data = drag_origin ? get_mode_data(drag_origin) : NULL;
  assert(MapArea_is_valid(bbox));

  if (origin_data) {
    // Dragging from a window belonging to this task
    assert(origin_data->dragged);
    assert(!mode_data->uk_drop_pending);

    EditSession *const session = Editor_get_session(editor);
    InfoEditContext const *const infos = Session_get_infos(session);

    if (mode_data->pending_drop) {
      if (MapArea_compare(&mode_data->drop_bbox, bbox) &&
          mode_data->pending_drop == origin_data->dragged) {
        DEBUGF("Drop pos unchanged\n");
        return hide_origin_bbox;
      }

      free_pending_drop(mode_data);
      Editor_redraw_ghost(editor); // undraw
    }

    Editor_clear_ghost_bbox(editor);

    SelectionBitmask_copy(&mode_data->tmp, &mode_data->occluded);
    SelectionBitmask_clear(&mode_data->occluded);

    InfoTransfers_find_occluded(infos, bbox->min, origin_data->dragged, &mode_data->occluded);
    InfoMode_add_ghost_bbox_for_transfer(editor, infos, bbox->min, origin_data->dragged, &mode_data->occluded);

    OccludedData data = {.editor = editor, .infos = infos};
    SelectionBitmask_for_each_changed(&mode_data->occluded, &mode_data->tmp,
                                      occluded_changed, &data);

    mode_data->pending_drop = origin_data->dragged;
    dfile_claim(InfoTransfer_get_dfile(origin_data->dragged));
  } else {
    // Dragging from a window belonging to another task
    assert(!mode_data->pending_drop);

    if (mode_data->uk_drop_pending) {
      if (MapArea_compare(&mode_data->drop_bbox, bbox)) {
        DEBUGF("Drop pos unchanged\n");
        return hide_origin_bbox;
      }

      Editor_redraw_ghost(editor); // undraw
    }

    if (MapPoint_compare(bbox->min, bbox->max)) {
      InfoMode_set_pending(editor, Pending_Point, NULL, bbox->min);
    } else {
      InfoMode_wipe_ghost(editor);
      Editor_clear_ghost_bbox(editor);
      Editor_add_ghost_unknown_info(editor, bbox);
    }
    mode_data->uk_drop_pending = true;
  }

  mode_data->drop_bbox = *bbox;

  Editor_redraw_ghost(editor); // draw
  return hide_origin_bbox;
}

static void InfoMode_hide_ghost_drop(Editor *const editor)
{
  InfoModeData *const mode_data = get_mode_data(editor);

  if (mode_data->pending_drop) {
    EditSession *const session = Editor_get_session(editor);
    InfoEditContext const *const infos = Session_get_infos(session);
    OccludedData data = {.editor = editor, .infos = infos};
    SelectionBitmask_for_each(&mode_data->occluded, occluded_changed, &data);
    SelectionBitmask_clear(&mode_data->occluded);
    Editor_redraw_ghost(editor); // undraw
    Editor_clear_ghost_bbox(editor);
    free_pending_drop(mode_data);
  }

  if (mode_data->uk_drop_pending) {
    if (mode_data->pending_shape != Pending_None) {
      InfoMode_wipe_ghost(editor);
    } else {
      Editor_redraw_ghost(editor); // undraw
      Editor_clear_ghost_bbox(editor);
    }
    mode_data->uk_drop_pending = false;
  }
}

static bool drag_obj_copy_core(Editor *const editor,
                           MapArea const *const bbox,
                           InfoTransfer *const dropped,
                           InfoEditContext const *const infos)
{
  assert(MapArea_is_valid(bbox));
  InfoModeData *const mode_data = get_mode_data(editor);

  SelectionBitmask_clear(&mode_data->selection);

  InfoTransfers_plot_to_map(infos, bbox->min, dropped, &mode_data->selection,
                                   &mode_data->change_info);
  return true;
}

static bool InfoMode_drag_obj_copy(Editor *const editor,
                                  MapArea const *const bbox,
                                  Editor const *const drag_origin)
{
  InfoModeData *const dst_data = get_mode_data(editor);
  InfoModeData *const origin_data = get_mode_data(drag_origin);
  EditSession *const session = Editor_get_session(editor);

  InfoEditChanges_init(&dst_data->change_info);

  if (!drag_obj_copy_core(editor, bbox, origin_data->dragged, Session_get_infos(session))) {
    return false;
  }

  changed_with_msg(editor);
  free_dragged(origin_data);

  return true;
}

static void InfoMode_cancel_drag_obj(Editor *const editor)
{
  InfoModeData *const mode_data = get_mode_data(editor);
  free_dragged(mode_data);
}

static void InfoMode_drag_obj_move(Editor *const editor,
                                  MapArea const *const bbox,
                                  Editor *const drag_origin)
{
  InfoModeData *const dst_data = get_mode_data(editor);
  InfoModeData *const origin_data = get_mode_data(drag_origin);
  EditSession *const session = Editor_get_session(editor);
  assert(session == Editor_get_session(drag_origin));

  InfoEditContext const *infos = Session_get_infos(session);

  MapPoint const vec = MapPoint_sub(bbox->min, origin_data->drag_start_pos);

  InfoEditChanges_init(&dst_data->change_info);
  InfoEditChanges_init(&origin_data->change_info);

  // Don't delete and re-add data to avoid losing its unique identity
  InfoEdit_move(infos,
                vec,
                &origin_data->selection,
                &origin_data->change_info);

  changed_with_msg(editor);
  if (editor != drag_origin) {
    changed_with_msg(drag_origin);
  }
  free_dragged(origin_data);
}

static bool InfoMode_drop(Editor *const editor, MapArea const *const bbox,
                             Reader *const reader, int const estimated_size,
                             DataType const data_type, char const *const filename)
{
  NOT_USED(data_type);
  NOT_USED(estimated_size);
  InfoModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);

  InfoTransfer *const dropped = InfoTransfer_create();
  if (dropped == NULL) {
    return false;
  }

  SFError err = read_compressed(InfoTransfer_get_dfile(dropped), reader);
  bool success = !report_error(err, filename, "");
  if (success) {
    InfoEditChanges_init(&mode_data->change_info);

    success = drag_obj_copy_core(editor, bbox, dropped, Session_get_infos(session));
    if (success) {
      changed_with_msg(editor);
    }
  }

  dfile_release(InfoTransfer_get_dfile(dropped));
  return success;
}

static char *InfoMode_get_help_msg(Editor const *const editor)
{
  char *msg = NULL; // remove help
  InfoModeData *const mode_data = get_mode_data(editor);

  switch (Editor_get_tool(editor)) {
    case EDITORTOOL_BRUSH:
      msg = msgs_lookup("MapInfoBrush");
      break;

    case EDITORTOOL_SELECT:
      msg = msgs_lookup(mode_data->pending_paste ? "MapInfoPaste" : "MapInfoSelect");
      break;

    default:
      break;
  }
  return msg;
}

static void InfoMode_tool_selected(Editor *const editor)
{
  InfoMode_wipe_ghost(editor);

  if (Editor_get_tool(editor) != EDITORTOOL_NONE)
  {
    InfoPalette_register(&editor->palette_data);
  }
}

static MapPoint InfoMode_map_to_grid_coords(MapPoint const pos, EditWin const *const edit_win)
{
  return MapLayout_map_coords_from_fine(EditWin_get_view(edit_win), pos);
}

MapArea InfoMode_map_to_grid_area(MapArea const *const map_area, EditWin const *const edit_win)
{
  return MapLayout_map_area_from_fine(EditWin_get_view(edit_win), map_area);
}

static MapPoint InfoMode_grid_to_map_coords(MapPoint const pos, EditWin const *const edit_win)
{
  return MapLayout_map_coords_to_centre(EditWin_get_view(edit_win), pos);
}

bool InfoMode_can_enter(Editor *const editor)
{
  return Session_has_data(Editor_get_session(editor), DataType_Mission);
}

bool InfoMode_enter(Editor *const editor)
{
  DEBUG("Entering info mode");
  assert(InfoMode_can_enter(editor));

  InfoModeData *const mode_data = malloc(sizeof(InfoModeData));
  if (mode_data == NULL) {
    report_error(SFERROR(NoMem), "", "");
    return false;
  }

  *mode_data = (InfoModeData){
    .pending_shape = Pending_None,
  };

  size_t const count = InfoEdit_count(Session_get_infos(Editor_get_session(editor)));
  SelectionBitmask_init(&mode_data->selection, count, redraw_selection, editor);
  SelectionBitmask_init(&mode_data->tmp, count, NULL, NULL);
  SelectionBitmask_init(&mode_data->occluded, count, NULL, NULL);

  editor->editingmode_data = mode_data;

  static DataType const type_list[] = {DataType_InfosTransfer, DataType_Count};

  static EditModeFuncts const info_mode_fns = {
    .coord_limit = {Map_Size, Map_Size},
    .dragged_data_types = type_list,
    .import_data_types = type_list,
    .export_data_types = type_list,
    .auto_select = InfoMode_auto_select,
    .auto_deselect = InfoMode_auto_deselect,
    .can_draw_grid = InfoMode_can_draw_grid,
    .draw_grid = InfoMode_draw_grid,
    .leave = InfoMode_leave,
    .map_to_grid_coords = InfoMode_map_to_grid_coords,
    .map_to_grid_area = InfoMode_map_to_grid_area,
    .grid_to_map_coords = InfoMode_grid_to_map_coords,
    .num_selected = InfoMode_num_selected,
    .max_selected = InfoMode_max_selected,
    .resource_change = InfoMode_resource_change,
    .can_delete = InfoMode_has_selection,
    .can_select_tool = InfoMode_can_select_tool,
    .tool_selected = InfoMode_tool_selected,
    .select_all = InfoMode_select_all,
    .clear_selection = InfoMode_clear_selection,
    .delete = InfoMode_delete,
    .cut = InfoMode_cut,
    .copy = InfoMode_copy,
    .can_edit_properties = InfoMode_can_edit_properties,
    .edit_properties = InfoMode_edit_properties,
    .update_title = InfoMode_update_title,
    .get_help_msg = InfoMode_get_help_msg,

    .pending_brush = InfoMode_pending_brush,
    .start_brush = InfoMode_start_brush,

    .start_select = InfoMode_start_select,
    .start_exclusive_select = InfoMode_start_exclusive_select,
    .update_select = InfoMode_update_select,
    .cancel_select = InfoMode_cancel_select,

    .start_drag_obj = InfoMode_start_drag_obj,
    .drag_obj_remote = InfoMode_drag_obj_remote,
    .drag_obj_copy = InfoMode_drag_obj_copy,
    .drag_obj_move = InfoMode_drag_obj_move,
    .cancel_drag_obj = InfoMode_cancel_drag_obj,

    .show_ghost_drop = InfoMode_show_ghost_drop,
    .hide_ghost_drop = InfoMode_hide_ghost_drop,
    .drop = InfoMode_drop,

    .edit_properties_at_pos = InfoMode_edit_properties_at_pos,

    .start_pending_paste = InfoMode_start_pending_paste,
    .pending_paste = InfoMode_pending_paste,
    .draw_paste = InfoMode_draw_paste,
    .cancel_paste = InfoMode_cancel_paste,

    .wipe_ghost = InfoMode_wipe_ghost,
  };
  editor->mode_functions = &info_mode_fns;

  InfoPropDboxes_init(&mode_data->prop_dboxes, editor);
  Editor_display_msg(editor, msgs_lookup("StatusInfoMode"), false);
  return true;
}

void InfoMode_free_clipboard(void)
{
  if (clipboard) {
    dfile_release(InfoTransfer_get_dfile(clipboard));
    clipboard = NULL;
  }
}

bool InfoMode_write_clipboard(struct Writer *writer,
                              DataType data_type,
                              char const *filename)
{
  NOT_USED(data_type);
  return !report_error(write_compressed(InfoTransfer_get_dfile(clipboard), writer), filename, "");
}

int InfoMode_estimate_clipboard(DataType data_type)
{
  NOT_USED(data_type);
  return worst_compressed_size(InfoTransfer_get_dfile(clipboard));
}

bool InfoMode_set_properties(Editor *editor, TargetInfo *const info, char const *strings[static TargetInfoTextIndex_Count])
{
  InfoModeData *const mode_data = get_mode_data(editor);
  InfoEditChanges_init(&mode_data->change_info);
  SFError err = InfoEdit_set_texts(info, strings, &mode_data->change_info);
  changed_with_msg(editor);
  return !report_error(err, "", "");
}
