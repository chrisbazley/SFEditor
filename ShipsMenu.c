/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Menu for selecting ship type to launch
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

#include <string.h>
#include <assert.h>

#include "event.h"
#include "toolbox.h"
#include "menu.h"

#include "err.h"
#include "msgtrans.h"
#include "Macros.h"
#include "strextra.h"

#include "Session.h"
#include "debug.h"
#include "utils.h"
#include "graphicsdata.h"
#include "EditWin.h"
#include "shipsmenu.h"
#include "Ships.h"
#include "Defenc.h"
#include "DataType.h"
#include "Mission.h"

static ComponentId which_ticked = NULL_ComponentId;
static Filename graphics_set = "";

static char menu_text[ShipType_Fighter4 - ShipType_Fighter1 + 1][32];

/* ---------------- Private functions ---------------- */

static int menu_selection(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event);
  NOT_USED(event_code);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  EditSession *const session = EditWin_get_session(edit_win);

  DEBUG("Ship menu item %d selected", id_block->self_component);

  if (id_block->self_component != which_ticked)
  {
    if (which_ticked != NULL_ComponentId)
    {
      E(menu_set_tick(0, id_block->self_id, which_ticked, 0));
    }

    which_ticked = id_block->self_component;
    E(menu_set_tick(0, id_block->self_id, which_ticked, 1));

    DefencesData *const defences = mission_get_defences(Session_get_mission(session));
    defences_set_ship_type(defences, (ShipType)(ShipType_Fighter1 + which_ticked));
    Session_notify_changed(session, DataType_Mission);
  }

  return 1; /* claim event */
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Rebuild menu (if necessary) and tick appropriate entry */
  NOT_USED(event);
  NOT_USED(event_code);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  EditSession *const session = EditWin_get_session(edit_win);
  FilenamesData const *const filenames = Session_get_filenames(session);

  DefencesData const *const defences = mission_get_defences(Session_get_mission(session));
  ShipType const launch_type = defences_get_ship_type(defences);

  if (stricmp(filenames_get(filenames, DataType_PolygonMeshes), graphics_set) != 0) {
    DEBUG("Rebuilding ships menu for graphics set '%s' (was '%s')",
    filenames_get(filenames, DataType_PolygonMeshes), graphics_set);

    /* Wipe all entries from menu */
    if (*graphics_set != '\0') {
      if (!wipe_menu(id_block->self_id, 3))
        return 1; /* error - return prematurely (claiming event) */
      which_ticked = NULL_ComponentId;
    }

    /* Rebuild menu for different graphics set */
    STRCPY_SAFE(graphics_set, filenames_get(filenames, DataType_PolygonMeshes));
    MenuTemplateEntry new_entry = {0};

    StringBuffer ship_name;
    stringbuffer_init(&ship_name);

    for (ShipType ship_no = ShipType_Fighter1;
         ship_no <= ShipType_Fighter4;
         ship_no++)
    {
      if (ship_no == launch_type)
      {
        new_entry.flags = Menu_Entry_Ticked;
        which_ticked = new_entry.component_id;
      }
      else
      {
        new_entry.flags = 0;
      }
      new_entry.click_event = Menu_Selection;

      if (!get_shipname_from_type(&ship_name, graphics_set, ship_no))
      {
        report_error(SFERROR(NoMem), "", "");
        break;
      }

      STRCPY_SAFE(menu_text[new_entry.component_id],
                  stringbuffer_get_pointer(&ship_name));

      new_entry.text = menu_text[new_entry.component_id];
      new_entry.max_text = sizeof(menu_text[new_entry.component_id]);

      DEBUGF("Adding entry %d to menu %d ('%s', tick %d)\n",
      new_entry.component_id, id_block->self_id, new_entry.text,
      TEST_BITS(new_entry.flags, Menu_Entry_Ticked));

      ON_ERR_RPT_RTN_V(menu_add_entry(0, id_block->self_id, Menu_AddEntryAtEnd,
        (char *)&new_entry, 0), 1);

      new_entry.component_id++;
    } /* next ship_no */

    stringbuffer_destroy(&ship_name);

  } else {
    ComponentId const new_tick = (ComponentId)(launch_type - ShipType_Fighter1);
    if (new_tick != which_ticked) {
      DEBUG("Moving menu tick from %d to %d", which_ticked, new_tick);

      /* Remove any existing menu tick */
      if (which_ticked != NULL_ComponentId)
      {
        E(menu_set_tick(0, id_block->self_id, which_ticked, 0));
      }

      /* Tick menu entry corresponding to current launch type */
      which_ticked = new_tick;
      E(menu_set_tick(0, id_block->self_id, which_ticked, 1));
    }
  }

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void ShipsMenu_created(ObjectId const id)
{
  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Menu_Selection, menu_selection },
    { Menu_AboutToBeShown, about_to_be_shown },
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}
