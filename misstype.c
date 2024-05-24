/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission type menu
 *  Copyright (C) 2020 Christopher Bazley
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
#include "misstype.h"
#include "DataType.h"
#include "Mission.h"

static ComponentId which_ticked = NULL_ComponentId;

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

  DEBUG("Mission type menu item %d selected", id_block->self_component);

  if (id_block->self_component != which_ticked)
  {
    if (which_ticked != NULL_ComponentId)
    {
      E(menu_set_tick(0, id_block->self_id, which_ticked, 0));
    }

    which_ticked = id_block->self_component;
    E(menu_set_tick(0, id_block->self_id, which_ticked, 1));

    MissionData *const m = Session_get_mission(session);
    mission_set_type(m, (MissionType)(MissionType_Normal + which_ticked));
    Session_notify_changed(session, DataType_Mission);
  }

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
  MissionType const type = mission_get_type(m);

  if ((ComponentId)(type - MissionType_Normal) != which_ticked) {
    DEBUG("Moving menu tick from %d to %d", which_ticked, type - MissionType_Normal);

    /* Remove any existing menu tick */
    if (which_ticked != NULL_ComponentId)
    {
      E(menu_set_tick(0, id_block->self_id, which_ticked, 0));
    }

    /* Tick menu entry corresponding to current type */
    which_ticked = (ComponentId)(type - MissionType_Normal);
    E(menu_set_tick(0, id_block->self_id, which_ticked, 1));
  }

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void misstype_created(ObjectId const id)
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
