/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Main program skeleton
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

/* ANSI library files */
#include "stdlib.h"
#include <string.h>
#include <stdbool.h>
#include "stdio.h"
#include <signal.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <ctype.h>

/* RISC OS library files */
#include "swis.h"
#include "kernel.h"
#include "wimp.h"
#include "toolbox.h"
#include "event.h"
#include "wimplib.h"
#include "window.h"

/* Tony Houghton's useful stuff */
#include "err.h"
#include "msgtrans.h"
#include "hourglass.h"

/* My useful stuff */
#include "Drag.h"
#include "Saver2.h"
#include "Entity2.h"
#include "WimpExtra.h"
#include "PalEntry.h"
#include "OSVDU.h"
#include "Macros.h"
#include "ClrTrans.h"
#include "Loader3.h"
#include "Pal256.h"
#include "NoBudge.h"
#include "InputFocus.h"
#include "SprFormats.h"
#include "debug.h"
#include "Scheduler.h"
#include "MessTrans.h"
#include "strextra.h"
#include "fileutils.h"

#include "utils.h"
#include "GraphicsFiles.h"
#include "AppIcon.h"
#include "Config.h"
#include "ConfigDbox.h"
#include "ConfigBrush.h"
#include "ConfigFill.h"
#include "ConfigWand.h"
#include "DCS_dialogue.h"
#include "filescan.h"
#include "IbarMenu.h"
#include "mapsmenu.h"
#include "ORDMenu.h"
//#include "NewMap.h"
//#include "NewMission.h"
#include "NewTransfer.h"
#include "RenameTrans.h"
#include "PreQuit.h"
#include "EMHmenu.h"
//#include "Utils.h"
#include "ViewsMenu.h"
#include "filesmenus.h"
#include "missopts.h"
#include "failthresh.h"
#include "groundlaser.h"
#include "Goto.h"
#include "EditMenu.h"
#include "SFFileInfo.h"
#include "SFSaveAs.h"
#include "Revert.h"
#include "MainMenu.h"
#include "ZoomMenu.h"
#include "OrientMenu.h"
#include "BackCol.h"
#include "SelCol.h"
#include "GhostCol.h"
#include "GridCol.h"
#include "ToolMenu.h"
#include "ModeMenu.h"
#include "EffectMenu.h"
#include "ShipsMenu.h"
#include "UtilsMenu.h"
#include "StatusBar.h"
#include "SaveMiss.h"
#include "SaveMap.h"
#include "Session.h"
#include "Desktop.h"
#include "LayersMenu.h"

#include "PerfMenu.h"
#include "OurEvents.h"
#include "RenameMap.h"
#include "RenameMiss.h"
#include "RenMissMenu.h"
#include "PlotMenu.h"
#include "TransInfo.h"
#include "TransMenu.h"
#include "TransMenu2.h"
#include "MapAnims.h"
#include "ObjGfxMesh.h"
#include "Picker.h"
#include "SnakesMenu.h"
#include "TilesMenu.h"
#include "MapFiles.h"
#include "MissFiles.h"
#include "misstype.h"
#include "SFInit.h"
#include "filepaths.h"

#include "hillcol.h"
#include "map.h"
#include "obj.h"
#include "MapTex.h"
#include "ObjGfx.h"
#include "polycol.h"
#include "Session.h"

enum {
  KnownWimpVersion = 310,
  ErrNum_ToSaveDrag = 0x80b633,
  ErrNum_LockedFile = 0x131c3,
  MaxTaskNameLen    = 31,
  NULL_TIME_SLICE = 10,
  MinWimpVersion = 321, /* Earliest version of window manager to support
                           Wimp_ReportError extensions */
  GameScreenMode = 13, /* 320 x 256, 8 bits per pixel */
};

typedef struct
{
  char const *template_name;
  void      (*initialise)(ObjectId const id);
}
ObjectInitInfo;

PaletteEntry loc_palette[NumColours];
PaletteEntry const (*palette)[NumColours];
char taskname[MaxTaskNameLen + 1] = APP_NAME;
int  wimp_version, task_handle;
MessagesFD messages;
void *tb_sprite_area;

/* ---------------- Private functions ---------------- */

static void cb_released(void)
{
  DEBUGF("Clipboard released - terminating\n");
  Session_all_delete();
  exit(EXIT_SUCCESS);
}

