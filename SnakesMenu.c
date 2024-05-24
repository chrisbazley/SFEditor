/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Snakes palette menu
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

#include <assert.h>

/* RISC OS library files */
#include "event.h"
#include "toolbox.h"
#include "menu.h"

#include "msgtrans.h"
#include "err.h"
#include "Macros.h"

#include "SnakesMenu.h"
#include "Utils.h"

/* Menu entries */
enum {
  SNAKESMENU_EDIT   = 0x0,
  SNAKESMENU_RELOAD = 0x1,
  SNAKESMENU_NAMES  = 0x2,
};

static ObjectId snakes_menu_id = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

static void update_snakes_menu(PaletteData *const pal_data)
{
  E(menu_set_tick(0, snakes_menu_id, SNAKESMENU_NAMES,
                  Palette_get_labels_flag(pal_data)));
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(handle);
  NOT_USED(event);
  NOT_USED(event_code);

  void *pal_data;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(
    0, id_block->ancestor_id, &pal_data), 0);

  update_snakes_menu(pal_data);
  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void SnakesMenu_created(ObjectId const id)
{
  snakes_menu_id = id;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Menu_AboutToBeShown, about_to_be_shown }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}

void SnakesMenu_attach(PaletteData *const pal_data)
{
  Palette_set_menu(pal_data, snakes_menu_id);
}

void SnakesMenu_update(PaletteData *const pal_data)
{
  if (get_ancestor_handle_if_showing(snakes_menu_id) == pal_data) {
    update_snakes_menu(pal_data);
  }
}
