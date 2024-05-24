/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map edit_win tools menu
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

#include <assert.h>

#include "toolbox.h"
#include "menu.h"
#include "event.h"

#include "err.h"
#include "Macros.h"

#include "Session.h"
#include "Editor.h"
#include "EditWin.h"
#include "utils.h"
#include "ToolMenu.h"
#include "MTransfers.h"
#include "MSnakes.h"
#include "DataType.h"

/* --------------------- Gadgets -------------------- */

enum {
  ComponentId_Snakes     = 0x4,
  ComponentId_SmoothWand = 0x5,
  ComponentId_Transfers  = 0x6,
  ComponentId_MakeSelect = 0x7,
  ComponentId_FillArea   = 0xa,
  ComponentId_PaintBrush = 0xb,
  ComponentId_PlotShapes = 0xc,
  ComponentId_Magnifier  = 0xd,
  ComponentId_Sampler  = 0xe,
};

static ComponentId const tool_menu_entries[] = {
  [EDITORTOOL_BRUSH] = ComponentId_PaintBrush,
  [EDITORTOOL_FILLREPLACE] = ComponentId_FillArea,
  [EDITORTOOL_PLOTSHAPES] = ComponentId_PlotShapes,
  [EDITORTOOL_SNAKE] = ComponentId_Snakes,
  [EDITORTOOL_SMOOTHWAND] = ComponentId_SmoothWand,
  [EDITORTOOL_TRANSFER] = ComponentId_Transfers,
  [EDITORTOOL_SELECT] = ComponentId_MakeSelect,
  [EDITORTOOL_MAGNIFIER] = ComponentId_Magnifier,
  [EDITORTOOL_SAMPLER] = ComponentId_Sampler};

static ObjectId ToolMenu_id = NULL_ObjectId;
static ComponentId selected = NULL_ComponentId;

/* ---------------- Private functions ---------------- */

static ComponentId tool_to_component(const EditorTool tool)
{
  if (tool >= 0 && (unsigned)tool < ARRAY_SIZE(tool_menu_entries)) {
    return tool_menu_entries[tool];
  } else {
    return NULL_ComponentId;
  }
}

static EditorTool component_to_tool(ComponentId const entry)
{
  for (size_t tool = 0; tool < ARRAY_SIZE(tool_menu_entries); tool++) {
    if (tool_menu_entries[tool] == entry)
      return (EditorTool)tool;
  }
  assert(false);
  return EDITORTOOL_NONE; /* unknown menu entry */
}

static void select_entry(ObjectId const menu_id, ComponentId const entry)
{
  assert(menu_id != NULL_ObjectId);

  if (entry != selected) {
    if (selected != NULL_ComponentId) {
      E(menu_set_tick(0, menu_id, selected, 0));
    }

    if (entry != NULL_ComponentId) {
      E(menu_set_tick(0, menu_id, entry, 1));
    }

    selected = entry;
  }
}

static void update_tool_menu(Editor const *const editor)
{
  select_entry(ToolMenu_id,
    tool_to_component(Editor_get_tool(editor)));

  for (size_t tool = 0; tool < ARRAY_SIZE(tool_menu_entries); tool++) {
    if (tool_menu_entries[tool] != NULL_ComponentId)
    {
      E(menu_set_fade(0, ToolMenu_id, tool_menu_entries[tool],
        !Editor_can_select_tool(editor, (EditorTool)tool)));
    }
  }
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Set up menu */
  NOT_USED(handle);
  NOT_USED(event);
  NOT_USED(event_code);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  Editor *const editor = EditWin_get_editor(edit_win);
  update_tool_menu(editor);

  return 1; /* claim event */
}

static int menu_selection(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(handle);
  NOT_USED(event);
  NOT_USED(event_code);

  if (id_block->self_component == selected)
    return 1; /* already selected - nothing to do here */

  EditorTool const tool = component_to_tool(id_block->self_component);
  if (tool == EDITORTOOL_NONE)
    return 0; /* menu entry not known */

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  Editor *const editor = EditWin_get_editor(edit_win);

  Editor_select_tool(editor, tool);

  select_entry(id_block->self_id, id_block->self_component);

  return 1; /* claim event */
}

static bool is_showing_for_session(Editor const *const editor)
{
  void *const edit_win = get_ancestor_handle_if_showing(ToolMenu_id);
  Editor const *const ancestor_editor =
    edit_win ? EditWin_get_editor(edit_win) : NULL;

  return ancestor_editor == editor;
}

/* ---------------- Public functions ---------------- */

void ToolMenu_created(ObjectId const menu_id)
{
  ToolMenu_id = menu_id;
  selected = NULL_ComponentId;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Menu_Selection, menu_selection },
    { Menu_AboutToBeShown, about_to_be_shown },
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(menu_id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}

void ToolMenu_update(Editor const *const editor)
{
  if (is_showing_for_session(editor))
  {
    update_tool_menu(editor);
  }
}
