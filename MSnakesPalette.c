/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map snakes palette
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
#include <assert.h>
#include <string.h>
#include "stdio.h"

#include "toolbox.h"
#include "window.h"
#include "wimp.h"
#include "wimplib.h"

#include "Err.h"
#include "Macros.h"
#include "MsgTrans.h"
#include "WimpExtra.h"

#include "Vertex.h"
#include "utils.h"
#include "Session.h"
#include "Editor.h"
#include "Palette.h"
#include "MSnakes.h"
#include "SnakesMenu.h"
#include "MSnakesPalette.h"
#include "MapMode.h"
#include "MapTex.h"
#include "FilenamesData.h"
#include "Filenames.h"
#include "MapTexData.h"
#include "DrawTiles.h"

#define CLIP_LABEL_WIDTH

static WimpPlotIconBlock plot_icon, plot_label;
static char truncated_name[sizeof(Filename) + sizeof("...") - 1];
/* -1 because both sizeof() values include space for a string terminator. */
static char spr_name[12];

/* ---------------- Private functions ---------------- */

static bool init(PaletteData *const pal_data, Editor *const editor, size_t *num_indices, bool const reinit)
{
  NOT_USED(reinit);
  MapTex *const textures = Session_get_textures(Editor_get_session(editor));
  assert(textures);

  /* Create thumbnail sprites, if not done already
  (This may be the case if we are sharing with another session) */
  if (!MapSnakes_ensure_thumbnails(&textures->snakes, &textures->tiles))
    return false; /* failure */

  if (num_indices != NULL)
  {
    *num_indices = MapSnakes_get_count(&textures->snakes);
  }

  SnakesMenu_attach(pal_data);
  return true;
}

static void start_redraw(Editor *const editor, bool const labels)
{
  /* Initialisation that can be done once before the redraw process starts,
     rather than upon processing each individual redraw rectangle. */
  MapTex *const textures = Session_get_textures(Editor_get_session(editor));
  assert(textures);

  if (!textures->snakes.have_thumbnails)
    return;

  /* Initialise Wimp icon data for the thumbnail sprites */
  plot_icon.flags = WimpIcon_Sprite | WimpIcon_Indirected |
                    WimpIcon_HCentred | WimpIcon_VCentred |
                    (WimpIcon_FGColour * WimpColour_Black) |
                    (WimpIcon_BGColour * WimpColour_White);

  plot_icon.data.is.sprite_area = SprMem_get_area_address(
    &textures->snakes.thumbnail_sprites);

  plot_icon.data.is.sprite = spr_name;
  plot_icon.data.is.sprite_name_length = sizeof(spr_name);

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

static void redraw_label(Editor *const editor, Vertex const origin,
  BBox const *const bbox, size_t const object_no, bool const selected)
{
  NOT_USED(origin);
  MapTex *const textures = Session_get_textures(Editor_get_session(editor));
  assert(textures);

  /* Truncate the file name with a ellipsis if it exceeds the
     width of the object to which it refers */
  MapSnakes_get_name(&textures->snakes, object_no, truncated_name, sizeof(truncated_name));
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
  BBox const *const bbox, size_t const object_no, bool const selected)
{
  NOT_USED(origin);
  NOT_USED(editor);

  /* Set the thumbnail sprite to appear in the icon */
  sprintf(spr_name, "%zu", object_no);

  /* Cover specified bounding box with the sprite icon */
  plot_icon.bbox = *bbox;

  /* Set the icon flags to reflect whether the object is selected */
#if 0
  if (selected)
    SET_BITS(plot_icon.flags, WimpIcon_Selected);
  else
    CLEAR_BITS(plot_icon.flags, WimpIcon_Selected);
#else
  NOT_USED(selected);
#endif

  /* Draw the sprite icon */
  E(wimp_plot_icon(&plot_icon));
}

static void end_redraw(Editor *const editor, bool const labels)
{
  /* Tidy up at the end of the redraw process. */
  NOT_USED(labels);
  MapTex *const textures = Session_get_textures(Editor_get_session(editor));
  if (!textures->snakes.have_thumbnails)
    return; /* failure */

  SprMem_put_area_address(&textures->snakes.thumbnail_sprites);
}

static void reload(Editor *const editor)
{
  EditSession *const session = Editor_get_session(editor);
  FilenamesData const *const filenames = Session_get_filenames(session);
  MapTex *const textures = Session_get_textures(session);
  assert(textures);

  MapSnakes_load(&textures->snakes,
                    filenames_get(filenames, DataType_MapTextures),
                    MapTexBitmaps_get_count(&textures->tiles));

  Session_all_textures_changed(textures, EDITOR_CHANGE_TEX_SNAKES_RELOADED, NULL);
}

static void edit(Editor *const editor)
{
  EditSession *const session = Editor_get_session(editor);
  FilenamesData const *const filenames = Session_get_filenames(session);
  MapSnakes_edit(filenames_get(filenames, DataType_MapTextures));
}

/* ---------------- Public functions ---------------- */

bool MapSnakesPalette_register(PaletteData *const palette)
{
  static const PaletteClientFuncts snakes_palette_definition =
  {
    /* Use eigen factors of thumbnail sprite because wimp_plot_icon does. */
    .object_size = {MapSnakesThumbnailWidth << DrawTilesModeXEig,
                    MapSnakesThumbnailHeight << DrawTilesModeYEig},
    .title_msg = "PalTitleSn",
    .selected_has_border = true,
    .overlay_labels = false,
    .menu_selects = false,
    .default_columns = 1,
    .initialise = init,
    .start_redraw = start_redraw,
    .redraw_object = redraw_object,
    .redraw_label = redraw_label,
    .end_redraw = end_redraw,
    .reload = reload,
    .edit = edit,
  };

  return Palette_register_client(palette, &snakes_palette_definition);
}
