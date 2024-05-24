/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map rename dialogue box
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
#include <string.h>
#include "stdlib.h"
#include <assert.h>

/* RISC OS library files */
#include "event.h"
#include "toolbox.h"
#include "window.h"
#include "gadgets.h"

#include "hourglass.h"
#include "err.h"
#include "Macros.h"
#include "msgtrans.h"
#include "PathTail.h"
#include "strextra.h"
#include "debug.h"
#include "GadgetUtil.h"

#include "RenameMap.h"
#include "Utils.h"
#include "Config.h"
#include "filepaths.h"
#include "pathutils.h"
#include "filescan.h"

/* Gadget numbers */
enum {
  RENAMEMAP_OPTION_COPY   = 0x74,
  RENAMEMAP_ACTION_CANCEL = 0x70,
  RENAMEMAP_ACTION_RENAME = 0x71,
  RENAMEMAP_DISP_SOURCE   = 0x78,
  RENAMEMAP_WRITABLE_DEST = 0x7c,
  RENAMEMAP_POPUP_SOURCE  = 0x79,
  RENAMEMAP_POPUP_DEST    = 0x7b,
};

ObjectId RenameMap_id = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

static void config_copy(char *const source_name)
{
  if (Config_get_use_extern_levels_dir()) {
    /* Forceably set 'Copy' option if any untouchable files of new source name
    exist in internal directory */
    bool exists = false;
    static char const *const subdirs[] = {
      BASEMAP_DIR, BASEGRID_DIR, BASEANIMS_DIR
    };
    for (size_t i = 0; !exists && i < ARRAY_SIZE(subdirs); ++i) {
      char *const map_intern_path = make_file_path_in_subdir(
        Config_get_read_dir(), subdirs[i], source_name);

      if (map_intern_path) {
        exists = file_exists(map_intern_path);
        free(map_intern_path);
      }
    }
    if (exists) {
      DEBUG("Setting copy option");
      E(optionbutton_set_state(0, RenameMap_id, RENAMEMAP_OPTION_COPY,
      true));
    }
    DEBUG("%s copy option", exists ? "Locking" : "Unlocking");
    E(set_gadget_faded(RenameMap_id, RENAMEMAP_OPTION_COPY, exists));
  } else {
    DEBUG("Unlocking copy option");
    E(set_gadget_faded(RenameMap_id, RENAMEMAP_OPTION_COPY, false));
  }
}

static void setup_win(void)
{
  hourglass_on();
  filescan_leafname *const sprscape_leaves = filescan_get_leaf_names(FS_BASE_SPRSCAPE, NULL);
  filescan_leafname *const fxdobj_leaves = filescan_get_leaf_names(FS_BASE_FXDOBJ, NULL);
  filescan_leafname *const anims_leaves = filescan_get_leaf_names(FS_BASE_ANIMS, NULL);
  hourglass_off();

  if (sprscape_leaves == NULL || fxdobj_leaves == NULL || anims_leaves == NULL)
  {
    return; /* error */
  }

  filescan_leafname *partial_combined_list =
    filescan_combine_filenames(sprscape_leaves, fxdobj_leaves);

  if (partial_combined_list == NULL)
  {
    return; /* error */
  }

  filescan_leafname *const new_combined_list = filescan_combine_filenames(
    partial_combined_list, anims_leaves);

  FREE_SAFE(partial_combined_list);

  if (new_combined_list == NULL)
  {
    return; /* error */
  }

  assert(*new_combined_list[0].leaf_name != '\0');

  {
    /* Get currently (or soon-to-be) displayed source leaf name */
    Filename source_name;
    ON_ERR_RPT_RTN(displayfield_get_value(0, RenameMap_id,
      RENAMEMAP_DISP_SOURCE, source_name, sizeof(source_name), NULL));

    /* Check to see whether that file exists on our list */
    for (size_t i = 0; *new_combined_list[i].leaf_name != '\0'; i++) {
      if (stricmp(new_combined_list[i].leaf_name, source_name) == 0) {
        DEBUG("Previous source leaf name '%s' validates", source_name);
        config_copy(source_name);
        return; /* we have matched displayed leaf name with one on list */
      }
    }

    /* Have reached end of list without match - substitute 1st known name */
    DEBUG("Substituting leaf name '%s' for previous source '%s'",
    new_combined_list[0].leaf_name, source_name);

    RenameMap_set_path(RENAMEMAP_POPUP_SOURCE,
      new_combined_list[0].leaf_name);
  }

  free(new_combined_list);
}

