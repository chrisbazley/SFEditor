/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  An editor instance
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

#include "stdlib.h"
#include <ctype.h>
#include "stdio.h"

#include "err.h"

#include "msgtrans.h"

#include "Writer.h"
#include "Reader.h"
#include "EditorData.h"
#include "MapMode.h"
#include "ObjectsMode.h"
#include "ShipsMode.h"
#include "InfoMode.h"
#include "Palette.h"
#include "EditWin.h"
#include "Session.h"
#include "DataType.h"
#include "FilePaths.h"
#include "Config.h"
#include "OurEvents.h"
#include "MapToolbar.h"
#include "Obj.h"

static void set_tool_msg(Editor *const editor)
{
  Editor_display_msg(editor, msgs_lookup_subn("StatusToolSel", 1,
                     Editor_get_tool_msg(editor, EDITORTOOL_NONE, true)), false);
}

static void clear_vertices(Editor *const editor)
{
  assert(editor);
  assert((size_t)editor->vertices_set <= ARRAY_SIZE(editor->vertex));
  editor->vertices_set = 0;
}

static void set_vertex(Editor *const editor)
{
  assert(editor);
  assert((size_t)editor->vertices_set < ARRAY_SIZE(editor->vertex));
  editor->vertex[editor->vertices_set++] = editor->map_pos;
}

static void vertex_msg(Editor *const editor)
{
  assert(editor);
  if (editor->vertices_set < 1 || editor->vertices_set > 2) {
    return;
  }

  char coords_str[12];
  sprintf(coords_str, "%3.3" PRIMapCoord ",%3.3" PRIMapCoord,
    editor->vertex[0].x, editor->vertex[0].y);

  if (editor->vertices_set > 1) {
    assert(editor->shape_to_plot == PLOTSHAPE_TRIANGLE);

    char coords_str_2[12];
    sprintf(coords_str_2, "%3.3" PRIMapCoord ",%3.3" PRIMapCoord,
      editor->vertex[1].x, editor->vertex[1].y);

    Editor_display_msg(editor, msgs_lookup_subn("StatusTri2", 2,
                        coords_str, coords_str_2), false);

  } else if (editor->shape_to_plot == PLOTSHAPE_CIRCLE) {
    char radius_str[12];
    sprintf(radius_str, "%" PRIMapCoord,
             MapPoint_dist(editor->vertex[0], editor->map_pos));

    Editor_display_msg(editor, msgs_lookup_subn("StatusCirc", 2,
      coords_str, radius_str), false);
  } else {
    char const *token = "";
    switch (editor->shape_to_plot) {
      case PLOTSHAPE_TRIANGLE:
        token = "StatusTri1";
        break;

      case PLOTSHAPE_RECTANGLE:
        token = "StatusRect";
        break;

      case PLOTSHAPE_LINE:
        token = "StatusLine";
        break;

      default:
        assert(false);
        break;
    }
    Editor_display_msg(editor, msgs_lookup_subn(token, 1, coords_str), false);
  }
}

static char *get_shapes_help_msg(Editor const *const editor)
{
  assert(editor);
  char const *help_msg_token = "";

  switch (editor->shape_to_plot) {
    case PLOTSHAPE_LINE:
      help_msg_token = (editor->vertices_set ? "PlotLineB" :
                       "PlotLineA");
      break;

    case PLOTSHAPE_CIRCLE:
      help_msg_token = (editor->vertices_set ? "PlotCircleB" :
                       "PlotCircleA");
      break;

    case PLOTSHAPE_TRIANGLE:
      switch (editor->vertices_set) {
        case 0:
          help_msg_token = "PlotTriangleA";
          break;

        case 1:
          help_msg_token = "PlotTriangleB";
          break;

        default:
          help_msg_token = "PlotTriangleC";
          break;
      }
      break;

    case PLOTSHAPE_RECTANGLE:
      help_msg_token = (editor->vertices_set ? "PlotRectangleB" :
                       "PlotRectangleA");
      break;

    default:
      return ""; /* unknown plot type */
  }
  return msgs_lookup(help_msg_token);
}

static void selection_size_msg(Editor *const editor, char const *const token)
{
  size_t const num_selected = Editor_num_selected(editor);
  DEBUG("%zu map locations are selected", num_selected);

  char count_str[12];
  sprintf(count_str, "%zu", num_selected);
  Editor_display_msg(editor, msgs_lookup_subn(token, 1, count_str), false);
}

static void disp_selection_size(Editor *const editor)
{
  selection_size_msg(editor, "StatusSelect");
}

static void disp_drag_size(Editor *const editor)
{
  selection_size_msg(editor, "StatusDrag");
}

static bool select_mode_with_fallback(Editor *const editor, EditMode mode)
{
  if (!Editor_can_set_edit_mode(editor, mode)) {
    for (mode = EDITING_MODE_FIRST;
         mode < EDITING_MODE_COUNT;
         ++mode) {
      if (Editor_can_set_edit_mode(editor, mode)) {
        break;
      }
    }
    if (mode == EDITING_MODE_COUNT) {
      mode = EDITING_MODE_NONE;
    }
  }

  return Editor_set_edit_mode(editor, mode, NULL);
}

static void select_tool_with_fallback(Editor *const editor, EditorTool tool)
{
  if (!Editor_can_select_tool(editor, tool)) {
    for (tool = EDITORTOOL_FIRST; tool < EDITORTOOL_COUNT; ++tool) {
      if (Editor_can_select_tool(editor, tool)) {
        break;
      }
    }
    if (tool == EDITORTOOL_COUNT) {
      tool = EDITING_MODE_NONE;
    }
  }

  Editor_select_tool(editor, tool);
}

/* ---------------- Public functions ---------------- */

bool Editor_init(Editor *const editor, EditSession *const session, Editor const *const editor_to_copy)
{
  DEBUG("Creating new editor on editing session %p", (void *)session);

  *editor = (Editor){
    .session = session,
    .editing_mode = EDITING_MODE_NONE,
    .tool = editor_to_copy ? Editor_get_tool(editor_to_copy) : Config_get_default_edit_tool(),
    .show_tool_bar = editor_to_copy ? editor_to_copy->show_tool_bar : Config_get_default_tool_bar_enabled(),
    .show_palette = editor_to_copy ? editor_to_copy->show_palette : Config_get_default_palette_enabled(),
    .global_fill = editor_to_copy ? editor_to_copy->global_fill : Config_get_default_fill_is_global(),
    .shape_to_plot = editor_to_copy ? editor_to_copy->shape_to_plot : Config_get_default_plot_shape(),
    .brush_size = editor_to_copy ? editor_to_copy->brush_size : Config_get_default_brush_size(),
    .wand_size = editor_to_copy ? editor_to_copy->wand_size : Config_get_default_wand_size(),
    .allow_drag_select = false,
  };

  if (!Palette_init(&editor->palette_data, editor)) {
    return false;
  }

  if (!MapToolbar_init(&editor->toolbar, editor)) {
    Palette_destroy(&editor->palette_data);
    return false;
  }

  EditMode mode = editor_to_copy ? Editor_get_edit_mode(editor_to_copy) : Config_get_default_edit_mode();
  return select_mode_with_fallback(editor, mode);
}

void Editor_destroy(Editor *const editor)
{
  assert(editor);
  Editor_set_edit_mode(editor, EDITING_MODE_NONE, NULL);
  MapToolbar_destroy(&editor->toolbar);
  Palette_destroy(&editor->palette_data);
  DEBUG("Editor object %p deleted", (void *)editor);
}

void Editor_update_title(Editor *const editor)
{
  assert(editor);
  Palette_update_title(&editor->palette_data);

  if (editor->mode_functions != NULL &&
      editor->mode_functions->update_title) {
    editor->mode_functions->update_title(editor);
  }
}

void Editor_resource_change(Editor *const editor, EditorChange const event,
  EditorChangeParams const *params)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  switch (event) {
    case EDITOR_CHANGE_TEX_ALL_RELOADED:
    case EDITOR_CHANGE_TEX_SNAKES_RELOADED:
    case EDITOR_CHANGE_TEX_GROUPS_RELOADED:
    case EDITOR_CHANGE_TEX_TRANSFERS_RELOADED:
    case EDITOR_CHANGE_TEX_TRANSFER_DELETED:
    case EDITOR_CHANGE_TEX_TRANSFER_ALL_DELETED:
    case EDITOR_CHANGE_TEX_TRANSFER_ADDED:
    case EDITOR_CHANGE_GFX_ALL_RELOADED:
    case EDITOR_CHANGE_GFX_SNAKES_RELOADED:
      select_tool_with_fallback(editor, editor->tool);
      MapToolbar_update_buttons(&editor->toolbar);
      break;
    case EDITOR_CHANGE_CLOUD_COLOURS:
      ObjectsMode_redraw_clouds(editor);
      break;
    default:
      break;
  }

  if (editor->mode_functions->resource_change) {
    editor->mode_functions->resource_change(editor, event, params);
  }

  Editor_redraw_pending(editor, false);
}

