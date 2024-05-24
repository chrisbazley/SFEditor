/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map transfers palette
 *  Copyright (C) 2007 Christopher Bazley
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
#include <string.h>
#include <assert.h>
#include "stdlib.h"

#include "toolbox.h"
#include "wimp.h"
#include "wimplib.h"

#include "Err.h"
#include "Macros.h"
#include "MsgTrans.h"
#include "PathTail.h"
#include "Debug.h"
#include "WimpExtra.h"

#include "Vertex.h"
#include "utils.h"
#include "Session.h"
#include "Editor.h"
#include "Palette.h"
#include "TransMenu.h"
#include "TransMenu2.h"
#include "MTransfers.h"
#include "MTransfersPalette.h"
#include "MapTexData.h"
#include "DrawTiles.h"
#include "FilenamesData.h"
#include "DFileUtils.h"

#define CLIP_LABEL_WIDTH

static WimpPlotIconBlock plot_icon, plot_label;
static char truncated_name[sizeof(Filename) + sizeof("...") - 1];
/* -1 because both sizeof() values include space for a string terminator. */

/* ---------------- Private functions ---------------- */

static bool init(PaletteData *const pal_data, Editor *const editor,
  size_t *const num_indices, bool const reinit)
{
  NOT_USED(reinit);
  MapTex *const textures = Session_get_textures(Editor_get_session(editor));

  /* Create thumbnail sprites, if not done already
  (This may be the case if we are sharing with another session) */
  if (!MapTransfers_ensure_thumbnails(&textures->transfers, &textures->tiles)) {
    return false; /* failure */
  }

  if (num_indices != NULL) {
    *num_indices = MapTransfers_get_count(&textures->transfers);
  }

  TransMenu_attach(pal_data);
  return true;
}

static void final(PaletteData *const pal_data, Editor *const editor, bool const reinit)
{
  NOT_USED(editor);
  if (!reinit)
  {
    /* Detach and delete our menu */
    Palette_set_menu(pal_data, NULL_ObjectId);
  }
}

static void start_redraw(Editor *const editor, bool const labels)
{
  /* Initialisation that can be done once before the redraw process starts,
     rather than upon processing each individual redraw rectangle. */

  MapTex *const textures = Session_get_textures(Editor_get_session(editor));
  if (!textures->transfers.have_thumbnails)
    return; /* failure */

  /* Initialise Wimp icon data for the thumbnail sprites */
  plot_icon.flags = WimpIcon_Sprite | WimpIcon_Indirected |
                    WimpIcon_HCentred | WimpIcon_VCentred |
                    (WimpIcon_FGColour * WimpColour_Black) |
                    (WimpIcon_BGColour * WimpColour_White);

  plot_icon.data.is.sprite_area = SprMem_get_area_address(
    &textures->transfers.thumbnail_sprites);

  if (labels)
  {
    /* Initialise Wimp icon data for the text labels */
    plot_label.flags = WimpIcon_Text | WimpIcon_Indirected |
                       WimpIcon_HCentred | WimpIcon_VCentred |
                       (WimpIcon_FGColour * WimpColour_Black) |
                       (WimpIcon_BGColour * WimpColour_VeryLightGrey);
    plot_label.data.it.buffer = truncated_name;
    plot_label.data.it.validation = "";
    plot_label.data.it.buffer_size = sizeof(truncated_name);
  }
}

static void redraw_label(Editor *const editor, Vertex origin, BBox const *bbox,
  size_t object_no, bool const selected)
{
  NOT_USED(origin);
  MapTex *const textures = Session_get_textures(Editor_get_session(editor));
  MapTransfer *const transfer = MapTransfers_find_by_index(
                                       &textures->transfers, object_no);
  if (transfer == NULL)
    return; /* failure */

  /* Truncate the file name with a ellipsis if it exceeds the
     width of the object to which it refers */
  STRCPY_SAFE(truncated_name, get_leaf_name(MapTransfer_get_dfile(transfer)));
  int const width = truncate_string(truncated_name, bbox->xmax - bbox->xmin);

  /* Reduce the width of the label icon to fit the truncated text */
#ifdef CLIP_LABEL_WIDTH
  plot_label.bbox.xmin = bbox->xmin + (bbox->xmax - bbox->xmin) / 2 - width / 2;
  plot_label.bbox.xmax = plot_label.bbox.xmin + width;
  plot_label.bbox.ymin = bbox->ymin;
  plot_label.bbox.ymax = bbox->ymax;
#else
  plot_label.bbox = *bbox;
#endif

  /* Set the icon flags to reflect whether the object is selected */
  if (selected)
    SET_BITS(plot_label.flags, WimpIcon_Selected | WimpIcon_Filled);
  else
    CLEAR_BITS(plot_label.flags, WimpIcon_Selected | WimpIcon_Filled);

  /* Draw the label text icon */
  E(wimp_plot_icon(&plot_label));
}

