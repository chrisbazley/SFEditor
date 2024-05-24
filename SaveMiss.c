/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Save complete mission dialogue box
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
#include "stdio.h"
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
#include "pathtail.h"
#include "StrExtra.h"
#include "GadgetUtil.h"

/* Local headers */
#include "utils.h"
#include "session.h"
#include "EditWin.h"
#include "dcs_dialogue.h"
#include "savemiss.h"
#include "filepaths.h"
#include "mission.h"
#include "pyram.h"

/* ----------------- Gadget Ids -------------------- */

enum {
  ComponentId_NUMBER_RANGE       = 0x6e,
  ComponentId_ACTION_CANCEL      = 0x70,
  ComponentId_ACTION_SAVE        = 0x71,
  ComponentId_WRITABLE_LEAFNAME  = 0x72,
  ComponentId_OPTION_ONLYCHANGES = 0x73,
  ComponentId_RADIO_EASY         = 0x74,
  ComponentId_RADIO_MEDIUM       = 0x75,
  ComponentId_RADIO_HARD         = 0x76,
  ComponentId_RADIO_EXTRA        = 0x77,
};

ObjectId SaveMiss_sharedid = NULL_ObjectId;

static ComponentId radio_selected, default_radio_selected = NULL_ComponentId;
static char miss_name[sizeof(Filename)-2];
static char default_miss_name[sizeof(Filename)-2];
static int miss_number, default_miss_number;

/* ---------------- Private functions ---------------- */

static void resetdbox(EditSession *const session, ObjectId const self_id)
{
  PyramidData *const p = mission_get_pyramid(Session_get_mission(session));

  switch (pyramid_get_difficulty(p)) {
    case Pyramid_Easy:
      radio_selected = ComponentId_RADIO_EASY;
      break;

    case Pyramid_Medium:
      radio_selected = ComponentId_RADIO_MEDIUM;
      break;

    case Pyramid_Hard:
      radio_selected = ComponentId_RADIO_HARD;
      break;

    case Pyramid_User:
      radio_selected = ComponentId_RADIO_EXTRA;
      break;

    default:
      assert(false);
      return;
  }
  default_radio_selected = radio_selected;
  E(radiobutton_set_state(0, self_id, radio_selected, 1));

  {
    char *const name = pathtail(Session_get_save_filename(session), 1);
    STRCPY_SAFE(miss_name, name);
    STRCPY_SAFE(default_miss_name, name);
    E(writablefield_set_value(0, self_id, ComponentId_WRITABLE_LEAFNAME, miss_name));
  }

  miss_number = pyramid_get_level_number(p);
  default_miss_number = miss_number;
  E(numberrange_set_value(0, self_id, ComponentId_NUMBER_RANGE,
    miss_number));

  E(set_gadget_faded(self_id,
    ComponentId_WRITABLE_LEAFNAME, radio_selected != ComponentId_RADIO_EXTRA));

  E(set_gadget_faded(self_id,
    ComponentId_NUMBER_RANGE, radio_selected == ComponentId_RADIO_EXTRA));

  E(set_gadget_faded(self_id, ComponentId_OPTION_ONLYCHANGES, false));

  E(window_set_default_focus(0, self_id,
    radio_selected == ComponentId_RADIO_EXTRA ?
    ComponentId_WRITABLE_LEAFNAME : ComponentId_NUMBER_RANGE));
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
    case ComponentId_ACTION_CANCEL:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust))
        resetdbox(session, id_block->self_id);
      return 1; /* claim event */

    case ComponentId_ACTION_SAVE:
      {
        Filename sub_path;
        Pyramid pyramid_number = Pyramid_Easy;

        default_radio_selected = radio_selected;
        default_miss_number = miss_number;
        STRCPY_SAFE(default_miss_name, miss_name);

        switch (radio_selected) {
          case ComponentId_RADIO_EASY:
            pyramid_number = Pyramid_Easy;
            break;
          case ComponentId_RADIO_MEDIUM:
            pyramid_number = Pyramid_Medium;
            break;
          case ComponentId_RADIO_HARD:
            pyramid_number = Pyramid_Hard;
            break;
          case ComponentId_RADIO_EXTRA:
            pyramid_number = Pyramid_User;
            break;
          default:
            assert(false);
            return 1;
        }

        int changes_only;
        ON_ERR_RPT_RTN_V(optionbutton_get_state(0, id_block->self_id,
          ComponentId_OPTION_ONLYCHANGES, &changes_only), 1);

        get_mission_file_name(sub_path, pyramid_number, miss_number, miss_name);

        PyramidData *const p = mission_get_pyramid(Session_get_mission(session));

        if (!Session_can_quick_save(session) ||
            stricmp(Session_get_filename(session), sub_path) != 0)
        {
          /* Update pyramid and level number stored in mission data */
          Pyramid const old_pyr = pyramid_get_difficulty(p);
          int old_lev = pyramid_get_level_number(p);

          if (pyramid_number != Pyramid_User)
          {
            pyramid_set_position(p, pyramid_number, miss_number);
          }
          else
          {
            pyramid_set_position(p, pyramid_number, 1);
          }

          /* Force all files to be saved (path has changed) */
          if (!Session_savemission(session, sub_path, true)) {
            pyramid_set_position(p, old_pyr, old_lev);
            return 1; /* error saving */
          }
        }
        else
        {
          /* Do not force save unless 'Changed only' button is unset */
          if (!Session_savemission(session, sub_path, !changes_only))
          {
            return 1; /* error saving */
          }
        }
      }

      /* We may have been opened from DCS dbox - notify it of completion */
      DCS_notifysaved(id_block->parent_id, session);

      return 1; /* claim event */

    default:
      return 0; /* not interested */
  }
}

