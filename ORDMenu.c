/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Open / Rename / Delete iconbar submenu
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

#include "event.h"
#include "toolbox.h"
#include "menu.h"

#include "err.h"
#include "msgtrans.h"
#include "Macros.h"

#include "Utils.h"
#include "filescan.h"
#include "ORDmenu.h"

enum {
  ORDMENU_BASEMAP = 0x0,
  ORDMENU_EASY    = 0x4,
  ORDMENU_MEDIUM  = 0x5,
  ORDMENU_HARD    = 0x6,
  ORDMENU_USER    = 0x8,
};

/* ---------------- Private functions ---------------- */

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  struct miss_menu_info {
    filescan_type directory;
    ComponentId component_id;
  };

  const struct miss_menu_info knowledge[] =
  {
    {FS_MISSION_E, ORDMENU_EASY}, {FS_MISSION_M, ORDMENU_MEDIUM},
    {FS_MISSION_H, ORDMENU_HARD}, {FS_MISSION_U, ORDMENU_USER}
  };

  /* Set menu title appropriately according to parent menu entry */
  {
    char title_buffer[12];
    if (!E(menu_get_entry_text(0, id_block->parent_id,
                               id_block->parent_component, title_buffer,
                               sizeof(title_buffer), NULL)))
      E(menu_set_title(0, id_block->self_id, title_buffer));
  }

  for (size_t i = 0; i < ARRAY_SIZE(knowledge); i++) {
    /* Are there any mission files on this pyramid? */
    E(menu_set_fade(0, id_block->self_id, knowledge[i].component_id,
                             !filescan_dir_not_empty(
                               knowledge[i].directory)));
  }

  /* Are there any base map files? */
  E(menu_set_fade(0, id_block->self_id, ORDMENU_BASEMAP,
                           !filescan_dir_not_empty(FS_BASE_SPRSCAPE) &&
                           !filescan_dir_not_empty(FS_BASE_FXDOBJ)));

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void ORDMenu_created(ObjectId const id)
{
  /* Install handlers */
  EF(event_register_toolbox_handler(id, Menu_AboutToBeShown,
                                    about_to_be_shown, NULL));
}