static void redraw_object(Editor *const editor, Vertex const origin,
  BBox const *bbox, size_t const object_no, bool const selected)
{
  NOT_USED(origin);
  MapTex *const textures = Session_get_textures(Editor_get_session(editor));
  if (!textures->transfers.have_thumbnails)
    return; /* failure */

  MapTransfer *const transfer = MapTransfers_find_by_index(
                                       &textures->transfers, object_no);
  if (transfer == NULL)
    return; /* failure */

  /* Set the thumbnail sprite to appear in the icon */
  plot_icon.data.is.sprite = get_leaf_name(MapTransfer_get_dfile(transfer));
  plot_icon.data.is.sprite_name_length = (int)strlen(plot_icon.data.is.sprite);

  /* Cover specified bounding box with the sprite icon */
  plot_icon.bbox = *bbox;

  /* Set the icon flags to reflect whether the object is selected */
  NOT_USED(selected);

  /* Draw the sprite icon */
  E(wimp_plot_icon(&plot_icon));
}

static void end_redraw(Editor *const editor, bool const labels)
{
  /* Tidy up at the end of the redraw process. */
  NOT_USED(labels);
  MapTex *const textures = Session_get_textures(Editor_get_session(editor));
  if (!textures->transfers.have_thumbnails)
    return; /* failure */

  SprMem_put_area_address(&textures->transfers.thumbnail_sprites);
}

static void update_menus(PaletteData *const pal_data)
{
  TransMenu_update(pal_data);
  TransMenu2_update(pal_data);
}

static void reload(Editor *const editor)
{
  EditSession *const session = Editor_get_session(editor);
  FilenamesData const *const filenames = Session_get_filenames(session);
  MapTex *const textures = Session_get_textures(session);

  MapTransfers_load_all(&textures->transfers, filenames_get(filenames, DataType_MapTextures));
  Session_all_textures_changed(textures, EDITOR_CHANGE_TEX_TRANSFERS_RELOADED, NULL);
}

static void edit(Editor *const editor)
{
  EditSession *const session = Editor_get_session(editor);
  MapTex *const textures = Session_get_textures(session);
  MapTransfers_open_dir(&textures->transfers);
}

static void delete_all(Editor *const editor)
{
  EditSession *const session = Editor_get_session(editor);
  FilenamesData const *const filenames = Session_get_filenames(session);
  if (dialogue_confirm(msgs_lookup_subn("DelAllTran", 1,
                       filenames_get(filenames, DataType_MapTextures)), "DelCanBut"))
  {
    MapTex *const textures = Session_get_textures(session);
    MapTransfers_remove_and_delete_all(&textures->transfers);
    Session_all_textures_changed(textures, EDITOR_CHANGE_TEX_TRANSFER_ALL_DELETED, NULL);
  }
}

static void delete(Editor *const editor, size_t const object_no)
{
  EditSession *const session = Editor_get_session(editor);
  MapTex *const textures = Session_get_textures(session);
  MapTransfer *const delete_me = MapTransfers_find_by_index(
    &textures->transfers, object_no);
  assert(delete_me != NULL);
  if (delete_me == NULL)
    return;

  if (dialogue_confirm(msgs_lookup_subn("ConfirmDelTran", 1,
      dfile_get_name(MapTransfer_get_dfile(delete_me))), "DelCanBut"))
  {
    MapTransfers_remove_and_delete(&textures->transfers,
      delete_me, true);

    Session_all_textures_changed(textures, EDITOR_CHANGE_TEX_TRANSFER_DELETED,
      &(EditorChangeParams){.transfer_deleted.index = object_no});
  }
}

/* ---------------- Public functions ---------------- */

bool MapTransfersPalette_register(PaletteData *const palette)
{
  static const PaletteClientFuncts transfers_palette_definition =
  {
    /* Use eigen factors of thumbnail sprite because wimp_plot_icon does. */
    .object_size = {TransfersThumbWidth << DrawTilesModeXEig,
                    TransfersThumbHeight << DrawTilesModeYEig},
    .title_msg = "PalTitleSt",
    .selected_has_border = true,
    .overlay_labels = false,
    .menu_selects = true,
    .default_columns = 1,
    .initialise = init,
    .start_redraw = start_redraw,
    .redraw_object = redraw_object,
    .redraw_label = redraw_label,
    .end_redraw = end_redraw,
    .finalise = final,
    .reload = reload,
    .edit = edit,
    .delete_all = delete_all,
    .delete = delete,
    .update_menus = update_menus,
  };

  return Palette_register_client(palette, &transfers_palette_definition);
}