void Editor_auto_select(Editor *const editor, EditWin *const edit_win)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->dragging_select || editor->dragging_obj) {
    return;
  }

  if (editor->mode_functions->auto_select) {
    editor->temp_menu_select = editor->mode_functions->auto_select(editor, editor->fine_pos, edit_win);
    if (editor->temp_menu_select) {
      DEBUGF("Created temporary selection\n");
      disp_selection_size(editor);
      Editor_redraw_pending(editor, false);
    }
  }
}

void Editor_auto_deselect(Editor *const editor)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->dragging_select || editor->dragging_obj) {
    return;
  }

  if (editor->temp_menu_select && editor->mode_functions->auto_deselect) {
    DEBUGF("Destroy temporary selection\n");
    editor->mode_functions->auto_deselect(editor);
    disp_selection_size(editor);
    editor->temp_menu_select = false;
    Editor_redraw_pending(editor, false);
  }
}

static void cancel_select(Editor *const editor, EditWin *const edit_win)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);
  if (!editor->dragging_select) {
    return;
  }

  if (editor->mode_functions->cancel_select) {
    MapArea select_box = {editor->drag_select_start, editor->drag_select_end};
    MapArea_make_valid(&select_box, &select_box);
    editor->mode_functions->cancel_select(editor,
      editor->drag_select_only_inside, &select_box, edit_win);
  }

  editor->dragging_select = false;
}

static void cancel_paste(Editor *const editor)
{
  assert(editor != NULL);
  if (!editor->paste_pending) {
    return;
  }

  if (editor->mode_functions != NULL &&
      editor->mode_functions->cancel_paste) {
    editor->mode_functions->cancel_paste(editor);
  }

  editor->paste_pending = false;
}

static void cancel_drag_obj(Editor *const editor)
{
  assert(editor != NULL);
  if (!editor->dragging_obj) {
    return;
  }

  if (editor->mode_functions != NULL &&
      editor->mode_functions->cancel_drag_obj) {
    editor->mode_functions->cancel_drag_obj(editor);
  }

  editor->dragging_obj = false;
}

static char *get_plot_cancel_msg(Editor *const editor)
{
  assert(editor != NULL);

  char shape_name[24];
  sprintf(shape_name, "Plot%d", editor->shape_to_plot);

  STRCPY_SAFE(shape_name, msgs_lookup(shape_name));
  *shape_name = toupper(*shape_name);

  return msgs_lookup_subn("StatusNoShape", 1, shape_name);
}

static void cancel_plot(Editor *const editor)
{
  assert(editor != NULL);
  if (editor->tool != EDITORTOOL_PLOTSHAPES) {
    return;
  }

  if (editor->shown_pending) {
    if (editor->mode_functions != NULL &&
        editor->mode_functions->cancel_plot) {
      editor->mode_functions->cancel_plot(editor);
    }
    editor->shown_pending = false;
  }

  clear_vertices(editor);
}

void Editor_wipe_ghost(Editor *const editor)
{
  assert(editor != NULL);

  if (editor->shown_pending) {
    if (editor->mode_functions != NULL &&
        editor->mode_functions->wipe_ghost) {
      editor->mode_functions->wipe_ghost(editor);
    }
    editor->shown_pending = false;
  }
}

void Editor_cancel(Editor *const editor, EditWin *const edit_win)
{
  assert(editor != NULL);
  char *msg = NULL;
  switch (editor->tool)
  {
    case EDITORTOOL_SELECT:
      if (editor->paste_pending) {
        cancel_paste(editor);
        msg = msgs_lookup("StatusNoPaste");
      } else if (editor->dragging_select) {
        EditWin_stop_drag_select(edit_win);
        cancel_select(editor, edit_win);
        msg = msgs_lookup("StatusNoSelect");
      } else if (editor->dragging_obj) {
        EditWin_stop_drag_obj(edit_win);
        cancel_drag_obj(editor);
        msg = msgs_lookup("StatusNoDrag");
      }
      break;

    case EDITORTOOL_PLOTSHAPES:
      cancel_plot(editor);
      msg = get_plot_cancel_msg(editor);
      break;

    default:
      break;
  }
  if (msg) {
    Editor_display_msg(editor, msg, false);
  }
  Editor_set_help_and_ptr(editor);
  Editor_redraw_pending(editor, false);
}

int Editor_misc_event(Editor *const editor, int const event_code)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->dragging_select || editor->dragging_obj) {
    return 0;
  }

  return editor->mode_functions->misc_event ?
         editor->mode_functions->misc_event(editor, event_code) :
         0;
}

void Editor_drag_select_ended(Editor *const editor,
  MapArea const *const select_box, EditWin const *const edit_win)
{
  assert(editor != NULL);
  assert(editor->dragging_select);
  assert(editor->mode_functions != NULL);
  assert(MapArea_is_valid(select_box));

  if (editor->mode_functions->update_select) {
    MapArea last_select_box = {editor->drag_select_start, editor->drag_select_end};
    MapArea_make_valid(&last_select_box, &last_select_box);

    editor->mode_functions->update_select(editor,
      editor->drag_select_only_inside, &last_select_box, select_box, edit_win);

    disp_selection_size(editor);
  }

  editor->dragging_select = false;
  Editor_redraw_pending(editor, false);
}

static MapPoint update_pointer(Editor *const editor, MapPoint const pointer_pos, EditWin const *const edit_win)
{
  assert(editor != NULL);
  editor->fine_pos = pointer_pos;
  MapPoint const last_map_pos = editor->map_pos;
  MapPoint const grid_pos = Editor_map_to_grid_coords(editor, pointer_pos, edit_win);
  bool const ptr_moved = !MapPoint_compare(grid_pos, editor->map_pos);
  if (ptr_moved)
  {
    editor->map_pos = grid_pos;
    DEBUG_VERBOSEF("Storing new pointer position %" PRIMapCoord ",%" PRIMapCoord "\n",
      grid_pos.x, grid_pos.y);
  }
  DEBUG("Pointer position (on map grid) is %schanged", ptr_moved ? "" : "un");
  return last_map_pos;
}

static void pending_shape(Editor *const editor)
{
  assert(editor);
  assert(editor->mode_functions);
  assert(editor->tool == EDITORTOOL_PLOTSHAPES);

  if (editor->vertices_set < 1) {
    if (editor->mode_functions->pending_plot) {
      editor->mode_functions->pending_plot(editor, editor->map_pos);
    }
  } else {
    switch (editor->shape_to_plot) {
      case PLOTSHAPE_LINE:
        if (editor->mode_functions->pending_line) {
          editor->mode_functions->pending_line(editor, editor->vertex[0], editor->map_pos);
        }
        break;

      case PLOTSHAPE_RECTANGLE:
        if (editor->mode_functions->pending_rect) {
          editor->mode_functions->pending_rect(editor, editor->vertex[0], editor->map_pos);
        }
        break;

      case PLOTSHAPE_CIRCLE:
        if (editor->mode_functions->pending_circ) {
          editor->mode_functions->pending_circ(editor, editor->vertex[0], editor->map_pos);
        }
        vertex_msg(editor);
        break;

      default:
        assert(editor->shape_to_plot == PLOTSHAPE_TRIANGLE);
        if (editor->vertices_set < 2) {
          if (editor->mode_functions->pending_line) {
            editor->mode_functions->pending_line(editor, editor->vertex[0], editor->map_pos);
         }
        } else {
          if (editor->mode_functions->pending_tri) {
            editor->mode_functions->pending_tri(editor, editor->vertex[0], editor->vertex[1], editor->map_pos);
          }
        }
        break;
    }
  }
  editor->shown_pending = true;
  Editor_redraw_pending(editor, true);
}

bool Editor_pointer_update(Editor *const editor,
  MapPoint const pointer_pos, int const button_held, EditWin const *const edit_win)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  DEBUG("Mouse pointer update %" PRIMapCoord ",%" PRIMapCoord " (buttons %d)",
        pointer_pos.x, pointer_pos.y, button_held);

  MapPoint const last_fine_pos = editor->fine_pos;
  MapPoint const last_map_pos = update_pointer(editor, pointer_pos, edit_win);
  bool const ptr_moved = !MapPoint_compare(last_map_pos, editor->map_pos);
#if COLLISION_BBOX_IS_SELECTION_BBOX
  bool const fine_moved = ptr_moved;
#else
  bool const fine_moved = !MapPoint_compare(last_fine_pos, editor->fine_pos);
