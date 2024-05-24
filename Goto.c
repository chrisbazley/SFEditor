/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Goto dialogue box
 *  Copyright (C) 2023 Christopher Bazley
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

/* ISO library files */
#include <stddef.h>
#include <assert.h>

/* RISC OS library files */
#include "toolbox.h"
#include "window.h"
#include "gadgets.h"
#include "event.h"

/* My library files */
#include "err.h"
#include "Macros.h"

/* Local headers */
#include "MapCoord.h"
#include "Goto.h"
#include "EditWin.h"
#include "Editor.h"

/* Window component IDs */
enum
{
  ComponentId_X_NumRange = 0x55,
  ComponentId_Y_NumRange = 0x57,
  ComponentId_Cancel_ActButton = 0xe,
  ComponentId_Go_ActButton = 0xf
};

static ObjectId Goto_id = NULL_ObjectId;

/* ----------------------------------------------------------------------- */
/*                         Private functions                               */

static void reset_dbox(EditWin *const edit_win, ObjectId const dbox_id)
{
  /* Ensure that the value displayed reflects the current caret position
     (or low boundary of selection) within the specified editing window */
  assert(edit_win != NULL);

  Editor const *const editor = EditWin_get_editor(edit_win);
  MapPoint const limit = Editor_get_coord_limit(editor);
  E(numberrange_set_bounds(NumberRange_UpperBound, dbox_id, ComponentId_X_NumRange, 0, (int)limit.x - 1, 0, 0));
  E(numberrange_set_bounds(NumberRange_UpperBound, dbox_id, ComponentId_Y_NumRange, 0, (int)limit.y - 1, 0, 0));

  MapPoint const pos = EditWin_get_scroll_pos(edit_win);
  E(numberrange_set_value(0, dbox_id, ComponentId_X_NumRange, (int)pos.x));
  E(numberrange_set_value(0, dbox_id, ComponentId_Y_NumRange, (int)pos.y));
}

/* ----------------------------------------------------------------------- */

static int goto_about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  void *client_handle;

  NOT_USED(event_code);
  NOT_USED(event);
  assert(id_block != NULL);
  NOT_USED(handle);

  /* Ensure that the value initially displayed reflects the caret position
     within the editing window which is an ancestor of this dialogue box */
  if (!E(toolbox_get_client_handle(0, id_block->ancestor_id, &client_handle)))
  {
    reset_dbox(client_handle, id_block->self_id);
  }

  return 1; /* claim event */
}

/* ----------------------------------------------------------------------- */

static int goto_actionbutton_selected(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  void *client_handle;

  NOT_USED(event_code);
  NOT_USED(event);
  assert(id_block != NULL);
  NOT_USED(handle);

  if (!E(toolbox_get_client_handle(0, id_block->ancestor_id, &client_handle)))
  {
    EditWin * const edit_win = client_handle;
    int x, y;

    switch (id_block->self_component)
    {
      case ComponentId_Cancel_ActButton:
        /* Reset the dialogue box so that it reverts to displaying the current
           caret position (in case the dbox is not about to be hidden) */
        reset_dbox(edit_win, id_block->self_id);
        break;

      case ComponentId_Go_ActButton:
       /* Move the caret to the specified position in the editing window which
          is an ancestor of this dialogue box */
        if (E(numberrange_get_value(0, id_block->self_id,
                ComponentId_X_NumRange, &x)))
          break;

        if (E(numberrange_get_value(0, id_block->self_id,
                ComponentId_Y_NumRange, &y)))
          break;

        EditWin_set_scroll_pos(edit_win, (MapPoint){x, y});
        break;

      default:
        return 0; /* not interested in this component */
    }
  }

  return 1; /* claim event */
}

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

void Goto_created(ObjectId id)
{
  static const struct
  {
    int event_code;
    ToolboxEventHandler *handler;
  }
  tbox_handlers[] =
  {
    {
      Window_AboutToBeShown,
      goto_about_to_be_shown
    },
    {
      ActionButton_Selected,
      goto_actionbutton_selected
    }
  };

  /* Register Toolbox event handlers */
  for (size_t i = 0; i < ARRAY_SIZE(tbox_handlers); i++)
  {
    EF(event_register_toolbox_handler(id, tbox_handlers[i].event_code,
         tbox_handlers[i].handler, NULL));
  }

  Goto_id = id;
}

void Goto_show(EditWin const *const edit_win)
{
  EditWin_show_dbox(edit_win, Toolbox_ShowObject_AsMenu, Goto_id);
}
