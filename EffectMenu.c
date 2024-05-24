/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Effects menu (for map editing mode)
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

#include "event.h"
#include "toolbox.h"
#include "menu.h"

#include "err.h"
#include "macros.h"

#include "Editor.h"
#include "mapmode.h"
#include "EffectMenu.h"
#include "EditWin.h"
#include "utils.h"

enum {
  ComponentId_FILL     = 0x8,
  ComponentId_SMOOTH   = 0x7,
  ComponentId_REVERSE  = 0xb,
  ComponentId_DELETEANIM = 0xa,
  ComponentId_DELETEACT  = 0x11,
};

static ObjectId EffectMenu_id = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

static void update_effect_menu(Editor const *const editor)
{
  E(menu_set_fade(0, EffectMenu_id, ComponentId_FILL,
    !Editor_can_replace(editor)));

  E(menu_set_fade(0, EffectMenu_id, ComponentId_SMOOTH,
    !Editor_can_smooth(editor)));

  E(menu_set_fade(0, EffectMenu_id, ComponentId_REVERSE,
    !Editor_anim_is_selected(editor)));

  E(menu_set_fade(0, EffectMenu_id, ComponentId_DELETEANIM,
    !Editor_anim_is_selected(editor)));

  E(menu_set_fade(0, EffectMenu_id, ComponentId_DELETEACT,
    !Editor_trigger_is_selected(editor)));
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

  Editor const *const editor = EditWin_get_editor(edit_win);
  update_effect_menu(editor);

  return 1; /* claim event */
}

static bool is_showing_for_session(Editor const *const editor)
{
  void *const edit_win = get_ancestor_handle_if_showing(EffectMenu_id);
  Editor const *const ancestor_editor =
    edit_win ? EditWin_get_editor(edit_win) : NULL;

  return ancestor_editor == editor;
}

/* ---------------- Public functions ---------------- */

void EffectMenu_created(ObjectId const id)
{
  EffectMenu_id = id;

  EF(event_register_toolbox_handler(id, Menu_AboutToBeShown,
                                    about_to_be_shown, NULL));
}

void EffectMenu_update(Editor const *const editor)
{
  if (is_showing_for_session(editor))
  {
    update_effect_menu(editor);
  }
}