#endif
  bool auto_scroll = false;

  switch (editor->tool) {
    case EDITORTOOL_PLOTSHAPES:
      if (ptr_moved || !editor->shown_pending) {
        pending_shape(editor);
      }
      break;

    case EDITORTOOL_TRANSFER:
      if (ptr_moved || !editor->shown_pending) {
        if (editor->mode_functions->pending_transfer) {
          editor->mode_functions->pending_transfer(editor, editor->map_pos);
          editor->shown_pending = true;
          Editor_redraw_pending(editor, true);
        }
      }
      break;

   case EDITORTOOL_SMOOTHWAND:
     if (TEST_BITS(button_held, BUTTONS_DRAG(Wimp_MouseButtonSelect))) {
       if (ptr_moved && editor->mode_functions->draw_smooth) {
         editor->mode_functions->draw_smooth(editor, editor->wand_size, last_map_pos, editor->map_pos);
         editor->shown_pending = false;
         Session_redraw_pending(editor->session, true);
       }
       auto_scroll = true;
     } else {
       if (ptr_moved || !editor->shown_pending) {
         if (editor->mode_functions->pending_smooth) {
           editor->mode_functions->pending_smooth(editor, editor->wand_size, editor->map_pos);
           Editor_redraw_pending(editor, true);
           editor->shown_pending = true;
         }
       }
     }
     break;

   case EDITORTOOL_BRUSH:
     if (TEST_BITS(button_held, BUTTONS_DRAG(Wimp_MouseButtonSelect))) {
       if (ptr_moved && editor->mode_functions->draw_brush) {
         editor->mode_functions->draw_brush(editor, editor->brush_size, last_map_pos, editor->map_pos);
         editor->shown_pending = false;
         Session_redraw_pending(editor->session, true);
       }
       auto_scroll = true;
     } else {
       if (ptr_moved || !editor->shown_pending) {
         if (editor->mode_functions->pending_brush) {
           editor->mode_functions->pending_brush(editor, editor->brush_size, editor->map_pos);
           Editor_redraw_pending(editor, true);
           editor->shown_pending = true;
         }
       }
     }
     break;

   case EDITORTOOL_FILLREPLACE:
     if (editor->global_fill) {
       if (fine_moved || !editor->shown_pending) {
         if (editor->mode_functions->pending_global_replace) {
           editor->mode_functions->pending_global_replace(editor, editor->fine_pos, editor->map_pos, edit_win);
           editor->shown_pending = true;
           Editor_redraw_pending(editor, true);
         }
       }
     } else {
       if (fine_moved || !editor->shown_pending) {
         if (editor->mode_functions->pending_flood_fill) {
           editor->mode_functions->pending_flood_fill(editor, editor->fine_pos, editor->map_pos, edit_win);
           editor->shown_pending = true;
           Editor_redraw_pending(editor, true);
         }
       }
     }
     break;

   case EDITORTOOL_SAMPLER:
     if (ptr_moved || !editor->shown_pending) {
       if (editor->mode_functions->pending_sample_obj) {
         editor->mode_functions->pending_sample_obj(editor, editor->map_pos);
         editor->shown_pending = true;
       }
     }
     break;

   case EDITORTOOL_SNAKE:
     if (TEST_BITS(button_held, BUTTONS_DRAG(Wimp_MouseButtonAdjust | Wimp_MouseButtonSelect))) {
       if (ptr_moved && editor->mode_functions->draw_snake) {
         editor->mode_functions->draw_snake(editor, editor->map_pos);
         editor->shown_pending = false;
         Session_redraw_pending(editor->session, true);
       }
       auto_scroll = true;
     } else {
       if (ptr_moved || !editor->shown_pending) {
         if (editor->mode_functions->pending_snake) {
           editor->mode_functions->pending_snake(editor, editor->map_pos);
           editor->shown_pending = true;
           Editor_redraw_pending(editor, true);
         }
       }
     }
     break;

   case EDITORTOOL_SELECT:
     if (editor->paste_pending) {
       if (ptr_moved || !editor->shown_pending) {
         if (editor->mode_functions->pending_paste) {
           editor->mode_functions->pending_paste(editor, editor->map_pos);
           editor->shown_pending = true;
           Editor_redraw_pending(editor, true);
         }
       }
       break;
     }

     if (editor->dragging_select) {
       DEBUG("A drag is in progress");
       if (fine_moved && editor->mode_functions->update_select) {
         DEBUG("Calling update drag function");

         /* Don't use last_fine_pos here because it isn't necessarily correct for this purpose.
            Regardless, it's more robust to store the selection rectangle endpoint separately. */
         MapArea last_select_box = {editor->drag_select_start, editor->drag_select_end};
         MapArea_make_valid(&last_select_box, &last_select_box);

         editor->drag_select_end = editor->fine_pos;
         MapArea select_box = {editor->drag_select_start, editor->drag_select_end};
         MapArea_make_valid(&select_box, &select_box);

         editor->mode_functions->update_select(editor,
           editor->drag_select_only_inside,
           &last_select_box, &select_box, edit_win);

         disp_selection_size(editor);
         Editor_redraw_pending(editor, true);
       }
       auto_scroll = true;
     }
     break;

   default:
     break;
  }

  return auto_scroll;
}

bool Editor_can_draw_grid(Editor *const editor, EditWin const *edit_win)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);
  if (editor->mode_functions->can_draw_grid) {
    return editor->mode_functions->can_draw_grid(editor, edit_win);
  }
  return false;
}

void Editor_draw_grid(Editor *const editor, Vertex const map_origin,
  MapArea const *const redraw_area, EditWin const *const edit_win)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);
  assert(MapArea_is_valid(redraw_area));
  if (editor->mode_functions->draw_grid) {
    editor->mode_functions->draw_grid(map_origin, redraw_area, edit_win);
  }
}

bool Editor_can_draw_numbers(Editor *const editor, EditWin const *edit_win)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);
  if (editor->mode_functions->can_draw_numbers) {
    return editor->mode_functions->can_draw_numbers(editor, edit_win);
  }
  return false;
}

void Editor_draw_numbers(Editor *const editor, Vertex const map_origin,
                     MapArea const *const redraw_area, EditWin const *const edit_win)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);
  assert(MapArea_is_valid(redraw_area));
  if (editor->mode_functions->draw_numbers) {
    editor->mode_functions->draw_numbers(editor, map_origin, redraw_area, edit_win);
  }
}

static void shapes_mouse_select(Editor *const editor)
{
  assert(editor);
  assert(editor->mode_functions);
  assert(editor->tool == EDITORTOOL_PLOTSHAPES);

  set_vertex(editor);

  switch (editor->shape_to_plot) {
    case PLOTSHAPE_LINE:
      if (editor->vertices_set == 2) {
        if (editor->mode_functions->plot_line) {
          editor->mode_functions->plot_line(editor, editor->vertex[0],
            editor->vertex[1]);
          editor->shown_pending = false;
        }
        clear_vertices(editor);
      }
      break;

    case PLOTSHAPE_RECTANGLE:
      if (editor->vertices_set == 2) {
        if (editor->mode_functions->plot_rect) {
          editor->mode_functions->plot_rect(editor, editor->vertex[0],
            editor->vertex[1]);
          editor->shown_pending = false;
        }
        clear_vertices(editor);
      }
      break;

    case PLOTSHAPE_CIRCLE:
      if (editor->vertices_set == 2) {
        if (editor->mode_functions->plot_circ) {
          editor->mode_functions->plot_circ(editor, editor->vertex[0],
            editor->vertex[1]);
          editor->shown_pending = false;
        }
        clear_vertices(editor);
      }
      break;

    default:
      assert(editor->shape_to_plot == PLOTSHAPE_TRIANGLE);
      if (editor->vertices_set == 3) {
        if (editor->mode_functions->plot_tri) {
          editor->mode_functions->plot_tri(editor, editor->vertex[0],
            editor->vertex[1], editor->vertex[2]);
          editor->shown_pending = false;
        }
        clear_vertices(editor);
      }
      break;
  }
  vertex_msg(editor);
  Editor_set_help_and_ptr(editor);
  Session_redraw_pending(editor->session, false);
}

static void shapes_mouse_adjust(Editor *const editor)
{
  assert(editor);
  assert(editor->mode_functions);
  assert(editor->tool == EDITORTOOL_PLOTSHAPES);

  clear_vertices(editor);
  set_vertex(editor);

  if (editor->shown_pending &&
      editor->mode_functions->pending_plot) {
    editor->mode_functions->pending_plot(editor, editor->map_pos);
  }

  vertex_msg(editor);
  Editor_set_help_and_ptr(editor);
  Editor_redraw_pending(editor, true);
}

