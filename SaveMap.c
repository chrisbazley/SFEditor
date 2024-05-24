/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Save complete base map dialogue box
 *  Copyright (C) 2005 Christopher Bazley
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

/* ANSI library headers */
#include <assert.h>
#include <string.h>

/* RISC OS library files */
#include "event.h"
#include "toolbox.h"
#include "window.h"
#include "gadgets.h"

/* CBLibrary headers */
#include "err.h"
#include "macros.h"
#include "msgtrans.h"
#include "macros.h"
#include "strextra.h"
#include "GadgetUtil.h"
#include "PathTail.h"

/* Local headers */
#include "utils.h"
#include "session.h"
#include "EditWin.h"
#include "dcs_dialogue.h"
#include "savemap.h"
#include "FilenamesData.h"

/* ----------------- Gadget Ids -------------------- */

enum {
  SAVEMAP_ACTION_CANCEL      = 0x70,
  SAVEMAP_ACTION_SAVE        = 0x71,
  SAVEMAP_WRITABLE_LEAFNAME  = 0x72,
  SAVEMAP_OPTION_ONLYCHANGES = 0x73,
};

ObjectId SaveMap_sharedid = NULL_ObjectId;

static Filename name_buffer;
static Filename default_name_buffer;

/* ---------------- Private functions ---------------- */

static void resetdbox(EditSession *const session, ObjectId const self_id)
{
  char *const name = pathtail(Session_get_save_filename(session), 1);

  STRCPY_SAFE(name_buffer, name);
  STRCPY_SAFE(default_name_buffer, name);
  E(writablefield_set_value(0, self_id, SAVEMAP_WRITABLE_LEAFNAME,
  name_buffer));

  E(set_gadget_faded(self_id, SAVEMAP_OPTION_ONLYCHANGES, false));
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

  EditSession *const session = EditWin_get_session(edit_win);

  resetdbox(session, id_block->self_id);
  return 1; /* claim event */
}

static int writablehandler(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(handle);
  const WritableFieldValueChangedEvent *const wfvce =
    (WritableFieldValueChangedEvent *)event;

  STRCPY_SAFE(name_buffer, wfvce->string);

  /* enable or disable the 'changes only button' as appropriate */
  E(set_gadget_faded(id_block->self_id, SAVEMAP_OPTION_ONLYCHANGES,
             stricmp(name_buffer, default_name_buffer) != 0));

  return 1; /* claim event */
}


static int actionhandler(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);

  EditSession *const session = EditWin_get_session(edit_win);

  switch (id_block->self_component) {
    case SAVEMAP_ACTION_CANCEL:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust))
        resetdbox(session, id_block->self_id);
      return 1; /* claim event */

    case SAVEMAP_ACTION_SAVE:
      STRCPY_SAFE(default_name_buffer, name_buffer);

      if (!Session_can_quick_save(session) ||
          stricmp(Session_get_filename(session), name_buffer) != 0) {
        if (!Session_savemap(session, name_buffer, true)) /* force save */
          return 1; /* error saving */
      } else {
        /* Do not force save unless 'Changed only' button is unset */
        int changes_only;
        ON_ERR_RPT_RTN_V(optionbutton_get_state(0, id_block->self_id,
          SAVEMAP_OPTION_ONLYCHANGES, &changes_only), 1);

        if (!Session_savemap(session, name_buffer, !changes_only))
          return 1; /* error saving */
      }

      /* We may have been opened from DCS dbox - notify it of completion */
      DCS_notifysaved(id_block->parent_id, session);

      return 1; /* claim event */

    default:
      return 0; /* not interested */
  }
}

/* ---------------- Public functions ---------------- */

void SaveMap_created(ObjectId const id)
{
  SaveMap_sharedid = id;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Window_AboutToBeShown, about_to_be_shown },
    { ActionButton_Selected, actionhandler },
    { WritableField_ValueChanged, writablehandler },
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}
