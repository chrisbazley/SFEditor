/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map transfers palette menu
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

#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include "stdio.h"

/* RISC OS library files */
#include "event.h"
#include "toolbox.h"
#include "menu.h"

#include "msgtrans.h"
#include "err.h"
#include "Macros.h"

#include "Session.h"
#include "TransMenu.h"
#include "Palette.h"
#include "utils.h"
#include "MTransfers.h"
#include "MapTexData.h"
#include "DataType.h"
#include "DFileUtils.h"

/* Menu entries */
enum {
  TRANSMENU_TRANSFER  = 0x2,
  TRANSMENU_DELETEALL = 0x3,
  TRANSMENU_OPENDIR   = 0x4,
  TRANSMENU_RESCANDIR = 0x5,
  TRANSMENU_NAMES     = 0x6,
};

static ObjectId trans_menu_id = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

static void update_trans_menu(PaletteData *const pal_data)
{
  EditSession *const session = Palette_get_session(pal_data);
  size_t const selected = Palette_get_selection(pal_data);

  MapTransfer *transfer_to_edit = NULL;
  if (selected != NULL_DATA_INDEX)
  {
    MapTex *const textures = Session_get_textures(session);
    transfer_to_edit = MapTransfers_find_by_index(&textures->transfers, selected);
    assert(transfer_to_edit != NULL);
  }

  E(menu_set_entry_text(0, trans_menu_id, TRANSMENU_TRANSFER,
      msgs_lookup_subn("Transfer", 1, transfer_to_edit == NULL ? "" :
                       get_leaf_name(MapTransfer_get_dfile(transfer_to_edit)))));

  E(menu_set_fade(0, trans_menu_id, TRANSMENU_TRANSFER,
                  selected == NULL_DATA_INDEX));

  E(menu_set_tick(0, trans_menu_id, TRANSMENU_NAMES, Palette_get_labels_flag(pal_data)));
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

  update_trans_menu(pal_data);
  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void TransMenu_created(ObjectId const id)
{
  trans_menu_id = id;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Menu_AboutToBeShown, about_to_be_shown }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}

void TransMenu_attach(PaletteData *const pal_data)
{
  Palette_set_menu(pal_data, trans_menu_id);
}

void TransMenu_update(PaletteData *const pal_data)
{
  if (get_ancestor_handle_if_showing(trans_menu_id) == pal_data) {
    update_trans_menu(pal_data);
  }
}
