/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground object snakes palette
 *  Copyright (C) 2021 Christopher Bazley
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
#include "OSnakes.h"
#include "SnakesMenu.h"
#include "OSnakesPalette.h"
#include "MapMode.h"
#include "ObjGfxData.h"
#include "FilenamesData.h"
#include "plot.h"
#include "Obj.h"
#include "ObjGfxMesh.h"

#define CLIP_LABEL_WIDTH

static WimpPlotIconBlock plot_label;
static char truncated_name[sizeof(Filename) + sizeof("...") - 1];
/* -1 because both sizeof() values include space for a string terminator. */
static ObjGfxMeshes *meshes;
static PolyColData const *poly_colours;
ObjGfx *graphics;
static ObjGfxMeshesView plot_ctx;

enum {
  MinDist = 65536,
  MaxDist = MinDist * 8,
  DistStep = (MaxDist - MinDist) / 16,
  VerticalAngle = -OBJGFXMESH_ANGLE_QUART * 3 / 4,
  EditWinMargin = 12,
  ObjGridSize = 1ul << 14,
  HorizontalAngle = OBJGFXMESH_ANGLE_QUART * 2,
  THUMB_TILE_SIZE = 5,
  THUMB_TILE_WIDTH = 5,
  EditWinWidth = THUMB_TILE_WIDTH * 64,
  EditWinHeight = THUMB_TILE_SIZE * 64,
};

/* ---------------- Private functions ---------------- */

typedef struct {
  SnakeContext super;
  ObjRef (*thumb_obj_refs)[THUMB_TILE_SIZE][THUMB_TILE_WIDTH];
} ObjSnakesMiniContext;

static size_t read_mini_map(MapPoint const map_pos, SnakeContext *const ctx)
{
  assert(ctx);
  ObjSnakesMiniContext const *const mctx = CONTAINER_OF(ctx, ObjSnakesMiniContext, super);

  if (map_pos.x < 0 ||
      map_pos.x >= THUMB_TILE_WIDTH ||
      map_pos.y < 0 ||
      map_pos.y >= THUMB_TILE_SIZE)
  {
    return Obj_RefNone;
  }

  return objects_ref_to_num((*mctx->thumb_obj_refs)[map_pos.y][map_pos.x]);
}

static void write_mini_map(MapPoint const map_pos, size_t const obj_ref, SnakeContext *const ctx)
{
  assert(ctx);
  ObjSnakesMiniContext const *const mctx = CONTAINER_OF(ctx, ObjSnakesMiniContext, super);
  assert(map_pos.x >= 0);
  assert(map_pos.x < THUMB_TILE_WIDTH);
  assert(map_pos.y >= 0);
  assert(map_pos.y < THUMB_TILE_SIZE);
  (*mctx->thumb_obj_refs)[map_pos.y][map_pos.x] = objects_ref_from_num(obj_ref);
}

static void plot_mini_map(ObjSnakesMiniContext *const ctx,
  ObjSnakes *const snakes_data, size_t const snake,
  MapPoint const points[], size_t const num_points)
{
  assert(snakes_data);
  assert(points);
  assert(num_points > 0);

  size_t i = 0;
  Snakes_begin_line(&ctx->super, &snakes_data->super, points[i++], snake,
    false, read_mini_map, write_mini_map);

  while (i < num_points) {
    Snakes_plot_line(&ctx->super, points[i++]);
  }
}

static void make_mini_map(ObjSnakes *const snakes_data, size_t const snake,
  ObjRef (*const thumb_obj_refs)[THUMB_TILE_SIZE][THUMB_TILE_WIDTH])
{
  for (int y = 0; y < THUMB_TILE_SIZE; ++y) {
    for (int x = 0; x < THUMB_TILE_WIDTH; ++x) {
      (*thumb_obj_refs)[y][x] = objects_ref_none();
    }
  }

  ObjSnakesMiniContext ctx = {.thumb_obj_refs = thumb_obj_refs};

  if (Snakes_has_bends(&snakes_data->super, snake)) {
    static MapPoint const s_bend[] = {
      {0,                    0},
      {0,                    THUMB_TILE_SIZE - 1},
      {THUMB_TILE_WIDTH / 2, THUMB_TILE_SIZE - 1},
      {THUMB_TILE_WIDTH / 2, 0},
      {THUMB_TILE_WIDTH - 1, 0},
      {THUMB_TILE_WIDTH - 1, THUMB_TILE_SIZE - 1}};

    plot_mini_map(&ctx, snakes_data, snake, s_bend, ARRAY_SIZE(s_bend));
  } else {
    static MapPoint const north_south_cross[] = {
      {THUMB_TILE_WIDTH / 2, THUMB_TILE_SIZE - 1},
      {THUMB_TILE_WIDTH / 2, 0}};

    plot_mini_map(&ctx, snakes_data, snake, north_south_cross, ARRAY_SIZE(north_south_cross));
  }

  if (Snakes_has_junctions(&snakes_data->super, snake)) {
    static MapPoint const east_west_cross[] = {
      {0,                    THUMB_TILE_SIZE / 2},
      {THUMB_TILE_WIDTH - 1, THUMB_TILE_SIZE / 2}};

    plot_mini_map(&ctx, snakes_data, snake, east_west_cross, ARRAY_SIZE(east_west_cross));
  }
}

