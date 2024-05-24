/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Global application configuration dialogue box
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
#include <string.h>
#include <stdbool.h>
#include "stdlib.h"
#include <assert.h>

#include "toolbox.h"
#include "window.h"
#include "event.h"
#include "gadgets.h"
#include "wimp.h"
#include "wimplib.h"

#include "msgtrans.h"
#include "err.h"
#include "Macros.h"
#include "Debug.h"
#include "PathTail.h"
#include "StrExtra.h"
#include "GadgetUtil.h"
#include "StringBuff.h"
#include "SFError.h"
#include "Config.h"
#include "Utils.h"
#include "filepaths.h"

/* --------------------- Gadgets -------------------- */

enum {
  ComponentId_AutoGameFind       = 0x3,
  ComponentId_CustomGameFind     = 0x4,
  ComponentId_CustomGamePath     = 0x5,
  ComponentId_GamePathDrop1      = 0x0,
  ComponentId_GamePathDrop2      = 0x1,
  ComponentId_UseExternalDir     = 0x7,
  ComponentId_ExternalDirBox     = 0x6,
  ComponentId_UserLevelsPath     = 0xa,
  ComponentId_UserLevelsDrop1    = 0x8,
  ComponentId_UserLevelsDrop2    = 0x9,
  ComponentId_TransfersPath      = 0x19,
  ComponentId_TransfersDrop1     = 0x17,
  ComponentId_TransfersDrop2     = 0x18,
  ComponentId_Lazy               = 0xd,
  ComponentId_Cancel             = 0x17,
  ComponentId_OK                 = 0x18,
};

/* Gadgets for the two dropzones: */
static ComponentId const gamedir_dropzone[] = {ComponentId_CustomGamePath, ComponentId_GamePathDrop1, ComponentId_GamePathDrop2};
static ComponentId const levelsdir_dropzone[] = {ComponentId_UserLevelsPath, ComponentId_UserLevelsDrop1, ComponentId_UserLevelsDrop2};
static ComponentId const transfersdir_dropzone[] = {ComponentId_TransfersPath, ComponentId_TransfersDrop1, ComponentId_TransfersDrop2};

static int wimp_handle;
static ObjectId Config_tboxID = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

static void setup_win(ObjectId const window_id, int delay_caret)
{
  /* Game levels location */
  E(radiobutton_set_state(0, window_id, Config_get_use_custom_game_dir() ?
  ComponentId_CustomGameFind : ComponentId_AutoGameFind, 1));

  E(writablefield_set_value(0, window_id, ComponentId_CustomGamePath,
  Config_get_custom_game_dir()));

  for (size_t i = 0; i < ARRAY_SIZE(gamedir_dropzone); i++) {
    E(set_gadget_faded(window_id, gamedir_dropzone[i], !Config_get_use_custom_game_dir()));
  }

  /* User levels location */
  E(optionbutton_set_state(0, window_id, ComponentId_UseExternalDir,
  Config_get_use_extern_levels_dir()));

  E(writablefield_set_value(0, window_id, ComponentId_UserLevelsPath,
  Config_get_extern_levels_dir()));

  for (size_t i = 0; i < ARRAY_SIZE(levelsdir_dropzone); i++) {
    E(set_gadget_faded(window_id, levelsdir_dropzone[i], !Config_get_use_extern_levels_dir()));
  }

  /* Transfers location */
  E(writablefield_set_value(0, window_id, ComponentId_TransfersPath, Config_get_transfers_dir()));

  /* Where can we put the caret legally? */
  if (delay_caret) {
    if (Config_get_use_custom_game_dir()) {
      /*if (delay_caret)*/
        E(window_set_default_focus(0, window_id, ComponentId_CustomGamePath));
      /*else
        E(gadget_set_focus(0, window_id, ComponentId_CustomGamePath))*/
    } else {
      if (Config_get_use_extern_levels_dir()) {
        /*if (delay_caret)*/
          E(window_set_default_focus(0, window_id, ComponentId_UserLevelsPath));
        /*else
          E(gadget_set_focus(0, window_id, ComponentId_UserLevelsPath))*/
      } else {
        /*if (delay_caret)*/
          E(window_set_default_focus(0, window_id, ComponentId_TransfersPath));
        /*else
          E(gadget_set_focus(0, window_id, ComponentId_TransfersPath))*/

      }
    }
  }
  /* Other options */
  E(optionbutton_set_state(0, window_id, ComponentId_Lazy,
  Config_get_lazydirscan()));
}

