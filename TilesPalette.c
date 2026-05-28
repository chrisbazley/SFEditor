/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map texture bitmaps palette
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
#include "stdio.h"
#include <stdbool.h>
#include <limits.h>

#include "toolbox.h"
#include "wimp.h"
#include "wimplib.h"

#include "Desktop.h"
#include "Err.h"
#include "Macros.h"
#include "Debug.h"
#include "msgtrans.h"
#include "WimpExtra.h"

#include "Map.h"
#include "Plot.h"
#include "Vertex.h"
#include "Session.h"
#include "Palette.h"
#include "TilesMenu.h"
#include "MapTexBitm.h"
#include "TilesPalette.h"
#include "Session.h"
#include "Editor.h"
#include "MapTexData.h"
#include "Smooth.h"
#include "DataType.h"
#include "Filenames.h"
#include "MapTex.h"
#include "MapTexBitm.h"
#include "MapCoord.h"

/* Makes the tile number hard to read in low resolution screen modes */
#define CROSS_HATCH 0

enum {
  NStepsLog2 = 2,
  PaletteAngle = MapAngle_North,
};

static bool got_font;
static int font_handle, font_height;
static bool blend;
static PaletteEntry last_font_colour;
static WimpPlotIconBlock plot_icon;
static char spr_name[12];

/* ---------------- Private functions ---------------- */

static bool init(PaletteData *const pal_data, Editor *const editor, int *const num_indices, bool const reinit)
{
  NOT_USED(reinit);

  if (!Session_has_data(Editor_get_session(editor), DataType_MapTextures))
  {
    return false;
  }

  MapTex *const textures = Session_get_textures(
    Editor_get_session(editor));

  if (num_indices != NULL)
  {
    bool const include_mask = Session_has_data(Editor_get_session(editor), DataType_OverlayMap);
    *num_indices = MapTexBitmaps_get_count(&textures->tiles) + (include_mask ? 1 : 0);
  }

  TilesMenu_attach(pal_data);

  return true; /* success */
}

static void start_redraw(Editor *const editor, bool const labels)
{
  /* Initialisation that can be done once before the redraw process starts,
     rather than upon processing each individual redraw rectangle. */
  MapTex *const textures = Session_get_textures(Editor_get_session(editor));

  /* Initialise Wimp icon data for the thumbnail sprites */
  plot_icon.flags = WimpIcon_Sprite | WimpIcon_Indirected |
                    WimpIcon_HCentred | WimpIcon_VCentred |
                    (WimpIcon_FGColour * WimpColour_Black) |
                    (WimpIcon_BGColour * WimpColour_VeryLightGrey);

  plot_icon.data.is.sprite_area = SprMem_get_area_address(
    &textures->tiles.sprites[PaletteAngle][0]);

  plot_icon.data.is.sprite = spr_name;
  plot_icon.data.is.sprite_name_length = sizeof(spr_name);

  if (!labels) {
    return;
  }

  /* Enable background blending if supported by the resident version of the
     font manager */
  blend = plot_can_blend_font();

  /* Get a handle with which to paint text using a monospaced ROM font
     at size 12 × 6 points, default no. of dots per inch. */
  Vertex const font_size = {6 << 1, 12 << 1};
  got_font = plot_find_font(font_size, &font_handle);
  if (!got_font) {
    return;
  }

  /* Read the smallest bounding box that covers any character in the font */
  BBox char_bbox;
  plot_get_char_bbox(font_handle, &char_bbox);
  font_height = char_bbox.ymax - char_bbox.ymin;
  DEBUG("Max height of font is %d", font_height);

  /* Force font colours to be set when plotting first label */
  last_font_colour = 1; /* neither black nor white */
}