static int generic_event_handler(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event);
  NOT_USED(handle);

  switch (event_code) {
    case EVENT_STD_HELP:
      open_file("<"APP_NAME"$Dir>.!Help");
      return 1; /* claim event */

    case EVENT_QUIT:
      if (!PreQuit_queryunsaved(0))
      {
        /* We may own the global clipboard, so offer the associated data to
           any 'holder' application before exiting. */
        EF(entity2_dispose_all(cb_released));
      }
      return 1; /* claim event */

    case EVENT_CHOICES:
      ConfigDbox_show();
      return 1; /* claim event */

    case EVENT_SAVE_CHOICES:
      Config_save();
      return 1; /* claim event */

    case EVENT_CREATE_BASEMAP:
      /* Create new session and load default map files for editing */
      Session_new_map();

      /* Hide the creation dbox if appropriate (event will have no
         component id if generated in response to key press) */
      if (id_block->self_component == NULL_ComponentId)
      {
        E(toolbox_hide_object(0, id_block->self_id));
      }
      return 1; /* claim event */

    case EVENT_CREATE_MISSION:
      /* Create new session and load default mission files for editing */
      Session_new_mission();

      /* Hide the creation dbox if appropriate (event will have no
         component id if generated in response to key press) */
      if (id_block->self_component == NULL_ComponentId)
      {
        E(toolbox_hide_object(0, id_block->self_id));
      }
      return 1; /* claim event */
  }
  return 0;
}

static void simple_exit(const _kernel_oserror *e)
{
  /* Limited amount we can do with no messages file... */
  wimp_report_error((_kernel_oserror *)e, Wimp_ReportError_Cancel, APP_NAME);
  exit(EXIT_FAILURE);
}

static int DataOpen_handler(WimpMessage *const message, void *const handle)
{
  /* User double-clicked on an object in a directory display */
  NOT_USED(handle);

  bool claim = false;
  char *filename = NULL;
  if (!E(canonicalise(&filename, NULL, NULL, message->data.data_open.path_name)))
  {
    DataType const data_type = file_type_to_data_type(
      message->data.data_open.file_type, filename);

    switch (data_type) {
    case DataType_BaseMap:
    case DataType_OverlayMap:
    case DataType_BaseObjects:
    case DataType_OverlayObjects:
    case DataType_OverlayMapAnimations:
    case DataType_BaseMapAnimations:
    case DataType_Mission:
      claim = Session_open_single_file(filename, data_type);
      break;

    default:
      break;
    }
    free(filename);
  }

  if (claim) {
    message->hdr.your_ref = message->hdr.my_ref;
    message->hdr.action_code = Wimp_MDataLoadAck;

    if (!E(wimp_send_message(Wimp_EUserMessage,
                message, message->hdr.sender, 0, NULL)))
    {
      DEBUGF("Sent DataLoadAck message (ref. %d)\n", message->hdr.my_ref);
    }
  }

  return claim;
}

static int quit_wimphandler(WimpMessage *const message, void *const handle)
{
  NOT_USED(message);
  NOT_USED(handle);

  /* We may own the global clipboard, so offer the associated data to
     any 'holder' application before exiting. */
  EF(entity2_dispose_all(cb_released));

  return 1; /* no return, but just to keep the compiler happy */
}

static int prequit_wimphandler(WimpMessage *const message, void *const handle)
{
  assert(message);
  NOT_USED(handle);

  DEBUGF("Received Wimp pre-quit message (ref. %d in reply to %d)\n",
        message->hdr.my_ref, message->hdr.your_ref);

  int flags = 0;
  if ((size_t)message->hdr.size >= offsetof(WimpMessage, data.words[1]))
  {
    flags = message->data.words[0];
  }

  if (PreQuit_queryunsaved((flags & 1) ? 0 : message->hdr.sender))
  {
    DEBUGF("Acknowledging pre-quit message to forestall death\n");
    message->hdr.your_ref = message->hdr.my_ref;
    E(wimp_send_message(Wimp_EUserMessageAcknowledge, message,
                                 message->hdr.sender, 0, NULL));
  }
  return 1; /* claim event */
}