static bool init(PaletteData *const pal_data, Editor *const editor, size_t *num_indices, bool const reinit)
{
  NOT_USED(reinit);
  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);

  if (num_indices != NULL)
  {
    *num_indices = ObjSnakes_get_count(&graphics->snakes);
  }

  SnakesMenu_attach(pal_data);
  return true;
}

static void start_redraw(Editor *const editor, bool const labels)
{
  /* Initialisation that can be done once before the redraw process starts,
     rather than upon processing each individual redraw rectangle. */
  EditSession *const session = Editor_get_session(editor);
  graphics = Session_get_graphics(session);
  meshes = &graphics->meshes;
  poly_colours = Session_get_poly_colours(session);

  ObjGfxMeshes_set_direction(&plot_ctx,
     (ObjGfxDirection){{HorizontalAngle}, {VerticalAngle}, {0}}, 0);

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
  NOT_USED(editor);
  NOT_USED(origin);
  assert(graphics);

  /* Truncate the file name with a ellipsis if it exceeds the
     width of the object to which it refers */
  ObjSnakes_get_name(&graphics->snakes, object_no, truncated_name, sizeof(truncated_name));
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

static void draw_snake(ObjGfxMeshes *const meshes, PolyColData const *const poly_colours,
  Vertex const plot_centre, long int const distance,
  ObjRef (*const thumb_refs)[THUMB_TILE_SIZE][THUMB_TILE_WIDTH],
  PaletteEntry const (*const pal)[NumColours], BBox *const bounding_box,
  ObjGfxMeshStyle const style)
{
  long int const xstart = -((THUMB_TILE_WIDTH / 2) * ObjGridSize);
  Vertex3D pos = {xstart, -((THUMB_TILE_SIZE / 2) * ObjGridSize), 0};

  /* Definition starts at bottom row but we must draw in back-to-front order */
  for (int y = 0; y < THUMB_TILE_SIZE; ++y) {
    /* Draw from both sides inwards */
    for (int x = 0; x < THUMB_TILE_WIDTH / 2; ++x) {
      ObjRef obj_ref = (*thumb_refs)[THUMB_TILE_SIZE - 1 - y][THUMB_TILE_WIDTH - 1 - x];
      if (!objects_ref_is_none(obj_ref) && !objects_ref_is_mask(obj_ref)) {
        pos.x = xstart + (x * ObjGridSize);
        BBox obj_bbox;

        ObjGfxMeshes_plot(meshes, &plot_ctx, poly_colours, obj_ref, plot_centre, distance,
          pos, pal, bounding_box ? &obj_bbox : NULL, style);

        if (bounding_box) {
          BBox_expand_for_area(bounding_box, &obj_bbox);
        }
      }

      obj_ref = (*thumb_refs)[THUMB_TILE_SIZE - 1 - y][x];
      if (!objects_ref_is_none(obj_ref) && !objects_ref_is_mask(obj_ref)) {
        pos.x = xstart + ((THUMB_TILE_WIDTH - 1 - x) * ObjGridSize);
        BBox obj_bbox;

        ObjGfxMeshes_plot(meshes, &plot_ctx, poly_colours, obj_ref, plot_centre, distance,
          pos, pal, bounding_box ? &obj_bbox : NULL, style);

        if (bounding_box) {
          BBox_expand_for_area(bounding_box, &obj_bbox);
        }
      }
    }

    if (THUMB_TILE_WIDTH % 2) {
      ObjRef obj_ref = (*thumb_refs)[THUMB_TILE_SIZE - 1 - y][THUMB_TILE_WIDTH / 2];
      if (!objects_ref_is_none(obj_ref) && !objects_ref_is_mask(obj_ref)) {
        pos.x = xstart + ((THUMB_TILE_WIDTH / 2) * ObjGridSize);
        BBox obj_bbox;

        ObjGfxMeshes_plot(meshes, &plot_ctx, poly_colours, obj_ref, plot_centre, distance,
          pos, pal, bounding_box ? &obj_bbox : NULL, style);

        if (bounding_box) {
          BBox_expand_for_area(bounding_box, &obj_bbox);
        }
      }
    }
    pos.y += ObjGridSize;
  }
}

static void redraw_object(Editor *const editor, Vertex const origin,
  BBox const *const bbox, size_t const object_no, bool const selected)
{
  NOT_USED(origin);
  NOT_USED(editor);

  BBox old_window;
  plot_get_window(&old_window);

  BBox plot_bbox;
  BBox_translate(bbox, origin, &plot_bbox);
  Vertex const centre = {EditWinWidth / 2, EditWinHeight / 2};
  plot_bbox.xmax--;
  plot_bbox.ymax--;

  BBox temp_window;
  BBox_intersection(&old_window, &plot_bbox, &temp_window);
  if (!BBox_is_valid(&temp_window))
  {
    return;
  }
  plot_set_window(&temp_window);

  ObjRef thumb_refs[THUMB_TILE_SIZE][THUMB_TILE_WIDTH];
  make_mini_map(&graphics->snakes, object_no, &thumb_refs);

  long int distance = ObjSnakes_get_pal_distance(&graphics->snakes, object_no);
  if (distance < 0)
  {
    for (distance = MinDist; distance < MaxDist; distance += DistStep)
    {
      BBox obj_bbox = BBox_make_invalid();
      draw_snake(meshes, NULL, centre, distance, &thumb_refs, NULL, &obj_bbox,
                 ObjGfxMeshStyle_BBox);

      assert(distance <= MaxDist);
      DEBUG("Bounding box at distance %ld: %d,%d,%d,%d", distance,
            obj_bbox.xmin, obj_bbox.ymin, obj_bbox.xmax, obj_bbox.ymax);

      static BBox const check_bbox = {
       .xmin = EditWinMargin,
       .ymin = EditWinMargin,
       .xmax = EditWinWidth - EditWinMargin,
       .ymax = EditWinHeight - EditWinMargin,
      };

      if (BBox_is_valid(&obj_bbox) && BBox_contains(&check_bbox, &obj_bbox))
      {
        break;
      }
    }

    distance = LOWEST(distance, MaxDist);
    ObjSnakes_set_pal_distance(&graphics->snakes, object_no, distance);
  }

  if (selected) {
    plot_set_bg_col(PAL_WHITE);
    plot_clear_window();
  }

  Vertex const plot_centre = Vertex_add(centre, BBox_get_min(&plot_bbox));
  plot_set_col(PAL_BLACK);
  ObjGfxMeshes_plot_grid(&plot_ctx, plot_centre, distance, (Vertex3D){0, 0, 0});
  draw_snake(meshes, poly_colours, plot_centre, distance, &thumb_refs, palette, NULL,
             ObjGfxMeshStyle_Filled);

  plot_set_window(&old_window);
}

static void end_redraw(Editor *const editor, bool const labels)
{
  /* Tidy up at the end of the redraw process. */
  NOT_USED(editor);
  NOT_USED(labels);
}

static void reload(Editor *const editor)
{
  EditSession *const session = Editor_get_session(editor);
  FilenamesData const *const filenames = Session_get_filenames(session);
  ObjGfx *const graphics = Session_get_graphics(session);

  ObjSnakes_load(&graphics->snakes,
                    filenames_get(filenames, DataType_PolygonMeshes),
                    ObjGfxMeshes_get_ground_count(&graphics->meshes));

  Session_all_graphics_changed(graphics, EDITOR_CHANGE_GFX_SNAKES_RELOADED, NULL);
}

static void edit(Editor *const editor)
{
  EditSession *const session = Editor_get_session(editor);
  FilenamesData const *const filenames = Session_get_filenames(session);
  ObjSnakes_edit(filenames_get(filenames, DataType_PolygonMeshes));
}

static void update_menus(PaletteData *const pal_data)
{
  SnakesMenu_update(pal_data);
}

/* ---------------- Public functions ---------------- */

bool ObjSnakesPalette_register(PaletteData *const palette)
{
  static const PaletteClientFuncts snakes_palette_definition =
  {
    .object_size = {EditWinWidth, EditWinHeight},
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
    .update_menus = update_menus,
  };

  return Palette_register_client(palette, &snakes_palette_definition);
}
