/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Special ship dialogue box
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
#include "stdlib.h"
#include <string.h>

#include "toolbox.h"
#include "event.h"
#include "window.h"
#include "gadgets.h"

#include "StrExtra.h"
#include "Macros.h"
#include "err.h"
#include "msgtrans.h"
#include "PathTail.h"
#include "GadgetUtil.h"
#include "StringBuff.h"
#include "EventExtra.h"

#include "SFError.h"
#include "SpecialShip.h"
#include "utils.h"
#include "Session.h"
#include "graphicsdata.h"
#include "DataType.h"
#include "Mission.h"
#include "Player.h"

/* --------------------- Gadgets -------------------- */

enum {
  ComponentId_Background = -2,
  ComponentId_CANCEL    = 0xe,
  ComponentId_OK        = 0xf,
  ComponentId_SHIPTYPE  = 0x58,
  ComponentId_ENABLE    = 0x59,
  ComponentId_SHIELDS   = 0x8,
  ComponentId_LASERTYPE = 0x0,
  ComponentId_SPEED     = 0x4,
  ComponentId_MANOEUVRE = 0x6,
  ComponentId_ATA       = 0x30,
  ComponentId_ATG       = 0x31,
  ComponentId_MINES     = 0x32,
  ComponentId_BOMBS     = 0x33,
  ComponentId_MEGALASER = 0x37,
  ComponentId_MULTI     = 0x38,
};

static ComponentId const gadgets_list[] = {
  ComponentId_SHIELDS, ComponentId_LASERTYPE,
  ComponentId_SPEED, ComponentId_MANOEUVRE, ComponentId_ATA, ComponentId_ATG,
  ComponentId_MINES, ComponentId_BOMBS, ComponentId_MEGALASER,
  ComponentId_MULTI
};

/* ---------------- Private functions --------------- */

static void setup_win(EditSession *const session, ObjectId const dbox_id)
{
  /* Update state of the dialogue box from the mission data associated with
     an editing session */
  PlayerData *const s = mission_get_player(Session_get_mission(session));

  E(stringset_set_selected(StringSet_IndexedSelection,
             dbox_id, ComponentId_SHIPTYPE,
             (char *)(player_get_ship_type(s) - ShipType_Player)));

  E(optionbutton_set_state(0, dbox_id, ComponentId_ENABLE,
             player_get_equip_enabled(s)));

  for (size_t i = 0; i < ARRAY_SIZE(gadgets_list); i++)
  {
    E(set_gadget_faded(dbox_id, gadgets_list[i],
      !player_get_equip_enabled(s)));
  }

  E(numberrange_set_value(0, dbox_id, ComponentId_SHIELDS,
             player_get_shields(s)));

  E(numberrange_set_value(0, dbox_id, ComponentId_LASERTYPE,
             player_get_laser_type(s)));

  E(numberrange_set_value(0, dbox_id, ComponentId_SPEED,
             player_get_engine(s)));

  E(numberrange_set_value(0, dbox_id, ComponentId_MANOEUVRE,
             player_get_control(s)));

  E(numberrange_set_value(0, dbox_id, ComponentId_ATA,
             player_get_ata(s)));

  E(numberrange_set_value(0, dbox_id, ComponentId_ATG,
             player_get_atg(s)));

  E(numberrange_set_value(0, dbox_id, ComponentId_MINES,
             player_get_mines(s)));

  E(numberrange_set_value(0, dbox_id, ComponentId_BOMBS,
             player_get_bombs(s)));

  E(numberrange_set_value(0, dbox_id, ComponentId_MEGALASER,
             player_get_mega_laser(s)));

  E(numberrange_set_value(0, dbox_id, ComponentId_MULTI,
             player_get_multi_ata(s)));
}

static void read_win(EditSession *const session, ObjectId const dbox_id)
{
  /* Update the mission data associated with an editing session from the
     state of the dialogue box */
  int state = 0;
  PlayerData *const s = mission_get_player(Session_get_mission(session));

  if (!E(stringset_get_selected(StringSet_IndexedSelection, dbox_id, ComponentId_SHIPTYPE, &state)))
  {
    player_set_ship_type(s, (ShipType)state);
  }

  /* Is the special ship enabled? */
  if (!E(optionbutton_get_state(0, dbox_id, ComponentId_ENABLE, &state)))
  {
    player_set_equip_enabled(s, state);
  }

  /* Read special ship details */
  if (!E(numberrange_get_value(0, dbox_id, ComponentId_SHIELDS, &state)))
  {
    player_set_shields(s, state);
  }

  if (!E(numberrange_get_value(0, dbox_id, ComponentId_LASERTYPE,
         &state)))
  {
    player_set_laser_type(s, state);
  }

  if (!E(numberrange_get_value(0, dbox_id, ComponentId_SPEED, &state)))
  {
    player_set_engine(s, state);
  }

  if (!E(numberrange_get_value(0, dbox_id, ComponentId_MANOEUVRE,
      &state)))
  {
    player_set_control(s, state);
  }

  if (!E(numberrange_get_value(0, dbox_id, ComponentId_ATA, &state)))
  {
    player_set_ata(s, state);
  }

  if (!E(numberrange_get_value(0, dbox_id, ComponentId_ATG, &state)))
  {
    player_set_atg(s, state);
  }

  if (!E(numberrange_get_value(0, dbox_id, ComponentId_MINES, &state)))
  {
    player_set_mines(s, state);
  }

  if (!E(numberrange_get_value(0, dbox_id, ComponentId_BOMBS, &state)))
  {
    player_set_bombs(s, state);
  }

  if (!E(numberrange_get_value(0, dbox_id, ComponentId_MEGALASER,
       &state)))
  {
    player_set_mega_laser(s, state);
  }

  if (!E(numberrange_get_value(0, dbox_id, ComponentId_MULTI, &state)))
  {
    player_set_multi_ata(s, state);
  }
}

