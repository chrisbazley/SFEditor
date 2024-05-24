/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission rename dialogue box
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
#include <assert.h>
#include <string.h>

/* RISC OS library files */
#include "event.h"
#include "toolbox.h"
#include "window.h"
#include "gadgets.h"

#include "err.h"
#include "Macros.h"
#include "msgtrans.h"
#include "PathTail.h"
#include "strextra.h"
#include "Debug.h"
#include "fileperc.h"
#include "GadgetUtil.h"

#include "Utils.h"
#include "RenameMiss.h"
#include "Config.h"
#include "filepaths.h"
#include "filescan.h"
#include "pathutils.h"
#include "pyram.h"

/* Gadget numbers */
enum {
  RENAMEMISS_NUMBER_RANGE  = 0x6e,
  RENAMEMISS_OPTION_COPY   = 0x74,
  RENAMEMISS_RADIO_MEDIUM  = 0x75,
  RENAMEMISS_RADIO_HARD    = 0x76,
  RENAMEMISS_RADIO_EXTRA   = 0x77,
  RENAMEMISS_ACTION_CANCEL = 0x78,
  RENAMEMISS_ACTION_RENAME = 0x79,
  RENAMEMISS_WRITABLE_DEST = 0x7d,
  RENAMEMISS_RADIO_EASY    = 0x7e,
  RENAMEMISS_DISP_SOURCE   = 0x87,
  RENAMEMISS_POPUP_SOURCE  = 0x88,
};

ObjectId RenameMiss_id = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

static void config_copy(const char *const source_path)
{
  if (Config_get_use_extern_levels_dir()) {
    /* Forceably set 'Copy' option if new source is untouchable file in internal
    directory */
    bool exists;
    {
      char *const miss_intern_path = make_file_path_in_dir_on_path(
         Config_get_read_dir(), MISSION_DIR, source_path);

      exists = file_exists(miss_intern_path);
      free(miss_intern_path);
    }
    if (exists) {
      DEBUG("Setting copy option");
      E(optionbutton_set_state(0, RenameMiss_id, RENAMEMISS_OPTION_COPY, true));
    }
    DEBUG("%s copy option", exists ? "Locking" : "Unlocking");
    E(set_gadget_faded(RenameMiss_id, RENAMEMISS_OPTION_COPY, exists));
  } else {
    DEBUG("Unlocking copy option");
    E(set_gadget_faded(RenameMiss_id, RENAMEMISS_OPTION_COPY, false));
  }
}

static void setup_win(void)
{
  /* Get currently (or soon-to-be) displayed source path */
  Filename source_sub_path;
  ON_ERR_RPT_RTN(displayfield_get_value(0, RenameMiss_id,
    RENAMEMISS_DISP_SOURCE, source_sub_path, sizeof(source_sub_path), NULL));

  struct {
    filescan_type dir;
    filescan_leafname *leaves;
  } data[] = {
    { FS_MISSION_E, NULL },
    { FS_MISSION_M, NULL },
    { FS_MISSION_H, NULL },
    { FS_MISSION_U, NULL },
  };

  {
    filescan_leafname *leaf_list = NULL;
    bool bad_path = true;
    for (size_t i = 0; bad_path && i < ARRAY_SIZE(data); ++i) {
      char const *const emh_path = filescan_get_emh_path(data[i].dir);
      if (strnicmp(source_sub_path, emh_path, strlen(emh_path)) == 0) {
        leaf_list = data[i].leaves = filescan_get_leaf_names(data[i].dir, NULL);
        bad_path = false;
      }
    }

    if (leaf_list == NULL) {
      return; /* error */
    }

    /* Check to see whether that file exists on relevant list */
    if (!bad_path) {
      for (size_t i = 0; *leaf_list[i].leaf_name != '\0'; i++) {
        if (stricmp(leaf_list[i].leaf_name, pathtail(source_sub_path, 1)) == 0) {
          DEBUG("Previous source leaf name '%s' validates", source_sub_path);
          config_copy(source_sub_path);
          return; /* we have matched displayed leaf name with one on list */
        }
      }
    }
  }

  /* Have reached end of list without match - substitute 1st known name */
  {
    filescan_leafname *first_leaf = NULL;
    char const *prefix = NULL;

    for (size_t i = 0; !first_leaf && i < ARRAY_SIZE(data); ++i) {
      if (data[i].leaves == NULL) {
        data[i].leaves = filescan_get_leaf_names(data[i].dir, NULL);
      }
      if (data[i].leaves[0].leaf_name != '\0') {
        first_leaf = data[i].leaves;
        prefix = filescan_get_emh_path(data[i].dir);
      }
    }
    if (first_leaf) {
      DEBUG("Substituting path '%s' for previous source '%s'",
        first_leaf->leaf_name, source_sub_path);

      Filename sub_path;
      strcpy(sub_path, prefix);
      strncat(sub_path, first_leaf->leaf_name,
              sizeof(sub_path) - 1 - strlen(prefix));
      RenameMiss_set_path(RENAMEMISS_POPUP_SOURCE, sub_path);
    }
  }
}

