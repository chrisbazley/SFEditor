/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Transfer creation dialogue box
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

#include <stddef.h>

#include "toolbox.h"
#include "window.h"
#include "event.h"
#include "gadgets.h"

#include "err.h"
#include "Macros.h"

#include "Session.h"
#include "EditWin.h"
#include "Editor.h"
#include "EditWin.h"
#include "NewTransfer.h"
#include "DataType.h"
#include "Filenames.h"
#include "FilenamesData.h"

/* --------------------- Gadgets -------------------- */

enum {
  NEWTRANSFER_GADGETS_NAME   = 0x15,
  NEWTRANSFER_GADGETS_CANCEL = 0x0,
  NEWTRANSFER_GADGETS_OK     = 0x1,
  NEWTRANSFER_GADGETS_TEXSET = 0x17,
};

static Filename initial_name = "";

/* ---------------- Private functions ---------------- */

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
  FilenamesData const *const filenames = Session_get_filenames(session);

  /* Show current tiles set */
  E(displayfield_set_value(0, id_block->self_id,
                           NEWTRANSFER_GADGETS_TEXSET,
                           filenames_get(filenames, DataType_MapTextures)));
                           // FIXME wrong for objects

  /* Save starting (e.g. last) filename */
  E(writablefield_get_value(0, id_block->self_id,
                                     NEWTRANSFER_GADGETS_NAME, initial_name,
                                     sizeof(initial_name), NULL));

  return 1; /* claim event */
}

static int actionbutton_selected(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(handle);

  switch (id_block->self_component) {
    case NEWTRANSFER_GADGETS_CANCEL:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust))
      {
        /* Restore starting filename */
        E(writablefield_set_value(0, id_block->self_id,
                   NEWTRANSFER_GADGETS_NAME, initial_name));
      }
      break;

    case NEWTRANSFER_GADGETS_OK:
      {
        void *edit_win;
        ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
          &edit_win), 0);
        Editor *const editor = EditWin_get_editor(edit_win);

        E(writablefield_get_value(0, id_block->self_id,
                   NEWTRANSFER_GADGETS_NAME, initial_name, sizeof(initial_name),
                   NULL));

        if (Editor_can_create_transfer(editor)) {
          Editor_create_transfer(editor, initial_name);
        }

        /* Close dialogue box on completion unless ADJUST-click */
        if (!TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust))
        {
          E(toolbox_hide_object(0, id_block->self_id));
        }
      }
      break;

    default:
      return 0; /* not interested in this button */
  }
  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

static ObjectId NewTransfer_sharedID = NULL_ObjectId;

void NewTransfer_created(ObjectId const window_id)
{
  NewTransfer_sharedID = window_id;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { ActionButton_Selected, actionbutton_selected },
    { Window_AboutToBeShown, about_to_be_shown }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(window_id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}

void NewTransfer_show(EditWin const *const edit_win)
{
  EditWin_show_dbox(edit_win, Toolbox_ShowObject_AsMenu, NewTransfer_sharedID);
}