static void select_mouse_click(Editor *const editor,
  MapPoint const fine_pos, int buttons, bool const shift, EditWin *const edit_win)
{
  assert(editor);
  assert(editor->mode_functions);
  assert(editor->tool == EDITORTOOL_SELECT);

  if (editor->allow_drag_select) {
    /* Begin selection/inversion */
    if (TEST_BITS(buttons,
        BUTTONS_DRAG(Wimp_MouseButtonSelect | Wimp_MouseButtonAdjust))) {
      editor->drag_select_start = editor->drag_select_end =  fine_pos;

      MapArea const initial_box = {
        .min = editor->drag_select_start,
        .max = editor->drag_select_end
      };
      editor->dragging_select = EditWin_start_drag_select(
        edit_win, Wimp_DragBox_DragRubberDash, &initial_box, true);
      return;
    }
  }

  if (TEST_BITS(buttons, BUTTONS_DRAG(Wimp_MouseButtonSelect))) {
    /* Drag selected objects */
    if (editor->mode_functions->start_drag_obj) {
      assert(!editor->dragging_obj);
      editor->dragging_obj = editor->mode_functions->start_drag_obj(editor, fine_pos, edit_win);
      if (editor->dragging_obj) {
        disp_drag_size(editor);
      }
    }
    return;
  }

  if (TEST_BITS(buttons, BUTTONS_SINGLE(Wimp_MouseButtonSelect))) {
    if (editor->paste_pending) {
      if (editor->mode_functions->draw_paste &&
          editor->mode_functions->draw_paste(editor, editor->map_pos)) {
        /* Prevent the paste action from turning into a drag selection
           if the button is held too long */
        editor->allow_drag_select = false;
        editor->paste_pending = false;
        Editor_set_help_and_ptr(editor);
        Session_redraw_pending(editor->session, true);
      }
    } else if (editor->mode_functions->start_exclusive_select) {
      /* Exclusively select object */
      editor->allow_drag_select = editor->mode_functions->start_exclusive_select(
        editor, shift, fine_pos, edit_win);

      if (editor->allow_drag_select) {
        editor->drag_select_only_inside = shift;
      }

      disp_selection_size(editor);
      Editor_redraw_pending(editor, true);
    } else {
      editor->allow_drag_select = false;
    }
    return;
  }

  if (TEST_BITS(buttons, BUTTONS_SINGLE(Wimp_MouseButtonAdjust))) {
    if (editor->paste_pending) {
      /* Prevent the paste action from turning into a drag selection
         if the button is held too long */
      editor->allow_drag_select = false;
    } else if (editor->mode_functions->start_select) {
      /* Select or deselect object */
      editor->allow_drag_select = editor->mode_functions->start_select(
        editor, shift, fine_pos, edit_win);

      if (editor->allow_drag_select) {
        editor->drag_select_only_inside = shift;
      }

      disp_selection_size(editor);
      Editor_redraw_pending(editor, true);
    } else {
      editor->allow_drag_select = false;
    }
    return;
  }

  if (TEST_BITS(buttons, BUTTONS_DOUBLE(Wimp_MouseButtonSelect))) {
    /* Edit object properties */
    if (editor->mode_functions->edit_properties_at_pos) {
      editor->mode_functions->edit_properties_at_pos(editor, fine_pos, edit_win);
    }
    return;
  }
}

bool Editor_mouse_click(Editor *const editor, MapPoint const fine_pos,
  int const buttons, bool const shift, EditWin *const edit_win)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  DEBUG("Mouse click at %" PRIMapCoord ",%" PRIMapCoord " (buttons %d, shift %d)",
        fine_pos.x, fine_pos.y, buttons, shift);

  switch (editor->tool) {
    case EDITORTOOL_SAMPLER:
      if (TEST_BITS(buttons, BUTTONS_CLICK(Wimp_MouseButtonSelect))) {
        if (editor->mode_functions->sample_obj) {
          editor->mode_functions->sample_obj(editor, fine_pos, editor->map_pos, edit_win);
          editor->shown_pending = false;
        }
        return false; /* don't trap mouse pointer */
      }
      break;

    case EDITORTOOL_BRUSH:
      if (TEST_BITS(buttons, BUTTONS_DRAG(Wimp_MouseButtonSelect))) {
        return true;
      }

      if (TEST_BITS(buttons, BUTTONS_CLICK(Wimp_MouseButtonSelect))) {
        if (editor->mode_functions->start_brush) {
          editor->mode_functions->start_brush(editor, editor->brush_size, editor->map_pos);
          editor->shown_pending = false;
          Session_redraw_pending(editor->session, true);
        }
      }
      break;

    case EDITORTOOL_SNAKE:
      if (TEST_BITS(buttons, BUTTONS_DRAG(Wimp_MouseButtonSelect | Wimp_MouseButtonAdjust))) {
        return true;
      }

      if (TEST_BITS(buttons,
          BUTTONS_CLICK(Wimp_MouseButtonSelect | Wimp_MouseButtonAdjust))) {
        if (editor->mode_functions->start_snake) {
          editor->mode_functions->start_snake(editor, editor->map_pos,
            !TEST_BITS(buttons, BUTTONS_CLICK(Wimp_MouseButtonSelect)));
          editor->shown_pending = false;
          Session_redraw_pending(editor->session, true);
        }
      }
      break;

    case EDITORTOOL_SELECT:
      select_mouse_click(editor, fine_pos, buttons, shift, edit_win);
      break;

    case EDITORTOOL_FILLREPLACE:
      if (TEST_BITS(buttons, BUTTONS_CLICK(Wimp_MouseButtonSelect))) {
        if (editor->global_fill) {
          if (editor->mode_functions->global_replace) {
            editor->mode_functions->global_replace(editor, fine_pos, editor->map_pos, edit_win);
          }
        } else {
          if (editor->mode_functions->flood_fill) {
            editor->mode_functions->flood_fill(editor, fine_pos, editor->map_pos, edit_win);
          }
        }
        editor->shown_pending = false;
        Session_redraw_pending(editor->session, false);
      }
      break;

    case EDITORTOOL_PLOTSHAPES:
      if (TEST_BITS(buttons, BUTTONS_CLICK(Wimp_MouseButtonSelect))) {
        shapes_mouse_select(editor);
      } else if (TEST_BITS(buttons, BUTTONS_CLICK(Wimp_MouseButtonAdjust))) {
        shapes_mouse_adjust(editor);
      }
      break;

    case EDITORTOOL_SMOOTHWAND:
      if (TEST_BITS(buttons, BUTTONS_DRAG(Wimp_MouseButtonSelect))) {
        return true;
      }

      if (TEST_BITS(buttons, BUTTONS_CLICK(Wimp_MouseButtonSelect))) {
        if (editor->mode_functions->start_smooth) {
          editor->mode_functions->start_smooth(editor, editor->wand_size, editor->map_pos);
          editor->shown_pending = false;
          Session_redraw_pending(editor->session, true);
        }
      }
      break;

    case EDITORTOOL_TRANSFER:
    /*case EDITORTOOL_CBPASTE:*/
      if (TEST_BITS(buttons, BUTTONS_CLICK(Wimp_MouseButtonSelect))) {
        if (editor->mode_functions->draw_transfer) {
          editor->mode_functions->draw_transfer(editor, editor->map_pos);
          editor->shown_pending = false;
        }
      }
      break;

    default:
      break;
  }
  return false;
}

MapArea Editor_map_to_grid_area(Editor const *const editor,
  MapArea const *const map_area, EditWin const *const edit_win)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);
  return editor->mode_functions->map_to_grid_area ?
         editor->mode_functions->map_to_grid_area(map_area, edit_win) :
         *map_area;
}

MapPoint Editor_map_to_grid_coords(Editor const *const editor,
  MapPoint const map_coords, EditWin const *const edit_win)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);
  return editor->mode_functions->map_to_grid_coords ?
         editor->mode_functions->map_to_grid_coords(map_coords, edit_win) :
         map_coords;
}

MapPoint Editor_grid_to_map_coords(Editor const *const editor,
  MapPoint const grid_coords, EditWin const *const edit_win)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);
  return editor->mode_functions->grid_to_map_coords ?
         editor->mode_functions->grid_to_map_coords(grid_coords, edit_win) :
         grid_coords;
}

size_t Editor_num_selected(Editor const *const editor)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);
  return editor->mode_functions->num_selected ?
         editor->mode_functions->num_selected(editor) :
         0;
}

size_t Editor_max_selected(Editor const *const editor)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);
  return editor->mode_functions->num_selected ?
         editor->mode_functions->max_selected(editor) :
         0;
}

char const *Editor_get_mode_name(Editor const *const editor)
{
  assert(editor != NULL);
  static char const *const tokens[] = {
    [EDITING_MODE_MAP] = "EMMap",
    [EDITING_MODE_OBJECTS] = "EMObj",
    [EDITING_MODE_SHIPS] = "EMShi",
    [EDITING_MODE_INFO] = "EMInf",
  };
  // Norcroft trashes this function, given an opportunity
  if (editor->editing_mode >= 0 &&
      (size_t)editor->editing_mode < ARRAY_SIZE(tokens)) {
    return msgs_lookup(tokens[editor->editing_mode]);
  }
  return msgs_lookup("EMNon");
}