static int optionbutton_state_changed(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Option button has been tweaked */
  NOT_USED(event_code);
  NOT_USED(handle);
  const OptionButtonStateChangedEvent *const obsce =
    (OptionButtonStateChangedEvent *)event;

  switch (id_block->self_component) {
    case ComponentId_ENABLE:
      /* Fade or unfade the controls over the special ship's attributes
         because it has been enabled or disabled */
      for (size_t i = 0; i < ARRAY_SIZE(gadgets_list); i++)
        E(set_gadget_faded(id_block->self_id, gadgets_list[i], !obsce->new_state));

      if (obsce->new_state)
        E(gadget_set_focus(0, id_block->self_id, ComponentId_SHIELDS));
      break;

    default:
      return 0; /* not interested */
  }
  return 1; /* event handled */
}

static int actionbutton_selected(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Cancel/OK button has been activated */
  NOT_USED(event_code);
  SpecialShipData *const special_ship_data = handle;
  assert(special_ship_data != NULL);

  switch (id_block->self_component) {
    case ComponentId_CANCEL:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust)) {
        /* Make the dialogue box reflect the mission data */
        setup_win(special_ship_data->session, id_block->self_id);
      }
      break;

    case ComponentId_OK:
      /* Update the mission data from the dialogue box */
      read_win(special_ship_data->session, id_block->self_id);
      break;
  }
  return 1; /* event handled */
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Dialogue box about to open */
  NOT_USED(event_code);
  NOT_USED(event);
  SpecialShipData *const special_ship_data = handle;

  /* Populate string set with list of ships & select appropriate one */
  FilenamesData const *const filenames = Session_get_filenames(special_ship_data->session);

  if (stricmp(filenames_get(filenames, DataType_PolygonMeshes),
              special_ship_data->polygonal_objects_set) != 0)
  {
    DEBUG("Rebuilding player ship type stringset for graphics set '%s' (was '%s')",
          filenames_get(filenames, DataType_PolygonMeshes), special_ship_data->polygonal_objects_set);

    STRCPY_SAFE(special_ship_data->polygonal_objects_set,
                filenames_get(filenames, DataType_PolygonMeshes));

    StringBuffer ships_stringset;
    stringbuffer_init(&ships_stringset);
    if (!build_ships_stringset(&ships_stringset, special_ship_data->polygonal_objects_set,
                               true, true, false, false)) {
      report_error(SFERROR(NoMem), "", "");
    } else {
      E(stringset_set_available(0, id_block->self_id,
                 ComponentId_SHIPTYPE, stringbuffer_get_pointer(&ships_stringset)));
    }
    stringbuffer_destroy(&ships_stringset);
  }

  setup_win(special_ship_data->session, id_block->self_id);

  /* Update the default input focus as necessary to avoid an error
     when all the writable gadgets are faded. */
  PlayerData *const s = mission_get_player(Session_get_mission(special_ship_data->session));

  E(window_set_default_focus(0, id_block->self_id,
             player_get_equip_enabled(s) ? ComponentId_SHIELDS : ComponentId_Background));

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

bool SpecialShip_init(SpecialShipData *const special_ship_data, EditSession *const session)
{
  special_ship_data->session = session;
  strcpy(special_ship_data->polygonal_objects_set, "");

  if (E(toolbox_create_object(0, "SpecialShip", &special_ship_data->my_object)))
    return false;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { ActionButton_Selected, actionbutton_selected },
    { OptionButton_StateChanged, optionbutton_state_changed },
    { Window_AboutToBeShown, about_to_be_shown }
  };

  bool success = true;
  for (size_t i = 0; success && (i < ARRAY_SIZE(handlers)); ++i)
  {
    if (E(event_register_toolbox_handler(special_ship_data->my_object,
                                         handlers[i].event_code,
                                         handlers[i].handler, special_ship_data)))
      success = false;
  }

  if (success)
  {
    SpecialShip_update_title(special_ship_data);
  }
  else
  {
    SpecialShip_destroy(special_ship_data);
  }

  return success;
}

void SpecialShip_update_title(SpecialShipData *const special_ship_data)
{
  assert(special_ship_data != NULL);
  char const *const file_name = Session_get_filename(special_ship_data->session);
  E(window_set_title(0, special_ship_data->my_object, msgs_lookup_subn("SpecTitle", 1,
                              pathtail(file_name, 1))));
}

void SpecialShip_show(SpecialShipData *const special_ship_data)
{
  assert(special_ship_data != NULL);
  E(toolbox_show_object(0, special_ship_data->my_object,
                                 Toolbox_ShowObject_Centre, NULL,
                                 NULL_ObjectId, NULL_ComponentId));
}

void SpecialShip_destroy(SpecialShipData *const special_ship_data)
{
  assert(special_ship_data != NULL);
  E(remove_event_handlers_delete(special_ship_data->my_object));
}
