/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Big ships performance dialogue box
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
#include "Mission.h"
#include "graphicsdata.h"
#include "BPerf.h"
#include "BPerfDbox.h"
#include "DataType.h"

/* --------------------- Gadgets -------------------- */

enum {
  ComponentId_SHIELDSTREN    = 0x8,
  ComponentId_SHIELDSTRENLAB = 0x9,
  ComponentId_REMOTE       = 0x59,

  ComponentId_LASERFREQ    = 0x0,
  ComponentId_LASERTYPE    = 0x2,

  ComponentId_ATAFREQ      = 0xa,

  ComponentId_NUMPLEBS     = 0x10,
  ComponentId_PLEBFREQ     = 0xd,

  ComponentId_CANCEL       = 0xe,
  ComponentId_OK           = 0xf,
};

/* ---------------- Private functions ---------------- */

static void read_win(ObjectId const performance_dbox, BPerfDboxData *const performance_data)
{
  DEBUGF("Reading performance data for big ship type %d\n", performance_data->ship_type);
  MissionData *const m = Session_get_mission(performance_data->session);
  BigPerform *const bperf = big_perform_get_ship(mission_get_big_perform(m), performance_data->ship_type);

  /* Get general performance data */
  int temp = 0;

  if (!E(optionbutton_get_state(0, performance_dbox, ComponentId_REMOTE, &temp)))
  {
    if (temp)
    {
      big_perform_set_remote_shield(bperf);
    }
    else if (!E(numberrange_get_value(0, performance_dbox, ComponentId_SHIELDSTREN, &temp)))
    {
      big_perform_set_shields(bperf, temp);
    }
  }

  if (!E(numberrange_get_value(0, performance_dbox, ComponentId_LASERFREQ, &temp)))
  {
    big_perform_set_laser_prob(bperf, temp);
  }

  if (!E(numberrange_get_value(0, performance_dbox, ComponentId_LASERTYPE, &temp)))
  {
    big_perform_set_laser_type(bperf, temp);
  }

  if (!E(numberrange_get_value(0, performance_dbox, ComponentId_ATAFREQ, &temp)))
  {
    big_perform_set_missile_prob(bperf, temp);
  }

  /* Get big ship performance data */
  if (!E(numberrange_get_value(0, performance_dbox, ComponentId_NUMPLEBS, &temp)))
  {
    big_perform_set_ship_count(bperf, temp);
  }

  if (!E(numberrange_get_value(0, performance_dbox, ComponentId_PLEBFREQ, &temp)))
  {
    big_perform_set_ship_prob(bperf, temp);
  }
}

static void fade_shield(ObjectId const performance_dbox, bool const remote_shield)
{
  static ComponentId const shield_gadgets[] = {
    ComponentId_SHIELDSTREN, ComponentId_SHIELDSTRENLAB};

  for (size_t i = 0; i < ARRAY_SIZE(shield_gadgets); ++i)
  {
    E(set_gadget_faded(performance_dbox, shield_gadgets[i], remote_shield));
  }
}

static void setup_win(ObjectId const performance_dbox, BPerfDboxData const *const performance_data)
{
  DEBUGF("Displaying performance data for big ship type %d\n", performance_data->ship_type);
  MissionData *const m = Session_get_mission(performance_data->session);
  BigPerform const *const bperf = big_perform_get_ship(mission_get_big_perform(m), performance_data->ship_type);

  /* Set general performance data */
  bool const has_remote_shield = big_perform_has_remote_shield(bperf);
  E(optionbutton_set_state(0, performance_dbox, ComponentId_REMOTE, has_remote_shield));

  fade_shield(performance_dbox, has_remote_shield);

  E(numberrange_set_value(0, performance_dbox,
             ComponentId_SHIELDSTREN, big_perform_get_shields(bperf)));

  E(numberrange_set_value(0, performance_dbox,
             ComponentId_LASERFREQ, big_perform_get_laser_prob(bperf)));

  E(numberrange_set_value(0, performance_dbox,
             ComponentId_LASERTYPE, big_perform_get_laser_type(bperf)));

  E(numberrange_set_value(0, performance_dbox,
             ComponentId_ATAFREQ, big_perform_get_missile_prob(bperf)));

  /* Set big ship performance data */
  E(numberrange_set_value(0, performance_dbox,
             ComponentId_NUMPLEBS, big_perform_get_ship_count(bperf)));

  E(numberrange_set_value(0, performance_dbox,
             ComponentId_PLEBFREQ, big_perform_get_ship_prob(bperf)));
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Dialogue box about to open */
  NOT_USED(event);
  NOT_USED(event_code);
  BPerfDboxData *const performance_data = handle;

  setup_win(id_block->self_id, performance_data);

  return 1; /* claim event */
}

static int optionhandler(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* State of option button has changed */
  NOT_USED(handle);
  NOT_USED(event_code);
  const OptionButtonStateChangedEvent *const obsce =
    (OptionButtonStateChangedEvent *)event;

  if (id_block->self_component != ComponentId_REMOTE)
    return 0; /* unknown gadget */

  fade_shield(id_block->self_id, obsce->new_state);
  return 1; /* claim event */
}

static int actionbutton_selected(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Cancel/OK button has been activated */
  NOT_USED(event_code);
  BPerfDboxData *const performance_data = handle;

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

bool BPerfDbox_init(BPerfDboxData *const performance_data,
                      EditSession *const session,
                      ShipType const ship_type)
{
  assert(performance_data != NULL);
  assert(session != NULL);
  assert(ship_type >= ShipType_Big1);
  assert(ship_type <= ShipType_Big3);

  *performance_data = (BPerfDboxData){
    .ship_type = ship_type,
    .session = session,
  };
  /* The rest of data is not set up until the dbox is opened */

  /* Create dialogue box object */
  if (E(toolbox_create_object(0, "BPerf", &performance_data->my_object)))
  {
    return false;
  }

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { ActionButton_Selected, actionbutton_selected },
    { Window_AboutToBeShown, about_to_be_shown },
    { OptionButton_StateChanged, optionhandler }
  };

  bool success = true;
  for (size_t i = 0; success && (i < ARRAY_SIZE(handlers)); ++i)
  {
    if (E(event_register_toolbox_handler(performance_data->my_object,
                                         handlers[i].event_code,
                                         handlers[i].handler, performance_data)))
      success = false;
  }

  if (success) {
    BPerfDbox_update_title(performance_data);
  } else {
    BPerfDbox_destroy(performance_data);
  }

  return success;
}

void BPerfDbox_update_title(BPerfDboxData *const performance_data)
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

void BPerfDbox_show(BPerfDboxData *const performance_data)
{
  assert(performance_data != NULL);
  E(toolbox_show_object(0, performance_data->my_object,
                                 Toolbox_ShowObject_Centre, NULL,
                                 NULL_ObjectId, NULL_ComponentId));
}

void BPerfDbox_destroy(BPerfDboxData *const performance_data)
{
  assert(performance_data != NULL);
  E(remove_event_handlers_delete(performance_data->my_object));
}
