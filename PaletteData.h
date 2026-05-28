/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Palette data
 *  Copyright (C) 2019 Christopher Bazley
 */

#ifndef PaletteData_h
#define PaletteData_h

#include <stdbool.h>

#include "toolbox.h"

#include "Vertex.h"
#include "Palette.h"

struct PaletteData {
  ObjectId my_object;
  struct Editor *parent_editor;

  bool redraw_error:1, /* prevent redraw after error */
       tools_change:1,
       mode_change:1,
       user_event:1,
       numeric_order:1,
       labels:1,
       is_showing:1;

  int sel_index, num_indices, max_columns;
  Vertex grid_size, sel_pos, object_size;
  const struct PaletteClientFuncts *client_functions;
};

#endif