static int writablehandler(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(handle);
  const WritableFieldValueChangedEvent *const wfvce =
    (WritableFieldValueChangedEvent *)event;

  STRCPY_SAFE(miss_name, wfvce->string);

  /* check whether we should enable/disable the 'changes only button' */
  if (radio_selected == ComponentId_RADIO_EXTRA) {
    E(set_gadget_faded(id_block->self_id, ComponentId_OPTION_ONLYCHANGES,
                                stricmp(miss_name, default_miss_name) != 0));
  }

  return 1; /* claim event */
}

static int numberhandler(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(handle);
  const NumberRangeValueChangedEvent *const nrvce =
    (NumberRangeValueChangedEvent *)event;

  miss_number = nrvce->new_value;

  /* check whether we should enable/disable the 'changes only button' */
  if (radio_selected != ComponentId_RADIO_EXTRA) {
    E(set_gadget_faded(id_block->self_id, ComponentId_OPTION_ONLYCHANGES,
                                miss_number != default_miss_number));
  }

  return 1; /* claim event */
}

static int radiobutton_state_changed(int const event_code,
  ToolboxEvent *const event, IdBlock *const id_block, void *const handle)
{
  /* Radio button has been tweaked */
  NOT_USED(event_code);
  NOT_USED(handle);
  const RadioButtonStateChangedEvent *const rbsce =
    (RadioButtonStateChangedEvent *)event;
  if (rbsce->state != 1)
    return 0; /* event not handled */

  radio_selected = id_block->self_component;

  /* Activate gadgets for newly pressed button */
  switch (radio_selected) {
    case ComponentId_RADIO_EASY:
    case ComponentId_RADIO_MEDIUM:
    case ComponentId_RADIO_HARD:
      if (rbsce->old_on_button == ComponentId_RADIO_EXTRA) {
        E(set_gadget_faded(id_block->self_id, ComponentId_WRITABLE_LEAFNAME, true));
        E(set_gadget_faded(id_block->self_id, ComponentId_NUMBER_RANGE, false));

        /* Set input focus to the level number writable field */
        E(gadget_set_focus(0, id_block->self_id, ComponentId_NUMBER_RANGE));
        E(window_set_default_focus(0, id_block->self_id,
                                            ComponentId_NUMBER_RANGE));
      }
      break;

    case ComponentId_RADIO_EXTRA:
      if (rbsce->old_on_button != ComponentId_RADIO_EXTRA) {
        E(set_gadget_faded(id_block->self_id, ComponentId_NUMBER_RANGE, true));
        E(set_gadget_faded(id_block->self_id, ComponentId_WRITABLE_LEAFNAME, false));

        /* Set input focus to the filename writable field */
        E(gadget_set_focus(0, id_block->self_id,
                                    ComponentId_WRITABLE_LEAFNAME));
        E(window_set_default_focus(0, id_block->self_id,
                                            ComponentId_WRITABLE_LEAFNAME));
      }
      break;
  }

  /* Enable or disable the 'Changes only' button as appropriate */
  {
    bool fade;
    if (default_radio_selected != radio_selected) {
      fade = true;
    } else {
      if (radio_selected == ComponentId_RADIO_EXTRA)
        fade = (stricmp(miss_name, default_miss_name) != 0);
      else
        fade = (miss_number != default_miss_number);
    }
    E(set_gadget_faded(id_block->self_id,
      ComponentId_OPTION_ONLYCHANGES, fade));
  }
  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void SaveMiss_created(ObjectId const id)
{
  SaveMiss_sharedid = id;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Window_AboutToBeShown, about_to_be_shown },
    { ActionButton_Selected, actionhandler },
    { WritableField_ValueChanged, writablehandler },
    { RadioButton_StateChanged, radiobutton_state_changed },
    { NumberRange_ValueChanged, numberhandler },
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}
