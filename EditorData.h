/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Data for an editor instance
 *  Copyright (C) 2021 Christopher Bazley
 */

#ifndef EditorData_h
#define EditorData_h

#include <stdbool.h>

#include "PaletteData.h"
#include "MapToolbar.h"
#include "Editor.h"
#include "EditMode.h"

struct Editor
{
  struct EditSession *session;

  bool show_tool_bar:1, show_palette:1, temp_menu_select:1,
       can_paste:1, global_fill:1, allow_drag_select:1,
       drag_select_only_inside:1, paste_pending:1,
       dragging_select:1, dragging_obj:1, shown_pending:1;

  ObjGfxAngle palette_rotation;
  int coord_field_width, last_anim,
      vertices_set, brush_size, wand_size;

  EditorTool tool;
  PlotShape shape_to_plot;
  struct PaletteData palette_data;
  MapToolbar toolbar;
  MapPoint map_pos, fine_pos, drag_select_start, drag_select_end, vertex[3];

  EditMode editing_mode; /* see editing modes defined above */
  EditModeFuncts const *mode_functions;
  void *editingmode_data; /* mode-specific data for selection models etc */
};



#endif
