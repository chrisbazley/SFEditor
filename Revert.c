/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Reversion menu
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
#include "stdio.h"
#include <string.h>
#include "stdlib.h"

#include "event.h"
#include "toolbox.h"
#include "menu.h"

#include "err.h"
#include "macros.h"
#include "msgtrans.h"
#include "strextra.h"
#include "debug.h"
#include "FileUtils.h"
#include "pathtail.h"

#include "Session.h"
#include "missfiles.h"
#include "mapfiles.h"
#include "filepaths.h"
#include "utils.h"
#include "EditWin.h"
#include "revert.h"
#include "MapAnims.h"

enum {
  REVERT_TOLASTSAVE = 0x0,
  REVERT_TOORIGINAL = 0x1,
};

static DataType data_type;

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
  EditSession *const session = EditWin_get_session(edit_win);

  /* Which file this is depends on which menu entries we came through to open
     the dialogue box */
  ComponentId grandparent_component;
  ON_ERR_RPT_RTN_V(toolbox_get_parent(0, id_block->parent_id, NULL,
  &grandparent_component), 1);

  if (Session_get_ui_type(session) == UI_TYPE_MISSION) {
    data_type = MissFiles_get_data_type(grandparent_component);
  } else {
    data_type = MapFiles_get_data_type(grandparent_component);
  }

  E(menu_set_fade(0, id_block->self_id, REVERT_TOORIGINAL,
        !Session_can_revert_to_original(session, data_type)));

  /* No point allowing reversion to last save if no changes since */
  E(menu_set_fade(0, id_block->self_id, REVERT_TOLASTSAVE,
        !Session_file_modified(session, data_type)));

  return 1; /* claim event */
}

static int menu_selection(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  EditSession *const session = EditWin_get_session(edit_win);

  switch (id_block->self_component) {
    case REVERT_TOLASTSAVE:
      if (!dialogue_confirm(msgs_lookup("RevertUns"), "RevBut"))
        return 1; /* claim event */

      Session_reload(session, data_type);
      break;

    case REVERT_TOORIGINAL:
      if (!dialogue_confirm(msgs_lookup("RevertUns"), "RevBut"))
        return 1; /* claim event */

      Session_revert_to_original(session, data_type);
      break;

    default:
      return 0; /* not interested */
  }

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void revert_created(ObjectId const id)
{
  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Menu_AboutToBeShown, about_to_be_shown },
    { Menu_Selection, menu_selection },
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}
