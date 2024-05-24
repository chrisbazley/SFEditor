/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Fighter performance dialogue box
 *  Copyright (C) 2020 Christopher Bazley
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
#include <assert.h>
#include <limits.h>
#include <string.h>

#include "event.h"
#include "toolbox.h"
#include "gadgets.h"
#include "window.h"

#include "debug.h"
#include "err.h"
#include "macros.h"
#include "msgtrans.h"
#include "PathTail.h"
#include "GadgetUtil.h"
#include "EventExtra.h"

#include "Ships.h"
#include "utils.h"
#include "Session.h"
#include "graphicsdata.h"
#include "FPerfDbox.h"
#include "DataType.h"
#include "Mission.h"

/* --------------------- Gadgets -------------------- */

enum {
  ComponentId_SHIELDSTREN    = 0x8,

  ComponentId_LASERFREQ    = 0x0,
  ComponentId_LASERTYPE    = 0x2,

  ComponentId_SPEED        = 0x4,
  ComponentId_MANOEUVRE    = 0x6,
  ComponentId_ATAFREQ      = 0xa,

  ComponentId_CANCEL       = 0xe,
  ComponentId_OK           = 0xf,
};

/* ---------------- Private functions ---------------- */

static void read_win(ObjectId const performance_dialogue, FPerfDboxData *const performance_data)
{
  DEBUG("Reading performance data for ship index %d", performance_data->ship_type);
  MissionData *const m = Session_get_mission(performance_data->session);
  FighterPerform *const fperf = fighter_perform_get_ship(mission_get_fighter_perform(m), performance_data->ship_type);

  /* Get general performance data */
  int temp = 0;
  if (!E(numberrange_get_value(0, performance_dialogue, ComponentId_SHIELDSTREN, &temp)))
  {
    fighter_perform_set_shields(fperf, temp);
  }

  if (!E(numberrange_get_value(0, performance_dialogue, ComponentId_LASERFREQ, &temp)))
  {
    fighter_perform_set_laser_prob(fperf, temp);
  }

  if (!E(numberrange_get_value(0, performance_dialogue, ComponentId_LASERTYPE, &temp)))
  {
    fighter_perform_set_laser_type(fperf, temp);
  }

  if (!E(numberrange_get_value(0, performance_dialogue, ComponentId_ATAFREQ, &temp)))
  {
    fighter_perform_set_missile_prob(fperf, temp);
  }

  /* Get fighter performance data */
  if (!E(numberrange_get_value(0, performance_dialogue, ComponentId_SPEED, &temp)))
  {
    fighter_perform_set_engine(fperf, temp);
  }

  if (!E(numberrange_get_value(0, performance_dialogue, ComponentId_MANOEUVRE, &temp)))
  {
    fighter_perform_set_control(fperf, temp);
  }
}

static void setup_win(ObjectId const performance_dialogue, FPerfDboxData *performance_data)
{
  DEBUG("Displaying performance data for ship index %d", performance_data->ship_type);
  MissionData *const m = Session_get_mission(performance_data->session);
  FighterPerform *const fperf = fighter_perform_get_ship(mission_get_fighter_perform(m), performance_data->ship_type);

  /* Set general performance data */
  E(numberrange_set_value(false, performance_dialogue,
             ComponentId_SHIELDSTREN, fighter_perform_get_shields(fperf)));

  E(numberrange_set_value(0, performance_dialogue,
             ComponentId_LASERFREQ, fighter_perform_get_laser_prob(fperf)));

  E(numberrange_set_value(0, performance_dialogue,
             ComponentId_LASERTYPE, fighter_perform_get_laser_type(fperf)));

  E(numberrange_set_value(0, performance_dialogue,
             ComponentId_ATAFREQ, fighter_perform_get_missile_prob(fperf)));

  /* Set fighter performance data */
  E(numberrange_set_value(0, performance_dialogue, ComponentId_SPEED,
             fighter_perform_get_engine(fperf)));

  E(numberrange_set_value(0, performance_dialogue,
             ComponentId_MANOEUVRE, fighter_perform_get_control(fperf)));
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Dialogue box about to open */
  NOT_USED(event);
  NOT_USED(event_code);
  FPerfDboxData *const performance_data = handle;

  setup_win(id_block->self_id, performance_data);

  return 1; /* claim event */
}

static int actionbutton_selected(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Cancel/OK button has been activated */
  NOT_USED(event_code);
  FPerfDboxData *const performance_data = handle;

  switch (id_block->self_component) {
    case ComponentId_CANCEL:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust)) {
        /* Restore settings */
        setup_win(id_block->self_id, performance_data);
      }
      break;

    case ComponentId_OK:
      /* Read settings from window */
      read_win(id_block->self_id, performance_data);
      Session_notify_changed(performance_data->session, DataType_Mission);
      break;

  }
  return 1; /* event handled */
}

/* ---------------- Public functions ---------------- */

bool FPerfDbox_init(FPerfDboxData *const performance_data,
                      EditSession *const session,
                      ShipType const ship_type)
{
  assert(performance_data != NULL);
  assert(session != NULL);
  assert(ship_type >= ShipType_Fighter1);
  assert(ship_type <= ShipType_Fighter4);

  *performance_data = (FPerfDboxData){
    .ship_type = ship_type,
    .session = session,
  };
  /* The rest of data is not set up until the dbox is opened */

  /* Create dialogue box object */
  if (E(toolbox_create_object(0, "FPerf", &performance_data->my_object)))
  {
    return false;
  }

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { ActionButton_Selected, actionbutton_selected },
    { Window_AboutToBeShown, about_to_be_shown },
  };

  bool success = true;
  for (size_t i = 0; success && (i < ARRAY_SIZE(handlers)); ++i)
  {
    if (E(event_register_toolbox_handler(performance_data->my_object,
                                         handlers[i].event_code,
                                         handlers[i].handler, performance_data)))
      success = false;
  }

  if (success)
  {
    FPerfDbox_update_title(performance_data);
  }
  else
  {
    FPerfDbox_destroy(performance_data);
  }

  return success;
}

void FPerfDbox_update_title(FPerfDboxData *const performance_data)
{
  assert(performance_data != NULL);
  char const *const file_name = Session_get_filename(performance_data->session);

  StringBuffer ship_name;
  stringbuffer_init(&ship_name);

  FilenamesData const *const filenames = Session_get_filenames(performance_data->session);

  if (get_shipname_from_type(&ship_name, filenames_get(filenames, DataType_PolygonMeshes),
                             performance_data->ship_type))
  {
    E(window_set_title(0, performance_data->my_object,
                                msgs_lookup_subn("PerfTitle", 2,
                                  pathtail(file_name, 1), stringbuffer_get_pointer(&ship_name))));
  }
  else
  {
    report_error(SFERROR(NoMem), "", "");
  }
  stringbuffer_destroy(&ship_name);
}

void FPerfDbox_show(FPerfDboxData *const performance_data)
{
  assert(performance_data != NULL);
  E(toolbox_show_object(0, performance_data->my_object,
                                 Toolbox_ShowObject_Centre, NULL,
                                 NULL_ObjectId, NULL_ComponentId));
}

void FPerfDbox_destroy(FPerfDboxData *const performance_data)
{
  assert(performance_data != NULL);
  E(remove_event_handlers_delete(performance_data->my_object));
}