static int actionbutton_selected(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Click on menu item */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  int copy;
  Filename dest_name, source_name;

  switch (id_block->self_component) {
    case RENAMEMAP_ACTION_CANCEL:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust)) {
        /* restore settings */
        setup_win();
      }
      return 1; /* claim event */

    case RENAMEMAP_ACTION_RENAME:
      ON_ERR_RPT_RTN_V(optionbutton_get_state(0, id_block->self_id,
        RENAMEMAP_OPTION_COPY, &copy), 1);

      ON_ERR_RPT_RTN_V(writablefield_get_value(0, id_block->self_id,
        RENAMEMAP_WRITABLE_DEST, dest_name, sizeof(dest_name), NULL), 1);

      ON_ERR_RPT_RTN_V(displayfield_get_value(0, id_block->self_id,
        RENAMEMAP_DISP_SOURCE, source_name, sizeof(source_name), NULL), 1);

      if (filepaths_map_exists(dest_name) &&
          !dialogue_confirm(msgs_lookup_subn("MultOv", 1, dest_name), "OvBut"))
      {
        return 1; /* aborted */
      }

      if (!filepaths_rename_map(source_name, dest_name, copy))
        return 1; /* failed */

      /* Close dialogue box on completion unless ADJUST-click */
      if (!TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust))
        E(toolbox_hide_object(0, id_block->self_id));

      return 1; /* claim event */
  }
  return 0;
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(id_block);
  NOT_USED(handle);

  setup_win();
  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void RenameMap_created(ObjectId const dbox_id)
{
  RenameMap_id = dbox_id;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { ActionButton_Selected, actionbutton_selected },
    { Window_AboutToBeShown, about_to_be_shown }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(dbox_id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}

void RenameMap_get_path(ComponentId const component, Filename *const file_path)
{
  switch (component) {
    case RENAMEMAP_POPUP_SOURCE:
      if (E(displayfield_get_value(0, RenameMap_id, RENAMEMAP_DISP_SOURCE,
      *file_path, sizeof(*file_path), NULL)))
        *file_path[0] = '\0';
      break;

    case RENAMEMAP_POPUP_DEST:
      if (E(writablefield_get_value(0, RenameMap_id, RENAMEMAP_WRITABLE_DEST,
      *file_path, sizeof(*file_path), NULL)))
        *file_path[0] = '\0';
      break;

    default:
      assert(false);
      break;
  }
}

char const *RenameMap_get_popup_title(ComponentId const component)
{
  switch (component) {
    case RENAMEMAP_POPUP_SOURCE:
      return "Source";

    case RENAMEMAP_POPUP_DEST:
      return "Dest";

    default:
      assert(false);
      return "";
  }
}

void RenameMap_set_path(ComponentId const component,
  char *const file_path)
{
  switch (component) {
    case RENAMEMAP_POPUP_SOURCE:
      E(displayfield_set_value(0, RenameMap_id,
                 RENAMEMAP_DISP_SOURCE, file_path));

      config_copy(file_path);
      break;

    case RENAMEMAP_POPUP_DEST:
      E(writablefield_set_value(0, RenameMap_id,
                 RENAMEMAP_WRITABLE_DEST, file_path));
      break;

    default:
      assert(false);
      break;
  }
}
