/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground objects palette
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
#include "stdio.h"
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#include "toolbox.h"
#include "wimp.h"
#include "event.h"
#include "wimplib.h"

#include "Err.h"
#include "Macros.h"
#include "Debug.h"
#include "MsgTrans.h"
#include "Scheduler.h"
#include "WimpExtra.h"

#include "Obj.h"
#include "Filenames.h"
#include "SprMem.h"
#include "Desktop.h"
#include "utils.h"
#include "Plot.h"
#include "Vertex.h"
#include "Session.h"
#include "Palette.h"
#include "ObjGfxMesh.h"
#include "ObjsPalette.h"
#include "Editor.h"
#include "DataType.h"
#include "graphicsdata.h"
#include "ObjGfx.h"
#include "ObjGfxData.h"
#include "Obj.h"
#include "FilenamesData.h"
#include "DrawCloud.h"

enum {
  MinDist = 65536,
  MaxDist = MinDist * 8,
  DistStep = (MaxDist - MinDist) / 16,
  NumSteps = 32,
  RotateStepPeriod = 8,
  FullRotatePeriod = RotateStepPeriod * NumSteps,
  VerticalAngle = -OBJGFXMESH_ANGLE_QUART / 4,
  HorizontalAngle = OBJGFXMESH_ANGLE_QUART * 2,
  EditWinMargin = 12,
  EditWinWidth = 320,
  EditWinHeight = 256,
  PreAllocSize = 512,
  MaxNameLen = 64,
  NumScaleSteps = 8,
  ScaleWidth = 8,
  ScaleMarkWidth = ScaleWidth * 3,
  StickToCloud = 2 * ScaleMarkWidth,
  MinCloudZoom = -4,
};

static WimpPlotIconBlock plot_label;
static char const *graphics_set;
static char truncated_name[MaxNameLen + sizeof("...") - 1];
/* -1 because both sizeof() values include space for a string terminator. */
static ObjGfx *graphics;
static PolyColData const *poly_colours;
static SprMem back_buffer;
static bool have_back_buffer, found_cloud;
static size_t pcount = 0, num_objects;
//static ScaleFactors scale_factors =
//{
//  .xmul = 2,
//  .ymul = 2,
//};
//static void *transtable;

static DrawCloudContext clouds_context;
static int ScaleStep, ScaleStickHeight;
static Vertex cloud_centre, stick_bottom, plot_cloud_offset;
static ObjGfxMeshesView deselect_ctx;

/* ---------------- Private functions ---------------- */

static void free_back_buffer(void)
{
  if (have_back_buffer)
  {
    DEBUGF("Discarding back buffer\n");
    SprMem_destroy(&back_buffer);
    have_back_buffer = false;
  }
}

static inline bool alloc_back_buffer(void)
{
  if (have_back_buffer)
  {
    return true;
  }

  if (!SprMem_init(&back_buffer, 0))
  {
    return false;
  }

  Vertex const eigen_factors = Desktop_get_eigen_factors();
  int const mode = Desktop_get_screen_mode();

  if (!SprMem_create_sprite(&back_buffer, "tmp", false,
                            (Vertex){EditWinWidth >> eigen_factors.x, EditWinHeight >> eigen_factors.y},
                            mode))
  {
    SprMem_destroy(&back_buffer);
    return false;
  }

  have_back_buffer = true;
  return true;
}

static int message_handler(WimpMessage *const message, void *const handle)
{
  assert(message != NULL);
  NOT_USED(handle);

  switch(message->hdr.action_code)
  {
    case Wimp_MModeChange:
    case Wimp_MPaletteChange:
      /* Simply discard the existing back buffer (saves
         time when dealing with PaletteChange and ModeChanged broadcast in
         quick succession). */
      free_back_buffer();
      break;

    default:
      break; /* not interested in this type of message */
  }

  return 0; /* don't claim event */
}

static bool init(PaletteData *const pal_data, Editor *const editor, size_t *const num_indices, bool const reinit)
{
  NOT_USED(reinit);
  EditSession *const session = Editor_get_session(editor);

  if (!Session_has_data(session, DataType_PolygonMeshes))
  {
    return false;
  }

  if (E(event_register_message_handler(-1, message_handler, pal_data)))
  {
    return false;
  }

  if (num_indices != NULL)
  {
    bool const include_mask = Session_has_data(session, DataType_OverlayObjects);
    ObjGfx *const graphics = Session_get_graphics(session);
    *num_indices = ObjGfxMeshes_get_ground_count(&graphics->meshes) +
                   (include_mask ? 1 : 0) + Obj_CloudCount + ObjsPaletteNumHills;
  }

  pcount++;
  return true; /* success */
}


