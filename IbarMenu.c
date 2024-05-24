/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Root iconbar menu
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

#include "stdlib.h"
#include <string.h>
#include <stdbool.h>
#include <assert.h>

/* RISC OS library files */
#include "event.h"
#include "toolbox.h"
#include "menu.h"

#include "err.h"
#include "msgtrans.h"
#include "Macros.h"
#include "ViewsMenu.h"

#include "filescan.h"
#include "filepaths.h"
#include "utils.h"
#include "IbarMenu.h"
#include "Session.h"
#include "PathUtils.h"
#include "Config.h"

/* Menu entries */
enum {
  IBARMENU_CREATE    = 0x7,
  IBARMENU_OPEN      = 0x4,
  IBARMENU_DELETE    = 0xb,
  IBARMENU_VIEWS     = 0x9,
};

ObjectId IbarMenu_id = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Grey/ungrey entries on the root iconbar menu */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  E(menu_set_fade(0, id_block->self_id, IBARMENU_CREATE, !Config_get_read_dir()));
  E(menu_set_fade(0, id_block->self_id, IBARMENU_OPEN, !Config_get_read_dir()));
  E(menu_set_fade(0, id_block->self_id, IBARMENU_DELETE, !Config_get_read_dir()));

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void IbarMenu_created(ObjectId const id)
{
  IbarMenu_id = id;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Menu_AboutToBeShown, about_to_be_shown }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }

  EF(ViewsMenu_parentcreated(id, IBARMENU_VIEWS));
}

bool IbarMenu_grey_intern_files(ComponentId const menu_entry)
{
  return (menu_entry == IBARMENU_DELETE && Config_get_use_extern_levels_dir());
}

char const *IbarMenu_get_sub_menu_title(void)
{
  return "BMtitle";
}

void IbarMenu_dosubmenuaction(ComponentId const menu_entry, const char *const file_path,
  bool const map)
{
  switch (menu_entry) {
    case IBARMENU_OPEN:
      if (map)
      {
        Session_open_map(file_path);
      }
      else
      {
        Session_open_mission(file_path);
      }
      break;

    case IBARMENU_DELETE:
      if (map)
      {
        if (dialogue_confirm(msgs_lookup_subn("ConfirmDelMap", 1, file_path),
            "DelCanBut")) {
          filepaths_delete_map(file_path);
        }
      }
      else
      {
        if (dialogue_confirm(msgs_lookup_subn("ConfirmDelMiss", 1, file_path),
            "DelCanBut")) {
          filepaths_delete_mission(file_path);
        }
      }
      break;

    default:
      assert(false);
      break;
  }
}
