/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Pre-quit dialogue box
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

#include "stdlib.h"
#include <stdbool.h>
#include "stdio.h"
#include <assert.h>

/* RISC OS library files */
#include "event.h"
#include "toolbox.h"
#include "quit.h"
#include "wimplib.h"

/* My library files */
#include "err.h"
#include "msgtrans.h"
#include "Macros.h"
#include "InputFocus.h"
#include "Entity2.h"
#include "Debug.h"

/* Local headers */
#include "Session.h"
#include "PreQuit.h"

/* Constant numeric values */
enum
{
  WimpKey_CtrlShiftF12 = 0x1FC,
  MaxUnsavedCountLen   = 15
};

static ObjectId dbox_id = NULL_ObjectId;
static int quit_sender;

/* ----------------------------------------------------------------------- */
/*                         Private functions                               */

static void cb_released(void)
{
  DEBUGF("Clipboard released - terminating\n");
  exit(EXIT_SUCCESS);
}

/* ----------------------------------------------------------------------- */

static int quit_handler(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* We won't be alive to hear the MenusDeleted msg, so fake it */
  NOT_USED(handle);
  NOT_USED(event);
  NOT_USED(event_code);
  NOT_USED(id_block);

  E(InputFocus_restorecaret());

  /* Do as Paint, Edit and Draw do - that is, discard all data */
  Session_all_delete();

  if (quit_sender)
  {
    /* Restart desktop shutdown */
    WimpKeyPressedEvent key_event;

    /* Restart desktop shutdown */
    if (!E(wimp_get_caret_position(&key_event.caret)))
    {
      key_event.key_code = WimpKey_CtrlShiftF12;
      DEBUGF("Sending event (w:%d i:%d x:%d y:%d) to task %d to restart desktop shutdown\n",
            key_event.caret.window_handle, key_event.caret.icon_handle,
            key_event.caret.xoffset, key_event.caret.yoffset, quit_sender);

      E(wimp_send_message(Wimp_EKeyPressed, &key_event, quit_sender,
                          0, NULL));
    }
  }
  else
  {
    /* We may own the global clipboard, so offer the associated data to
       any 'holder' application before exiting. */
    E(entity2_dispose_all(cb_released));
  }

  return 1; /* claim event */
}

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

void PreQuit_created(ObjectId const PreQuit_id)
{
  /* Record ID */
  dbox_id = PreQuit_id;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Quit_Quit, quit_handler },
    { Quit_AboutToBeShown, InputFocus_recordcaretpos }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(PreQuit_id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}

bool PreQuit_queryunsaved(int const task_handle)
{
  /* Return true from this function in order to prevent immediate quit */
  int const unsaved_count = Session_all_count_modified();

  if (unsaved_count > 1)
  {
    char number[MaxUnsavedCountLen + 1];
    sprintf(number, "%d", unsaved_count);
    E(quit_set_message(0, dbox_id,
                 msgs_lookup_subn("UnsWarn", 1, number)));
  }
  else if (unsaved_count > 0)
  {
    E(quit_set_message(0, dbox_id, msgs_lookup("UnsWarn1")));
  }
  else
  {
    /* No files have unsaved modifications */
    return false; /* may quit */
  }

  quit_sender = task_handle;

  E(toolbox_show_object(Toolbox_ShowObject_AsMenu, dbox_id,
     Toolbox_ShowObject_Centre, NULL, NULL_ObjectId, NULL_ComponentId));

  return true; /* cannot quit whilst dialogue box is open */
}
