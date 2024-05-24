/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Various menus of files
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

#include "stdio.h"
#include "stdlib.h"
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "event.h"
#include "toolbox.h"
#include "menu.h"
#include "flex.h"

#include "err.h"
#include "msgtrans.h"
#include "Macros.h"
#include "strextra.h"
#include "hourglass.h"

#include "filescan.h"
#include "Session.h"
#include "Filenames.h"
#include "filepaths.h"
#include "utils.h"
#include "filesmenus.h"
#include "EditWin.h"
#include "debug.h"
#include "Palette.h"
#include "MapToolbar.h"
#include "GraphicsFiles.h"
#include "MapAnims.h"
#include "ObjGfxMesh.h"
#include "GfxConfig.h"
#include "MapEditChg.h"
#include "DataType.h"
#include "fsmenu.h"

struct fm_menu_info {
  ComponentId ticked;
  ComponentId next_cid;
  int vsn; /* 0 is special value meaning menu not built before */
};

static struct fm_menu_info menu_states[] =
{
  [FS_BASE_SPRSCAPE] = {NULL_ComponentId, .next_cid = 0, .vsn = 0},
  [FS_BASE_FXDOBJ] = {NULL_ComponentId, .next_cid = 0, .vsn = 0},
  [FS_BASE_ANIMS] = {NULL_ComponentId, .next_cid = 0, .vsn = 0},
  [FS_SPRITES] = {NULL_ComponentId, .next_cid = 0, .vsn = 0},
  [FS_GRAPHICS] = {NULL_ComponentId, .next_cid = 0, .vsn = 0},
  [FS_HILL] ={NULL_ComponentId, .next_cid = 0, .vsn = 0},
  [FS_PALETTE] = {NULL_ComponentId, .next_cid = 0, .vsn = 0},
  [FS_SKY] = {NULL_ComponentId, .next_cid = 0, .vsn = 0},
  [FS_PLANETS] = {NULL_ComponentId, .next_cid = 0, .vsn = 0}
};

/* ---------------- Private functions ---------------- */