char *Editor_get_help_msg(Editor const *const editor)
{
  assert(editor != NULL);

  switch (editor->tool) {
  case EDITORTOOL_FILLREPLACE:
    return msgs_lookup(editor->global_fill ? "GlobalFill" : "FloodFill");

  case EDITORTOOL_PLOTSHAPES:
    return get_shapes_help_msg(editor);

  case EDITORTOOL_MAGNIFIER:
    return msgs_lookup("MapMagnify");
    break;

  default:
    return (editor->mode_functions != NULL &&
            editor->mode_functions->get_help_msg) ?
            editor->mode_functions->get_help_msg(editor) : NULL;
  }
}

EditMode Editor_get_edit_mode(Editor const *const editor)
{
  assert(editor != NULL);
  return editor->editing_mode;
}

static void set_coord_field_width(Editor *const editor)
{
  if (editor->mode_functions == NULL) {
    return;
  }

  MapPoint grid_pos = editor->mode_functions->coord_limit;

  editor->coord_field_width = 0;
  while (grid_pos.x > 0) {
    grid_pos.x /= 10;
    editor->coord_field_width++;
  }
}

int Editor_get_coord_field_width(Editor const *const editor)
{
  int coord_field_width = 0;
  if (Editor_get_edit_mode(editor) != EDITING_MODE_NONE) {
    coord_field_width = editor->coord_field_width;
  }
  return coord_field_width;
}

MapPoint Editor_get_coord_limit(Editor const *const editor)
{
  if (editor->mode_functions == NULL) {
    return (MapPoint){0,0};
  }
  return editor->mode_functions->coord_limit;
}

bool Editor_can_set_edit_mode(Editor *const editor, EditMode const new_mode)
{
  assert(editor != NULL);

  bool can_enter = false;
  switch (new_mode) {
  case EDITING_MODE_MAP:
    can_enter = MapMode_can_enter(editor);
    break;
  case EDITING_MODE_OBJECTS:
    can_enter = ObjectsMode_can_enter(editor);
    break;
  case EDITING_MODE_SHIPS:
    can_enter = ShipsMode_can_enter(editor);
    break;
  case EDITING_MODE_INFO:
    can_enter = InfoMode_can_enter(editor);
    break;
  case EDITING_MODE_NONE:
    can_enter = true;
    break;
  default:
    break;
  }
  return can_enter;
}

bool Editor_set_edit_mode(Editor *const editor, EditMode const new_mode, EditWin *const edit_win)
{
  assert(Editor_can_set_edit_mode(editor, new_mode));

  if (new_mode == editor->editing_mode)
  {
    return true; /* nothing to do */
  }

  EditorTool const tool = editor->tool;
  Editor_select_tool(editor, EDITORTOOL_NONE);

  if (editor->editing_mode != EDITING_MODE_NONE)
  {
    assert(editor->mode_functions != NULL);
    editor->mode_functions->leave(editor);
    editor->mode_functions = NULL;
  }

  bool success = false;
  switch (new_mode) {
  case EDITING_MODE_MAP:
    success = MapMode_enter(editor);
    break;
  case EDITING_MODE_OBJECTS:
    success = ObjectsMode_enter(editor);
    break;
  case EDITING_MODE_SHIPS:
    success = ShipsMode_enter(editor);
    break;
  case EDITING_MODE_INFO:
    success = InfoMode_enter(editor);
    break;
  case EDITING_MODE_NONE:
    break;
  default:
    break;
  }

  editor->editing_mode = success ? new_mode : EDITING_MODE_NONE;

  if (success)
  {
    MapToolbar_update_buttons(&editor->toolbar);
    set_coord_field_width(editor);
    select_tool_with_fallback(editor, tool);
  }
  else
  {
    assert(editor->mode_functions == NULL);
  }

#if PER_VIEW_SELECT
  if (edit_win) {
    if (success) {
      Editor_set_tools_shown(editor, editor->show_tool_bar, edit_win);
    }
    EditWin_display_mode(edit_win);
  }
#else
  NOT_USED(edit_win);
  Session_display_mode(editor->session);
#endif

  editor->can_paste = false;

  return success;
}

bool Editor_get_tools_shown(Editor const *const editor)
{
  assert(editor != NULL);
  return editor->show_tool_bar;
}

void Editor_set_tools_shown(Editor *const editor, bool const shown, EditWin *const edit_win)
{
  assert(editor != NULL);
  if (shown) {
    MapToolbar_reveal(&editor->toolbar, edit_win);
  } else {
    MapToolbar_hide(&editor->toolbar);
  }
  editor->show_tool_bar = shown;
}

bool Editor_get_pal_shown(Editor const *const editor)
{
  assert(editor != NULL);
  return editor->show_palette;
}

void Editor_set_pal_shown(Editor *const editor, bool const shown, EditWin *const edit_win)
{
  assert(editor != NULL);
  if (shown) {
    Palette_show(&editor->palette_data, edit_win);
  } else {
    Palette_hide(&editor->palette_data);
  }
  editor->show_palette = shown;
}

void Editor_pal_was_hidden(Editor *const editor)
{
  assert(editor != NULL);
  editor->show_palette = false;
}

void Editor_reveal_palette(Editor *const editor)
{
  editor->show_palette = true;
  Palette_reveal(&editor->palette_data);
}

void Editor_display_msg(Editor *const editor, const char *hint, bool temp)
{
  assert(editor != NULL);
#if PER_VIEW_SELECT
  EditWin *const edit_win = Session_editor_to_win(editor);
  if (edit_win) {
    EditWin_display_hint(edit_win, hint);
  }
#else
  Session_display_msg(editor->session, hint, temp);
#endif
}

EditSession *Editor_get_session(Editor const *editor)
{
  assert(editor != NULL);
  return editor->session;
}

void Editor_set_help_and_ptr(Editor *const editor)
{
  assert(editor != NULL);
  char *const help = Editor_get_help_msg(editor);
  PointerType const ptr = Editor_get_ptr_type(editor);

#if PER_VIEW_SELECT
  EditWin *const edit_win = Session_editor_to_win(editor);
  if (edit_win) {
    EditWin_set_help_and_ptr(edit_win, help, ptr);
  }
#else
  Session_set_help_and_ptr(editor->session, help, ptr);
#endif
}

PointerType Editor_get_ptr_type(Editor const *const editor)
{
  EditorTool const tool = editor->tool;
  static PointerType const table[] = {
    [EDITORTOOL_BRUSH] = Pointer_Brush,
    [EDITORTOOL_FILLREPLACE] = Pointer_Fill,
    [EDITORTOOL_PLOTSHAPES] = Pointer_Crosshair,
    [EDITORTOOL_SAMPLER] = Pointer_Sample,
    [EDITORTOOL_SNAKE] = Pointer_Snake,
    [EDITORTOOL_SMOOTHWAND] = Pointer_Wand,
    [EDITORTOOL_TRANSFER] = Pointer_Paste,
    [EDITORTOOL_SELECT] = Pointer_Standard,
    [EDITORTOOL_MAGNIFIER] = Pointer_Zoom,
  };
  return (tool >= 0 && (size_t)tool < ARRAY_SIZE(table)) ? table[tool] : Pointer_Standard;
}

bool Editor_can_clip_overlay(Editor const *const editor)
{
  bool can_clip = false;
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->mode_functions->can_clip_overlay) {
    can_clip = editor->mode_functions->can_clip_overlay(editor);
  }
  DEBUGF("%s clip overlay\n", can_clip ? "Can" : "Can't");
  return can_clip;
}

void Editor_clip_overlay(Editor *const editor)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->dragging_select || editor->dragging_obj) {
    return;
  }

  if (editor->mode_functions->clip_overlay) {
    editor->mode_functions->clip_overlay(editor);

    /* Although the filtered data should be unchanged we may be
       showing numbers or not showing all layers */
    Session_redraw_pending(editor->session, false);
  }
}

void Editor_paint_selected(Editor *editor)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->dragging_select || editor->dragging_obj) {
    return;
  }

  if (editor->mode_functions->paint_selected) {
    editor->mode_functions->paint_selected(editor);
    Session_redraw_pending(editor->session, false);
  }
}

bool Editor_anim_is_selected(Editor const *const editor)
{
  bool anim_is_selected = false;
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->mode_functions->anim_is_selected) {
    anim_is_selected = editor->mode_functions->anim_is_selected(editor);
  }
  DEBUGF("Animation %s selected\n", anim_is_selected ? "is" : "isn't");
  return anim_is_selected;
}

bool Editor_can_edit_properties(Editor const *const editor)
{
  bool can_edit_properties = false;
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->mode_functions->can_edit_properties) {
    can_edit_properties = editor->mode_functions->can_edit_properties(editor);
  }
  DEBUGF("%s edit properties\n", can_edit_properties ? "Can" : "Can't");
  return can_edit_properties;
}