static int actionbutton_selected(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Click on menu item */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  int copy;
  Filename dest_sub_path, source_sub_path;

  switch (id_block->self_component) {
    case RENAMEMISS_ACTION_CANCEL:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust)) {
        /* restore settings */
        setup_win();
      }
      return 1; /* claim event */

    case RENAMEMISS_ACTION_RENAME:
      ON_ERR_RPT_RTN_V(optionbutton_get_state(0, id_block->self_id,
        RENAMEMISS_OPTION_COPY, &copy), 1);

      ON_ERR_RPT_RTN_V(displayfield_get_value(0, id_block->self_id,
        RENAMEMISS_DISP_SOURCE, source_sub_path, sizeof(source_sub_path), NULL),
      1);

      int miss_number = 1;
      Pyramid pyramid_number = Pyramid_Easy;
      char miss_name[sizeof(Filename)-2];
      {
        ComponentId radio_selected;
        ON_ERR_RPT_RTN_V(radiobutton_get_state(0, id_block->self_id,
          RENAMEMISS_RADIO_EASY, NULL, &radio_selected), 1);

        if (radio_selected != RENAMEMISS_RADIO_EXTRA)
          ON_ERR_RPT_RTN_V(numberrange_get_value(0, id_block->self_id,
            RENAMEMISS_NUMBER_RANGE, &miss_number), 1);
        else
          ON_ERR_RPT_RTN_V(writablefield_get_value(0, id_block->self_id,
            RENAMEMISS_WRITABLE_DEST, miss_name, sizeof(miss_name), NULL), 1);

        switch (radio_selected) {
          case RENAMEMISS_RADIO_EASY:
            pyramid_number = Pyramid_Easy;
            break;

          case RENAMEMISS_RADIO_MEDIUM:
            pyramid_number = Pyramid_Medium;
            break;

          case RENAMEMISS_RADIO_HARD:
            pyramid_number = Pyramid_Hard;
            break;

          case RENAMEMISS_RADIO_EXTRA:
            pyramid_number = Pyramid_User;
            break;

          default:
            assert(false);
            return 1;
        }
        get_mission_file_name(dest_sub_path, pyramid_number,
          miss_number, miss_name);
        DEBUG("Source file path tail: '%s'", dest_sub_path);
      }

      if (filepaths_mission_exists(dest_sub_path) &&
          !dialogue_confirm(msgs_lookup_subn("MultOv", 1, dest_sub_path), "OvBut"))
      {
        return 1; /* aborted */
      }

      if (!filepaths_rename_mission(source_sub_path, dest_sub_path,
                                    pyramid_number, miss_number, copy))
      {
        return 1; /* failed */
      }

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

int radiobutton_state_changed(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Radio button has been tweaked */
  NOT_USED(event_code);
  NOT_USED(handle);
  const RadioButtonStateChangedEvent *const rbsce =
    (RadioButtonStateChangedEvent *)event;

  if (rbsce->state != 1)
    return 0; /* event not handled */

  /* Activate gadgets for newly pressed button */
  switch (id_block->self_component) {
    case RENAMEMISS_RADIO_EASY:
    case RENAMEMISS_RADIO_MEDIUM:
    case RENAMEMISS_RADIO_HARD:
      if (rbsce->old_on_button == RENAMEMISS_RADIO_EXTRA) {
        E(set_gadget_faded(id_block->self_id, RENAMEMISS_WRITABLE_DEST, true));
        E(set_gadget_faded(id_block->self_id, RENAMEMISS_NUMBER_RANGE, false));

        /* Set input focus to the level number writable field */
        E(gadget_set_focus(0, id_block->self_id,
        RENAMEMISS_NUMBER_RANGE));

        E(window_set_default_focus(0, id_block->self_id,
        RENAMEMISS_NUMBER_RANGE));
      }
      break;

    case RENAMEMISS_RADIO_EXTRA:
      if (rbsce->old_on_button != RENAMEMISS_RADIO_EXTRA) {
        E(set_gadget_faded(id_block->self_id, RENAMEMISS_NUMBER_RANGE, true));
        E(set_gadget_faded(id_block->self_id, RENAMEMISS_WRITABLE_DEST, false));

        /* Set input focus to the filename writable field */
        E(gadget_set_focus(0, id_block->self_id,
        RENAMEMISS_WRITABLE_DEST));

        E(window_set_default_focus(0, id_block->self_id,
        RENAMEMISS_WRITABLE_DEST));
      }
      break;
  }

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void RenameMiss_created(ObjectId const dbox_id)
{
  RenameMiss_id = dbox_id;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { ActionButton_Selected, actionbutton_selected },
    { Window_AboutToBeShown, about_to_be_shown },
    { RadioButton_StateChanged, radiobutton_state_changed }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(dbox_id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}

void RenameMiss_get_path(ComponentId const component, Filename *const file_path)
{
  if (component != RENAMEMISS_POPUP_SOURCE)
    return;

  if (E(displayfield_get_value(0, RenameMiss_id, RENAMEMISS_DISP_SOURCE,
  *file_path, sizeof(*file_path), NULL)))
    *file_path[0] = '\0';
}

void RenameMiss_set_path(ComponentId const component, char *const file_path)
{
  if (component != RENAMEMISS_POPUP_SOURCE)
    return;

  E(displayfield_set_value(0, RenameMiss_id, RENAMEMISS_DISP_SOURCE,
                                    file_path));

  config_copy(file_path);
}
