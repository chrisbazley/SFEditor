/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Editing window toolbox
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef MapToolbar_h
#define MapToolbar_h

#include <stdbool.h>

#include "toolbox.h"

#include "Session.h"
#include "MapMode.h"

struct EditWin;

typedef struct
{
  ObjectId     my_object;
  Editor      *editor;
  bool         null_poller;
  ComponentId  mouse_over_button;
  ComponentId  button_selected;
} MapToolbar;

bool MapToolbar_init(MapToolbar *toolbar, Editor *editor);

void MapToolbar_tool_selected(MapToolbar *toolbar, EditorTool tool);
bool MapToolbar_update_buttons(MapToolbar *toolbar);
void MapToolbar_destroy(MapToolbar *toolbar);
void MapToolbar_hide(MapToolbar *toolbar);
void MapToolbar_reveal(MapToolbar *toolbar, struct EditWin *edit_win);

#endif
