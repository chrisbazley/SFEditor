/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Maps menu
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
#include <assert.h>

#include "event.h"
#include "toolbox.h"
#include "menu.h"

#include "err.h"
#include "msgtrans.h"
#include "Macros.h"
#include "strextra.h"
#include "Debug.h"
#include "hourglass.h"

#include "filescan.h"
#include "IbarMenu.h"
#include "filesmenus.h"
#include "utils.h"
#include "MapsMenu.h"
#include "RenameMap.h"
#include "fsmenu.h"

static ComponentId ticked = NULL_ComponentId;
static filescan_leafname *combined_list;

/* ---------------- Private functions ---------------- */

static int menu_selection(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Click on maps menu */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  Filename leafname;

  /* get filename from menu text */
  ON_ERR_RPT_RTN_V(menu_get_entry_text(0, id_block->self_id,
  id_block->self_component, leafname, sizeof(leafname), NULL), 1);

  if (id_block->ancestor_id == IbarMenu_id) {
    IbarMenu_dosubmenuaction(id_block->ancestor_component, leafname, true);
  } else {
    assert(id_block->ancestor_id == RenameMap_id);

    if (id_block->ancestor_id == RenameMap_id &&
        id_block->self_component != ticked) {

      if (ticked != NULL_ComponentId)
        E(menu_set_tick(0, id_block->self_id, ticked, 0));

      E(menu_set_tick(0, id_block->self_id,
        id_block->self_component, 1));

      ticked = id_block->self_component;

      RenameMap_set_path(id_block->ancestor_component, leafname);
    }
  }

  return 1; /* claim event */
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Rebuild maps menu (dir was scanned when root menu opened) */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  char const *token = NULL;

  if (id_block->ancestor_id == IbarMenu_id)
  {
    token = IbarMenu_get_sub_menu_title();
  }
  else
  {
    assert(id_block->ancestor_id == RenameMap_id);
    token = RenameMap_get_popup_title(id_block->ancestor_component);
  }

  E(menu_set_title(0, id_block->self_id, msgs_lookup(token)));

  hourglass_on();

  int new_vsn_sprscape, new_vsn_fxdobj, new_vsn_anims;
  filescan_leafname *const sprscape_leaves = filescan_get_leaf_names(FS_BASE_SPRSCAPE,
    &new_vsn_sprscape);

  filescan_leafname *const fxdobj_leaves = filescan_get_leaf_names(FS_BASE_FXDOBJ,
    &new_vsn_fxdobj);

  filescan_leafname *const anims_leaves = filescan_get_leaf_names(FS_BASE_ANIMS,
    &new_vsn_anims);

  hourglass_off();

  if (fxdobj_leaves == NULL || sprscape_leaves == NULL || anims_leaves == NULL) {
    return 1; /* error */
  }

  /* We must keep the combined list of leafnames for lifetime of the menu
     or risk dangling pointers */
  filescan_leafname *partial_combined_list =
    filescan_combine_filenames(sprscape_leaves, fxdobj_leaves);

  if (partial_combined_list == NULL)
  {
    return 1; /* error */
  }

  filescan_leafname *const new_combined_list =
    filescan_combine_filenames(partial_combined_list, anims_leaves);

  FREE_SAFE(partial_combined_list);

  if (new_combined_list == NULL)
  {
    return 1; /* error */
  }

  free(combined_list);
  combined_list = new_combined_list;

  char const *leafname_ptr = NULL;
  Filename leafname_buf;
  bool grey_internal = false;
  static bool intern_greyed;

  if (id_block->ancestor_id == IbarMenu_id)
  {
    grey_internal = IbarMenu_grey_intern_files(id_block->ancestor_component);
  }
  else
  {
    assert(id_block->ancestor_id == RenameMap_id);
    if (id_block->ancestor_id == RenameMap_id)
    {
      RenameMap_get_path(id_block->ancestor_component, &leafname_buf);
      leafname_ptr = leafname_buf;
    }
  }

  static int vsn_sprscape, vsn_fxdobj, vsn_anims;
  static ComponentId next_free;

  if (vsn_fxdobj != new_vsn_fxdobj ||
      vsn_sprscape != new_vsn_sprscape ||
      vsn_anims != new_vsn_anims)
  {
    /* Rebuild menu */
    if (wipe_menu(id_block->self_id, next_free - 1))
    {
      ticked = fsmenu_build(id_block->self_id, combined_list, &next_free,
               false, false, grey_internal, leafname_ptr); /* exclude "Blank" */

      vsn_fxdobj = new_vsn_fxdobj;
      vsn_sprscape = new_vsn_sprscape;
      vsn_anims = new_vsn_anims;
      intern_greyed = grey_internal;
    }
    hourglass_off();
    return 1; /* claim event */
  }

  if (intern_greyed != grey_internal)
  {
    /* No need to rebuild menu - just update fading of internal files */
    fsmenu_grey_internal(id_block->self_id, combined_list, false,
    grey_internal);

    intern_greyed = grey_internal;
  }


  /* Remove any existing menu tick */
  if (ticked != NULL_ComponentId) {
    DEBUG("Unticking entry %d of menu %d", ticked, id_block->self_id);
    E(menu_set_tick(0, id_block->self_id, ticked, 0));
    ticked = NULL_ComponentId;
  }

  if (leafname_ptr != NULL) {
    /* Search the menu for an entry matching the current leaf name */
    for (ComponentId entry = 0; entry < next_free; entry++) {
      Filename read_name;
      if (E(menu_get_entry_text(0, id_block->self_id, entry,
      (char *)&read_name, sizeof(read_name), 0)))
        break;

      if (stricmp(read_name, leafname_ptr) == 0) {
        /* Tick menu entry to show it is selected */
        E(menu_set_tick(0, id_block->self_id, entry, 1));
        DEBUG("Ticking entry %d of menu %d", entry, id_block->self_id);
        ticked = entry;
        break;
      }
    }
  }

  hourglass_off();
  return 1; /* claim event */
}

static void mapsmenu_cleanup(void)
{
  free(combined_list);
}

/* ---------------- Public functions ---------------- */

void mapsmenu_created(ObjectId const id)
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
  atexit(mapsmenu_cleanup);
}