static void final(PaletteData *const pal_data, Editor *const editor, bool const reinit)
{
  NOT_USED(editor);
  NOT_USED(reinit);

  E(event_deregister_message_handler(-1, message_handler, pal_data));

  if (--pcount == 0)
  {
    free_back_buffer();
  }
}

static SchedulerTime animate(Editor *const editor, SchedulerTime time_now)
{
  SchedulerTime const elapsed_since_rot_start = time_now % FullRotatePeriod;
  Editor_set_palette_rotation(editor, (ObjGfxAngle){(OBJGFXMESH_ANGLE_QUART * 4 * elapsed_since_rot_start) / FullRotatePeriod});

  SchedulerTime const elapsed_since_rot_step = time_now % RotateStepPeriod;
  SchedulerTime const step_start_time = time_now - elapsed_since_rot_step;
  return step_start_time + RotateStepPeriod;
}

static void start_redraw(Editor *const editor, bool const labels)
{
  /* Initialisation that can be done once before the redraw process starts,
     rather than upon processing each individual redraw rectangle. */
  EditSession *const session = Editor_get_session(editor);
  graphics = Session_get_graphics(session);
  poly_colours = Session_get_poly_colours(session);
  num_objects = ObjGfxMeshes_get_ground_count(&graphics->meshes);

  /* This inverts the x dimension and swaps y with z
     (z becomes y again when converted to screen coordinates)
     Rotated x vector: -1,0,0
     Rotated y vector:  0,0,1
     Rotated z vector:  0,1,0 */
  ObjGfxMeshes_set_direction(&deselect_ctx,
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

    FilenamesData const *const filenames = Session_get_filenames(session);
    graphics_set = filenames_get(filenames, DataType_PolygonMeshes);
  }

  //Vertex const eigen_factors = Desktop_get_eigen_factors();
  //scale_factors.xdiv = 1 << eigen_factors.x;
  //scale_factors.ydiv = 1 << eigen_factors.y;

  //transtable = Desktop_get_trans_table();

  found_cloud = false;
}