static bool read_win(ObjectId const window_id)
{
  char read_custom_game_dir[MaxPathSize],
       read_extern_levels_dir[MaxPathSize],
       read_transfers_dir[MaxPathSize];

  int read_use_custom_game_dir, read_use_extern_levels_dir;

  /* Game location */
  if (E(radiobutton_get_state(0, window_id, ComponentId_CustomGameFind,
         &read_use_custom_game_dir, NULL))) {
    return false;
  }

  if (E(writablefield_get_value(0, window_id, ComponentId_CustomGamePath,
         read_custom_game_dir, sizeof(read_custom_game_dir), NULL))) {
    return false;
  }

  /* User levels location */
  if (E(optionbutton_get_state(0, window_id, ComponentId_UseExternalDir,
         &read_use_extern_levels_dir))) {
    return false;
  }

  if (E(writablefield_get_value(0, window_id, ComponentId_UserLevelsPath,
         read_extern_levels_dir, sizeof(read_extern_levels_dir), NULL))) {
    return false;
  }

  /* Transfers location */
  if (E(writablefield_get_value(0, window_id, ComponentId_TransfersPath,
         read_transfers_dir, sizeof(read_transfers_dir), NULL))) {
    return false;
  }

  /* Check that levels directory paths are still valid */
  if (read_use_extern_levels_dir && !file_exists(read_extern_levels_dir)) {
    /* External levels directory not found */
    report_error(SFERROR(ExternNotFound), read_extern_levels_dir, "");
    return false;
  }

  char const *const game_dir = read_use_custom_game_dir ?
                               read_custom_game_dir : FIXED_GAME_DIR;
  if (!file_exists(game_dir)) {
    /* Main game directory not found */
    report_error(SFERROR(GameNotFound), game_dir, "");
    return false;
  }

  if (!file_exists(read_transfers_dir)) {
    report_error(SFERROR(TransfersNotFound), read_transfers_dir, "");
    return false;
  }

  /* OK, speculative paths config is OK so use it */
  Config_set_custom_game_dir(read_custom_game_dir);
  Config_set_extern_levels_dir(read_extern_levels_dir);
  Config_set_transfers_dir(read_transfers_dir);
  Config_set_use_custom_game_dir(read_use_custom_game_dir);
  Config_set_use_extern_levels_dir(read_use_extern_levels_dir);

  /* update game_dir, SFeditorLevels$Path */
  if (!Config_setup_levels_path()) {
    return false;
  }

  /* Other options */
  int read_opt;
  if (E(optionbutton_get_state(0, window_id, ComponentId_Lazy, &read_opt))) {
    return false;
  }

  Config_set_lazydirscan(read_opt);

  return true; /* success - close window */
}

static void send_dataloadack(WimpMessage *const message)
{
  /* Acknowledge that a directory was 'loaded' successfully
     (just a courtesy message, we don't expect a reply) */
  message->hdr.your_ref = message->hdr.my_ref;
  message->hdr.action_code = Wimp_MDataLoadAck;
  E(wimp_send_message(Wimp_EUserMessage, message,
             message->hdr.sender, 0, NULL));
  DEBUG("Sent DataLoadAck message (ref. %d)", message->hdr.my_ref);
}

static int radiobutton_state_changed(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(handle);
  RadioButtonStateChangedEvent *const rbsce =
    (RadioButtonStateChangedEvent *)event;

  if (id_block->self_component != ComponentId_CustomGameFind)
    return 0; /* go on, piss off! */

  for (size_t i = 0; i < ARRAY_SIZE(gamedir_dropzone); i++)
    E(set_gadget_faded(id_block->self_id, gamedir_dropzone[i], !(rbsce->state)));

  if (rbsce->state)
    E(gadget_set_focus(0, id_block->self_id, ComponentId_CustomGamePath));

  return 1; /* claim event */
}

static int optionbutton_state_changed(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(handle);
  const OptionButtonStateChangedEvent *const obsce =
    (OptionButtonStateChangedEvent *)event;

  if (id_block->self_component != ComponentId_UseExternalDir) {
    return 0; /* go on, piss off! */
  }

  for (size_t i = 0; i < ARRAY_SIZE(levelsdir_dropzone); i++) {
    E(set_gadget_faded(id_block->self_id, levelsdir_dropzone[i], !obsce->new_state));
  }

  if (obsce->new_state) {
    E(gadget_set_focus(0, id_block->self_id, ComponentId_UserLevelsPath));
  }

  return 1; /* claim event */
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Config window about to open */
  NOT_USED(event);
  NOT_USED(event_code);
  NOT_USED(handle);
  setup_win(id_block->self_id, 1);
  return 1; /* claim event */
}

static int actionbutton_selected(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(handle);
  switch (id_block->self_component) {
    case ComponentId_Cancel:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust)) {
        /* restore settings */
        setup_win(id_block->parent_id, 0);
      }
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Select)) {
        /* Close dialogue window */
        E(toolbox_hide_object(0, id_block->parent_id));
      }
      break;

    case ComponentId_OK:
      /* read settings from window */
      if (read_win(id_block->parent_id)) {
        if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Select)) {
          E(toolbox_hide_object(0, id_block->parent_id));
        }
      }
      break;

    default:
      return 0; /* not interested in this button */
  }
  return 1; /* claim event */
}

