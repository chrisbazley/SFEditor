/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  DCS dialogue box
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

#include "event.h"
#include "toolbox.h"
#include "DCS.h"

#include "err.h"
#include "Macros.h"
#include "msgtrans.h"

#include "SaveMiss.h"
#include "SaveMap.h"
#include "EditWin.h"
#include "utils.h"
#include "DCS_dialogue.h"
#include "Session.h"

static bool dcs_open_parent = false;
static ObjectId dcs_sharedid = NULL_ObjectId;

/* ----------------------------------------------------------------------- */
/*                         Private functions                               */

static int actions_handler(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  EditSession *const session = EditWin_get_session(edit_win);

  switch (event_code) {
    case DCS_Save:
      if (!Session_can_quick_save(session)) {
        /* Must open savebox first */
        open_topleftofwin(Toolbox_ShowObject_AsMenu,
                          Session_get_ui_type(session) == UI_TYPE_MISSION ?
                          SaveMiss_sharedid : SaveMap_sharedid,
                          id_block->ancestor_id,
                          id_block->self_id,
                          id_block->self_component);
        return 1; /* claim event */
      }

      /* Save immediately */
      if (!Session_quick_save(session))
        return 1; /* failed */

      /* After successful save, carry straight on as for Discard... */

    case DCS_Discard:
      DCS_notifysaved(id_block->self_id, session);
      return 1; /* claim event */
  }

  return 0; /* He's not the messiah, he's a very naughty boy! */
}

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

void DCS_created(ObjectId const dcs_id)
{
  /* Record ID */
  dcs_sharedid = dcs_id;

  /* Install handlers */
  EF(event_register_toolbox_handler(dcs_id, -1, actions_handler, NULL));
}

void DCS_queryunsaved(ObjectId const edit_win, int const num_files,
  bool const open_parent)
{
  if (num_files > 1) {
    char numstr[12];
    sprintf(numstr, "%d", num_files);

    E(dcs_set_message(0, dcs_sharedid, msgs_lookup_subn("UnsWarn", 1,
    numstr)));
  } else
    E(dcs_set_message(0, dcs_sharedid, msgs_lookup("UnsWarn1")));

  open_topleftofwin(Toolbox_ShowObject_AsMenu, dcs_sharedid, edit_win, edit_win,
  NULL_ComponentId);

  dcs_open_parent = open_parent;
}

void DCS_notifysaved(ObjectId const savebox_parent, EditSession *const session)
{
  if (savebox_parent != dcs_sharedid)
    return;

  if (dcs_open_parent)
    Session_openparentdir(session); /* Open parent directory */

  Session_destroy(session);
}
