/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Easy/Medium/Hard/User mission menus
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

#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "event.h"
#include "toolbox.h"
#include "menu.h"

#include "err.h"
#include "macros.h"
#include "strextra.h"
#include "Debug.h"
#include "hourglass.h"

#include "filescan.h"
#include "IbarMenu.h"
#include "RenameMiss.h"
#include "filesmenus.h"
#include "utils.h"
#include "fsmenu.h"
#include "EMHmenu.h"

static struct
{
  ComponentId ticked;
  ComponentId next_cid;
  int vsn; /* 0 is special value meaning menu not built before */
  bool intern_greyed;
}
menu_states[] =
{
  [FS_MISSION_E] = {.ticked = NULL_ComponentId, .next_cid = 0, .vsn = 0, .intern_greyed = false},
  [FS_MISSION_M] = {.ticked = NULL_ComponentId, .next_cid = 0, .vsn = 0, .intern_greyed = false},
  [FS_MISSION_H] = {.ticked = NULL_ComponentId, .next_cid = 0, .vsn = 0, .intern_greyed = false},
  [FS_MISSION_U] = {.ticked = NULL_ComponentId, .next_cid = 0, .vsn = 0, .intern_greyed = false},
};

/* ---------------- Private functions ---------------- */

static int selectionhandler(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);

  filescan_type const which = (filescan_type)(int)handle;
  Filename leafname, path;

  /* get filename from menu text */
  ON_ERR_RPT_RTN_V(menu_get_entry_text(0, id_block->self_id,
   id_block->self_component, leafname, sizeof(leafname), NULL), 1);

  char const *const emh_path = filescan_get_emh_path(which);
  strcpy(path, emh_path);
  strncat(path, leafname, sizeof(path) - 1 - strlen(emh_path));

  if (id_block->ancestor_id == IbarMenu_id)
  {
    IbarMenu_dosubmenuaction(id_block->ancestor_component, path, false);
  }
  else
  {
    assert(id_block->ancestor_id == RenameMiss_id);

    if (id_block->ancestor_id == RenameMiss_id &&
        id_block->self_component != menu_states[which].ticked) {

      if (menu_states[which].ticked != NULL_ComponentId) {
        E(menu_set_tick(0, id_block->self_id,
                   menu_states[which].ticked, 0));
      }

      E(menu_set_tick(0, id_block->self_id,
                               id_block->self_component, 1));
      menu_states[which].ticked = id_block->self_component;

      RenameMiss_set_path(id_block->ancestor_component, path);
    }
  }

  return 1; /* claim event */
}

static int openhandler(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);

  filescan_type const which = (filescan_type)(int)handle;
  int new_vsn;
  bool grey_internal;
  Filename leafname_buf;
  char const *leafname_ptr = NULL;

  hourglass_on();
  DEBUG("Mission selection menu %d (for dir %d) opened", id_block->self_id,
  which);

  /* Get pointer to array of leaf names of files within this directory */
  filescan_leafname *leaves = filescan_get_leaf_names(which, &new_vsn);
  if (leaves == NULL) {
    hourglass_off();
    return 1; /* error */
  }

  if (id_block->ancestor_id == IbarMenu_id) {
    grey_internal = IbarMenu_grey_intern_files(id_block->ancestor_component);
  } else {
    grey_internal = false;

    assert(id_block->ancestor_id == RenameMiss_id);
    if (id_block->ancestor_id == RenameMiss_id) {
      Filename path_buf;
      RenameMiss_get_path(id_block->ancestor_component, &path_buf);

      if (strnicmp(path_buf, filescan_get_emh_path(which), 2) == 0) {
        strcpy(leafname_buf, (char *)path_buf + 2);
        leafname_ptr = leafname_buf;
      }
    }
  }

  if (menu_states[which].vsn != new_vsn) {
    /* Rebuild menu */
    if (wipe_menu(id_block->self_id, menu_states[which].next_cid - 1)) {
      menu_states[which].ticked =
        fsmenu_build(id_block->self_id, leaves,
                     &menu_states[which].next_cid, true, false,
                     grey_internal, leafname_ptr);
      /* don't care about excluding "Blank" */

      menu_states[which].vsn = new_vsn;
      menu_states[which].intern_greyed = grey_internal;
    }
    hourglass_off();
    return 1; /* claim event */
  }

  if (menu_states[which].intern_greyed != grey_internal) {
    /* No need to rebuild menu - just update fading of internal files */
    fsmenu_grey_internal(id_block->self_id, leaves, true, grey_internal);

    menu_states[which].intern_greyed = grey_internal;
  }

  /* Remove any existing menu tick */
  if (menu_states[which].ticked != NULL_ComponentId) {
    DEBUG("Unticking entry %d of menu %d", menu_states[which].ticked,
          id_block->self_id);
    E(menu_set_tick(0, id_block->self_id,
                             menu_states[which].ticked, 0));

    menu_states[which].ticked = NULL_ComponentId;
  }

  if (leafname_ptr != NULL) {
    /* Search the menu for an entry matching the current leaf name */
    for (ComponentId entry = 0; entry < menu_states[which].next_cid; entry++) {
      Filename read_name;
      if (E(menu_get_entry_text(0, id_block->self_id, entry,
                                &*read_name, sizeof(read_name), 0)))
        break;

      if (stricmp(read_name, leafname_ptr) == 0) {
        /* Tick menu entry to show it is selected */
        E(menu_set_tick(0, id_block->self_id, entry, 1));
        DEBUG("Ticking entry %d of menu %d", entry, id_block->self_id);
        menu_states[which].ticked = entry;
        break;
      }
    }
  }
  hourglass_off();
  return 1; /* claim event */
}

static void EMHmenu_created(ObjectId const id, filescan_type which)
{
  /* Install event handlers
  (cannot cast enum to pointer - don't know why) */
  assert(which >= FS_MISSION_E);
  assert(which <= FS_MISSION_U);

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Menu_Selection, selectionhandler },
    { Menu_AboutToBeShown, openhandler }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler,
                                      (void *)(int)which));
  }
}

/* ---------------- Public functions ---------------- */

void easymenu_created(ObjectId const id)
{
  EMHmenu_created(id, FS_MISSION_E);
}

void mediummenu_created(ObjectId const id)
{
  EMHmenu_created(id, FS_MISSION_M);
}

void hardmenu_created(ObjectId const id)
{
  EMHmenu_created(id, FS_MISSION_H);
}

void usermenu_created(ObjectId const id)
{
  EMHmenu_created(id, FS_MISSION_U);
}
