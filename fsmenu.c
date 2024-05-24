/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Generic code for selection from a menu of files
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

#include <stdbool.h>
#include <string.h>

#include "toolbox.h"
#include "menu.h"

#include "strextra.h"
#include "err.h"
#include "debug.h"
#include "msgtrans.h"

#include "filepaths.h"
#include "filescan.h"
#include "fsmenu.h"

void fsmenu_grey_internal(ObjectId const menu,
                              const filescan_leafname *const leaf_names,
                              bool const inc_blank, bool const grey_internal)
{
  ComponentId menu_entry = 0;

  DEBUG("Updating menu %d to %s internal files", menu,
  grey_internal ? "fade" : "unfade");

  for (size_t name_num = 0; *leaf_names[name_num].leaf_name != '\0'; name_num++) {
    if (inc_blank || stricmp(leaf_names[name_num].leaf_name, BLANK_FILE) != 0) {
      if (leaf_names[name_num].is_internal) {
        DEBUG("%sfading entry %d", grey_internal ? "" : "un", menu_entry);
        ON_ERR_RPT_RTN(menu_set_fade(0, menu, menu_entry, grey_internal));
      }
      menu_entry++;
    }
  }
}

ComponentId fsmenu_build(ObjectId const menu,
                         filescan_leafname *const leaf_names,
                         ComponentId *const ret_next_cid,
                         bool const inc_blank, bool const add_none,
                         bool const grey_internal,
                         char const *const tick_me)
{

  ComponentId ticked = NULL_ComponentId;
  MenuTemplateEntry new_entry = {0};

  DEBUG("Building menu %d from leafname array %p (%sinclude 'Blank', "
        "%sadd 'None', %sfade internal files)", menu, (void *)leaf_names,
        inc_blank ? "" : "don't ",
        add_none ? "" : "don't ",
        grey_internal ? "" : "don't ");
        new_entry.component_id = 0;

  assert(leaf_names != NULL);

  for (size_t name_num = 0; *leaf_names[name_num].leaf_name != '\0'; name_num++) {
    if (inc_blank || stricmp(leaf_names[name_num].leaf_name, BLANK_FILE) != 0) {
      if (tick_me && stricmp(leaf_names[name_num].leaf_name, tick_me) == 0) {
        new_entry.flags = Menu_Entry_Ticked;
        ticked = new_entry.component_id;
      } else {
        new_entry.flags = 0;
      }

      if (grey_internal && leaf_names[name_num].is_internal)
        new_entry.flags |= Menu_Entry_Faded;

      if (add_none && *leaf_names[name_num + 1].leaf_name == '\0') {
        new_entry.flags |= Menu_Entry_DottedLine;
      }

      new_entry.click_event = Menu_Selection;
      new_entry.text = leaf_names[name_num].leaf_name;
      new_entry.max_text = sizeof(Filename);

      DEBUGF("Adding entry %d to menu %d ('%s'%s%s%s)\n", new_entry.component_id,
            menu, new_entry.text,
            TEST_BITS(new_entry.flags, Menu_Entry_Ticked) ? ", ticked" : "",
            TEST_BITS(new_entry.flags, Menu_Entry_DottedLine) ? ", underlined" : "",
            TEST_BITS(new_entry.flags, Menu_Entry_Faded) ? ", faded" : "");

      if (E(menu_add_entry(Menu_AddEntryBefore, menu, Menu_AddEntryAtEnd,
            (char *)&new_entry, NULL)))
        break;

      new_entry.component_id++;
    }
  }

  if (add_none) {
    if (ticked == NULL_ComponentId && tick_me && strcmp(NO_FILE, tick_me) == 0) {
      new_entry.flags = Menu_Entry_Ticked;
      ticked = new_entry.component_id;
    } else {
      new_entry.flags = 0;
    }
    new_entry.text = msgs_lookup("None");
    new_entry.click_event = Menu_Selection;
    new_entry.max_text = (int)strlen(new_entry.text)+1;

    DEBUGF("Adding entry %d to menu %d ('%s'%s%s%s)\n", new_entry.component_id,
          menu, new_entry.text,
          TEST_BITS(new_entry.flags, Menu_Entry_Ticked) ? ", ticked" : "",
          TEST_BITS(new_entry.flags, Menu_Entry_DottedLine) ? ", underlined" : "",
          TEST_BITS(new_entry.flags, Menu_Entry_Faded) ? ", faded" : "");

    if (!E(menu_add_entry(Menu_AddEntryBefore, menu, Menu_AddEntryAtEnd,
        (char *)&new_entry, NULL))) {
      new_entry.component_id++;
    }
  }

  *ret_next_cid = new_entry.component_id;
  return ticked;
}