static int error_handler(int event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  assert(event);
  NOT_USED(id_block);
  NOT_USED(handle);
  const ToolboxErrorEvent *const totee = (ToolboxErrorEvent *)event;

  /* "To save drag..." or "locked file" are not serious errors */
  if (totee->errnum == ErrNum_ToSaveDrag || totee->errnum == ErrNum_LockedFile)
    err_report(totee->errnum, totee->errmess);
  else
    err_complain(totee->errnum, totee->errmess);

  return 1; /* claim event */
}

static int compare_init_info(const void *const key, const void *const element)
{
  const ObjectInitInfo *const init_info = element;

  assert(element != NULL);
  assert(key != NULL);
  return strcmp(key, init_info->template_name);
}

static int autocreate_handler(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Catch auto-created objects and initialise handlers etc. */
  const ToolboxObjectAutoCreatedEvent * const toace =
    (ToolboxObjectAutoCreatedEvent *)event;

  /* This array must be in alphabetical order to allow binary search */
  static const ObjectInitInfo auto_created[] =
  {
    { "AppIcon", AppIcon_created },
    { "BackCol", BackCol_created },
    { "Config", ConfigDbox_created },
    { "ConfigBrush", ConfigBrush_created },
    { "ConfigFill", ConfigFill_created },
    { "ConfigWand", ConfigWand_created },
    { "DCS", DCS_created },
    { "EditMenu", EditMenu_created },
    { "EffectMenu", EffectMenu_created },
    { "FileInfo", sffileinfo_created },
    { "GhostCol", GhostCol_created },
    { "Goto", Goto_created },
    { "GridCol", GridCol_created },
    { "GroundLaser", groundlaser_created },
    { "IbarMenu", IbarMenu_created },
    { "LayersMenu", LayersMenu_created },
    { "MainMenu", MainMenu_created },
    { "MapFiles", MapFiles_created },
    { "MissFiles", MissFiles_created },
    { "ModeMenu", ModeMenu_created },
    { "NewTransfer", NewTransfer_created },
    { "ORDMenu", ORDMenu_created },
    { "OrientMenu", OrientMenu_created },
    { "PerfMenu", PerfMenu_created },
    { "Picker", Picker_created },
    { "PlotMenu", PlotMenu_created },
    { "PreQuit", PreQuit_created },
    { "RenMissMenu", RenMissMenu_created },
    { "RenameMap", RenameMap_created },
    { "RenameMiss", RenameMiss_created },
    { "RenameTrans", RenameTrans_created },
    { "Revert", revert_created },
    { "SaveAs", sfsaveas_created },
    { "SaveMap", SaveMap_created },
    { "SaveMiss", SaveMiss_created },
    { "SelCol", SelCol_created },
    { "ShipsMenu", ShipsMenu_created },
    { "SnakesMenu", SnakesMenu_created },
    { "TilesMenu", TilesMenu_created },
    { "ToolMenu", ToolMenu_created },
    { "TransInfo", TransInfo_created },
    { "TransMenu", TransMenu_created },
    { "TransMenu2", TransMenu2_created },
    { "UtilsMenu", UtilsMenu_created },
    { "ZoomMenu", ZoomMenu_created },
    { "basefxdmenu", basefxdmenu_created },
    { "basesprmenu", basesprmenu_created },
    { "coloursmenu", coloursmenu_created },
    { "easymenu", easymenu_created },
    { "failthresh", failthresh_created },
    { "gfxfiles", GraphicsFiles_created },
    { "hardmenu", hardmenu_created },
    { "hillcolmenu", hillcolmenu_created },
    { "mapsmenu", mapsmenu_created },
    { "mediummenu", mediummenu_created },
    { "missopts", missopts_created },
    { "misstype", misstype_created },
    { "planetsmenu", planetsmenu_created },
    { "polysetmenu", polysetmenu_created },
    { "skymenu", skymenu_created },
    { "tilesetmenu", tilesetmenu_created },
    { "usermenu", usermenu_created },
  };
  const ObjectInitInfo *match;

  NOT_USED(event_code);
  assert(event != NULL);
  assert(id_block != NULL);
  NOT_USED(handle);

  /* Find the relevant initialisation function from the name of the template
     used to auto-create the object */
  match = bsearch(toace->template_name,
                  auto_created,
                  ARRAY_SIZE(auto_created),
                  sizeof(auto_created[0]),
                  compare_init_info);
  if (match != NULL)
  {
    assert(strcmp(toace->template_name, match->template_name) == 0);
    DEBUGF("Calling function for object 0x%x created from template '%s'\n",
           id_block->self_id, toace->template_name);

    match->initialise(id_block->self_id);
    return 1; /* claim event */
  }
  else
  {
    DEBUGF("Don't know how to init object 0x%x created from template '%s'!\n",
           id_block->self_id, toace->template_name);
    return 0; /* event not handled */
  }
}

