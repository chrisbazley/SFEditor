/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Graphics files dialogue box
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
#include <stdbool.h>

#include "toolbox.h"
#include "event.h"
#include "menu.h"

#include "err.h"
#include "Macros.h"
#include "msgtrans.h"
#include "debug.h"

#include "Config.h"
#include "Session.h"
#include "EditWin.h"
#include "GraphicsFiles.h"
#include "Picker.h"
#include "GfxConfig.h"
#include "DataType.h"

/* ----------------- Menu entries -------------------- */

enum {
  ComponentId_MAPTILES     = 0x0,
  ComponentId_POLYOBJS     = 0x1,
  ComponentId_OBJCOLS      = 0x2,
  ComponentId_HILLCOLS     = 0x3,
  ComponentId_CLOUDCOLOUR1 = 0x6,
  ComponentId_CLOUDCOLOUR2 = 0x5,
  ComponentId_SKYCOLOURS   = 0x7,
  ComponentId_SKYPICTURES  = 0x8,
  ComponentId_SAVEPREF     = 0x4,
};

ObjectId GraphicsFiles_id = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

static int menu_selection(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  EditSession *const session = EditWin_get_session(edit_win);

  switch (id_block->self_component) {
    case ComponentId_SAVEPREF:
      /* read settings from window */
      if (Session_get_ui_type(session) == UI_TYPE_MAP) {
        Session_save_gfx_config(session);
      }
      break;

    default:
      return 0; /* not interested in this button */
  }
  return 1; /* event handled */
}

static int menu_submenu(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Set up picker dbox before it is shown */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  EditSession *const session = EditWin_get_session(edit_win);
  const CloudColData *const clouds = Session_get_cloud_colours(session);

  unsigned int colour;
  switch (id_block->self_component) {
    case ComponentId_CLOUDCOLOUR1:
      colour = clouds_get_colour(clouds, 0);
      break;

    case ComponentId_CLOUDCOLOUR2:
      colour = clouds_get_colour(clouds, 1);
      break;

    default:
      return 0; /* not interested */
  }

  Picker_set_colour(colour);

  char title[32];
  if (!E(menu_get_entry_text(0, id_block->self_id, id_block->self_component,
         title, sizeof(title), NULL))) {
    Picker_set_title(title);
  }

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

  bool const has_miss = Session_has_data(session, DataType_Mission);

  bool const has_obj = Session_has_data(session, DataType_BaseObjects) ||
                       Session_has_data(session, DataType_OverlayObjects) ||
                       has_miss;

  bool const has_map = Session_has_data(session, DataType_BaseMap) ||
                       Session_has_data(session, DataType_OverlayMap) ||
                       has_miss;

  E(menu_set_fade(0, id_block->self_id, ComponentId_MAPTILES, !has_map));
  E(menu_set_fade(0, id_block->self_id, ComponentId_POLYOBJS, !has_obj));
  E(menu_set_fade(0, id_block->self_id, ComponentId_OBJCOLS, !has_obj));
  E(menu_set_fade(0, id_block->self_id, ComponentId_HILLCOLS, !has_obj));
  E(menu_set_fade(0, id_block->self_id, ComponentId_CLOUDCOLOUR1, !has_obj));
  E(menu_set_fade(0, id_block->self_id, ComponentId_CLOUDCOLOUR2, !has_obj));
  E(menu_set_fade(0, id_block->self_id, ComponentId_SKYCOLOURS, !has_miss));
  E(menu_set_fade(0, id_block->self_id, ComponentId_SKYPICTURES, !has_miss));

  E(menu_set_fade(0, id_block->self_id, ComponentId_SAVEPREF,
                           !Session_can_save_all(session) ||
                           Session_get_ui_type(session) != UI_TYPE_MAP));

  return 1; /* event handled */
}

/* ---------------- Public functions ---------------- */

void GraphicsFiles_created(ObjectId const id)
{
  GraphicsFiles_id = id;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Menu_Selection, menu_selection },
    { Menu_AboutToBeShown, about_to_be_shown },
    { Menu_SubMenu, menu_submenu }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}

int GraphicsFiles_colour_selected(EditSession *const session,
                                  ComponentId const parent_component,
                                  unsigned int const colour)
{
  /* User made selection from 256 colour palette */
  CloudColData *const clouds = Session_get_cloud_colours(session);

  switch (parent_component) {
    case ComponentId_CLOUDCOLOUR1:
      clouds_set_colour(clouds, 0, colour);
      break;

    case ComponentId_CLOUDCOLOUR2:
      clouds_set_colour(clouds, 1, colour);
      break;

    default:
      return 0; /* unknown pop-up gadget */
  }

  Session_notify_changed(session, DataType_Mission);
  Session_resource_change(session, EDITOR_CHANGE_CLOUD_COLOURS, NULL);

  return 1; /* claim event */
}
