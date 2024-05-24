/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  File menu (mission version)
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

#include "toolbox.h"
#include "menu.h"
#include "event.h"

#include "err.h"
#include "Macros.h"

#include "Session.h"
#include "utils.h"
#include "debug.h"
#include "EditWin.h"
#include "missfiles.h"
#include "DataType.h"

enum {
  MISSFILES_SAVEALL    = 0x5,
  MISSFILES_CLOSE      = 0x13,
  MISSFILES_NEWVIEW    = 0x14,
  MISSFILES_MAPOVERLAY = 0x1,
  MISSFILES_OBJOVERLAY = 0x2,
  MISSFILES_ANIMATIONS = 0x3,
  MISSFILES_MISSION    = 0x4,
};

ObjectId MissFiles_sharedid = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

static int menu_submenu(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Set title of submenu as appropriate */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  char title[32];
  ON_ERR_RPT_RTN_V(menu_get_entry_text(0, id_block->self_id,
                                       id_block->self_component,
                                       title, sizeof(title), NULL), 1);

  ObjectId sub_menu;
  ON_ERR_RPT_RTN_V(menu_get_sub_menu_show(0, id_block->self_id,
                                          id_block->self_component,
                                          &sub_menu), 1);

  DEBUG("Setting title of sub menu %d to '%s'", sub_menu, title);
  E(menu_set_title(0, sub_menu, title));

  return 1; /* claim event */
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  EditSession *const session = EditWin_get_session(edit_win);

  E(menu_set_fade(0, id_block->self_id, MISSFILES_SAVEALL,
                           !Session_can_save_all(session)));

  static ComponentId const menu_entries[] = {
    MISSFILES_MAPOVERLAY, MISSFILES_OBJOVERLAY, MISSFILES_ANIMATIONS,
    MISSFILES_MISSION};

  for (size_t i = 0; i < ARRAY_SIZE(menu_entries); ++i) {
    DataType const data_type = MissFiles_get_data_type(menu_entries[i]);
    E(menu_set_fade(0, id_block->self_id, menu_entries[i],
                             !Session_has_data(session, data_type)));
  }

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void MissFiles_created(ObjectId const id)
{
  MissFiles_sharedid = id;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Menu_SubMenu, menu_submenu },
    { Menu_AboutToBeShown, about_to_be_shown }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}

DataType MissFiles_get_data_type(ComponentId const menu_entry)
{
  DataType data_type = DataType_OverlayMap;
  switch (menu_entry) {
    case MISSFILES_MAPOVERLAY:
      data_type = DataType_OverlayMap;
      break;
    case MISSFILES_OBJOVERLAY:
      data_type = DataType_OverlayObjects;
      break;
    case MISSFILES_ANIMATIONS:
      data_type = DataType_OverlayMapAnimations;
      break;
    case MISSFILES_MISSION:
      data_type = DataType_Mission;
      break;
  }
  return data_type;
}
