/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Miscellaneous mission options menu
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

#include "Session.h"
#include "utils.h"
#include "EditWin.h"
#include "missopts.h"
#include "DataType.h"
#include "Mission.h"

enum {
  ComponentId_NOSCANNER      = 0x4,
  ComponentId_NOGROUNDDAMAGE = 0x5,
  ComponentId_DOCKTOCOMPLETE = 0x6,
};

/* ---------------- Private functions ---------------- */

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
  MissionData *const m = Session_get_mission(session);

  switch (id_block->self_component) {
    case ComponentId_NOSCANNER:
      mission_set_scanners_down(m, update_menu_tick(id_block));
      break;

    case ComponentId_NOGROUNDDAMAGE:
      mission_set_impervious_map(m, update_menu_tick(id_block));
      break;

    case ComponentId_DOCKTOCOMPLETE:
      mission_set_dock_to_finish(m, update_menu_tick(id_block));
      break;

    default:
      return 0; /* not interested */
  }

  Session_notify_changed(session, DataType_Mission);
  return 1; /* claim event */
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
  EditSession *const session = EditWin_get_session(edit_win);
  MissionData const *const m = Session_get_mission(session);

  E(menu_set_tick(0, id_block->self_id, ComponentId_NOSCANNER,
             mission_get_scanners_down(m)));

  E(menu_set_tick(0, id_block->self_id, ComponentId_NOGROUNDDAMAGE,
             mission_get_impervious_map(m)));

  E(menu_set_tick(0, id_block->self_id, ComponentId_DOCKTOCOMPLETE,
             mission_get_dock_to_finish(m)));

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void missopts_created(ObjectId const id)
{
  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Menu_Selection, menu_selection },
    { Menu_AboutToBeShown, about_to_be_shown }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}
