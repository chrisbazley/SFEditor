/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map edit_win zoom menu
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
#include "utils.h"
#include "ZoomMenu.h"

/* --------------------- Gadgets -------------------- */

enum {
  ZOOMMENU_XHALF = 0x05, /* 1:16 */
  ZOOMMENU_X1    = 0x00, /* 1:8 */
  ZOOMMENU_X2    = 0x01, /* 1:4 */
  ZOOMMENU_X4    = 0x02, /* 1:2 */
  ZOOMMENU_X8    = 0x03, /* 1:1 */
  ZOOMMENU_X16   = 0x04, /* 2:1 */
  ZOOMMENU_X32   = 0x06, /* 4:1 */
};

static ObjectId ZoomMenu_id = NULL_ObjectId;
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

  int const zoom = EditWin_get_zoom(edit_win);

  if (selected != NULL_ComponentId)
    E(menu_set_tick(0, id_block->self_id, selected, 0));

  assert(zoom >= -2 && zoom <= 4);
  switch (zoom) {
    case 4:
      selected = ZOOMMENU_XHALF;
      break;

    case 3:
      selected = ZOOMMENU_X1;
      break;

    case 2:
      selected = ZOOMMENU_X2;
      break;

    case 1:
      selected = ZOOMMENU_X4;
      break;

    case 0:
      selected = ZOOMMENU_X8;
      break;

    case -1:
      selected = ZOOMMENU_X16;
      break;

    case -2:
      selected = ZOOMMENU_X32;
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

  if (id_block->self_component == selected)
    return 1; /* already selected - nothing to do here */

  int zoom_factor = 0;
  switch (id_block->self_component) {
    case ZOOMMENU_XHALF:
      zoom_factor = 4;
      break;

    case ZOOMMENU_X1:
      zoom_factor = 3;
      break;

    case ZOOMMENU_X2:
      zoom_factor = 2;
      break;

    case ZOOMMENU_X4:
      zoom_factor = 1;
      break;

    case ZOOMMENU_X8:
      zoom_factor = 0;
      break;

    case ZOOMMENU_X16:
      zoom_factor = -1;
      break;

    case ZOOMMENU_X32:
      zoom_factor = -2;
      break;

    default:
      return 0; /* not interested in this menu entry */
  }

  EditWin_set_zoom(edit_win, zoom_factor);

  if (selected != NULL_ComponentId)
    E(menu_set_tick(0, id_block->self_id, selected, 0));

  E(menu_set_tick(0, id_block->self_id, id_block->self_component, 1));
  selected = id_block->self_component;

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void ZoomMenu_created(ObjectId const menu_id)
{
  ZoomMenu_id = menu_id;

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

void ZoomMenu_show(EditWin const *const edit_win)
{
  EditWin_show_dbox(edit_win, Toolbox_ShowObject_AsMenu, ZoomMenu_id);
}

void ZoomMenu_show_at_ptr(EditWin const *const edit_win)
{
  EditWin_show_dbox_at_ptr(edit_win, ZoomMenu_id);
}