/* ---------------- Public functions ---------------- */

void initialise()
{
  static int wimp_messages[] = {
        Wimp_MDataOpen,
        Wimp_MDataSave, Wimp_MDataSaveAck, Wimp_MDataLoad, Wimp_MDataLoadAck,
        Wimp_MRAMFetch, Wimp_MRAMTransmit,
        Wimp_MModeChange, Wimp_MPaletteChange, Wimp_MToolsChanged,
        Wimp_MDragging, Wimp_MDragClaim,
        Wimp_MClaimEntity, Wimp_MDataRequest, Wimp_MReleaseEntity,
        Wimp_MMenusDeleted,
        Wimp_MPreQuit, Wimp_MQuit /* must be last */ };

  hourglass_on();

  /*
   * Prevent termination on SIGINT (we use the escape key ourselves)
   */
   signal(SIGINT, SIG_IGN);

  /*
   * register ourselves with the Toolbox.
   */

  static IdBlock id_block;
  int toolbox_events = 0;
  const _kernel_oserror *e = toolbox_initialise (0, KnownWimpVersion, wimp_messages,
                                                 &toolbox_events, "<"APP_NAME"Res$Dir>",
                                                 &messages, &id_block, &wimp_version,
                                                 &task_handle,
                                                 &tb_sprite_area);
  if (e != NULL)
    simple_exit(e);

  e = messagetrans_lookup(&messages,
                          "_TaskName",
                          taskname,
                          sizeof(taskname),
                          NULL,
                          0);
  if (e != NULL)
    simple_exit(e);

  e = err_initialise(taskname, wimp_version >= MinWimpVersion, &messages);
  if (e != NULL)
    simple_exit(e);

  /*
   * initialise the flex library
   */

  flex_init(taskname, NULL, 0); /* (use Wimpslot and default English messages) */
  flex_set_budge(1); /* allow budging of flex when heap extends */
  //nobudge_register(100*1024); /* normal state is nobudge */

  /*
   * initialise the event library.
   */

  EF(event_initialise (&id_block));
  EF(event_set_mask (Wimp_Poll_NullMask | Wimp_Poll_KeyPressedMask));

  /*
   * register permanent event handlers.
   */

  EF(event_register_toolbox_handler(-1, Toolbox_ObjectAutoCreated, autocreate_handler, NULL));
  EF(event_register_toolbox_handler(-1, Toolbox_Error, error_handler, NULL));
  EF(event_register_toolbox_handler(-1, -1, generic_event_handler, NULL));

  EF(event_register_message_handler(Wimp_MPreQuit, prequit_wimphandler, NULL));
  EF(event_register_message_handler(Wimp_MQuit, quit_wimphandler, NULL));
  EF(event_register_message_handler(Wimp_MDataOpen, DataOpen_handler, NULL));

  /*
   * initialise the CBLibrary components that we use.
   */

  EF(msgs_initialise(&messages));
  EF(InputFocus_initialise());
  EF(scheduler_initialise(NULL_TIME_SLICE, &messages, err_check_rep));
  EF(saver2_initialise(task_handle, &messages));
  EF(entity2_initialise(&messages, err_check_rep));
  EF(ViewsMenu_create(&messages, err_check_rep));
  EF(drag_initialise(&messages, err_check_rep));
  EF(loader3_initialise(&messages));

  /* Read the default palette for screen mode 13 */
  ColourTransContext source = {
    .type = ColourTransContextType_Screen,
    .data = {
      .screen = {
        .mode = GameScreenMode,
        .palette = ColourTrans_DefaultPalette
      }
    }
  };
  EF(colourtrans_read_palette(0, &source, loc_palette, sizeof(loc_palette), NULL));
  palette = &loc_palette;

  Config_init();
  ObjGfxMeshes_global_init();
  Desktop_init();
  filescan_init();

  hillcol_init();
  map_init();
  objects_init();
  MapTex_init();
  ObjGfx_init();
  polycol_init();
  Session_init();

  hourglass_off();
}
