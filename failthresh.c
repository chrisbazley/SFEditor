/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission failure thresholds dialogue box
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
#include "window.h"
#include "gadgets.h"

#include "Macros.h"
#include "err.h"
#include "GadgetUtil.h"

#include "Session.h"
#include "Triggers.h"
#include "EditWin.h"
#include "Utils.h"
#include "failthresh.h"
#include "DataType.h"
#include "Mission.h"

/* --------------------- Gadgets -------------------- */

enum {
  FAILTHRESH_NUMHITS      = 0x59,
  FAILTHRESH_HASTIMELIMIT = 0x7b,
  FAILTHRESH_TIMELIMIT    = 0x7d,
  FAILTHRESH_OK           = 0x80,
  FAILTHRESH_CANCEL       = 0x81,
};

/* ---------------- Private functions ---------------- */

static void read_win(EditSession *const session, ObjectId const dbox_id)
{
  int tempint = 0;

  MissionData *const m = Session_get_mission(session);
  assert(m != NULL);

  if (!E(numberrange_get_value(0, dbox_id, FAILTHRESH_NUMHITS, &tempint)))
  {
    if (tempint < 0) tempint = 0;
    triggers_set_max_losses(mission_get_triggers(m), (size_t)tempint);
  }

  if (!E(optionbutton_get_state(0, dbox_id, FAILTHRESH_HASTIMELIMIT, &tempint)))
  {
    if (tempint)
    {
      if (!E(numberrange_get_value(0, dbox_id, FAILTHRESH_TIMELIMIT, &tempint)))
      {
        mission_set_time_limit(m, tempint);
      }
    } else {
      mission_disable_time_limit(m);
    }
  }

  Session_notify_changed(session, DataType_Mission);
}

static void setup_win(EditSession *const session, ObjectId const dbox_id)
{
  MissionData *const m =Session_get_mission(session);
  assert(m != NULL);

  size_t max_losses = triggers_get_max_losses(mission_get_triggers(m));
  if (max_losses > INT_MAX) max_losses = INT_MAX;
  E(numberrange_set_value(0, dbox_id, FAILTHRESH_NUMHITS, (int)max_losses));

  E(optionbutton_set_state(0, dbox_id, FAILTHRESH_HASTIMELIMIT,
    !mission_time_limit_is_disabled(m)));

  E(set_gadget_faded(dbox_id, FAILTHRESH_TIMELIMIT,
    mission_time_limit_is_disabled(m)));

  E(numberrange_set_value(0, dbox_id, FAILTHRESH_TIMELIMIT,
    mission_get_time_limit(m)));
}

static int optionbutton_state_changed(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(handle);
  const OptionButtonStateChangedEvent *const obsce =
    (OptionButtonStateChangedEvent *)event;

  switch (id_block->self_component) {
    case FAILTHRESH_HASTIMELIMIT:
      E(set_gadget_faded(id_block->self_id, FAILTHRESH_TIMELIMIT, !obsce->new_state));
      break;

    default:
      return 0; /* not interested */
  }
  return 1; /* event handled */
}

static int actionbutton_selected(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Cancel/OK button has been tweaked on button bar */
  NOT_USED(event_code);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  EditSession *const session = EditWin_get_session(edit_win);

  switch (id_block->self_component) {
    case FAILTHRESH_CANCEL:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust)) {
        /* restore settings */
        setup_win(session, id_block->self_id);
      }
      break;

    case FAILTHRESH_OK:
      /* read settings from window */
      read_win(session, id_block->self_id);
      break;

    default:
      return 0; /* not interested in this button */
  }
  return 1; /* event handled */
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Set up dialogue window from mission data */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  EditSession *const session = EditWin_get_session(edit_win);

  setup_win(session, id_block->self_id);

  return 0; /* pass event on */
}

/* ---------------- Public functions ---------------- */

void failthresh_created(ObjectId const id)
{
  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Window_AboutToBeShown, about_to_be_shown },
    { ActionButton_Selected, actionbutton_selected },
    { OptionButton_StateChanged, optionbutton_state_changed }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(
      id, handlers[i].event_code, handlers[i].handler, NULL));
  }
}
