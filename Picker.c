/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Colour picker dialogue box
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
#include "event.h"
#include "window.h"

#include "Macros.h"
#include "err.h"
#include "Pal256.h"
#include "SFInit.h"

#include "EditWin.h"
#include "GraphicsFiles.h"
#include "Picker.h"

static ObjectId picker_id = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

static int colourselhandler(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Cloud colour selected via picker dbox */
  NOT_USED(event_code);
  NOT_USED(handle);
  const Pal256ColourSelectedEvent *const pcse = (Pal256ColourSelectedEvent *)
                                                event;

  /* If we have more than one potential parent then we can
     pick one here. */
  if (id_block->parent_id != GraphicsFiles_id)
    return 0; /* none of our business */

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  EditSession *const session = EditWin_get_session(edit_win);

  return GraphicsFiles_colour_selected(session,
                                       id_block->parent_component,
                                       pcse->colour_number);
}

/* ---------------- Public functions ---------------- */

void Picker_created(ObjectId const id)
{
  picker_id = id;

  EF(Pal256_initialise(id, *palette, &messages, err_check_rep));
  EF(event_register_toolbox_handler(id, Pal256_ColourSelected, colourselhandler, NULL));
}

void Picker_set_title(char *const title)
{
  E(window_set_title(0, picker_id, title));
}

void Picker_set_colour(unsigned int const colour)
{
  E(Pal256_set_colour(picker_id, colour));
}
