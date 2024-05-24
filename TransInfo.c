/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Transfer info window
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
#include "stdio.h"
#include "stdlib.h"
#include <string.h>

#include "event.h"
#include "toolbox.h"
#include "fileinfo.h"

#include "err.h"
#include "macros.h"

#include "Palette.h"
#include "TransInfo.h"
#include "MTransfers.h"
#include "Session.h"
#include "MapTexData.h"
#include "Utils.h"
#include "DfileUtils.h"

/* Extra gadgets over normal FileInfo object */
enum {
  TRANSINFO_DIMENSIONS = 0x82ac0f,
  TRANSINFO_NUMANIMS   = 0x82ac0d,
};

/* ---------------- Private functions ---------------- */

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(handle);
  assert(id_block);
  NOT_USED(event);
  NOT_USED(event_code);

  void *pal_data;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(
    0, id_block->ancestor_id, &pal_data), 0);

  EditSession *const session = Palette_get_session(pal_data);
  size_t const selected = Palette_get_selection(pal_data);
  if (selected == NULL_DATA_INDEX)
    return 1; /* no transfer selected in palette - just claim event */

  MapTex *const textures = Session_get_textures(session);
  MapTransfer *const transfer = MapTransfers_find_by_index(
                                        &textures->transfers, selected);
  DFile *const dfile = MapTransfer_get_dfile(transfer);

  E(fileinfo_set_file_size(0, id_block->self_id,
    get_compressed_size(dfile)));

  E(fileinfo_set_file_name(0, id_block->self_id,
    dfile_get_name(dfile)));

  E(fileinfo_set_date(0, id_block->self_id,
    dfile_get_date(dfile)));

  ObjectId window;
  if (E(fileinfo_get_window_id(0, id_block->self_id, &window)))
    return 1; /* claim event */

  MapPoint const dims = MapTransfers_get_dims(transfer);
  char dim[32];
  sprintf(dim, "%" PRIMapCoord " × %" PRIMapCoord, dims.x, dims.y);
  E(displayfield_set_value(0, window, TRANSINFO_DIMENSIONS, dim));

  size_t acount = MapTransfers_get_anim_count(transfer);
  if (acount > INT_MAX) acount = INT_MAX;
  E(numberrange_set_value(0, window, TRANSINFO_NUMANIMS, (int)acount));

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void TransInfo_created(ObjectId const id)
{
  EF(event_register_toolbox_handler(id, FileInfo_AboutToBeShown,
                                    about_to_be_shown, NULL));
}
