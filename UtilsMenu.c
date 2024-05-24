/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Utilities menu
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
#include "utils.h"
#include "EditWin.h"
#include "UtilsMenu.h"
#include "MapAnims.h"
#include "MapEditCtx.h"
#include "DataType.h"
#include "Editor.h"

/* --------------------- Gadgets -------------------- */

enum {
  ComponentId_ZOOM            = 0xf,
  ComponentId_GRID            = 0x1,
  ComponentId_STATUSBAR       = 0x12,
  ComponentId_NUMBERS         = 0x10,
  ComponentId_BACKGROUND      = 0x11,
  ComponentId_REVEALPALETTE = 0x3,
  ComponentId_PALETTE       = 0x18,
  ComponentId_TOOLBOX       = 0xc,
  ComponentId_SHOWANIMS     = 0xd,
};

static ObjectId UtilsMenu_id = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

static void update_utils_menu(EditWin *const edit_win)
{
  EditSession *const session = EditWin_get_session(edit_win);
  Editor *const editor = EditWin_get_editor(edit_win);

  E(menu_set_tick(0, UtilsMenu_id, ComponentId_TOOLBOX,
                           Editor_get_tools_shown(editor)));

  E(menu_set_tick(0, UtilsMenu_id, ComponentId_PALETTE,
                           Editor_get_pal_shown(editor)));

  E(menu_set_fade(0, UtilsMenu_id, ComponentId_SHOWANIMS,
                           !Session_has_data(session, DataType_OverlayMapAnimations)));

  E(menu_set_tick(0, UtilsMenu_id, ComponentId_SHOWANIMS,
                           Session_get_anims_shown(session)));

  E(menu_set_fade(0, UtilsMenu_id, ComponentId_NUMBERS,
                           !Session_has_data(session, DataType_OverlayMap) &&
                           !Session_has_data(session, DataType_BaseMap) &&
                           !Session_has_data(session, DataType_BaseObjects) &&
                           !Session_has_data(session, DataType_OverlayObjects)));

  ViewDisplayFlags const display_flags = EditWin_get_display_flags(edit_win);

  E(menu_set_tick(0, UtilsMenu_id, ComponentId_GRID,
                  display_flags.GRID));

  E(menu_set_tick(0, UtilsMenu_id, ComponentId_NUMBERS,
                  display_flags.NUMBERS));

  E(menu_set_tick(0, UtilsMenu_id, ComponentId_STATUSBAR,
                  EditWin_get_status_shown(edit_win)));
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

  update_utils_menu(edit_win);

  return 1; /* claim event */
}

static bool is_showing_for_session(EditWin const *const edit_win)
{
  void *const ancestor_edit_win = get_ancestor_handle_if_showing(UtilsMenu_id);
  const EditSession *const ancestor_session =
    ancestor_edit_win ? EditWin_get_session(ancestor_edit_win) : NULL;

  return ancestor_session == EditWin_get_session(edit_win);
}

/* ---------------- Public functions ---------------- */

void UtilsMenu_created(ObjectId const menu_id)
{
  UtilsMenu_id = menu_id;

  EF(event_register_toolbox_handler(menu_id, Menu_AboutToBeShown,
                                            about_to_be_shown, NULL));
}

void UtilsMenu_update(EditWin *const edit_win)
{
  if (is_showing_for_session(edit_win))
  {
    update_utils_menu(edit_win);
  }
}
