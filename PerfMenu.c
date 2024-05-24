/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Menu for selecting ship type for which to set performance
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
#include "PerfMenu.h"
#include "Ships.h"
#include "Defenc.h"
#include "DataType.h"
#include "Filenames.h"
#include "FilenamesData.h"

static Filename graphics_set = "";

/* ---------------- Private functions ---------------- */

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Update menu (if necessary) */
  NOT_USED(event);
  NOT_USED(event_code);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  EditSession *const session = EditWin_get_session(edit_win);
  FilenamesData const *const filenames = Session_get_filenames(session);

  if (!stricmp(filenames_get(filenames, DataType_PolygonMeshes), graphics_set)) {
    return 1;
  }

  DEBUG("Rebuilding performance menu for graphics set '%s' (was '%s')",
        filenames_get(filenames, DataType_PolygonMeshes), graphics_set);

  STRCPY_SAFE(graphics_set, filenames_get(filenames, DataType_PolygonMeshes));

  StringBuffer ship_name;
  stringbuffer_init(&ship_name);

  for (ShipType ship_no = ShipType_Fighter1;
       ship_no <= ShipType_Fighter4;
       ship_no++)
  {
    if (!get_shipname_from_type(&ship_name, graphics_set, ship_no))
    {
      report_error(SFERROR(NoMem), "", "");
      break;
    }

    if (E(menu_set_entry_text(0, id_block->self_id, (ComponentId)ship_no,
           msgs_lookup_subn("PerfMenuEntry", 1, stringbuffer_get_pointer(&ship_name)))))
    {
      break;
    }
  }

  for (ShipType ship_no = ShipType_Big1;
       ship_no <= ShipType_Big3;
       ship_no++)
  {
    if (!get_shipname_from_type(&ship_name, graphics_set, ship_no))
    {
      report_error(SFERROR(NoMem), "", "");
      break;
    }

    if (E(menu_set_entry_text(0, id_block->self_id, (ComponentId)ship_no,
            msgs_lookup_subn("PerfMenuEntry", 1, stringbuffer_get_pointer(&ship_name)))))
    {
      break;
    }
  }

  stringbuffer_destroy(&ship_name);
  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void PerfMenu_created(ObjectId const id)
{
  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Menu_AboutToBeShown, about_to_be_shown },
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}