static int dataload_message(WimpMessage *const message, void *const handle)
{
  /* Request that we load data from a file */
  NOT_USED(handle);
  DEBUG("Config received a DataLoad message (ref. %d in reply to %d)",
        message->hdr.my_ref, message->hdr.your_ref);

  /* Check that this message is intended for the Config dialogue box */
  DEBUG("Destination window is %d", message->data.data_load.destination_window);
  if (message->data.data_load.destination_window != wimp_handle)
    return 0; /* unknown destination (do not claim message) */

  ComponentId gadget_id;
  ObjectId window_id;

  ON_ERR_RPT_RTN_V(window_wimp_to_toolbox(0,
                   message->data.data_load.destination_window,
                   message->data.data_load.destination_icon, &window_id,
                   &gadget_id), 0 /* do not claim message on error */);

  if (window_id != Config_tboxID) {
    return 0; /* message not intended for the Configure dbox */
  }

  for (size_t i = 0; i < ARRAY_SIZE(gamedir_dropzone); i++) {
    if (gadget_id == gamedir_dropzone[i]) {
      if (message->data.data_load.file_type != FileType_Application ||
      stricmp(pathtail(message->data.data_load.leaf_name, 1), "!Star3000"))
        WARN_RTN_V("NeedApp", 1 /* claim message */);

      /* Set location of levels directory inside !Star3000 application */
      StringBuffer add_leaf;
      stringbuffer_init(&add_leaf);
      if (!stringbuffer_append_all(&add_leaf, message->data.data_load.leaf_name) ||
          !stringbuffer_append_all(&add_leaf, ".Landscapes")) {
        report_error(SFERROR(NoMem), "", "");
      } else {
        E(writablefield_set_value(0, Config_tboxID,
                   ComponentId_CustomGamePath, stringbuffer_get_pointer(&add_leaf)));
      }
      stringbuffer_destroy(&add_leaf);

      send_dataloadack(message);
      return 1; /* claim message */
    }
  }

  for (size_t i = 0; i < ARRAY_SIZE(levelsdir_dropzone); i++) {
    if (gadget_id == levelsdir_dropzone[i]) {
      if (message->data.data_load.file_type != FileType_Directory)
        WARN_RTN_V("NeedDir", 1 /* claim message */);

      /* Set location of external levels directory
         (cast is only required because of the crap veneer) */
      E(writablefield_set_value(0, Config_tboxID,
                 ComponentId_UserLevelsPath,
                 (char *)message->data.data_load.leaf_name));

      send_dataloadack(message);
      return 1; /* claim message */
    }
  }

  for (size_t i = 0; i < ARRAY_SIZE(transfersdir_dropzone); i++) {
    if (gadget_id == transfersdir_dropzone[i]) {
      if (message->data.data_load.file_type != FileType_Directory)
        WARN_RTN_V("NeedDir", 1 /* claim message */);

      /* Set location of map transfers directory
         (cast is only required because of the crap veneer) */
      E(writablefield_set_value(0, Config_tboxID, ComponentId_TransfersPath,
                 (char *)message->data.data_load.leaf_name));

      send_dataloadack(message);
      return 1; /* claim message */
    }
  }

  return 1; /* boring drag destination, but claim message anyway */
}

/* ---------------- Public functions ---------------- */

void ConfigDbox_created(ObjectId const window_id)
{
  /* Config window has been auto-created */
  Config_tboxID = window_id;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Window_AboutToBeShown, about_to_be_shown },
    { RadioButton_StateChanged, radiobutton_state_changed },
    { OptionButton_StateChanged, optionbutton_state_changed }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i) {
    EF(event_register_toolbox_handler(window_id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }

  {
    ObjectId buttonbar;
    EF(window_get_tool_bars(Window_InternalBottomLeftToolbar, window_id,
      &buttonbar, NULL, NULL, NULL));

    EF(event_register_toolbox_handler(buttonbar, ActionButton_Selected,
       actionbutton_selected, NULL));
  }

  /* Register a Wimp message handler to update the relevant file path when a
     directory icon is dragged to the configuration window. */
  EF(event_register_message_handler(Wimp_MDataLoad, dataload_message,
     NULL));

  /* Record the Wimp window handle of the dialogue box
     (for later use in identifying relevant DataLoad messages) */
  EF(window_get_wimp_handle(0, Config_tboxID, &wimp_handle));
}

void ConfigDbox_show(void)
{
  E(toolbox_show_object(0, Config_tboxID, Toolbox_ShowObject_Centre,
      NULL, NULL_ObjectId, NULL_ComponentId));
}