static void redraw_label(Editor *const editor, Vertex origin, BBox const *bbox,
                         int object_no, bool const selected)
{
  NOT_USED(selected);
  int string_width;
  char string[12];
  PaletteEntry font_colour;

  if (!got_font) {
    return;
  }

  MapTex *const textures = Session_get_textures(Editor_get_session(editor));
  int const num_objects = MapTexBitmaps_get_count(&textures->tiles);
  if (object_no > num_objects - 1) {
    object_no = Map_RefMask;
    font_colour = PAL_BLACK;
  }
  /* Set text colour to black or white, whichever gives greater
      contrast with average colour of this tile */
  else {
    assert(object_no <= UCHAR_MAX);
    if (MapTexBitmaps_is_bright(&textures->tiles, map_ref_from_num((unsigned char)object_no))) {
      font_colour = PAL_BLACK;
    } else {
      font_colour = PAL_WHITE;
    }
  }

  /* We don't use wimp_set_font_colours because cannot rely on
  default Wimp palette (e.g. colour 7 may not be black) */
  if (font_colour != last_font_colour)
  {
    plot_set_font_col(font_handle, font_colour, font_colour);
    last_font_colour = font_colour;
  }

  /* Generate string and calculate width */
  sprintf(string, "%d", object_no);
  string_width = plot_get_font_width(font_handle, string);

  /* Paint number string overlayed on tile icon */
  Vertex const font_coord = {
    origin.x + bbox->xmin + ((bbox->xmax - bbox->xmin) / 2) - (string_width / 2),
    origin.y + bbox->ymin + ((bbox->ymax - bbox->ymin) / 2) - (font_height / 4)
  };
  plot_font(font_handle, string, NULL, font_coord, blend);
}

