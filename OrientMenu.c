/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map edit_win orientation menu
 *  Copyright (C) 2022 Christopher Bazley
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
#include "utils.h"
#include "OrientMenu.h"

/* --------------------- Gadgets -------------------- */

enum {
  ORIENTMENU_NORTH = 0x00,
  ORIENTMENU_EAST  = 0x01,
  ORIENTMENU_SOUTH = 0x02,
  ORIENTMENU_WEST  = 0x03,
};

static ObjectId OrientMenu_id = NULL_ObjectId;
static ComponentId selected = NULL_ComponentId;

/* ---------------- Private functions ---------------- */

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Set up menu */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);

  MapAngle const angle = EditWin_get_angle(edit_win);

  if (selected != NULL_ComponentId) {
    E(menu_set_tick(0, id_block->self_id, selected, 0));
  }

  switch (angle) {
    case MapAngle_North:
      selected = ORIENTMENU_NORTH;
      break;

    case MapAngle_East:
      selected = ORIENTMENU_EAST;
      break;

    case MapAngle_South:
      selected = ORIENTMENU_SOUTH;
      break;

    case MapAngle_West:
      selected = ORIENTMENU_WEST;
      break;

    default:
      return 1; /* claim event */
  }

  E(menu_set_tick(0, id_block->self_id, selected, 1));

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

  if (id_block->self_component == selected) {
    return 1; /* already selected */
  }

  MapAngle angle = MapAngle_North;

  switch (id_block->self_component) {
    case ORIENTMENU_NORTH:
      angle = MapAngle_North;
      break;

    case ORIENTMENU_EAST:
      angle = MapAngle_East;
      break;

    case ORIENTMENU_SOUTH:
      angle = MapAngle_South;
      break;

    case ORIENTMENU_WEST:
      angle = MapAngle_West;
      break;

    default:
      return 0; /* not interested in this menu entry */
  }

  EditWin_set_angle(edit_win, angle);

  if (selected != NULL_ComponentId) {
    E(menu_set_tick(0, id_block->self_id, selected, 0));
  }

  E(menu_set_tick(0, id_block->self_id, id_block->self_component, 1));
  selected = id_block->self_component;

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void OrientMenu_created(ObjectId const menu_id)
{
  OrientMenu_id = menu_id;

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

void OrientMenu_show(EditWin const *const edit_win)
{
  EditWin_show_dbox(edit_win, Toolbox_ShowObject_AsMenu, OrientMenu_id);
}

void OrientMenu_show_at_ptr(EditWin const *const edit_win)
{
  EditWin_show_dbox_at_ptr(edit_win, OrientMenu_id);
}
