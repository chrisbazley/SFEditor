/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Wand configuration
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

#include "stdio.h"

#include "toolbox.h"
#include "window.h"
#include "event.h"
#include "gadgets.h"

#include "err.h"
#include "Macros.h"
#include "TboxBugs.h"
#include "msgtrans.h"

#include "Session.h"
#include "EditWin.h"
#include "ConfigWand.h"
#include "ToolMenu.h"

/* --------------------- Gadgets -------------------- */

enum {
  CONFIGWAND_GADGETS_SIZE   = 0x0,
  CONFIGWAND_GADGETS_CANCEL = 0x3,
  CONFIGWAND_GADGETS_OK     = 0x2,
};

static ObjectId ConfigWand_id = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

  /* Note conversion from internal wand radius to user 'wand size':
     radius 0 (internal) = wand size '1', radius 1 (internal) = wand size '3',
     radius 2 (internal) = wand size '5'... etc    #
                        #                         ###
   # Wand of radius 0  ### Wand of radius 1      ##### Wand of radius 2
                        #                         ###
                                                   #
  */

static int wand_size_to_diam(int const wand_size)
{
  assert(wand_size >= 0);
  return (wand_size * 2) + 1;
}

static int diam_to_wand_size(int const diameter)
{
  assert(diameter >= 0);
  return (diameter - 1) / 2;
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
  Editor *const editor = EditWin_get_editor(edit_win);

  /* Set up window */
  int const wand_size = Editor_get_wand_size(editor);

  E(numberrange_set_value(0, id_block->self_id,
    CONFIGWAND_GADGETS_SIZE, wand_size_to_diam(wand_size)));

  return 1; /* claim event */
}

static int actionbutton_selected(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  Editor *const editor = EditWin_get_editor(edit_win);

  switch (id_block->self_component) {
    case CONFIGWAND_GADGETS_CANCEL:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust)) {
        /* restore settings */
        int const wand_size = Editor_get_wand_size(editor);

        E(numberrange_set_value(0, id_block->self_id,
          CONFIGWAND_GADGETS_SIZE, wand_size_to_diam(wand_size)));
      }
      break;

    case CONFIGWAND_GADGETS_OK:
      /* read settings from window */
      {
        int diameter = 0;
        if (!E(numberrange_get_value(0, id_block->self_id,
                    CONFIGWAND_GADGETS_SIZE, &diameter))) {
          Editor_set_wand_size(editor, diam_to_wand_size(diameter));
          ToolMenu_update(editor);
        }
      }
      break;

    default:
      return 0; /* not interested in this button */
  }
  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void ConfigWand_created(ObjectId const id)
{
  ConfigWand_id = id;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Window_AboutToBeShown, about_to_be_shown },
    { ActionButton_Selected, actionbutton_selected }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}

void ConfigWand_show_at_ptr(EditWin const *const edit_win)
{
  EditWin_show_dbox_at_ptr(edit_win, ConfigWand_id);
}
