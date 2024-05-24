/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Root of pop-up source menu for Rename Mission dbox
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

#include "filescan.h"
#include "RenMissMenu.h"

enum {
  RENMISSMENU_EASY   = 0x4,
  RENMISSMENU_MEDIUM = 0x5,
  RENMISSMENU_HARD   = 0x6,
  RENMISSMENU_USER   = 0x8,
};

/* ---------------- Private functions ---------------- */

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  struct miss_menu_info {
    int vsn;
    filescan_type directory;
    ComponentId component_id;
  };

  static struct miss_menu_info knowledge[] =
  {
    {0, FS_MISSION_E, RENMISSMENU_EASY},
    {0, FS_MISSION_M, RENMISSMENU_MEDIUM},
    {0, FS_MISSION_H, RENMISSMENU_HARD},
    {0, FS_MISSION_U, RENMISSMENU_USER}
  };

  for (size_t i = 0; i < ARRAY_SIZE(knowledge); i++) {
    /* Are there any mission files on this pyramid? */
    int new_vsn;
    const filescan_leafname *const leaves =
      filescan_get_leaf_names(knowledge[i].directory, &new_vsn);
    if (knowledge[i].vsn != new_vsn && leaves != NULL) {
      knowledge[i].vsn = new_vsn;
      E(menu_set_fade(0, id_block->self_id, knowledge[i].component_id,
                 (leaves == NULL || *leaves[0].leaf_name == '\0')));
    }
  }

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void RenMissMenu_created(ObjectId const id)
{
  /* Install handlers */
  EF(event_register_toolbox_handler(id, Menu_AboutToBeShown, about_to_be_shown, NULL));
}