static void redraw_label(Editor *const editor, Vertex origin, BBox const *bbox,
                         size_t const object_no, bool const selected)
{
  NOT_USED(editor);
  NOT_USED(origin);

  StringBuffer obj_name;
  stringbuffer_init(&obj_name);
  if (!get_objname_from_type(&obj_name, graphics_set, objects_ref_from_num((size_t)object_no)))
  {
    report_error(SFERROR(NoMem), "", "");
    return;
  }
  *truncated_name = '\0';
  strncat(truncated_name, stringbuffer_get_pointer(&obj_name), MaxNameLen);
  stringbuffer_destroy(&obj_name);

  int const width = truncate_string(truncated_name, bbox->xmax - bbox->xmin);

  /* Reduce the width of the label icon to fit the truncated text */
#ifdef CLIP_LABEL_WIDTH
  plot_label.bbox.xmin = bbox->xmin + (bbox->xmax - bbox->xmin) / 2 - width / 2;
  plot_label.bbox.xmax = plot_label.bbox.xmin + width;
  plot_label.bbox.ymin = bbox->ymin;
  plot_label.bbox.ymax = bbox->ymax;
#else
  NOT_USED(width);
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

static void redraw_object(Editor *const editor, Vertex origin, BBox const *bbox,
                          size_t const object_no, bool const selected)
{
  BBox old_window;
  plot_get_window(&old_window);

  BBox plot_bbox;
  BBox_translate(bbox, origin, &plot_bbox);
  Vertex const centre = {EditWinWidth / 2, EditWinHeight / 4};
  plot_bbox.xmax--;
  plot_bbox.ymax--;

  BBox temp_window;
  BBox_intersection(&old_window, &plot_bbox, &temp_window);
  if (!BBox_is_valid(&temp_window))
  {
    return;
  }
  plot_set_window(&temp_window);

  long int distance = MinDist;

  if (object_no > 0 && object_no < num_objects)
  {
    ObjRef const obj_ref = objects_ref_from_num((size_t)object_no);
    distance = ObjGfxMeshes_get_pal_distance(&graphics->meshes, obj_ref);
    if (distance < 0)
    {
      for (distance = MinDist; distance < MaxDist; distance += DistStep)
      {
        bool is_contained = true;

        for (int rot = OBJGFXMESH_ANGLE_QUART * 3;
             rot >= 0 && is_contained;
             rot -= OBJGFXMESH_ANGLE_QUART)
        {
          ObjGfxMeshesView tmp_ctx;
          ObjGfxMeshes_set_direction(&tmp_ctx,
            (ObjGfxDirection){{rot}, {VerticalAngle}, {0}}, 0);

          BBox obj_bbox;
          ObjGfxMeshes_plot(&graphics->meshes, &tmp_ctx, NULL, obj_ref,
                            centre, distance, (Vertex3D){0, 0, 0},
                            NULL, &obj_bbox, ObjGfxMeshStyle_BBox);

          assert(distance <= MaxDist);
          DEBUG("Bounding box at distance %ld: %d,%d,%d,%d", distance,
                obj_bbox.xmin, obj_bbox.ymin, obj_bbox.xmax, obj_bbox.ymax);

          static BBox const check_bbox = {
           .xmin = EditWinMargin,
           .ymin = EditWinMargin,
           .xmax = EditWinWidth - EditWinMargin,
           .ymax = EditWinHeight - EditWinMargin,
          };

          is_contained = BBox_is_valid(&obj_bbox) && BBox_contains(&check_bbox, &obj_bbox);
        }

        if (is_contained)
        {
          break;
        }
      }

      distance = LOWEST(distance, MaxDist);
      ObjGfxMeshes_set_pal_distance(&graphics->meshes, obj_ref, distance);
    }
  }

  if (selected)
  {
    alloc_back_buffer();
  }

  if (!selected || (have_back_buffer && SprMem_output_to_sprite(&back_buffer, "tmp")))
  {
    ObjRef const obj_ref = objects_ref_from_num((size_t)object_no);
    if (selected) {
      plot_set_bg_col(PAL_WHITE);
      plot_clear_window();
    }

    ObjGfxMeshesView select_ctx;
    Vertex plot_centre;
    if (selected) {
      ObjGfxMeshes_set_direction(&select_ctx,
        (ObjGfxDirection){Editor_get_palette_rotation(editor), {VerticalAngle}, {0}}, 0);
      plot_centre = centre;
    } else {
      plot_centre = Vertex_add(centre, BBox_get_min(&plot_bbox));
    }

    Vertex3D const pos = {0, 0, 0};

    if (objects_ref_is_cloud(obj_ref))
    {
      if (!found_cloud) {
        /* One-time initialization */
        found_cloud = true;

        int zoom = MinCloudZoom;
        Vertex const cloud_size = DrawCloud_get_size_os();
        Vertex scaled_cloud_size = Vertex_div_log2(cloud_size, zoom);
        DEBUGF("zoom %d scaled_cloud_size %d,%d\n", zoom, scaled_cloud_size.x, scaled_cloud_size.y);

        while ((scaled_cloud_size.x / 2) > (EditWinWidth / 2) - EditWinMargin - StickToCloud ||
               (scaled_cloud_size.y * 2) > EditWinHeight - (2 * EditWinMargin))
        {
          ++zoom; // bigger zoom means smaller sprite
          scaled_cloud_size = Vertex_div_log2(cloud_size, zoom);
          DEBUGF("zoom %d scaled_cloud_size %d,%d\n", zoom, scaled_cloud_size.x, scaled_cloud_size.y);
        }

        CloudColData const *const clouds = Session_get_cloud_colours(Editor_get_session(editor));
        DrawCloud_init(&clouds_context, clouds, palette, NULL, zoom, false);

        int const ScaleStickMaxHeight = EditWinHeight - (2 * EditWinMargin) - scaled_cloud_size.y;
        ScaleStep = (ScaleStickMaxHeight + (NumScaleSteps / 2)) / NumScaleSteps;
        ScaleStickHeight = NumScaleSteps * ScaleStep;
        plot_cloud_offset = Vertex_div_log2(scaled_cloud_size, 1);
        cloud_centre = (Vertex){EditWinWidth / 2, EditWinMargin + plot_cloud_offset.y};
        stick_bottom = (Vertex){cloud_centre.x - plot_cloud_offset.x - StickToCloud, cloud_centre.y};
      }

      Vertex plot_stick_bottom = stick_bottom, plot_cloud_centre = cloud_centre;

      if (!selected) {
        plot_stick_bottom = Vertex_add(stick_bottom, BBox_get_min(&plot_bbox));
        plot_cloud_centre = Vertex_add(cloud_centre, BBox_get_min(&plot_bbox));
      }

      /* Draw ruler */
      plot_set_col(PAL_BLACK);
      plot_move(plot_stick_bottom);
      plot_fg_line((Vertex){plot_stick_bottom.x, plot_stick_bottom.y + ScaleStickHeight});

      /* Draw scale on ruler */
      bool long_mark = true;
      for (int y = 0; y <= ScaleStickHeight; y += ScaleStep, long_mark = !long_mark) {
        int const mark_width = long_mark ? ScaleWidth : (ScaleWidth / 2);
        plot_move((Vertex){plot_stick_bottom.x, plot_stick_bottom.y + y});
        plot_fg_line((Vertex){plot_stick_bottom.x + mark_width, plot_stick_bottom.y + y});
      }

      /* Draw height mark on ruler */
      int const height = objects_ref_get_cloud_height(obj_ref);
      int const height_range = Obj_MaxCloudHeight - Obj_MinCloudHeight;
      int const relative_height = height - Obj_MinCloudHeight;
      plot_cloud_centre.y += ((relative_height * ScaleStickHeight) + (height_range / 2)) / height_range;

      plot_set_col(PaletteEntry_RedMask);
      plot_move((Vertex){plot_stick_bottom.x, plot_cloud_centre.y});
      plot_fg_line((Vertex){plot_stick_bottom.x + ScaleMarkWidth, plot_cloud_centre.y});

      /* We can't really predict the tint -- only show that it varies depending on cloud height */
      Vertex const plot_cloud_min = Vertex_sub(plot_cloud_centre, plot_cloud_offset);
      DrawCloud_plot(&clouds_context, plot_cloud_min, false,
        objects_ref_get_cloud_tint(obj_ref, (MapPoint){0,0}));
    }
    else
    {
      ObjGfxMeshesView *const ctx = selected ? &select_ctx : &deselect_ctx;

      plot_set_col(PAL_BLACK);
      ObjGfxMeshes_plot_grid(ctx, plot_centre, distance, pos);

      if (objects_ref_is_hill(obj_ref))
      {
        ObjGfxMeshes_plot_hill(ctx, plot_centre, distance, pos);
      }
      else if (objects_ref_is_mask(obj_ref))
      {
        ObjGfxMeshes_plot_mask(ctx, plot_centre, distance, pos);
      }
      else if (!objects_ref_is_none(obj_ref))
      {
        ObjGfxMeshes_plot(&graphics->meshes, ctx, poly_colours, obj_ref,
          plot_centre, distance, pos, palette, NULL, ObjGfxMeshStyle_Filled);
      }
    }

    if (selected) {
      SprMem_restore_output(&back_buffer);

      SprMem_plot_scaled_sprite(&back_buffer, "tmp", BBox_get_min(&plot_bbox),
        SPRITE_ACTION_OVERWRITE, NULL, NULL);//&scale_factors, transtable);
    }
  }

  plot_set_window(&old_window);
}

static size_t index_to_object(Editor *const editor, size_t const index)
{
  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);
  size_t const num_objects = ObjGfxMeshes_get_ground_count(&graphics->meshes);
  ObjRef obj_ref;

  if (index == 0) {
    obj_ref = objects_ref_none();
  } else if (index < num_objects) {
    obj_ref = objects_ref_object(index - 1);
  } else if (index < num_objects + Obj_CloudCount) {
    obj_ref = objects_ref_cloud(index - num_objects);
  } else if (index < num_objects + Obj_CloudCount + ObjsPaletteNumHills) {
    obj_ref = objects_ref_hill();
  } else {
    assert(Session_has_data(session, DataType_OverlayObjects));
    obj_ref = objects_ref_mask();
  }

  return objects_ref_to_num(obj_ref);
}

