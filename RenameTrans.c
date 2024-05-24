/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Transfer rename dialogue box
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
#include "stdlib.h"
#include <string.h>
#include <assert.h>

/* RISC OS library files */
#include "event.h"
#include "toolbox.h"
#include "window.h"
#include "gadgets.h"

#include "err.h"
#include "Macros.h"
#include "SprFormats.h"
#include "msgtrans.h"
#include "StrExtra.h"
#include "Debug.h"

#include "Session.h"
#include "Palette.h"
#include "RenameTrans.h"
#include "MTransfers.h"
#include "utils.h"
#include "MapTexData.h"
#include "FilenamesData.h"
#include "DFileUtils.h"

/* Gadget numbers */
enum {
  RENAMETRANS_NAME = 0x0,
  RENAMETRANS_OK   = 0x1,
};

/* ---------------- Private functions ---------------- */

static int actionbutton_selected(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Click on menu item */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  void *pal_data;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(
    0, id_block->ancestor_id, &pal_data), 0);

  EditSession *const session = Palette_get_session(pal_data);
  MapTex *const textures = Session_get_textures(session);

  size_t sel_index, new_index;
  Filename new_name;
  MapTransfer *transfer_to_rename;

  switch (id_block->self_component) {
    case RENAMETRANS_OK:
      sel_index = Palette_get_selection(pal_data);
      if (sel_index == NULL_DATA_INDEX)
        return 1; /* claim event */

      transfer_to_rename = MapTransfers_find_by_index(&textures->transfers,
                              sel_index);
      assert(transfer_to_rename != NULL);
      if (transfer_to_rename == NULL)
        return 1; /* claim event */

      ON_ERR_RPT_RTN_V(writablefield_get_value(0, id_block->self_id,
                       RENAMETRANS_NAME, new_name, sizeof(new_name), NULL),
                       1 /* claim event */);

      if (!MapTransfers_rename(&textures->transfers,
             transfer_to_rename, new_name, &new_index))
        return 1; /* claim event */

      /* Update the palettes */
      Session_all_textures_changed(textures, EDITOR_CHANGE_TEX_TRANSFER_RENAMED,
        &(EditorChangeParams){.transfer_renamed.index = sel_index,
                              .transfer_renamed.new_index = new_index});

      /* Close dialogue box on completion unless ADJUST-click */
      if (!TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust))
        E(toolbox_hide_object(0, id_block->self_id));

      return 1; /* claim event */
  }
  return 0; /* pass event on */
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  void *pal_data;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(
    0, id_block->ancestor_id, &pal_data), 0);

  EditSession *const session = Palette_get_session(pal_data);
  MapTex *const textures = Session_get_textures(session);

  size_t const selected = Palette_get_selection(pal_data);
  if (selected == NULL_DATA_INDEX)
    return 1; /* nothing selected - simply claim event */

  MapTransfer *const transfer_to_rename = MapTransfers_find_by_index(
                                                &textures->transfers, selected);
  assert(transfer_to_rename != NULL);
  if (transfer_to_rename == NULL)
    return 1; /* erk - fail! */

  E(writablefield_set_value(0, id_block->self_id, RENAMETRANS_NAME,
             transfer_to_rename != NULL ? get_leaf_name(MapTransfer_get_dfile(transfer_to_rename)) : ""));

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void RenameTrans_created(ObjectId const id)
{
  /* Install handlers */
  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { ActionButton_Selected, actionbutton_selected },
    { Window_AboutToBeShown, about_to_be_shown },
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}
