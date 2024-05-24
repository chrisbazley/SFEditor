/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ships/flightpaths editing mode
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
#include <assert.h>

#include "toolbox.h"

#include "Macros.h"
#include "err.h"
#include "msgtrans.h"
#include "debug.h"

#include "MapCoord.h"
#include "EditorData.h"
#include "EditMode.h"
#include "Session.h"
#include "Mission.h"
#include "Utils.h"
#include "DataType.h"
#include "EditWin.h"

typedef struct
{
  char ships_selected[32];
  int  current_tool;
}
ShipsModeData;

/* ---------------- Private functions ---------------- */

static inline ShipsModeData *get_mode_data(Editor const *const editor)
{
  assert(Editor_get_edit_mode(editor) == EDITING_MODE_SHIPS);
  assert(editor->editingmode_data);
  return editor->editingmode_data;
}

/* ---------------- Public functions ---------------- */

static size_t ShipsMode_num_selected(Editor const *const editor)
{
  ShipsModeData *const mode_data = get_mode_data(editor);

  return 0;
}

static size_t ShipsMode_max_selected(Editor const *const editor)
{
  return ships_get_count(mission_get_ships(Session_get_mission(Editor_get_session(editor))));
}

static bool ShipsMode_auto_select(Editor *const editor, MapPoint const fine_pos, EditWin *const edit_win)
{
  return false;
}

static void ShipsMode_auto_deselect(Editor *const editor)
{
}

static int ShipsMode_misc_event(Editor *const editor,
  int const event_code)
{
  return 0; /* not interested */
}

static void ShipsMode_leave(Editor *const editor)
{
  free(editor->editingmode_data);
}

static void ShipsMode_draw_grid(Vertex const map_origin,
  MapArea const *redraw_area, EditWin const *edit_win)
{
  PaletteEntry const colour = EditWin_get_grid_colour(edit_win);
  int const zoom = EditWin_get_zoom(edit_win);
}

void ShipsMode_draw(Editor *const editor, Vertex const map_origin,
  MapArea const *redraw_area, int const zoom)
{
  /* Process redraw rectangle */
}

static MapPoint ShipsMode_map_to_grid_coords(MapPoint map_coords,
  EditWin const *const edit_win)
{
  /* Convert generic map coordinates to ship coordinates
  (2^19 = 524288 units per ground map texel) */
  MapPoint const grid_pos = {
    map_coords.x * 2,
    map_coords.y * 2
  };
  return grid_pos;
}

static void ShipsMode_draw_numbers(Editor *const editor,
  Vertex const scr_orig, MapArea const *const redraw_area,
  EditWin const *const edit_win)
{
  ViewDisplayFlags const display_flags = EditWin_get_display_flags(edit_win);
  int const zoom = EditWin_get_zoom(edit_win);
  PaletteEntry const bg_colour = EditWin_get_bg_colour(edit_win);
}

bool ShipsMode_can_enter(Editor *const editor)
{
  EditSession *const session = Editor_get_session(editor);

  return Session_has_data(session, DataType_Mission);
}

bool ShipsMode_enter(Editor *const editor)
{
  DEBUG("Entering ships mode");
  assert(ShipsMode_can_enter(editor));

  ShipsModeData *const mode_data = malloc(sizeof(ShipsModeData));
  if (mode_data == NULL) {
    report_error(SFERROR(NoMem), "", "");
    return false;
  }

  editor->editingmode_data = mode_data;

  static DataType const type_list[] = {DataType_Count};

  static EditModeFuncts const ships_mode_fns = {
    .export_data_types = type_list,
    .auto_select = ShipsMode_auto_select,
    .auto_deselect = ShipsMode_auto_deselect,
    .misc_event = ShipsMode_misc_event,
    .draw_grid = ShipsMode_draw_grid,
    .leave = ShipsMode_leave,
    .draw_numbers = ShipsMode_draw_numbers,
    .map_to_grid_coords = ShipsMode_map_to_grid_coords,
    .num_selected = ShipsMode_num_selected,
    .max_selected = ShipsMode_max_selected,
  };
  editor->mode_functions = &ships_mode_fns;

  Editor_display_msg(editor, msgs_lookup("StatusShipMode"), false);

  return true;
}
