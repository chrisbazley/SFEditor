/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Fill tool configuration
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
#include "EditWin.h"
#include "ToolMenu.h"
#include "utils.h"
#include "ConfigFill.h"

/* --------------------- Menu entries -------------------- */

enum {
  CONFIGFILL_LOCAL  = 0x1,
  CONFIGFILL_GLOBAL = 0x2,
};

/* ---------------- Private functions ---------------- */

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
  Editor const *const editor = EditWin_get_editor(edit_win);
  bool const fill_is_global = Editor_get_fill_is_global(editor);

  E(menu_set_tick(0, id_block->self_id, CONFIGFILL_GLOBAL,
                           fill_is_global));

  E(menu_set_tick(0, id_block->self_id, CONFIGFILL_LOCAL,
                           !fill_is_global));

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
  Editor *const editor = EditWin_get_editor(edit_win);

  switch (id_block->self_component) {
    case CONFIGFILL_LOCAL:
    case CONFIGFILL_GLOBAL:
      /* Change fill type */
      Editor_set_fill_is_global(editor, id_block->self_component == CONFIGFILL_GLOBAL);

      /* Update position of menu tick */
      E(menu_set_tick(0, id_block->self_id, CONFIGFILL_GLOBAL,
                               id_block->self_component == CONFIGFILL_GLOBAL));

      E(menu_set_tick(0, id_block->self_id, CONFIGFILL_LOCAL,
                               id_block->self_component != CONFIGFILL_GLOBAL));

      /* Tick corresponding entry on parent menu if part of tree */
      ToolMenu_update(editor);

      return 1; /* claim event */

    default:
      return 0; /* not interested in this button */
  }
}

/* ---------------- Public functions ---------------- */

static ObjectId ConfigFill_id = NULL_ObjectId;

void ConfigFill_created(ObjectId const id)
{
  ConfigFill_id = id;

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

void ConfigFill_show_at_ptr(EditWin const *const edit_win)
{
  EditWin_show_dbox_at_ptr(edit_win, ConfigFill_id);
}