static size_t object_to_index(Editor *const editor, size_t const object_no)
{
  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);
  size_t const num_objects = ObjGfxMeshes_get_ground_count(&graphics->meshes);
  size_t index = object_no;

  ObjRef const obj_ref = objects_ref_from_num((size_t)object_no);
  if (objects_ref_is_cloud(obj_ref)) {
    size_t const cloud_type = object_no - Obj_RefMinCloud;
    index = num_objects + cloud_type;
  } else if (objects_ref_is_hill(obj_ref)) {
    index = num_objects + Obj_CloudCount;
  } else if (objects_ref_is_mask(obj_ref)) {
    assert(Session_has_data(session, DataType_OverlayObjects));
    index = num_objects + Obj_CloudCount + ObjsPaletteNumHills;
  } else {
    assert(object_no < num_objects);
  }

  return index;
}

/* ---------------- Public functions ---------------- */

bool ObjsPalette_register(PaletteData *const palette)
{
  static const PaletteClientFuncts objects_palette_definition =
  {
    .object_size = {EditWinWidth, EditWinHeight},
    .title_msg = "PalTitleO",
    .selected_has_border = true,
    .overlay_labels = false,
    .menu_selects = false,
    .default_columns = 1,
    .initialise = init,
    .start_redraw = start_redraw,
    .redraw_object = redraw_object,
    .redraw_label = redraw_label,
    .finalise = final,
    .index_to_object = index_to_object,
    .object_to_index = object_to_index,
    .animate = animate,
  };

  return Palette_register_client(palette, &objects_palette_definition);
}
