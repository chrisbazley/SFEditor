/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground defences dialogue box
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

#include "Session.h"
#include "EditWin.h"
#include "utils.h"
#include "groundlaser.h"
#include "DataType.h"
#include "Mission.h"

/* --------------------- Gadgets -------------------- */

enum {
  GROUNDLASER_GUNS_FREQ       = 0x60,
  GROUNDLASER_GUNS_TYPE       = 0x63,
  GROUNDLASER_HANGAR_CAPACITY = 0x68,
  GROUNDLASER_HANGAR_FREQ     = 0x6a,
  GROUNDLASER_DEFENCETIME     = 0x7f,
  GROUNDLASER_OK              = 0x80,
  GROUNDLASER_CANCEL          = 0x81,
};

/* ---------------- Private functions ---------------- */

static void read_win(EditSession *const session, ObjectId const dbox_id)
{
  int tempint;
  DefencesData *const defences = mission_get_defences(Session_get_mission(session));

  if (!E(numberrange_get_value(0, dbox_id, GROUNDLASER_GUNS_FREQ, &tempint)))
  {
    defences_set_fire_prob(defences, tempint);
  }

  if (!E(numberrange_get_value(0, dbox_id, GROUNDLASER_GUNS_TYPE, &tempint)))
  {
    defences_set_laser_type(defences, tempint);
  }

  if (!E(numberrange_get_value(0, dbox_id, GROUNDLASER_HANGAR_CAPACITY, &tempint)))
  {
    defences_set_ships_per_hangar(defences, tempint);
  }

  if (!E(numberrange_get_value(0, dbox_id, GROUNDLASER_HANGAR_FREQ, &tempint)))
  {
    defences_set_ship_prob(defences, tempint);
  }

  if (!E(numberrange_get_value(0, dbox_id, GROUNDLASER_DEFENCETIME, &tempint)))
  {
    defences_set_timer(defences, tempint);
  }

  Session_notify_changed(session, DataType_Mission);

}

static void setup_win(EditSession *const session, ObjectId const dbox_id)
{
  DefencesData const *const defences = mission_get_defences(Session_get_mission(session));

  E(numberrange_set_value(0, dbox_id, GROUNDLASER_DEFENCETIME, defences_get_timer(defences)));

  E(numberrange_set_value(0, dbox_id, GROUNDLASER_GUNS_FREQ, defences_get_fire_prob(defences)));
  E(numberrange_set_value(0, dbox_id, GROUNDLASER_GUNS_TYPE, defences_get_laser_type(defences)));

  E(numberrange_set_value(0, dbox_id, GROUNDLASER_HANGAR_CAPACITY, defences_get_ships_per_hangar(defences)));
  E(numberrange_set_value(0, dbox_id, GROUNDLASER_HANGAR_FREQ, defences_get_ship_prob(defences)));

}

static int actionbutton_selected(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Cancel/OK button has been tweaked on button bar */
  NOT_USED(handle);
  NOT_USED(event_code);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  EditSession *const session = EditWin_get_session(edit_win);

  switch (id_block->self_component) {
    case GROUNDLASER_CANCEL:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust)) {
        /* restore settings */
        setup_win(session, id_block->self_id);
      }
      break;

    case GROUNDLASER_OK:
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

void groundlaser_created(ObjectId const id)
{
  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { ActionButton_Selected, actionbutton_selected },
    { Window_AboutToBeShown, about_to_be_shown }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}