void Editor_edit_properties(Editor *const editor, EditWin *const edit_win)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->dragging_select || editor->dragging_obj) {
    return;
  }

  if (editor->mode_functions->edit_properties) {
    editor->mode_functions->edit_properties(editor, edit_win);
  }
}

bool Editor_can_create_transfer(Editor const *const editor)
{
  bool can_create_transfer = false;
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->mode_functions->can_create_transfer) {
    can_create_transfer = editor->mode_functions->can_create_transfer(editor);
  }
  DEBUGF("%s create transfer\n", can_create_transfer ? "Can" : "Can't");
  return can_create_transfer;
}

void Editor_create_transfer(Editor *const editor, const char *const name)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->dragging_select || editor->dragging_obj) {
    return;
  }

  if (editor->mode_functions->create_transfer) {
    editor->mode_functions->create_transfer(editor, name);
  }
}

bool Editor_trigger_is_selected(Editor const *const editor)
{
  bool trigger_is_selected = false;
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->mode_functions->trigger_is_selected) {
    trigger_is_selected = editor->mode_functions->trigger_is_selected(editor);
  }
  DEBUGF("Trigger %s selected\n", trigger_is_selected ? "is" : "isn't");
  return trigger_is_selected;
}

bool Editor_can_delete(Editor const *const editor)
{
  bool can_delete = false;
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->mode_functions->can_delete) {
    can_delete = editor->mode_functions->can_delete(editor);
  }
  DEBUGF("%s delete\n", can_delete ? "Can" : "Can't");
  return can_delete;
}

bool Editor_can_select_tool(Editor const *const editor, EditorTool const tool)
{
  bool can_select_tool = false;
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->mode_functions->can_select_tool) {
    can_select_tool = editor->mode_functions->can_select_tool(editor, tool);
  }
  DEBUGF("%s select tool %d\n", can_select_tool ? "Can" : "Can't", (int)tool);
  return can_select_tool;
}

void Editor_select_tool(Editor *const editor, EditorTool const tool)
{
  assert(editor != NULL);

  DEBUGF("Selecting tool %d\n", (int)tool);
  if (tool == editor->tool) {
    return;
  }

  cancel_paste(editor);
  cancel_plot(editor);
  cancel_drag_obj(editor);

  editor->tool = tool;
  editor->allow_drag_select = false;
  editor->shown_pending = false;

  if (editor->mode_functions &&
      editor->mode_functions->tool_selected) {
    editor->mode_functions->tool_selected(editor);
  }

  if (tool != EDITORTOOL_NONE) {
    MapToolbar_tool_selected(&editor->toolbar, editor->tool);
    set_tool_msg(editor);
  }

  Editor_redraw_pending(editor, false);
  Editor_set_help_and_ptr(editor);
}

EditorTool Editor_get_tool(Editor const *const editor)
{
  assert(editor != NULL);
  EditorTool const tool = editor->tool;
  DEBUGF("Tool %d is selected\n", (int)tool);
  return tool;
}

bool Editor_can_replace(Editor const *const editor)
{
  bool can_replace = false;
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->mode_functions->can_replace) {
    can_replace = editor->mode_functions->can_replace(editor);
  }
  DEBUGF("%s replace selected\n", can_replace ? "Can" : "Can't");
  return can_replace;
}

bool Editor_can_smooth(Editor const *const editor)
{
  bool can_smooth = false;
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->mode_functions->can_smooth) {
    can_smooth = editor->mode_functions->can_smooth(editor);
  }
  DEBUGF("%s smooth selected\n", can_smooth ? "Can" : "Can't");
  return can_smooth;
}

static EditMode clipboard_mode;

int Editor_estimate_clipboard(DataType const data_type)
{
  int size = 0;

  switch (clipboard_mode) {
    case EDITING_MODE_MAP:
      size = MapMode_estimate_clipboard(data_type);
      break;
    case EDITING_MODE_OBJECTS:
      size = ObjectsMode_estimate_clipboard(data_type);
      break;
    case EDITING_MODE_INFO:
      return InfoMode_estimate_clipboard(data_type);
    default:
      break;
  }

  return size;
}

bool Editor_write_clipboard(Writer *const writer, DataType const data_type,
  char const *const filename)
{
  switch (clipboard_mode) {
    case EDITING_MODE_MAP:
      return MapMode_write_clipboard(writer, data_type, filename);
    case EDITING_MODE_OBJECTS:
      return ObjectsMode_write_clipboard(writer, data_type, filename);
    case EDITING_MODE_INFO:
      return InfoMode_write_clipboard(writer, data_type, filename);
    default:
      break;
  }

  return false;
}

void Editor_free_clipboard(void)
{
  switch (clipboard_mode) {
    case EDITING_MODE_MAP:
      MapMode_free_clipboard();
      break;
    case EDITING_MODE_OBJECTS:
      ObjectsMode_free_clipboard();
      break;
    case EDITING_MODE_INFO:
      InfoMode_free_clipboard();
      break;
    default:
      break;
  }

  clipboard_mode = EDITING_MODE_NONE;
}

void Editor_select_all(Editor *const editor)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->dragging_select || editor->dragging_obj) {
    return;
  }

  if (editor->mode_functions->select_all) {
    editor->mode_functions->select_all(editor);
    disp_selection_size(editor);
    editor->temp_menu_select = false;
    Editor_redraw_pending(editor, false);
    DEBUGF("Selected all\n");
  }
}

void Editor_clear_selection(Editor *const editor)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->dragging_select || editor->dragging_obj) {
    return;
  }

  if (editor->mode_functions->clear_selection) {
    editor->mode_functions->clear_selection(editor);
    disp_selection_size(editor);
    editor->temp_menu_select = false;
    Editor_redraw_pending(editor, false);
  }
}

void Editor_delete(Editor *const editor)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->dragging_select || editor->dragging_obj) {
    return;
  }

  if (editor->mode_functions->delete) {
    editor->mode_functions->delete(editor);
    editor->temp_menu_select = false;
    Session_redraw_pending(editor->session, false);
  }
}

bool Editor_cut(Editor *const editor)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->dragging_select || editor->dragging_obj) {
    return false;
  }

  if (editor->mode_functions->cut && editor->mode_functions->cut(editor)) {
    editor->temp_menu_select = false;
    clipboard_mode = editor->editing_mode;
    Session_redraw_pending(editor->session, false);
    return true;
  }

  return false;
}

bool Editor_copy(Editor *const editor)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->dragging_select || editor->dragging_obj) {
    return false;
  }

  if (editor->mode_functions->copy && editor->mode_functions->copy(editor)) {
    clipboard_mode = editor->editing_mode;
    return true;
  }

  return false;
}

bool Editor_start_pending_paste(Editor *const editor, Reader *const reader,
  int const estimated_size, DataType const data_type,
  char const *const filename)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->dragging_select || editor->dragging_obj) {
    return false;
  }

  Editor_select_tool(editor, EDITORTOOL_SELECT);
  cancel_paste(editor);

  bool success = false;
  if (editor->mode_functions->start_pending_paste) {
    success = editor->mode_functions->start_pending_paste(
                  editor, reader, estimated_size, data_type, filename);
  }

  if (success) {
    Editor_display_msg(editor, msgs_lookup("StatusPaste"), false);
  }

  editor->paste_pending = success;
  Editor_set_help_and_ptr(editor);
  Editor_redraw_pending(editor, false);

  return success;
}

void Editor_cancel_drag_obj(Editor *const editor)
{
  assert(editor != NULL);
  assert(editor->dragging_obj);
  assert(editor->mode_functions != NULL);

  if (!editor->dragging_obj) {
    return;
  }
  cancel_drag_obj(editor);
  Editor_display_msg(editor, msgs_lookup("StatusNoDrag"), false);
  Editor_redraw_pending(editor, false);
}

bool Editor_drag_obj_remote(Editor *const editor, Writer *const writer,
  DataType const data_type, char const *const filename)
{
  assert(editor != NULL);
  assert(editor->dragging_obj);
  assert(editor->mode_functions != NULL);

  editor->dragging_obj = false;

  bool success = false;
  if (editor->mode_functions->drag_obj_remote) {
    success = editor->mode_functions->drag_obj_remote(
                  editor, writer, data_type, filename);
  }

  return success;
}

bool Editor_allow_drop(Editor const *editor)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);
  return editor->mode_functions->drop != NULL;
}

DataType const *Editor_get_dragged_data_types(Editor const *editor)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  static DataType const no_dragged[] = {DataType_Count};

  return editor->mode_functions->dragged_data_types ?
         editor->mode_functions->dragged_data_types :
         no_dragged;
}

