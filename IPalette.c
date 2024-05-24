/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Strategic target information palette
 *  Copyright (C) 2023 Christopher Bazley
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

#include "toolbox.h"
#include "wimp.h"
#include "wimplib.h"

#include "Err.h"
#include "Macros.h"
#include "MsgTrans.h"
#include "WimpExtra.h"

#include "utils.h"
#include "Session.h"
#include "Palette.h"
#include "IPalette.h"
#include "DrawInfo.h"

#define SPRITE_NAME "info"
#define CLIP_LABEL_WIDTH

static WimpPlotIconBlock plot_icon, plot_label;
static char truncated_name[sizeof(SPRITE_NAME) + sizeof("...") - 1];
/* -1 because both sizeof() values include space for a string terminator. */

/* ---------------- Private functions ---------------- */

static bool init(PaletteData *const pal_data, Editor *const editor, size_t *num_indices, bool const reinit)
{
  NOT_USED(pal_data);
  NOT_USED(editor);
  NOT_USED(reinit);

  if (num_indices != NULL)
  {
    *num_indices = 1;
  }

  return true;
}

static void start_redraw(Editor *const editor, bool const labels)
{
  /* Initialisation that can be done once before the redraw process starts,
     rather than upon processing each individual redraw rectangle. */
  NOT_USED(editor);

  /* Initialise Wimp icon data for the thumbnail sprites */
  plot_icon.flags = WimpIcon_Sprite | WimpIcon_Indirected |
                    WimpIcon_HCentred | WimpIcon_VCentred |
                    (WimpIcon_FGColour * WimpColour_Black) |
                    (WimpIcon_BGColour * WimpColour_White);

  plot_icon.data.is.sprite_area = get_sprite_area();
  plot_icon.data.is.sprite = SPRITE_NAME;
  plot_icon.data.is.sprite_name_length = sizeof(SPRITE_NAME);

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
  NOT_USED(object_no);
  NOT_USED(editor);
  NOT_USED(origin);
  assert(object_no == 0);

  /* Truncate the file name with a ellipsis if it exceeds the
     width of the object to which it refers */
  *truncated_name = '\0';
  strncat(truncated_name, msgs_lookup("PalNameI"), sizeof(truncated_name)-1);
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
  NOT_USED(object_no);
  NOT_USED(origin);
  NOT_USED(editor);
  assert(object_no == 0);

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

/* ---------------- Public functions ---------------- */

bool InfoPalette_register(PaletteData *const palette)
{
  static PaletteClientFuncts info_palette_definition =
  {
    .title_msg = "PalTitleI",
    .selected_has_border = true,
    .overlay_labels = false,
    .menu_selects = false,
    .default_columns = 1,
    .initialise = init,
    .start_redraw = start_redraw,
    .redraw_object = redraw_object,
    .redraw_label = redraw_label,
  };
  info_palette_definition.object_size = DrawInfo_get_size_os(false);

  return Palette_register_client(palette, &info_palette_definition);
}