static int menu_selection(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event);
  NOT_USED(event_code);
  assert(id_block);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  EditSession *const session = EditWin_get_session(edit_win);

  filescan_type const which = (filescan_type)(int)handle;
  DataType const data_type = filescan_get_data_type(which);

  DEBUG("Entry %d of files menu %d (for dir %d) selected",
        id_block->self_component, id_block->self_id, which);

  DEBUG("%d entries%s", menu_states[which].next_cid,
        data_type_allow_none(data_type) ? " inc. 'None'" : "");

  if (id_block->self_component == menu_states[which].ticked) {
    DEBUG("Menu entry already selected");
    return 1;
  }

  Filename leaf = "";

  if (data_type_allow_none(data_type) &&
      id_block->self_component == menu_states[which].next_cid - 1) {
    DEBUG("'None' menu entry selected");
    strcpy(leaf, NO_FILE);
  } else {
    ON_ERR_RPT_RTN_V(menu_get_entry_text(0, id_block->self_id,
                     id_block->self_component, leaf, sizeof(leaf), NULL), 1);
  }

  if (Session_switch_file(session, data_type, leaf))
  {
    if (menu_states[which].ticked != NULL_ComponentId) {
      E(menu_set_tick(0, id_block->self_id, menu_states[which].ticked, 0));
    }

    E(menu_set_tick(0, id_block->self_id, id_block->self_component, 1));

    menu_states[which].ticked = id_block->self_component;
  }
  return 1; /* claim event */
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Rebuild menu (if necessary) and tick appropriate entry */
  NOT_USED(event);
  NOT_USED(event_code);
  assert(id_block);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  EditSession *const session = EditWin_get_session(edit_win);

  int new_vsn;
  /* Directory to use is encoded in handle */
  filescan_type const which = (filescan_type)(int)handle;
  DataType const data_type = filescan_get_data_type(which);

  hourglass_on();

  DEBUG("Files selection menu %d (for dir %d) opened", id_block->self_id, which);

  /* Get pointer to array of leaf names of files within this directory */
  filescan_leafname *const f = filescan_get_leaf_names(which, &new_vsn);
  FilenamesData const *const filenames = Session_get_filenames(session);
  char const *const selected_name = filenames_get(filenames, filescan_get_data_type(which));
  DEBUG("Leaf name for directory %d is %s", which, selected_name);

  if (menu_states[which].vsn != new_vsn) {
    /* Wipe all entries from menu */
    DEBUG("Array of leaf names may have changed - current version %d, "
          "latest is %d", menu_states[which].vsn, new_vsn);
    menu_states[which].vsn = new_vsn;

    if (menu_states[which].next_cid > 0) {
      if (!wipe_menu(id_block->self_id, menu_states[which].next_cid - 1)) {
        hourglass_off();
        return 1; /* error - return prematurely (claiming event) */
      }
    }

    if (f != NULL) {
      /* Add entries to the menu from the array of filenames
         (don't care about excluding "Blank") */
      menu_states[which].ticked = fsmenu_build(id_block->self_id, f,
                                    &menu_states[which].next_cid, true,
                                    data_type_allow_none(data_type), false,
                                    selected_name);
      DEBUG("fsmenu_build informs us that entry %d of menu %d is ticked",
      menu_states[which].ticked, id_block->self_id);
    }

  } else {
    /* Remove any existing menu tick */
    if (menu_states[which].ticked != NULL_ComponentId) {
      DEBUG("Unticking entry %d of menu %d", menu_states[which].ticked,
            id_block->self_id);

      E(menu_set_tick(0, id_block->self_id, menu_states[which].ticked,
                               0));

      menu_states[which].ticked = NULL_ComponentId;
    }

    /* Search the menu for an entry matching the current leaf name */
    for (ComponentId entry = 0; entry < menu_states[which].next_cid; entry++) {
      Filename read_name;
      ON_ERR_RPT_RTN_V(menu_get_entry_text(0, id_block->self_id, entry,
                                           (char *)&read_name,
                                           sizeof(read_name), 0), 1);

      assert(menu_states[which].next_cid > 0);
      bool const tick_none = (data_type_allow_none(data_type) &&
                              entry == menu_states[which].next_cid-1 &&
                              strcmp(selected_name, NO_FILE) == 0);

      if (stricmp(read_name, selected_name) == 0 || tick_none) {
        /* Tick menu entry to show it is selected */
        E(menu_set_tick(0, id_block->self_id, entry, 1));

        DEBUG("Ticking entry %d of menu %d for directory %d", entry,
        id_block->self_id, which);

        menu_states[which].ticked = entry;
        break;
      }
    }
  }
  hourglass_off();

  if (menu_states[which].ticked == NULL_ComponentId) {
    DEBUG("Could not find 'current' leaf name in menu");
  }

  return 1; /* claim event */
}

static void filesmenu_created(ObjectId const id, filescan_type const which)
{
  /* Install event handlers
  (cannot cast enum to pointer - don't know why) */
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
                                      handlers[i].handler, (void *)(int)which));
  }
}

/* ---------------- Public functions ---------------- */

void tilesetmenu_created(ObjectId const id)
{
  filesmenu_created(id, FS_SPRITES);
}

void polysetmenu_created(ObjectId const id)
{
  filesmenu_created(id, FS_GRAPHICS);
}

void coloursmenu_created(ObjectId const id)
{
  filesmenu_created(id, FS_PALETTE);
}

void hillcolmenu_created(ObjectId const id)
{
  filesmenu_created(id, FS_HILL);
}

void basefxdmenu_created(ObjectId const id)
{
  filesmenu_created(id, FS_BASE_FXDOBJ);
}

void basesprmenu_created(ObjectId const id)
{
  filesmenu_created(id, FS_BASE_SPRSCAPE);
}

void skymenu_created(ObjectId const id)
{
  filesmenu_created(id, FS_SKY);
}

void planetsmenu_created(ObjectId const id)
{
  filesmenu_created(id, FS_PLANETS);
}