static void redraw_object(Editor *const editor, Vertex origin, BBox const *bbox,
                          int object_no, bool const selected)
{
  NOT_USED(editor);
  NOT_USED(origin);

  MapTex *const textures = Session_get_textures(Editor_get_session(editor));
  int const num_objects = MapTexBitmaps_get_count(&textures->tiles);
  if (object_no > num_objects - 1) {

    BBox scr_bbox;
    BBox_translate(bbox, origin, &scr_bbox);
    Vertex scr_min = (Vertex){scr_bbox.xmin, scr_bbox.ymin};
    Vertex scr_max = (Vertex){scr_bbox.xmax, scr_bbox.ymax};

    plot_set_col(PAL_BLACK);
#if CROSS_HATCH
    Vertex const step = Vertex_div_log2(BBox_size(bbox), NStepsLog2);
    DEBUGF("Step for cross hatches is %d,%d\n", step.x, step.y);
    Vertex top = scr_max, left = scr_min;
    Vertex right = scr_max, bot = scr_min;

    for (int hash = 0; hash <= (1 << NStepsLog2); hash++)
    {
      plot_move(left);
      plot_fg_line_ex_end(top);
      plot_fg_line_ex_both(right);
      plot_fg_line_ex_start(bot);
      plot_fg_line_ex_both(left);

      left.y += step.y;
      right.y -= step.y;
      top.x -= step.x;
      bot.x += step.x;
    }
#else
    Vertex const eig = Desktop_get_eigen_factors();
    int const width = bbox->xmax - bbox->xmin;
    for (Vertex pos = scr_min; pos.y < scr_max.y; pos.y += (2 << eig.y))
    {
      plot_move(pos);
      plot_fg_line_ex_end((Vertex){pos.x + width, pos.y});
    }
#endif
  } else {
    /* Set the tile sprite to appear in the icon */
    sprintf(spr_name, "%d", object_no);

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
}

static void end_redraw(Editor *const editor, bool const labels)
{
  /* Tidy up at the end of the redraw process. */
  MapTex *const textures = Session_get_textures(Editor_get_session(editor));
  SprMem_put_area_address(&textures->tiles.sprites[PaletteAngle][0]);

  if (labels && got_font) {
    plot_lose_font(font_handle);
  }
}

static int grid_to_index(Editor *const editor, Vertex grid_pos, int num_columns)
{
  /* Converts a grid location within the palette window's current layout
     to an object index (i.e. tile number). Returns NULL_DATA_INDEX if
     the specified grid location is empty. */

  MapTex *const textures = Session_get_textures(Editor_get_session(editor));

  DEBUGF("Finding tile no. at grid location %d,%d in %d columns\n",
    grid_pos.x, grid_pos.y, num_columns);

  int num_groups = MapTexGroups_get_count(&textures->groups);
  bool const include_mask = Session_has_data(Editor_get_session(editor), DataType_OverlayMap);
  if (include_mask) {
    ++num_groups;
  }
  int group_index = 0, member_count = 0;
  int group_start_row = 0, group_max_row = -1;

  for (group_index = 0; group_index < num_groups; ++group_index) {
    member_count = include_mask && (group_index == num_groups - 1) ? 1:
                   MapTexGroups_get_num_group_members(&textures->groups, group_index);

    /* Skip empty groups (including super-groups) */
    if (member_count == 0) {
      continue;
    }

    group_start_row = group_max_row + 1;
    int const rows_per_group = ((member_count + (num_columns - 1))) / num_columns;
    assert(rows_per_group <= INT_MAX);
    group_max_row += rows_per_group;

    DEBUG_VERBOSEF("Group %d spans rows %d to %d\n",
      group_index, group_start_row, group_max_row);

    if (group_max_row >= grid_pos.y) {
      break;
    }
  }

  if (group_index >= num_groups) {
    DEBUGF("Grid location is below the last displayed group\n");
    return NULL_DATA_INDEX;
  }

  /* Be careful of blank grid locations at tail of group */
  int const member_index = ((grid_pos.y - group_start_row) * num_columns) + grid_pos.x;
  int object_no;
  if (member_index < member_count) {
    object_no = include_mask && (group_index == num_groups - 1) ?
       MapTexBitmaps_get_count(&textures->tiles) :
       map_ref_to_num(MapTexGroups_get_group_member(&textures->groups, group_index, member_index));

    DEBUGF("Grid location is member %d of group %d: tile %d\n", member_index,
      group_index, object_no);
  } else {
    DEBUGF("Grid location is off the tail of group %d\n", group_index);
    object_no = NULL_DATA_INDEX;
  }

  return object_no;
}

static Vertex index_to_grid(Editor *const editor, int index,
  int const num_columns)
{
  /* Converts an object index (i.e. tile number) to a grid location
     within the palette window's current layout. */
  MapTex *const textures = Session_get_textures(Editor_get_session(editor));

  DEBUG("Will find location of tile %d within palette layout of %d columns",
        index, num_columns);

  assert(textures != NULL);
  int const num_objects = MapTexBitmaps_get_count(&textures->tiles);
  int const num_groups = MapTexGroups_get_count(&textures->groups);

  /* Which group is this tile a member of? */
  int const sel_group = (index == NULL_DATA_INDEX || index > num_objects - 1) ?
    num_groups :
    MapTexGroups_get_group_of_tile(&textures->groups, map_ref_from_num((unsigned char)index));
  DEBUG("Group containing tile no. %d is %d", index, sel_group);

  /* Find starting row for that group */
  Vertex grid_pos = {0, 0};

  int group_index;
  for (group_index = 0; group_index < num_groups; ++group_index) {

    int const member_count = MapTexGroups_get_num_group_members(&textures->groups, group_index);
    /* Skip empty groups (including super-groups) */
    if (member_count == 0) {
      continue;
    }

    if (group_index == sel_group) {
      break; /* have found start row of group containing selected tile */
    }

    int const num_skip = (member_count + (num_columns - 1)) / num_columns;
    DEBUG("Skipping group %d (%d members, %d rows)", group_index,
          member_count, num_skip);

    assert(num_skip <= INT_MAX);
    grid_pos.y += num_skip;
  }

  if (group_index < num_groups) {
    /* find tile's position within group */
    int member_index = 0;
    while (map_ref_to_num(MapTexGroups_get_group_member(&textures->groups, group_index, member_index)) != index) {
      ++member_index;
    }

    DEBUG("tile %d is member %d of group %d", index, member_index, sel_group);

    assert(member_index <= INT_MAX);
    grid_pos.y += (member_index / num_columns);
    grid_pos.x = (member_index % num_columns);
  }

  DEBUG("Returning grid location %d,%d", grid_pos.x, grid_pos.y);
  return grid_pos;
}

static int get_max_width(Editor *const editor)
{
  /* Width of palette may not exceed no. of members of largest tile group */
  MapTex *const textures = Session_get_textures(Editor_get_session(editor));
  int const num_groups = MapTexGroups_get_count(&textures->groups);
  int columns_limit = 0;

  for (int g = 0; g < num_groups; g++) {
    int const member_count =
      MapTexGroups_get_num_group_members(&textures->groups, g);

    if (columns_limit < member_count) {
      columns_limit = member_count;
    }
  }

  return columns_limit;
}

static int get_num_rows(Editor *const editor, int const num_columns)
{
  MapTex *const textures = Session_get_textures(Editor_get_session(editor));
  int const num_groups = MapTexGroups_get_count(&textures->groups);
  int num_rows = 0;

  for (int group_num = 0; group_num < num_groups; group_num++)
  {
    /* round up each group to the nearest whole row */
    int const member_count = MapTexGroups_get_num_group_members(
      &textures->groups, group_num);

    num_rows += (member_count + num_columns - 1) / num_columns;
    DEBUGF("%d rows after considering group %d of %d tiles\n", num_rows, group_num, member_count);
  }

  bool const include_mask = Session_has_data(Editor_get_session(editor), DataType_OverlayMap);
  if (include_mask) {
    ++num_rows;
  }

  return num_rows;
}

static void reload(Editor *const editor)
{
  EditSession *const session = Editor_get_session(editor);
  FilenamesData const *const filenames = Session_get_filenames(session);
  MapTex *const textures = Session_get_textures(session);

  MapTexGroups_load(&textures->groups, filenames_get(filenames, DataType_MapTextures),
                    MapTexBitmaps_get_count(&textures->tiles));

  Session_all_textures_changed(textures, EDITOR_CHANGE_TEX_GROUPS_RELOADED, NULL);
}

static void edit(Editor *const editor)
{
  EditSession *const session = Editor_get_session(editor);
  FilenamesData const *const filenames = Session_get_filenames(session);
  MapTexGroups_edit(filenames_get(filenames, DataType_MapTextures));
}

static int index_to_object(Editor *const editor, int const index)
{
  EditSession *const session = Editor_get_session(editor);
  int object_no;
  MapTex *const textures = Session_get_textures(session);

  if (index >= MapTexBitmaps_get_count(&textures->tiles)) {
    assert(Session_has_data(session, DataType_OverlayMap));
    object_no = Map_RefMask;
  } else {
    object_no = index;
    assert(object_no == index);
  }
  return object_no;
}

static int object_to_index(Editor *const editor, int const object_no)
{
  EditSession *const session = Editor_get_session(editor);
  int index = object_no;

  if (object_no == Map_RefMask)
  {
    assert(Session_has_data(session, DataType_OverlayMap));
    MapTex *const textures = Session_get_textures(session);
    index = MapTexBitmaps_get_count(&textures->tiles);
  }
  return index;
}

static void update_menus(PaletteData *const pal_data)
{
  TilesMenu_update(pal_data);
}

/* ---------------- Public functions ---------------- */

bool TilesPalette_register(PaletteData *const pal_data)
{
  static const PaletteClientFuncts tiles_palette_definition =
  {
    .object_size = {MapTexSize << MapTexModeXEig, MapTexSize << MapTexModeYEig},
    .title_msg = "PalTitleT",
    .selected_has_border = true,
    .overlay_labels = true,
    .menu_selects = false,
    .default_columns = 4,
    .initialise = init,
    .start_redraw = start_redraw,
    .redraw_object = redraw_object,
    .redraw_label = redraw_label,
    .end_redraw = end_redraw,
    .grid_to_index = grid_to_index,
    .index_to_grid = index_to_grid,
    .get_max_columns = get_max_width,
    .get_num_rows = get_num_rows,
    .reload = reload,
    .edit = edit,
    .index_to_object = index_to_object,
    .object_to_index = object_to_index,
    .update_menus = update_menus,
  };

  return Palette_register_client(pal_data, &tiles_palette_definition);
}
