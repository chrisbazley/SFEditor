/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Brush configuration
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
#include "ConfigBrush.h"
#include "ToolMenu.h"

/* --------------------- Gadgets -------------------- */

enum {
  CONFIGBRUSH_GADGETS_SIZE   = 0x0,
  CONFIGBRUSH_GADGETS_CANCEL = 0x3,
  CONFIGBRUSH_GADGETS_OK     = 0x2,
};

static ObjectId ConfigBrush_id = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

  /* Note conversion from internal brush radius to user 'brush size':
     radius 0 (internal) = brush size '1', radius 1 (internal) = brush size '3',
     radius 2 (internal) = brush size '5'... etc    #
                         #                         ###
   # Brush of radius 0  ### Brush of radius 1     ##### Brush of radius 2
                         #                         ###
                                                    #
  */

static int brush_size_to_diam(int const brush_size)
{
  assert(brush_size >= 0);
  return (brush_size * 2) + 1;
}

static int diam_to_brush_size(int const diameter)
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
  int const brush_size = Editor_get_brush_size(editor);

  E(numberrange_set_value(0, id_block->self_id,
    CONFIGBRUSH_GADGETS_SIZE, brush_size_to_diam(brush_size)));


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
    case CONFIGBRUSH_GADGETS_CANCEL:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust)) {
        /* restore settings */
        int const brush_size = Editor_get_brush_size(editor);

        E(numberrange_set_value(0, id_block->self_id,
          CONFIGBRUSH_GADGETS_SIZE, brush_size_to_diam(brush_size)));
      }
      break;

    case CONFIGBRUSH_GADGETS_OK:
      /* read settings from window */
      {
        int diameter = 0;
        if (!E(numberrange_get_value(0, id_block->self_id,
                    CONFIGBRUSH_GADGETS_SIZE, &diameter))) {
          Editor_set_brush_size(editor, diam_to_brush_size(diameter));
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

void ConfigBrush_created(ObjectId const id)
{
  ConfigBrush_id = id;

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

void ConfigBrush_show_at_ptr(EditWin const *const edit_win)
{
  EditWin_show_dbox_at_ptr(edit_win, ConfigBrush_id);
}
