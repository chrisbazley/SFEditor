/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Main menu for map window
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

#include "toolbox.h"
#include "event.h"
#include "menu.h"

#include "err.h"
#include "Macros.h"
#include "Debug.h"

#include "MissFiles.h"
#include "MapFiles.h"
#include "EditWin.h"
#include "Session.h"
#include "MainMenu.h"
#include "EditMenu.h"
#include "Utils.h"
#include "DataType.h"
#include "Editor.h"

enum {
  ComponentId_FILE     = 0x2,
  ComponentId_EDIT     = 0x3,
  ComponentId_EFFECT   = 0x6,
  ComponentId_TOOL     = 0x4,
  ComponentId_MODE     = 0x1,
  ComponentId_GRAPHICS = 0x15,
  ComponentId_MISSION  = 0x14,
  ComponentId_HELP     = 0x5,
};

#define ERR_BAD_OBJECT_ID    0x1b80cb02

static ObjectId MainMenu_id = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  Editor *const editor = EditWin_get_editor(edit_win);
  EditSession *const session = EditWin_get_session(edit_win);

  E(menu_set_fade(0, id_block->self_id, ComponentId_MISSION,
                           !Session_has_data(session, DataType_Mission)));

  /* Attach appropriate version of 'File' submenu depending on whether
     editing map or mission */
  E(menu_set_sub_menu_show(
    0, id_block->self_id, ComponentId_FILE,
    Session_get_ui_type(session) == UI_TYPE_MISSION ?
      MissFiles_sharedid : MapFiles_sharedid));

  /* Notify the current editing mode, e.g. to allow auto-selection of the
     object under the mouse pointer */
  Editor_auto_select(editor, edit_win);

  EditWin_update_can_paste(edit_win);

  return 1; /* claim event */
}

static int has_been_hidden(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  /* We may receive this event after our parent edit_win has been deleted... */
  if (toolbox_get_object_state(0, id_block->ancestor_id, NULL) != NULL)
    return 1; /* ...in which case do nothing */

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  Editor *const editor = EditWin_get_editor(edit_win);

  /* Notify the current editing mode to clear any transient selection */
  Editor_auto_deselect(editor);

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void MainMenu_created(ObjectId const id)
{
  MainMenu_id = id;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Menu_HasBeenHidden, has_been_hidden },
    { Menu_AboutToBeShown, about_to_be_shown }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}

void MainMenu_hide(void)
{
  E(toolbox_hide_object(0, MainMenu_id));
}

EditSession *MainMenu_get_session(void)
{
  EditSession *session = NULL;
  void *const edit_win = get_ancestor_handle_if_showing(MainMenu_id);

  if (edit_win != NULL) {
    session = EditWin_get_session(edit_win);
  }

  return session;
}
