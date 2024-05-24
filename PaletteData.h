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

  bool redraw_error:1; /* prevent redraw after error */
  bool tools_change:1;
  bool mode_change:1;
  bool user_event:1;
  bool numeric_order:1;
  bool labels:1;
  bool is_showing:1;

  size_t sel_index;
  size_t num_indices;
  Vertex grid_size;
  size_t max_columns;
  Vertex sel_pos;
  Vertex object_size;
  const struct PaletteClientFuncts *client_functions;
};

#endif
