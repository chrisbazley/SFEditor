/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Standard edit menu
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

#include "event.h"
#include "toolbox.h"
#include "menu.h"

#include "err.h"
#include "macros.h"

#include "Editor.h"
#include "editmenu.h"
#include "utils.h"
#include "EditWin.h"

enum {
  ComponentId_CUT            = 0x0,
  ComponentId_COPY           = 0x1,
  ComponentId_PASTE          = 0x8,
  ComponentId_DELETE         = 0x3,
  ComponentId_SELECTALL      = 0x4,
  ComponentId_CLEARSELECTION = 0x5,
  ComponentId_CLIPOVERLAY    = 0x14,
  ComponentId_CREATETRANS    = 0x6,
  ComponentId_PROPERTIES     = 0x13,
};

static ObjectId EditMenu_id = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

static void update_edit_menu(EditWin *const edit_win)
{
  Editor *const editor = EditWin_get_editor(edit_win);
  size_t const num_selected = Editor_num_selected(editor);
  size_t const max_selected = Editor_max_selected(editor);

  E(menu_set_fade(0, EditMenu_id, ComponentId_CUT,
    !Editor_can_delete(editor)));

  E(menu_set_fade(0, EditMenu_id, ComponentId_COPY, !num_selected));

  E(menu_set_fade(0, EditMenu_id, ComponentId_PASTE,
    !Editor_allow_paste(editor)));

  E(menu_set_fade(0, EditMenu_id, ComponentId_DELETE,
    !Editor_can_delete(editor)));

  E(menu_set_fade(0, EditMenu_id, ComponentId_SELECTALL,
                           num_selected == max_selected));

  E(menu_set_fade(0, EditMenu_id, ComponentId_CLEARSELECTION,
                           !num_selected));

  E(menu_set_fade(0, EditMenu_id, ComponentId_PROPERTIES,
    !Editor_can_edit_properties(editor)));

  E(menu_set_fade(0, EditMenu_id, ComponentId_CLIPOVERLAY,
    !Editor_can_clip_overlay(editor)));

  E(menu_set_fade(0, EditMenu_id, ComponentId_CREATETRANS,
    !Editor_can_create_transfer(editor)));
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);

  update_edit_menu(edit_win);

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void EditMenu_created(ObjectId const id)
{
  EditMenu_id = id;

  EF(event_register_toolbox_handler(id, Menu_AboutToBeShown, about_to_be_shown, NULL));
}

void EditMenu_update(Editor const *const editor)
{
  EditWin *const edit_win = get_ancestor_handle_if_showing(EditMenu_id);
  if (edit_win && EditWin_get_editor(edit_win) == editor)
  {
    EditWin_update_can_paste(edit_win);
    update_edit_menu(edit_win);
  }
}

void EditMenu_show_at_ptr(EditWin const *const edit_win)
{
  EditWin_show_dbox_at_ptr(edit_win, EditMenu_id);
}
