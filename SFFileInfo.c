/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  File info window
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
#include "fileinfo.h"

#include "err.h"
#include "macros.h"

#include "Session.h"
#include "missfiles.h"
#include "mapfiles.h"
#include "EditWin.h"
#include "sffileinfo.h"
#include "DataType.h"
#include "FilePaths.h"

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

  DataType data_type;
  if (Session_get_ui_type(session) == UI_TYPE_MISSION) {
    data_type = MissFiles_get_data_type(grandparent_component);
  } else {
    data_type = MapFiles_get_data_type(grandparent_component);
  }

  E(fileinfo_set_file_size(0, id_block->self_id,
    Session_get_file_size(session, data_type)));

  E(fileinfo_set_file_type(0, id_block->self_id,
    data_type_to_file_type(data_type)));

  E(fileinfo_set_file_name(0, id_block->self_id,
    Session_get_file_name(session, data_type)));

  E(fileinfo_set_modified(0, id_block->self_id,
    Session_file_modified(session, data_type)));

  E(fileinfo_set_date(0, id_block->self_id,
    Session_get_file_date(session, data_type)));

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void sffileinfo_created(ObjectId const id)
{
  /* Install handlers */
  EF(event_register_toolbox_handler(id, FileInfo_AboutToBeShown,
                                    about_to_be_shown, NULL));
}
