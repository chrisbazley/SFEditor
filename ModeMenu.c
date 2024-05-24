/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Menu for selection of editing mode
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

#include "EditWin.h"
#include "ModeMenu.h"
#include "utils.h"
#include "MainMenu.h"
#include "DataType.h"
#include "Editor.h"

/* --------------------- Gadgets -------------------- */

enum {
  MODEMENU_MAP  = 0x2,
  MODEMENU_OBJ  = 0x5,
  MODEMENU_INFO = 0x6,
  MODEMENU_SHIP = 0x0,
};

ObjectId ModeMenu_id = NULL_ObjectId;

static EditMode selected = EDITING_MODE_NONE;

static const EditMode modes[] = {EDITING_MODE_MAP, EDITING_MODE_OBJECTS, EDITING_MODE_INFO, EDITING_MODE_SHIPS};

static const ComponentId mode_to_component_id[] =
{
  [EDITING_MODE_MAP] = MODEMENU_MAP,
  [EDITING_MODE_OBJECTS] = MODEMENU_OBJ,
  [EDITING_MODE_INFO] = MODEMENU_INFO,
  [EDITING_MODE_SHIPS] = MODEMENU_SHIP,
};

/* ---------------- Private functions ---------------- */

static int mm_about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Set up menu */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  void *ancestor;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &ancestor), 0);
  EditWin *const edit_win = ancestor;
  Editor *const editor = EditWin_get_editor(edit_win);

  for (size_t i = 0; i < ARRAY_SIZE(modes); ++i)
  {
    EditMode const m = modes[i];
    E(menu_set_fade(0, id_block->self_id, mode_to_component_id[m],
                             !Editor_can_set_edit_mode(editor, m)));
  }

  if (selected != EDITING_MODE_NONE)
  {
    E(menu_set_tick(0, id_block->self_id, mode_to_component_id[selected], 0));
  }

  EditMode const mode = Editor_get_edit_mode(editor);
  assert(mode >= 0);
  assert((size_t)mode < ARRAY_SIZE(mode_to_component_id));

  selected = mode;
  E(menu_set_tick(0, id_block->self_id, mode_to_component_id[selected], 1));

  return 1; /* claim event */
}

static int mm_selection(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  void *ancestor;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &ancestor), 0);
  EditWin *const edit_win = ancestor;
  Editor *const editor = EditWin_get_editor(edit_win);

  if (id_block->self_component == mode_to_component_id[selected])
  {
    return 1; /* already selected - nothing to do here */
  }

  EditMode new_mode = EDITING_MODE_NONE;

  for (size_t i = 0; i < ARRAY_SIZE(modes); ++i)
  {
    EditMode const m = modes[i];
    if (id_block->self_component == mode_to_component_id[m])
    {
      new_mode = m;
      break;
    }
  }

  if (new_mode == EDITING_MODE_NONE)
  {
    return 0; /* not interested in this menu entry */
  }

  Editor_set_edit_mode(editor, new_mode, edit_win);

  if (selected != EDITING_MODE_NONE)
  {
    E(menu_set_tick(0, id_block->self_id, mode_to_component_id[selected], 0));
  }

  E(menu_set_tick(0, id_block->self_id, id_block->self_component, 1));
  selected = new_mode;

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void ModeMenu_created(ObjectId const menu_id)
{
  ModeMenu_id = menu_id;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Menu_Selection, mm_selection },
    { Menu_AboutToBeShown, mm_about_to_be_shown }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(menu_id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}

void ModeMenu_show_at_ptr(EditWin const *const edit_win)
{
  EditWin_show_dbox_at_ptr(edit_win, ModeMenu_id);
}