DataType const *Editor_get_import_data_types(Editor const *editor)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  static DataType const no_import[] = {DataType_Count};

  return editor->mode_functions->import_data_types ?
         editor->mode_functions->import_data_types :
         no_import;
}

DataType const *Editor_get_export_data_types(Editor const *editor)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  static DataType const no_export[] = {DataType_Count};

  return editor->mode_functions->export_data_types ?
         editor->mode_functions->export_data_types :
         no_export;
}

void Editor_set_paste_enabled(Editor *const editor, bool const can_paste)
{
  assert(editor != NULL);
  DEBUGF("%s paste\n", can_paste ? "Enable" : "Disable");
  editor->can_paste = can_paste;
}

bool Editor_allow_paste(Editor const *const editor)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);
  bool const can_paste = editor->mode_functions->start_pending_paste && editor->can_paste &&
                         !editor->dragging_select && !editor->dragging_obj;
  DEBUGF("%s paste\n", can_paste ? "Can" : "Can't");
  return can_paste;
}

void Editor_set_palette_rotation(Editor *const editor, ObjGfxAngle const rot)
{
  assert(editor != NULL);
  editor->palette_rotation = rot;
}

ObjGfxAngle Editor_get_palette_rotation(Editor const *const editor)
{
  assert(editor != NULL);
  return editor->palette_rotation;
}

void Editor_palette_selection(Editor *const editor, size_t const object)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);

  if (editor->mode_functions->palette_selection != NULL) {
    editor->mode_functions->palette_selection(editor, object);
  }

  if (!editor->shown_pending) {
    return;
  }

  switch (editor->tool) {
    case EDITORTOOL_PLOTSHAPES:
      pending_shape(editor);
      break;

    case EDITORTOOL_TRANSFER:
      if (editor->mode_functions->pending_transfer) {
        editor->mode_functions->pending_transfer(editor, editor->map_pos);
        Editor_redraw_pending(editor, true);
      }
      break;

   case EDITORTOOL_BRUSH:
     if (editor->mode_functions->pending_brush) {
       editor->mode_functions->pending_brush(editor, editor->brush_size, editor->map_pos);
       Editor_redraw_pending(editor, true);
     }
     break;

   case EDITORTOOL_SNAKE:
     if (editor->mode_functions->pending_snake) {
       editor->mode_functions->pending_snake(editor, editor->map_pos);
       Editor_redraw_pending(editor, true);
     }
     break;

   default:
     break;
  }
}

bool Editor_show_ghost_drop(Editor *const editor,
                       MapArea const *const bbox,
                       Editor const *drag_origin)
{
  bool hide_origin_bbox = false;

  assert(editor != NULL);
  assert(editor->mode_functions != NULL);
  DEBUGF("Show ghost in editor %p to %"PRIMapCoord",%"PRIMapCoord
         ",%"PRIMapCoord",%"PRIMapCoord" in %p\n",
         (void *)drag_origin, bbox->min.x, bbox->min.y, bbox->max.x, bbox->max.y, (void *)editor);

  assert(MapArea_is_valid(bbox));

  if (drag_origin && Editor_get_edit_mode(drag_origin) != Editor_get_edit_mode(editor))
  {
    drag_origin = NULL;
  }

  if (editor->mode_functions->show_ghost_drop) {
    hide_origin_bbox = editor->mode_functions->show_ghost_drop(editor, bbox, drag_origin);
    Editor_redraw_pending(editor, true);
  }
  return hide_origin_bbox;
}

void Editor_hide_ghost_drop(Editor *const editor)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);
  DEBUGF("Hide ghost in editor %p\n", (void *)editor);

  if (editor->mode_functions->hide_ghost_drop) {
    editor->mode_functions->hide_ghost_drop(editor);
    Editor_redraw_pending(editor, false);
  }
}

void Editor_drag_obj_move(Editor *const editor,
                      MapArea const *const bbox,
                      Editor *const drag_origin)
{
  assert(editor != NULL);
  assert(drag_origin != NULL);
  assert(drag_origin->dragging_obj);
  assert(editor->mode_functions != NULL);
  assert(MapArea_is_valid(bbox));
  assert(editor->session == drag_origin->session);

  DEBUGF("Move from editor %p to %"PRIMapCoord",%"PRIMapCoord
         ",%"PRIMapCoord",%"PRIMapCoord" in %p\n",
         (void *)drag_origin, bbox->min.x, bbox->min.y,
         bbox->max.x, bbox->max.y, (void *)editor);

  drag_origin->dragging_obj = false;

  if (Editor_get_edit_mode(editor) != Editor_get_edit_mode(drag_origin)) {
    return;
  }

  Editor_select_tool(editor, EDITORTOOL_SELECT);

  if (editor->mode_functions->drag_obj_move) {
    editor->mode_functions->drag_obj_move(editor, bbox, drag_origin);
    Session_redraw_pending(editor->session, false);
  }
}

bool Editor_drag_obj_copy(Editor *const editor,
                       MapArea const *const bbox,
                       Editor *const drag_origin)
{
  assert(editor != NULL);
  assert(drag_origin != NULL);
  assert(drag_origin->dragging_obj);
  assert(editor->mode_functions != NULL);
  DEBUGF("Copy from editor %p to %"PRIMapCoord",%"PRIMapCoord
         ",%"PRIMapCoord",%"PRIMapCoord" in %p\n",
         (void *)drag_origin, bbox->min.x, bbox->min.y,
         bbox->max.x, bbox->max.y, (void *)editor);

  assert(MapArea_is_valid(bbox));

  drag_origin->dragging_obj = false;

  if (Editor_get_edit_mode(editor) != Editor_get_edit_mode(drag_origin)) {
    return false;
  }

  Editor_select_tool(editor, EDITORTOOL_SELECT);

  if (editor->mode_functions->drag_obj_copy) {
    bool success = editor->mode_functions->drag_obj_copy(editor, bbox, drag_origin);
    Session_redraw_pending(editor->session, false);
    return success;
  }

  return false;
}

bool Editor_drag_obj_link(Editor *const editor, int const window, int const icon,
  Editor *const drag_origin)
{
  assert(editor != NULL);
  assert(drag_origin != NULL);
  assert(drag_origin->dragging_obj);
  assert(editor->mode_functions != NULL);


  if (editor->mode_functions->drag_obj_link) {
    if (editor->mode_functions->drag_obj_link(editor, window, icon, drag_origin)) {
      drag_origin->dragging_obj = false;
      return true;
    }
  }

  return false;
}

bool Editor_drop(Editor *const editor, MapArea const *const bbox,
                 Reader *const reader, int const estimated_size,
                 DataType const data_type, char const *const filename)
{
  assert(editor != NULL);
  assert(editor->mode_functions != NULL);
  DEBUGF("Drop at %"PRIMapCoord",%"PRIMapCoord",%"PRIMapCoord
         ",%"PRIMapCoord" in %p\n",
         bbox->min.x, bbox->min.y, bbox->max.x, bbox->max.y, (void *)editor);

  assert(MapArea_is_valid(bbox));

  Editor_select_tool(editor, EDITORTOOL_SELECT);

  if (editor->mode_functions->drop) {
    bool success = editor->mode_functions->drop(editor, bbox, reader,
      estimated_size, data_type, filename);
    Session_redraw_pending(editor->session, false);
    return success;
  }

  return false;
}

void Editor_set_fill_is_global(Editor *const editor, bool const global_fill)
{
  assert(editor);

  if (editor->global_fill != global_fill) {
    editor->global_fill = global_fill;

    if (editor->tool == EDITORTOOL_FILLREPLACE) {
      set_tool_msg(editor);
      Editor_set_help_and_ptr(editor);
    }
  }

  Editor_select_tool(editor, EDITORTOOL_FILLREPLACE);
}

bool Editor_get_fill_is_global(Editor const *const editor)
{
  assert(editor);
  return editor->global_fill;
}

PlotShape Editor_get_plot_shape(Editor const *const editor)
{
  assert(editor);
  return editor->shape_to_plot;
}

void Editor_set_plot_shape(Editor *const editor, PlotShape const shape_to_plot)
{
  assert(editor);
  assert(shape_to_plot >= PLOTSHAPE_FIRST);
  assert(shape_to_plot < PLOTSHAPE_COUNT);

  if (editor->shape_to_plot != shape_to_plot) {
    editor->shape_to_plot = shape_to_plot;

    if (editor->tool == EDITORTOOL_PLOTSHAPES) {
      clear_vertices(editor);

      if (editor->shown_pending &&
          editor->mode_functions->pending_plot) {
        editor->mode_functions->pending_plot(editor, editor->map_pos);
      }

      set_tool_msg(editor);
      Editor_set_help_and_ptr(editor);
      Editor_redraw_pending(editor, false);
    } else {
      Editor_select_tool(editor, EDITORTOOL_PLOTSHAPES);
    }
  } else {
    Editor_select_tool(editor, EDITORTOOL_PLOTSHAPES);
  }
}

