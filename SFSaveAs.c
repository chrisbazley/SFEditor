/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Save dialogue box
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

#include "event.h"
#include "toolbox.h"
#include "saveas.h"

#include "err.h"
#include "macros.h"
#include "FileUtils.h"
#include "msgtrans.h"
#include "Debug.h"
#include "StrExtra.h"

#include "Session.h"
#include "missfiles.h"
#include "mapfiles.h"
#include "utils.h"
#include "EditWin.h"
#include "sfsaveas.h"
#include "DataType.h"
#include "FilePaths.h"

static DataType data_type;

/* ---------------- Private functions ---------------- */

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

  /* Which file this is depends on which menu entries we came through to open
     the dialogue box */
  ComponentId grandparent_component;

  ON_ERR_RPT_RTN_V(toolbox_get_parent(0, id_block->parent_id, NULL,
  &grandparent_component), 1);

  if (Session_get_ui_type(session) == UI_TYPE_MISSION) {
    data_type = MissFiles_get_data_type(grandparent_component);
  } else {
    data_type = MapFiles_get_data_type(grandparent_component);
  }

  E(saveas_set_file_type(0, id_block->self_id,
    data_type_to_file_type(data_type)));

  char *const savepath = Session_get_file_name_for_save(
    session, data_type);

  if (savepath != NULL) {
    E(saveas_set_file_name(0, id_block->self_id, savepath));
    free(savepath);
  }

  return 1; /* claim event */
}

static int save_to_file(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(handle);
  NOT_USED(event_code);
  SaveAsSaveToFileEvent *const sastf = (SaveAsSaveToFileEvent *)event;
  bool success = false;

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);

  EditSession *const session = EditWin_get_session(edit_win);

  /* Warn if we are about to save to a different file path and a file of
     that name already exists */
  char *canonicalised = NULL;
  if (!E(canonicalise(&canonicalised, NULL, NULL, sastf->filename))) {
    char *const old_filename = Session_get_file_name(session, data_type);

    if (old_filename && stricmp(old_filename, canonicalised) != 0 &&
        file_exists(canonicalised) &&
        stricmp(sastf->filename, "<Wimp$Scrap>") != 0) {

      if (dialogue_confirm(msgs_lookup_subn("FileOv", 1, canonicalised),
          "OvBut")) {
        success = Session_save_file(session, data_type, canonicalised);
      }
    } else {
      success = Session_save_file(session, data_type, canonicalised);
    }
    free(canonicalised);
  }
  saveas_file_save_completed(success, id_block->self_id, sastf->filename);

  return 1; /* claim event */
}

static int save_completed(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(handle);
  NOT_USED(event_code);
  SaveAsSaveCompletedEvent *const sasc = (SaveAsSaveCompletedEvent *)event;

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);

  EditSession *const session = EditWin_get_session(edit_win);

  if (TEST_BITS(sasc->hdr.flags, SaveAs_DestinationSafe))
  {
    Session_notify_saved(session, data_type, sasc->filename);
  }

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void sfsaveas_created(ObjectId const id)
{
  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { SaveAs_AboutToBeShown, about_to_be_shown },
    { SaveAs_SaveCompleted, save_completed },
    { SaveAs_SaveToFile, save_to_file },
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i) {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}
