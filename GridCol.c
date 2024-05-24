/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Dialogue box for selection of grid colour
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
#include "colourdbox.h"
#include "event.h"
#include "menu.h"

#include "err.h"
#include "Macros.h"
#include "Debug.h"

#include "EditWin.h"
#include "GridCol.h"
#include "UtilsMenu.h"

ObjectId GridCol_id = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Set up colour */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);

  int colour_block[2] = {(int)EditWin_get_grid_colour(edit_win), 0};
  E(colourdbox_set_colour(0, id_block->self_id, colour_block));

  return 1; /* claim event */
}

static int colour_selected(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Colour selected by user */
  NOT_USED(event_code);
  NOT_USED(handle);

  const ColourDboxColourSelectedEvent *const cdcse =
    (ColourDboxColourSelectedEvent *)event;

  DEBUG("Colour %X selected", cdcse->colour_block[0]);

  void *c_handle;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &c_handle), 0);
  EditWin *const edit_win = c_handle;

  EditWin_set_grid_colour(edit_win, cdcse->colour_block[0]);

  ViewDisplayFlags display_flags = EditWin_get_display_flags(edit_win);

  display_flags.GRID = !TEST_BITS(event->hdr.flags,
                         ColourDbox_ColourNoneSelected);

  EditWin_set_display_flags(edit_win, display_flags);
  UtilsMenu_update(edit_win);

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void GridCol_created(ObjectId const id)
{
  GridCol_id = id;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { ColourDbox_ColourSelected, colour_selected },
    { ColourDbox_AboutToBeShown, about_to_be_shown }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}