int Editor_get_brush_size(Editor const *const editor)
{
  assert(editor);
  return editor->brush_size;
}

void Editor_set_brush_size(Editor *const editor, int const size)
{
  assert(editor);
  assert(size >= 0);

  if (editor->brush_size != size) {
    editor->brush_size = size;

    if (editor->tool == EDITORTOOL_BRUSH) {
      if (editor->shown_pending &&
          editor->mode_functions->pending_brush) {
        editor->mode_functions->pending_brush(editor, editor->brush_size, editor->map_pos);
        Editor_redraw_pending(editor, true);
      }

      set_tool_msg(editor);
      Editor_set_help_and_ptr(editor);
    } else {
      Editor_select_tool(editor, EDITORTOOL_BRUSH);
    }
  } else {
    Editor_select_tool(editor, EDITORTOOL_BRUSH);
  }
}

int Editor_get_wand_size(Editor const *const editor)
{
  assert(editor);
  return editor->wand_size;
}

void Editor_set_wand_size(Editor *const editor, int const size)
{
  assert(editor);
  assert(size >= 0);

  if (editor->wand_size != size) {
    editor->wand_size = size;

    if (editor->tool == EDITORTOOL_SMOOTHWAND) {
      if (editor->shown_pending &&
          editor->mode_functions->pending_smooth) {
        editor->mode_functions->pending_smooth(editor, editor->wand_size, editor->map_pos);
      }

      set_tool_msg(editor);
      Editor_set_help_and_ptr(editor);
    } else {
      Editor_select_tool(editor, EDITORTOOL_SMOOTHWAND);
    }
  } else {
    Editor_select_tool(editor, EDITORTOOL_SMOOTHWAND);
  }
}

#ifdef DEBUG_OUTPUT
char const *EditorChange_to_string(EditorChange const event)
{
  char const *strings[] = {
#define DECLARE_CHANGE(c) [EDITOR_CHANGE_ ## c] = #c,
#include "DeclChange.h"
#undef DECLARE_CHANGE
  };
  assert(event >= 0);
  assert((size_t)event < ARRAY_SIZE(strings));
  return strings[event];
}
#endif

char const *Editor_get_tool_msg(Editor *const editor,
                                EditorTool tool, bool const caps)
{
  assert(editor);

  static char desc_string[32];
  char parameter[24], token[12];

  if (tool == EDITORTOOL_NONE) {
    tool = editor->tool;
  }

  switch (tool) {
    case EDITORTOOL_BRUSH:
      sprintf(parameter, "%d",
        (2 * editor->brush_size) + 1);
      break;

    case EDITORTOOL_SMOOTHWAND:
      sprintf(parameter, "%d",
        (2 * editor->wand_size) + 1);
      break;

    case EDITORTOOL_FILLREPLACE:
      STRCPY_SAFE(parameter, msgs_lookup(editor->global_fill ? "Fill1" :
                  "Fill0"));

      break;

    case EDITORTOOL_PLOTSHAPES:
      sprintf(parameter, "Plot%d", editor->shape_to_plot);
      STRCPY_SAFE(parameter, msgs_lookup(parameter));
      break;

    default:
      *parameter = '\0'; /* we are paranoid so terminate string */
      break;
  }

  sprintf(token, "Tool%d", tool);
  STRCPY_SAFE(desc_string, msgs_lookup_subn(token, 1, parameter));

  /* Ensure initial letter is correct case */
  *desc_string = caps ? toupper(*desc_string) : tolower(*desc_string);

  return desc_string;
}

void Editor_redraw_map(Editor *const editor, MapArea const *const area)
{
  DEBUGF("%s\n", __func__);
  assert(editor != NULL);
#if PER_VIEW_SELECT
  EditWin *const edit_win = Session_editor_to_win(editor);
  if (edit_win) {
    EditWin_redraw_map(edit_win, area);
  }
#else
  Session_redraw_map(editor->session, area);
#endif
}

void Editor_redraw_object(Editor *const editor, MapPoint const pos, ObjRef const obj_ref, bool const has_triggers)
{
  assert(editor != NULL);
#if PER_VIEW_SELECT
  EditWin *const edit_win = Session_editor_to_win(editor);
  if (edit_win) {
    EditWin_redraw_object(edit_win, pos, objects_ref_none(), obj_ref, obj_ref, has_triggers);
  }
#else
  Session_redraw_object(editor->session, pos, objects_ref_none(), obj_ref, obj_ref, has_triggers);
#endif
}

void Editor_redraw_info(Editor *const editor, MapPoint const pos)
{
  assert(editor != NULL);
#if PER_VIEW_SELECT
  EditWin *const edit_win = Session_editor_to_win(editor);
  if (edit_win) {
    EditWin_redraw_info(edit_win, pos);
  }
#else
  Session_redraw_info(editor->session, pos);
#endif
}

void Editor_occluded_obj_changed(Editor *const editor, MapPoint const pos, ObjRef const obj_ref)
{
  assert(editor != NULL);
#if PER_VIEW_SELECT
  EditWin *const edit_win = Session_editor_to_win(editor);
  if (edit_win) {
    EditWin_occluded_obj_changed(edit_win, pos, obj_ref);
  }
#else
  Session_occluded_obj_changed(editor->session, pos, obj_ref);
#endif
}

void Editor_occluded_info_changed(Editor *const editor, MapPoint const pos)
{
  assert(editor != NULL);
#if PER_VIEW_SELECT
  EditWin *const edit_win = Session_editor_to_win(editor);
  if (edit_win) {
    EditWin_occluded_info_changed(edit_win, pos);
  }
#else
  Session_occluded_info_changed(editor->session, pos);
#endif
}

void Editor_redraw_ghost(Editor *const editor)
{
  assert(editor != NULL);
#if PER_VIEW_SELECT
  EditWin *const edit_win = Session_editor_to_win(editor);
  if (edit_win) {
    EditWin_redraw_ghost(edit_win);
  }
#else
  Session_redraw_ghost(editor->session);
#endif
}

void Editor_clear_ghost_bbox(Editor *const editor)
{
  assert(editor != NULL);
#if PER_VIEW_SELECT
  EditWin *const edit_win = Session_editor_to_win(editor);
  if (edit_win) {
    EditWin_clear_ghost_bbox(edit_win);
  }
#else
  Session_clear_ghost_bbox(editor->session);
#endif
}

void Editor_set_ghost_map_bbox(Editor *const editor, MapArea const *area)
{
  assert(editor != NULL);
#if PER_VIEW_SELECT
  EditWin *const edit_win = Session_editor_to_win(editor);
  if (edit_win) {
    EditWin_set_ghost_map_bbox(edit_win, area);
  }
#else
  Session_set_ghost_map_bbox(editor->session, area);
#endif
}

void Editor_add_ghost_obj(Editor *const editor, MapPoint const pos, ObjRef const obj_ref)
{
  assert(editor != NULL);
#if PER_VIEW_SELECT
  EditWin *const edit_win = Session_editor_to_win(editor);
  if (edit_win) {
    EditWin_add_ghost_obj(edit_win, pos, obj_ref);
  }
#else
  Session_add_ghost_obj(editor->session, pos, obj_ref);
#endif
}

void Editor_add_ghost_info(Editor *const editor, MapPoint const pos)
{
  assert(editor != NULL);
#if PER_VIEW_SELECT
  EditWin *const edit_win = Session_editor_to_win(editor);
  if (edit_win) {
    EditWin_add_ghost_info(edit_win, pos);
  }
#else
  Session_add_ghost_info(editor->session, pos);
#endif
}

void Editor_add_ghost_unknown_obj(Editor *const editor, MapArea const *const bbox)
{
  assert(editor != NULL);
#if PER_VIEW_SELECT
  EditWin *const edit_win = Session_editor_to_win(editor);
  if (edit_win) {
    EditWin_add_ghost_unknown_obj(edit_win, bbox);
  }
#else
  Session_add_ghost_unknown_obj(editor->session, bbox);
#endif
}

void Editor_add_ghost_unknown_info(Editor *const editor, MapArea const *const bbox)
{
  assert(editor != NULL);
#if PER_VIEW_SELECT
  EditWin *const edit_win = Session_editor_to_win(editor);
  if (edit_win) {
    EditWin_add_ghost_unknown_info(edit_win, bbox);
  }
#else
  Session_add_ghost_unknown_info(editor->session, bbox);
#endif
}

void Editor_redraw_pending(Editor *const editor, bool const immediate)
{
  assert(editor != NULL);
#if PER_VIEW_SELECT
  EditWin *const edit_win = Session_editor_to_win(editor);
  if (edit_win) {
    EditWin_redraw_pending(edit_win, immediate);
  }
#else
  Session_redraw_pending(editor->session, immediate);
#endif
}
