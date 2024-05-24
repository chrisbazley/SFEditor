/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Layers menu
 *  Copyright (C) 2021 Christopher Bazley
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

#include <assert.h>

#include "toolbox.h"
#include "menu.h"
#include "event.h"
#include "window.h"

#include "err.h"
#include "Macros.h"

#include "EditWin.h"
#include "Session.h"
#include "utils.h"
#include "LayersMenu.h"
#include "DataType.h"

/* --------------------- Gadgets -------------------- */

enum {
  ComponentId_SHOWMAP         = 0x6,
  ComponentId_SHOWOBJECTS     = 0x7,
  ComponentId_SHOWSHIPS       = 0x8,
  ComponentId_SHOWMAPOVERLAY  = 0x9,
  ComponentId_SHOWOBJSOVERLAY = 0xa,
  ComponentId_SHOWINFO        = 0xb,
  ComponentId_SHOWMAPANIMS    = 0xc,
};

static ObjectId LayersMenu_id = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

static void update_disp_menu(EditWin const *const edit_win)
{
  ViewDisplayFlags const display_flags = EditWin_get_display_flags(edit_win);

  const EditSession *const session = EditWin_get_session(edit_win);

  E(menu_set_tick(0, LayersMenu_id, ComponentId_SHOWMAP,
                           display_flags.MAP));

  E(menu_set_tick(0, LayersMenu_id, ComponentId_SHOWMAPOVERLAY,
                           display_flags.MAP_OVERLAY));

  E(menu_set_tick(0, LayersMenu_id, ComponentId_SHOWOBJECTS,
                           display_flags.OBJECTS));

  E(menu_set_tick(0, LayersMenu_id, ComponentId_SHOWOBJSOVERLAY,
                           display_flags.OBJECTS_OVERLAY));

  E(menu_set_tick(0, LayersMenu_id, ComponentId_SHOWSHIPS,
                           display_flags.SHIPS));

  E(menu_set_tick(0, LayersMenu_id, ComponentId_SHOWINFO,
                           display_flags.INFO));

  E(menu_set_tick(0, LayersMenu_id, ComponentId_SHOWMAPANIMS,
                           display_flags.MAP_ANIMS));

  static const struct {
    ComponentId entry;
    DataType data_type;
  } ticks[] = {
    { ComponentId_SHOWMAP, DataType_BaseMap },
    { ComponentId_SHOWMAPOVERLAY, DataType_OverlayMap },
    { ComponentId_SHOWOBJECTS, DataType_BaseObjects },
    { ComponentId_SHOWOBJSOVERLAY, DataType_OverlayObjects },
    { ComponentId_SHOWSHIPS, DataType_Mission },
    { ComponentId_SHOWINFO, DataType_Mission },
    { ComponentId_SHOWMAPANIMS, DataType_OverlayMapAnimations },
  };

  for (size_t i = 0; i < ARRAY_SIZE(ticks); ++i) {
    E(menu_set_fade(0, LayersMenu_id, ticks[i].entry,
                    !Session_has_data(session, ticks[i].data_type)));
  }
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Set up menu */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);

  update_disp_menu(edit_win);

  return 1; /* claim event */
}

static int menu_selection(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);

 ViewDisplayFlags display_flags = EditWin_get_display_flags(edit_win);

  bool const shown = update_menu_tick(id_block);

  switch (id_block->self_component) {
  case ComponentId_SHOWMAP:
    display_flags.MAP = shown;
    break;
  case ComponentId_SHOWMAPOVERLAY:
    display_flags.MAP_OVERLAY = shown;
    break;
  case ComponentId_SHOWOBJECTS:
    display_flags.OBJECTS = shown;
    break;
  case ComponentId_SHOWOBJSOVERLAY:
    display_flags.OBJECTS_OVERLAY = shown;
    break;
  case ComponentId_SHOWSHIPS:
    display_flags.SHIPS = shown;
    break;
  case ComponentId_SHOWINFO:
    display_flags.INFO = shown;
    break;
  case ComponentId_SHOWMAPANIMS:
    display_flags.MAP_ANIMS = shown;
    break;
  default:
    return 0;
  }

  EditWin_set_display_flags(edit_win, display_flags);

  return 1;
}

static bool is_showing_for_edit_win(EditWin const *const edit_win)
{
  return get_ancestor_handle_if_showing(LayersMenu_id) == edit_win;
}

/* ---------------- Public functions ---------------- */

void LayersMenu_created(ObjectId const id)
{
  LayersMenu_id = id;

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
}

void LayersMenu_update(EditWin const *const edit_win)
{
  if (is_showing_for_edit_win(edit_win))
  {
    update_disp_menu(edit_win);
  }
}
