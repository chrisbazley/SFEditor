/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Objects/triggers editing mode
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
#include <limits.h>
#include <stdbool.h>
#include <math.h>
#include "stdlib.h"
#include "stdio.h"
#include <ctype.h>

#include "toolbox.h"
#include "wimp.h"
#include "wimplib.h"

#include "OurEvents.h"
#include "msgtrans.h"
#include "Macros.h"
#include "err.h"
#include "hourglass.h"
#include "Pal256.h"
#include "StrExtra.h"
#include "PalEntry.h"

#include "View.h"
#include "Session.h"
#include "ObjectsMode.h"
#include "StatusBar.h"
#include "EditWin.h"
#include "debug.h"
#include "plot.h"
#include "ObjGfxMesh.h"
#include "utils.h"
#include "Palette.h"
#include "ObjectsEdit.h"
#include "MapCoord.h"
#include "EditMode.h"
#include "DataType.h"
#include "EditorData.h"
#include "ObjsPalette.h"
#include "Desktop.h"
#include "ObjEditCtx.h"
#include "Filenames.h"
#include "ObjEditSel.h"
#include "Vertex.h"
#include "Obj.h"
#include "ObjGfxData.h"
#include "OSnakes.h"
#include "OSnakesPalette.h"
#include "DataType.h"
#include "MapTexBitm.h"
#include "Triggers.h"
#include "OTransfers.h"
#include "DFileUtils.h"
#include "OPropDbox.h"
#include "IntDict.h"
#include "graphicsdata.h"
#include "Shapes.h"
#include "DrawObjs.h"
#include "ObjLayout.h"
#include "ObjEditChg.h"

typedef enum {
  OBJPALETTE_TYPE_NONE = -1,
  OBJPALETTE_TYPE_SNAKES,
  OBJPALETTE_TYPE_OBJS
} ObjModePaletteType;

typedef struct
{
  ObjEditSelection selection, occluded, tmp;
  ObjModePaletteType palette_type;
  bool uk_drop_pending:1, lock_selection:1;
  ObjRef ghost_obj_ref;
  ObjTransfer *pending_transfer, *pending_paste, *pending_drop, *dragged;
  ObjEditChanges change_info; /* for accumulation */
  PendingShape pending_shape;
  MapPoint fine_pos, drag_start_pos, pending_vert[3];
  MapArea drop_bbox, ghost_bbox;
  ObjSnakesContext snake_ctx;
  ObjPropDboxes prop_dboxes;
}
ObjectsModeData;

enum {
  GRID_GAP_SIZE = MapTexSize << (TexelToOSCoordLog2 + Map_SizeLog2 - Obj_SizeLog2),
  MaxDrawObjZoom = 3,
};

/* ---------------- Private functions ---------------- */

static ObjRef read_ref_if_overlap(ObjGfxMeshes *const meshes, View const *const view,
  ObjEditContext const *const objects,
  MapPoint const grid_pos,
  MapArea const *const map_area)
{
  /* If there is an object at the specified grid location, and some part of it
     overlaps the specified (inclusive) rectangle then return the object type */
  ObjRef const obj_ref = ObjectsEdit_read_ref(objects, grid_pos);

  bool triggers = false;
  if (objects && objects->triggers && triggers_check_locn(objects->triggers, grid_pos)) {
    triggers = true;
  }
  return DrawObjs_touch_ghost_bbox(meshes, view, triggers, obj_ref, grid_pos, map_area) ?
         obj_ref : objects_ref_none();
}

static ObjRef read_overlay_if_overlap(ObjGfxMeshes *const meshes, View const *const view,
  ObjEditContext const *const objects,
  MapPoint const grid_pos,
  MapArea const *const map_area)
{
  /* If there is an object at the specified grid location, and some part of it
     overlaps the specified (inclusive) rectangle then return the object type */
  ObjRef const obj_ref = ObjectsEdit_read_overlay(objects, grid_pos);

  bool triggers = false;
  if (objects && objects->triggers && triggers_check_locn(objects->triggers, grid_pos)) {
    triggers = true;
  }
  return DrawObjs_touch_ghost_bbox(meshes, view, triggers, obj_ref, grid_pos, map_area) ?
         obj_ref : objects_ref_none();
}

typedef struct {
  EditWin const *edit_win;
  MapArea const *const redraw_area;
  ObjRef obj_ref;
  Vertex min_os;
  MapArea const *bbox;
  MapArea const *overlapping_area;
  ObjEditContext const *objects;
  ObjGfxMeshes *meshes;
  PolyColData const *poly_colours;
  HillColData const *hill_colours;
  struct CloudColData const *clouds;
  View const *view;
} DrawShapeShadow;

static ObjRef filter_ghost_obj(ObjEditContext const *objects, MapPoint const map_pos, ObjRef obj_ref,
  ObjGfxMeshes *const meshes, ObjEditSelection *const occluded)
{
  if (!ObjectsEdit_can_place(objects, map_pos, obj_ref, meshes, occluded)) {
    return objects_ref_mask();
  }

  /* Placing the mask value could reveal a base map object */
  if (objects_ref_is_mask(obj_ref)) {
    obj_ref = ObjectsEdit_read_base(objects, map_pos);
  }

  return obj_ref;
}

static ObjRef read_ghost_obj(void *const cb_arg, MapPoint const map_pos)
{
  const DrawShapeShadow *const args = cb_arg;
  if (!objects_bbox_contains(args->bbox, map_pos)) {
    return objects_ref_mask();
  }
  ObjRef const obj_ref = filter_ghost_obj(args->objects, map_pos, args->obj_ref, args->meshes, NULL);
  return DrawObjs_touch_ghost_bbox(args->meshes, args->view, false, obj_ref, map_pos, args->redraw_area) ?
         obj_ref : objects_ref_mask();
}

static HillType read_ghost_hill(void *const cb_arg, MapPoint const map_pos,
  unsigned char (*const colours)[Hill_MaxPolygons],
  unsigned char (*const heights)[HillCorner_Count])
{
  return HillType_None; // FIXME
}

static void draw_area_as_ghost(MapArea const *const bbox, void *const cb_arg)
{
  assert(MapArea_is_valid(bbox));
  assert(cb_arg);
  DrawShapeShadow *const args = cb_arg;

  DEBUGF("Drawing ghost of objects to place\n");

  args->bbox = bbox;

  if (objects_overlap(args->overlapping_area, bbox)) {
    MapArea const scr_area = ObjLayout_rotate_map_area_to_scr(args->view->config.angle, args->overlapping_area);
    DrawObjs_to_screen(args->poly_colours, args->hill_colours, args->clouds, args->meshes,
                       args->view, &scr_area, read_ghost_obj, read_ghost_hill, args,
                       NULL, NULL, args->min_os, true, NULL);
  }
}

typedef struct {
  HillsData const *hills;
  ObjEditContext const *objects;
  ObjGfxMeshes *meshes;
  View const *const view;
  MapArea const *redraw_area;
} ObjReadArgs;

static ObjRef redraw_read_grid(void *const cb_arg, MapPoint const map_pos)
{
  const ObjReadArgs *const args = cb_arg;
  return read_ref_if_overlap(args->meshes, args->view, args->objects, map_pos, args->redraw_area);
}

static ObjRef redraw_read_overlay(void *const cb_arg, MapPoint const map_pos)
{
  const ObjReadArgs *const args = cb_arg;
  return read_overlay_if_overlap(args->meshes, args->view, args->objects, map_pos, args->redraw_area);
}

typedef struct {
  ObjTransfer *transfer;
  ObjGfxMeshes *meshes;
  View const *view;
  MapArea const *const redraw_area;
  MapArea transfer_area;
} TransferReadArgs;

static ObjRef read_transfer(void *const cb_arg, MapPoint map_pos)
{
  const TransferReadArgs *const args = cb_arg;

  if (!objects_bbox_contains(&args->transfer_area, map_pos)) {
    return objects_ref_mask();
  }

  map_pos = objects_wrap_coords(map_pos);
  MapPoint const min = objects_wrap_coords(args->transfer_area.min);
  DEBUGF("Min abs. coords %" PRIMapCoord ",%" PRIMapCoord "\n", min.x, min.y);

  if (min.x > map_pos.x) {
    map_pos.x += Obj_Size;
  }

  if (min.y > map_pos.y) {
    map_pos.y += Obj_Size;
  }

  DEBUGF("Abs. read coords %" PRIMapCoord ",%" PRIMapCoord "\n", map_pos.x, map_pos.y);

  ObjRef const obj_ref = ObjTransfers_read_ref(args->transfer,
                           MapPoint_sub(map_pos, min));

  return DrawObjs_touch_ghost_bbox(args->meshes, args->view, false, obj_ref, map_pos, args->redraw_area) ?
         obj_ref : objects_ref_mask();
}

static void draw_unknown_drop(EditWin const *const edit_win, MapArea const *const drop_bbox,
  Vertex const scr_orig, MapArea const *const overlapping_area)
{
  MapArea intersect;
  MapArea_intersection(drop_bbox, overlapping_area, &intersect);
  if (MapArea_is_valid(&intersect)) {
    View const *const view = EditWin_get_view(edit_win);
    MapArea const scr_area = ObjLayout_rotate_map_area_to_scr(view->config.angle, &intersect);
    DrawObjs_unknown_to_screen(view, &scr_area, scr_orig);
  }
}

static void draw_ghost_paste(ObjTransfer *const transfer,
  MapPoint const bl, EditWin const *const edit_win, Vertex const scr_orig,
  MapArea const *const redraw_area, MapArea const *const overlapping_area)
{
  DEBUGF("Drawing ghost of transfer %p at %" PRIMapCoord ",%" PRIMapCoord "\n",
         (void *)transfer, bl.x, bl.y);

  EditSession *const session = EditWin_get_session(edit_win);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;
  View const *const view = EditWin_get_view(edit_win);

  MapPoint const transfer_dims = ObjTransfers_get_dims(transfer);
  TransferReadArgs transfer_args = {
    .meshes = meshes,
    .view = view,
    .transfer = transfer, .redraw_area = redraw_area,
    .transfer_area = {
      .min = bl,
      .max = MapPoint_add(bl, MapPoint_sub(transfer_dims, (MapPoint){1,1}))
    },
  };

  PolyColData const *const poly_colours = Session_get_poly_colours(session);
  HillColData const *const hill_colours = Session_get_hill_colours(session);
  struct CloudColData const *const clouds = Session_get_cloud_colours(session);
  MapArea const scr_area = ObjLayout_rotate_map_area_to_scr(view->config.angle, overlapping_area);

  DrawObjs_to_screen(poly_colours, hill_colours, clouds, meshes, view,
                     &scr_area, read_transfer, read_ghost_hill,
                     &transfer_args,
                     NULL, NULL, scr_orig, true, NULL);
}

static void draw_pending(ObjectsModeData const *const mode_data, ObjEditContext const *const objects,
  EditWin const *const edit_win, Vertex const scr_orig,
  MapArea const *const redraw_area, MapArea const *overlapping_area)
{
  DEBUGF("Drawing pending shape type %d\n", mode_data->pending_shape);
  if (mode_data->pending_shape == Pending_Transfer) {
    draw_ghost_paste(mode_data->pending_transfer, mode_data->pending_vert[0], edit_win,
                     scr_orig, redraw_area, overlapping_area);
  } else {
    EditSession *const session = EditWin_get_session(edit_win);
    ObjGfx *const graphics = Session_get_graphics(session);

    DrawShapeShadow data = {
      .objects = objects,
      .min_os = scr_orig,
      .redraw_area = redraw_area,
      .edit_win = edit_win,
      .obj_ref = mode_data->ghost_obj_ref,
      .overlapping_area = overlapping_area,
      .meshes = &graphics->meshes,
      .poly_colours = Session_get_poly_colours(session),
      .hill_colours = Session_get_hill_colours(session),
      .clouds = Session_get_cloud_colours(session),
      .view = EditWin_get_view(edit_win),
    };

    switch (mode_data->pending_shape) {
      case Pending_Point:
        draw_area_as_ghost(&(MapArea){mode_data->pending_vert[0], mode_data->pending_vert[0]}, &data);
        break;

      case Pending_Line:
        Shapes_line(draw_area_as_ghost, &data, mode_data->pending_vert[0],
          mode_data->pending_vert[1], 0);
        break;

      case Pending_Triangle:
        Shapes_tri(draw_area_as_ghost, &data, mode_data->pending_vert[0],
          mode_data->pending_vert[1], mode_data->pending_vert[2]);
        break;

      case Pending_Rectangle:
        Shapes_rect(draw_area_as_ghost, &data, mode_data->pending_vert[0],
          mode_data->pending_vert[1]);
        break;

      case Pending_Circle:
        Shapes_circ(draw_area_as_ghost, &data, mode_data->pending_vert[0],
          MapPoint_dist(mode_data->pending_vert[0], mode_data->pending_vert[1]));
        break;

      default:
        return; /* unknown plot type */
    }
  }
}

static HillType read_hill(void *const cb_arg, MapPoint const map_pos,
  unsigned char (*const colours)[Hill_MaxPolygons],
  unsigned char (*const heights)[HillCorner_Count])
{
  const ObjReadArgs *const args = cb_arg;
  return args->hills ? hills_read(args->hills, map_pos, colours, heights) : HillType_None;
}

void ObjectsMode_draw(Editor *const editor,
  Vertex const scr_orig, MapArea const *const redraw_area,
  EditWin const *const edit_win)
{
  int const zoom = EditWin_get_zoom(edit_win);

  /* Process redraw rectangle */
  DEBUG("Request to redraw objects for area %" PRIMapCoord " <= x <= %" PRIMapCoord ", %" PRIMapCoord " <= y <= %" PRIMapCoord,
        redraw_area->min.x, redraw_area->max.x, redraw_area->min.y, redraw_area->max.y);
  assert(redraw_area->max.x >= redraw_area->min.x);
  assert(redraw_area->max.y >= redraw_area->min.y);

  EditSession *const session = Editor_get_session(editor);

  if (!Session_has_data(session, DataType_BaseObjects) &&
      !Session_has_data(session, DataType_OverlayObjects))
  {
    DEBUGF("Nothing to plot\n");
    return;
  }

  if (zoom > MaxDrawObjZoom)
  {
    DEBUGF("Zoomed too far out to draw objects sensibly\n");
    return;
  }

  MapArea overlapping_area;
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;
  View const *const view = EditWin_get_view(edit_win);
  DrawObjs_get_overlapping_draw_area(meshes, view, redraw_area, &overlapping_area);

  ObjEditContext const *const read_obj_ctx = EditWin_get_read_obj_ctx(edit_win);

  ObjectsModeData *const mode_data =
      Editor_get_edit_mode(editor) == EDITING_MODE_OBJECTS ?
      editor->editingmode_data : NULL;

  ObjEditSelection const *const selection = mode_data ? &mode_data->selection : NULL;
  ObjEditSelection const *const occluded = mode_data && (mode_data->pending_drop ||
                                                         mode_data->pending_shape != Pending_None) ?
                                           &mode_data->occluded : NULL;

  ObjReadArgs read_args = {EditWin_get_hills(edit_win), read_obj_ctx, meshes, view, redraw_area};

  PolyColData const *const poly_colours = Session_get_poly_colours(session);
  HillColData const *const hill_colours = Session_get_hill_colours(session);
  struct CloudColData const *const clouds = Session_get_cloud_colours(session);
  MapArea const scr_area = ObjLayout_rotate_map_area_to_scr(view->config.angle, &overlapping_area);

  DrawObjs_to_screen(poly_colours, hill_colours, clouds, meshes, EditWin_get_view(edit_win), &scr_area,
                     read_obj_ctx->base ? redraw_read_grid : redraw_read_overlay,
                     read_hill, &read_args,
                     read_obj_ctx->triggers,
                     selection, scr_orig, false, occluded);

  if (mode_data && mode_data->pending_shape != Pending_None) {
    plot_set_col(EditWin_get_ghost_colour(edit_win));
    draw_pending(mode_data, read_obj_ctx, edit_win, scr_orig, redraw_area, &overlapping_area);
  }

  if (mode_data && mode_data->pending_drop) {
    draw_ghost_paste(mode_data->pending_drop, mode_data->drop_bbox.min, edit_win,
                     scr_orig, redraw_area, &overlapping_area);
  }

  if (mode_data && mode_data->uk_drop_pending) {
    plot_set_col(EditWin_get_ghost_colour(edit_win));

    draw_unknown_drop(edit_win, &mode_data->drop_bbox, scr_orig, &overlapping_area);
  }
}

typedef struct {
  ObjGfxMeshes *meshes;
  ObjRef obj_ref;
  ObjEditContext const *objects;
  ObjEditSelection *occluded;
  Editor *editor;
  bool any;
} GetShapeBBox;

static void expand_ghost_bbox(MapArea const *const bbox, void *const cb_arg)
{
  assert(MapArea_is_valid(bbox));
  assert(cb_arg);
  GetShapeBBox *const args = cb_arg;

  DEBUGF("Updating ghost bbox of objects to place\n");

  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, bbox);
      !MapAreaIter_done(&iter);
      p = MapAreaIter_get_next(&iter))
  {
    ObjRef const obj_ref = filter_ghost_obj(args->objects, p, args->obj_ref, args->meshes, args->occluded);
    if (objects_ref_is_mask(obj_ref)) {
      continue;
    }

    Editor_add_ghost_obj(args->editor, p, obj_ref);
    args->any = true;
  }
}

static void occluded_changed(MapPoint const pos, void *const arg)
{
  Editor *const editor = arg;
  EditSession *const session = Editor_get_session(editor);
  ObjEditContext const *const objects = Session_get_objects(session);

  ObjRef const obj_ref = ObjectsEdit_read_ref(objects, pos);
  Editor_occluded_obj_changed(editor, pos, obj_ref);
}

static void ObjectsMode_wipe_ghost(Editor *const editor)
{
  assert(editor);
  ObjectsModeData *const mode_data = editor->editingmode_data;

  if (mode_data->pending_shape == Pending_None) {
    return;
  }

  DEBUGF("Wiping ghost object(s)\n");

  ObjEditSelection_for_each(&mode_data->occluded, occluded_changed, editor);
  ObjEditSelection_clear(&mode_data->occluded);

  Editor_redraw_ghost(editor); // undraw
  Editor_clear_ghost_bbox(editor);

  mode_data->pending_shape = Pending_None;
  mode_data->pending_transfer = NULL;
}

static void ObjectsMode_add_ghost_bbox_for_transfer(Editor *const editor, MapPoint const bl,
                                                ObjTransfer *const transfer)
{
  DEBUGF("Ghost of transfer %p at grid coordinates %" PRIMapCoord ",%" PRIMapCoord "\n",
         (void *)transfer, bl.x, bl.y);

  MapPoint const t_dims = ObjTransfers_get_dims(transfer);

  for (MapPoint trans_pos = {.y = 0}; trans_pos.y < t_dims.y; trans_pos.y++) {
    for (trans_pos.x = 0; trans_pos.x < t_dims.x; trans_pos.x++) {
      ObjRef const obj_ref = ObjTransfers_read_ref(transfer, trans_pos);
      if (!objects_ref_is_mask(obj_ref)) {
        Editor_add_ghost_obj(editor, MapPoint_add(bl, trans_pos), obj_ref);
      }
    }
  }
}

static void ObjectsMode_set_pending(Editor *const editor, PendingShape const pending_shape,
  ObjRef const obj_ref, ObjTransfer *const pending_transfer, MapPoint const pos, ...)
{
  assert(editor);
  ObjectsModeData *const mode_data = editor->editingmode_data;
  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;
  ObjEditContext const *const objects = Session_get_objects(session);

  if (mode_data->pending_shape != Pending_None) {
    Editor_redraw_ghost(editor); // undraw
  }

  Editor_clear_ghost_bbox(editor);

  ObjEditSelection_copy(&mode_data->tmp, &mode_data->occluded);
  ObjEditSelection_clear(&mode_data->occluded);

  GetShapeBBox data = {
    .objects = objects,
    .meshes = meshes,
    .obj_ref = obj_ref,
    .occluded = &mode_data->occluded,
    .editor = editor,
    .any = false,
  };

  va_list args;
  va_start(args, pos);

  switch (pending_shape) {
    case Pending_Point:
      expand_ghost_bbox(&(MapArea){pos, pos}, &data);
      break;

    case Pending_Line:
    mode_data->pending_vert[1] = va_arg(args, MapPoint);
      Shapes_line(expand_ghost_bbox, &data, pos,
        mode_data->pending_vert[1], 0);
      break;

    case Pending_Triangle:
     mode_data->pending_vert[1] = va_arg(args, MapPoint);
     mode_data->pending_vert[2] = va_arg(args, MapPoint);
      Shapes_tri(expand_ghost_bbox, &data, pos,
        mode_data->pending_vert[1], mode_data->pending_vert[2]);
      break;

    case Pending_Rectangle:
      mode_data->pending_vert[1] = va_arg(args, MapPoint);
      Shapes_rect(expand_ghost_bbox, &data, pos,
        mode_data->pending_vert[1]);
      break;

    case Pending_Circle:
      mode_data->pending_vert[1] = va_arg(args, MapPoint);
      Shapes_circ(expand_ghost_bbox, &data, pos,
        MapPoint_dist(pos, mode_data->pending_vert[1]));
      break;

    case Pending_Transfer:
      if (ObjTransfers_can_plot_to_map(objects, pos,
                                          pending_transfer, meshes,
                                          &mode_data->occluded)) {
        ObjectsMode_add_ghost_bbox_for_transfer(editor,
          pos, pending_transfer);
        data.any = true;
      }
      break;

    default:
      break; /* unknown plot type */
  }

  va_end(args);

  if (!data.any) {
    mode_data->pending_shape = Pending_None;
    mode_data->pending_transfer = NULL;
    ObjEditSelection_for_each(&mode_data->tmp /* previously occluded */, occluded_changed, editor);
  } else {
    mode_data->pending_shape = pending_shape;
    mode_data->ghost_obj_ref = obj_ref;
    mode_data->pending_transfer = pending_transfer;
    mode_data->pending_vert[0] = pos;
    ObjEditSelection_for_each_changed(&mode_data->occluded, &mode_data->tmp, // previously occluded
       NULL, occluded_changed, editor);
  }

  Editor_redraw_ghost(editor); // draw
}

static bool ObjectsMode_can_select_tool(Editor const *const editor, EditorTool const tool)
{
  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);
  bool can_select_tool = false;

  switch (tool)
  {
  case EDITORTOOL_BRUSH:
  case EDITORTOOL_SELECT:
  case EDITORTOOL_MAGNIFIER:
  case EDITORTOOL_SAMPLER:
  case EDITORTOOL_PLOTSHAPES:
  case EDITORTOOL_FILLREPLACE:
    can_select_tool = true;
    break;

  case EDITORTOOL_SNAKE:
    can_select_tool = (ObjSnakes_get_count(&graphics->snakes) > 0);
    break;
  default:
    break;
  }

  return can_select_tool;
}

static inline ObjectsModeData *get_mode_data(Editor const *const editor)
{
  assert(Editor_get_edit_mode(editor) == EDITING_MODE_OBJECTS);
  assert(editor->editingmode_data);
  return editor->editingmode_data;
}

static bool ObjectsMode_has_selection(Editor const *const editor)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);

  return !ObjEditSelection_is_none(&mode_data->selection);
}

static bool ObjectsMode_can_clip_overlay(Editor const *editor)
{
  EditSession *const session = Editor_get_session(editor);

  /* need both base and overlay maps to clip latter */
  return Session_has_data(session, DataType_OverlayObjects) &&
         Session_has_data(session, DataType_BaseObjects);
}

static bool ObjectsMode_can_edit_properties(Editor const *const editor)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  return ObjEditSelection_size(&mode_data->selection) == 1;
}

static void ObjectsMode_edit_properties(Editor *const editor, EditWin *const edit_win)
{
  assert(ObjectsMode_can_edit_properties(editor));
  ObjectsModeData *const mode_data = get_mode_data(editor);
  ObjEditSelIter iter;
  MapPoint const pos = ObjEditSelIter_get_first(&iter, &mode_data->selection);
  assert(!ObjEditSelIter_done(&iter));
  ObjPropDboxes_open(&mode_data->prop_dboxes, pos, edit_win);
}

static bool ObjectsMode_trigger_is_selected(Editor const *const editor)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  ObjEditContext const *const objects = Session_get_objects(session);

  if (!objects->triggers) {
    return false;
  }

  DEBUGF("Searching triggers for the first selected\n");
  MapArea sel_area;
  if (!ObjEditSelection_get_bounds(&mode_data->selection, &sel_area)) {
    return false;
  }

  TriggersIter iter;
  for (MapPoint p = TriggersIter_get_first(&iter, objects->triggers, &sel_area, NULL);
       !TriggersIter_done(&iter);
       p = TriggersIter_get_next(&iter, NULL))
  {
    DEBUGF("Trigger at %" PRIMapCoord ",%" PRIMapCoord "\n", p.x, p.y);
    if (ObjEditSelection_is_selected(&mode_data->selection, p)) {
      return true;
    }
  }
  return false;
}

static void ObjectsMode_update_title(Editor *const editor)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  ObjPropDboxes_update_title(&mode_data->prop_dboxes);
}

static void notify_changed(EditSession *const session,
  ObjEditChanges *const change_info)
{
  DEBUG("Assimilating change record %p", (void *)change_info);

  if (ObjEditChanges_triggers_changed(change_info))
  {
    Session_notify_changed(session, DataType_Mission);
  }

  if (ObjEditChanges_refs_changed(change_info))
  {
    Session_notify_changed(session, (Session_get_objects(session)->overlay !=
                           NULL ? DataType_OverlayObjects : DataType_BaseObjects));
  }
}

static Vertex calc_grid_size(int const zoom)
{
  Vertex const grid_size = {
    SIGNED_R_SHIFT(GRID_GAP_SIZE, zoom),
    SIGNED_R_SHIFT(GRID_GAP_SIZE, zoom)
  };
  DEBUG("Grid size for zoom %d = %d, %d", zoom, grid_size.x, grid_size.y);
  assert(grid_size.x > 0);
  assert(grid_size.y > 0);
  return grid_size;
}

static Vertex grid_to_os_coords(Vertex origin, MapPoint const map_pos,
  Vertex const grid_size)
{
  assert((map_pos.x == Obj_Size && map_pos.y == Obj_Size) ||
         objects_coords_in_range(map_pos));
  assert(grid_size.x > 0);
  assert(grid_size.y > 0);

  Vertex const mpos = {(int)map_pos.x, (int)map_pos.y};
  Vertex const os_coords = Vertex_add(origin, Vertex_mul(mpos, grid_size));
  DEBUG("OS origin = %d,%d Map coords = %" PRIMapCoord ",%" PRIMapCoord
        " OS coords = %d,%d", origin.x, origin.y, map_pos.x, map_pos.y,
        os_coords.x, os_coords.y);
  return os_coords;
}

static void display_msg(Editor *const editor,
  ObjEditChanges const *const change_info)
{
  char *const msg = ObjEditChanges_get_message(change_info);
  if (msg) {
    Editor_display_msg(editor, msg, true);
  }
}

static void free_pending_paste(ObjectsModeData *const mode_data)
{
  assert(mode_data);
  if (mode_data->pending_paste) {
    assert(mode_data->pending_paste != mode_data->pending_transfer);
    dfile_release(ObjTransfer_get_dfile(mode_data->pending_paste));
    mode_data->pending_paste = NULL;
  }
}

static void free_dragged(ObjectsModeData *const mode_data)
{
  assert(mode_data);
  if (mode_data->dragged) {
    assert(mode_data->dragged != mode_data->pending_transfer);
    dfile_release(ObjTransfer_get_dfile(mode_data->dragged));
    mode_data->dragged = NULL;
  }
}

static void free_pending_drop(ObjectsModeData *const mode_data)
{
  assert(mode_data);
  if (mode_data->pending_drop) {
    assert(mode_data->pending_drop != mode_data->pending_transfer);
    dfile_release(ObjTransfer_get_dfile(mode_data->pending_drop));
    mode_data->pending_drop = NULL;
  }
}

static void ObjectsMode_cancel_plot(Editor *const editor)
{
  ObjectsMode_wipe_ghost(editor);
}

static ObjEditContext get_no_prechange_cb_ctx(ObjEditContext const *const objects)
{
  assert(objects);

  ObjEditContext no_prechange_cb_ctx = *objects;
  // Suppress EDITOR_CHANGE_OBJ_PRECHANGE messages
  no_prechange_cb_ctx.prechange_cb = NULL;
  return no_prechange_cb_ctx;
}

static ObjRef read_ref_if_select_overlap(ObjGfxMeshes *const meshes, View const *const view,
  ObjEditContext const *const objects,
  MapPoint const grid_pos, MapArea const *const map_area)
{
  /* If there is an object at the specified grid location, and some part of it
     overlaps the specified (inclusive) rectangle then return the object type */
  ObjRef const obj_ref = ObjectsEdit_read_ref(objects, grid_pos);
  return DrawObjs_touch_select_bbox(meshes, view, obj_ref, grid_pos, map_area) ?
         obj_ref : objects_ref_none();
}

static ObjRef read_ref_if_select_encloses(ObjGfxMeshes *const meshes, View const *const view,
  ObjEditContext const *const objects,
  MapPoint const grid_pos, MapArea const *const map_area)
{
  /* If there is an object at the specified grid location, and all of it
     lies within the specified (inclusive) rectangle then return the object type */
  ObjRef const obj_ref = ObjectsEdit_read_ref(objects, grid_pos);
  return DrawObjs_in_select_bbox(meshes, view, obj_ref, grid_pos, map_area) ?
         obj_ref : objects_ref_none();
}

static ObjRef get_obj_at_point(ObjGfxMeshes *const meshes, View const *const view,
  ObjEditContext const *const read_obj_ctx,
  MapPoint const fine_pos, MapPoint *const grid_coords_out)
{
  /* If there is an object at the specified grid location then return its
     type. Otherwise, search for any nearby objects that overlap the specified
     location. If one is found then return its type and update the input coordinates
     to its location. */
  assert(grid_coords_out);
  DEBUG("Will search for an object overlapping point %" PRIMapCoord ",%" PRIMapCoord,
        fine_pos.x, fine_pos.y);

  MapArea const sample_point = {.min = fine_pos, .max = fine_pos};
  MapPoint const search_centre = ObjLayout_map_coords_from_fine(view, fine_pos);

  /* First, check the object at the grid location within which the specified
     map coordinates lie. */
  ObjRef obj_ref = read_ref_if_select_overlap(meshes, view, read_obj_ctx, search_centre, &sample_point);

  if (!objects_ref_is_none(obj_ref)) {
    DEBUG("Found object %zu at exact location", objects_ref_to_num(obj_ref));
    *grid_coords_out = search_centre;
  } else {
    /* Nothing at the specified grid location, so search outwards  */
    MapArea overlapping_area;
    DrawObjs_get_overlapping_select_area(meshes, view, &sample_point, &overlapping_area);

    MapAreaIter iter;
    for (MapPoint p = MapAreaIter_get_first(&iter, &overlapping_area);
         !MapAreaIter_done(&iter);
         p = MapAreaIter_get_next(&iter))
    {
      obj_ref = read_ref_if_select_overlap(meshes, view, read_obj_ctx, p, &sample_point);
      if (!objects_ref_is_none(obj_ref)) {
        *grid_coords_out = p;
        break;
      }
    }
  }

  if (!objects_ref_is_none(obj_ref)) {
    DEBUG("Found overlapping object of type %zu at %" PRIMapCoord ",%" PRIMapCoord,
          objects_ref_to_num(obj_ref),
          grid_coords_out->x, grid_coords_out->y);
  } else {
    DEBUG("No overlapping object found");
  }
  return obj_ref;
}

static bool drag_select_invert(ObjGfxMeshes *const meshes, View const *const view,
  ObjEditSelection *const selected,
  ObjEditContext const *const objects,
  bool const only_inside, MapArea const *select_box,
  MapArea *const changed_grid, bool const do_redraw)
{
  bool is_changed = false;
  MapArea overlapping_area;
  DrawObjs_get_overlapping_select_area(meshes, view, select_box, &overlapping_area);

  MapAreaIter iter;
  for (MapPoint p = MapAreaIter_get_first(&iter, &overlapping_area);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    ObjRef const obj_ref = only_inside ?
      read_ref_if_select_encloses(meshes, view, objects, p, select_box) :
      read_ref_if_select_overlap(meshes, view, objects, p, select_box);

    if (!objects_ref_is_none(obj_ref)) {
      ObjEditSelection_invert(selected, p, do_redraw);
      is_changed = true;
      if (changed_grid) {
        MapArea_expand(changed_grid, p);
      }
    }
  }
  return is_changed;
}

static void redraw_selection(MapPoint const pos, void *const arg)
{
  Editor *const editor = arg;
  EditSession *const session = Editor_get_session(editor);
  ObjEditContext const *const objects = Session_get_objects(session);

  ObjRef const obj_ref = ObjectsEdit_read_ref(objects, pos);
  bool const has_triggers = objects->triggers && triggers_check_locn(objects->triggers, pos);
  Editor_redraw_object(editor, pos, obj_ref, has_triggers);
}

static void ObjectsMode_update_select(Editor *const editor, bool const only_inside,
  MapArea const *const last_select_box, MapArea const *const select_box,
  EditWin const *const edit_win)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  ObjEditContext const *const read_obj_ctx = EditWin_get_read_obj_ctx(edit_win);
  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;
  View const *const view = EditWin_get_view(edit_win);

  // Copy current selection to allow us to determine the changed area later
  ObjEditSelection_copy(&mode_data->tmp, &mode_data->selection);

  MapArea changed_grid = MapArea_make_invalid();

  // Undo the current selection bounding box by inverting the state of objects within it
  bool changed = drag_select_invert(meshes, view, &mode_data->selection, read_obj_ctx,
                                  only_inside, last_select_box, &changed_grid, false);

  // Apply the new selection bounding box by inverting the state of objects within it
  if (!drag_select_invert(meshes, view, &mode_data->selection, read_obj_ctx, only_inside,
                        select_box, &changed_grid, false) &&
      !changed) {
    return;
  }

  // Redraw only the objects whose state changed
  ObjEditSelection_for_each_changed(&mode_data->selection, &mode_data->tmp,
                                    &changed_grid, redraw_selection, editor);
}

static void ObjectsMode_cancel_select(Editor *const editor,
  bool const only_inside, MapArea const *const last_select_box, EditWin *const edit_win)
{
  /* Abort selection drag by undoing effect of last rectangle */
  ObjectsModeData *const mode_data = get_mode_data(editor);
  ObjEditContext const *const read_obj_ctx = EditWin_get_read_obj_ctx(edit_win);
  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;
  View const *const view = EditWin_get_view(edit_win);

  drag_select_invert(meshes, view, &mode_data->selection, read_obj_ctx,
                     only_inside, last_select_box, NULL, true);
}

static void changed_with_msg(Editor *const editor)
{
  EditSession *const session = Editor_get_session(editor);
  ObjectsModeData *const mode_data = get_mode_data(editor);
  assert(mode_data);

  notify_changed(session, &mode_data->change_info);
  display_msg(editor, &mode_data->change_info);
}

static bool paste_generic(Editor *const editor,
  ObjTransfer *const transfer, MapPoint map_pos)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  ObjEditContext const *const objects = Session_get_objects(session);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;

  if (!ObjTransfers_can_plot_to_map(objects, map_pos, transfer, meshes, NULL)) {
    Editor_display_msg(editor, msgs_lookup("StatusNoPlace"), true);
    return false;
  }

  ObjectsMode_wipe_ghost(editor);

  /* Plot transfer at mouse pointer */
  MapPoint const t_dims = ObjTransfers_get_dims(mode_data->pending_paste);
  map_pos = MapPoint_sub(map_pos, MapPoint_div_log2(t_dims, 1));

  ObjEditChanges_init(&mode_data->change_info);

  ObjEditSelection_clear(&mode_data->selection);
  ObjTransfers_plot_to_map(objects, map_pos, transfer,
                           meshes, &mode_data->selection, &mode_data->change_info);
  changed_with_msg(editor);
  return true;
}

static bool ObjectsMode_start_select(Editor *const editor, bool const only_inside,
  MapPoint const fine_pos, EditWin *const edit_win)
{
  NOT_USED(only_inside);
  ObjectsModeData *const mode_data = get_mode_data(editor);
  ObjEditContext const *const read_obj_ctx = EditWin_get_read_obj_ctx(edit_win);
  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;
  View const *const view = EditWin_get_view(edit_win);

  MapPoint sel_coords;
  ObjRef const obj_ref = get_obj_at_point(meshes, view, read_obj_ctx,
    fine_pos, &sel_coords);

  if (!objects_ref_is_none(obj_ref)) {
    ObjEditSelection_invert(&mode_data->selection, sel_coords, true);
  }

  return objects_ref_is_none(obj_ref);
}

static bool ObjectsMode_start_exclusive_select(Editor *const editor, bool const only_inside,
  MapPoint const fine_pos, EditWin *const edit_win)
{
  NOT_USED(only_inside);
  ObjectsModeData *const mode_data = get_mode_data(editor);
  ObjEditContext const *const read_obj_ctx = EditWin_get_read_obj_ctx(edit_win);
  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;
  View const *const view = EditWin_get_view(edit_win);

  MapPoint sel_coords;
  ObjRef const obj_ref = get_obj_at_point(meshes, view, read_obj_ctx,
    fine_pos, &sel_coords);

  if (!objects_ref_is_none(obj_ref)) {
    if (!ObjEditSelection_is_selected(&mode_data->selection, sel_coords)) {
      ObjEditSelection_clear(&mode_data->selection);
      ObjEditSelection_invert(&mode_data->selection, sel_coords, true);
    }
  } else {
    ObjEditSelection_clear(&mode_data->selection);
  }

  return objects_ref_is_none(obj_ref);
}

static ObjRef get_selected_obj(Editor *const editor)
{
  assert(editor);
  size_t const pal_index = Palette_get_selection(&editor->palette_data);
  return objects_ref_from_num(pal_index != NULL_DATA_INDEX ? pal_index : Obj_RefNone);
}

static void ObjectsMode_edit_properties_at_pos(Editor *const editor, MapPoint const fine_pos, EditWin *const edit_win)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  ObjEditContext const *const read_obj_ctx = EditWin_get_read_obj_ctx(edit_win);
  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;
  View const *const view = EditWin_get_view(edit_win);

  MapPoint sel_coords;
  ObjRef const obj_ref = get_obj_at_point(meshes, view, read_obj_ctx, fine_pos, &sel_coords);

  if (!objects_ref_is_none(obj_ref)) {
    ObjPropDboxes_open(&mode_data->prop_dboxes, sel_coords, edit_win);
  }
}

static void ObjectsMode_clip_overlay(Editor *const editor)
{
  assert(ObjectsMode_can_clip_overlay(editor));
  ObjectsModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  ObjEditContext const *const objects = Session_get_objects(session);

  ObjEditChanges_init(&mode_data->change_info);

  ObjectsEdit_crop_overlay(objects, &mode_data->change_info);
  changed_with_msg(editor);
}

static void set_selected_obj(Editor *const editor, ObjRef const obj_ref)
{
  assert(editor);
  size_t const index = objects_ref_to_num(obj_ref);
  Palette_set_selection(&editor->palette_data, index);
}

static void ObjectsMode_sample_obj(Editor *const editor, MapPoint const fine_pos, MapPoint const map_pos,
  EditWin const *const edit_win)
{
  /* Sample the objects grid at the mouse click location */
  NOT_USED(map_pos);
  ObjEditContext const *const read_obj_ctx = EditWin_get_read_obj_ctx(edit_win);
  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;
  View const *const view = EditWin_get_view(edit_win);

  MapPoint grid_coords;
  ObjRef obj_ref = get_obj_at_point(meshes, view, read_obj_ctx, fine_pos, &grid_coords);
  /*if (objects_ref_is_none(obj_ref)) {
    obj_ref = ObjectsEdit_read_ref(read_obj_ctx, ObjLayout_map_coords_from_fine(EditWin_get_view(edit_win), fine_pos));
  }*/

  set_selected_obj(editor, obj_ref);
}

static void ObjectsMode_pending_fill(Editor *const editor, MapPoint const fine_pos,
  MapPoint const map_pos, EditWin const *const edit_win)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;
  View const *const view = EditWin_get_view(edit_win);

  ObjRef const obj_ref = get_selected_obj(editor);

  ObjEditContext const *const read_obj_ctx = EditWin_get_read_obj_ctx(edit_win);
  MapPoint flood_coords;
  if (objects_ref_is_none(get_obj_at_point(meshes, view, read_obj_ctx, fine_pos, &flood_coords))) {
    flood_coords = map_pos;
  }

  if (mode_data->pending_shape != Pending_Point ||
      !objects_ref_is_equal(mode_data->ghost_obj_ref, obj_ref) ||
      !objects_coords_compare(mode_data->pending_vert[0], flood_coords)) {

    ObjectsMode_set_pending(editor, Pending_Point, obj_ref, NULL, flood_coords);
  }
}

static void ObjectsMode_flood_fill(Editor *const editor, MapPoint const fine_pos,
  MapPoint const map_pos, EditWin const *const edit_win)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  ObjEditContext *const objects = Session_get_objects(session);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;
  View const *const view = EditWin_get_view(edit_win);

  ObjectsMode_wipe_ghost(editor);

  ObjRef const replace = get_selected_obj(editor);

  ObjEditChanges_init(&mode_data->change_info);

  ObjEditContext const *const read_obj_ctx = EditWin_get_read_obj_ctx(edit_win);
  MapPoint flood_coords;
  if (objects_ref_is_none(get_obj_at_point(meshes, view, read_obj_ctx, fine_pos, &flood_coords))) {
    flood_coords = map_pos;
  }

  ObjectsEdit_flood_fill(objects, replace, flood_coords,
                         &mode_data->change_info, meshes);

  changed_with_msg(editor);
}

static void ObjectsMode_global_replace(Editor *const editor, MapPoint const fine_pos,
  MapPoint const map_pos, EditWin const *const edit_win)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  ObjEditContext *const objects = Session_get_objects(session);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;
  View const *const view = EditWin_get_view(edit_win);

  ObjectsMode_wipe_ghost(editor);

  ObjRef const replace = get_selected_obj(editor);

  ObjEditChanges_init(&mode_data->change_info);

  ObjEditContext const *const read_obj_ctx = EditWin_get_read_obj_ctx(edit_win);
  MapPoint replace_coords;
  ObjRef find = get_obj_at_point(meshes, view, read_obj_ctx, fine_pos, &replace_coords);
  if (objects_ref_is_none(find)) {
    find = ObjectsEdit_read_ref(objects, map_pos);
  }

  ObjectsEdit_global_replace(objects, find, replace, &mode_data->change_info, meshes);

  changed_with_msg(editor);
}

static void ObjectsMode_pending_brush(Editor *const editor, int brush_size,
   MapPoint const map_pos)
{
  ObjRef const obj_ref = get_selected_obj(editor);

  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;

  MapPoint const size = (objects_ref_is_none(obj_ref) || objects_ref_is_cloud(obj_ref) ||
                         objects_ref_is_hill(obj_ref) || objects_ref_is_mask(obj_ref)) ?
    (MapPoint){0,0} : ObjGfxMeshes_get_collision_size(meshes, obj_ref);

  if (size.x != 0 || size.y != 0) {
    brush_size = 0;
  }

  MapPoint const bs = {brush_size, brush_size};
  MapArea grid_area;
  MapArea_from_points(&grid_area, MapPoint_sub(map_pos, bs), MapPoint_add(map_pos, bs));

  MapPoint const circum_pos = {map_pos.x, map_pos.y + brush_size};
  ObjectsMode_set_pending(editor, Pending_Circle, obj_ref, NULL, map_pos, circum_pos);
}

static void ObjectsMode_start_brush(Editor *const editor, int brush_size,
  MapPoint const map_pos)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);

  ObjRef const obj_ref = get_selected_obj(editor);

  EditSession *const session = Editor_get_session(editor);
  ObjEditContext *const objects = Session_get_objects(session);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;

  ObjEditChanges_init(&mode_data->change_info);

  ObjectsMode_wipe_ghost(editor);

  MapPoint const size = (objects_ref_is_none(obj_ref) || objects_ref_is_cloud(obj_ref) ||
                         objects_ref_is_hill(obj_ref) || objects_ref_is_mask(obj_ref)) ?
    (MapPoint){0,0} : ObjGfxMeshes_get_collision_size(meshes, obj_ref);

  if (size.x != 0 || size.y != 0) {
    brush_size = 0;
  }

  ObjectsEdit_plot_circ(objects, map_pos, brush_size, obj_ref,
                        &mode_data->change_info, meshes);

  changed_with_msg(editor);
}

static void ObjectsMode_draw_brush(Editor *const editor, int const brush_size,
  MapPoint const last_map_pos, MapPoint const map_pos)
{
  NOT_USED(brush_size);
  ObjectsModeData *const mode_data = get_mode_data(editor);

  ObjRef const obj_ref = get_selected_obj(editor);

  EditSession *const session = Editor_get_session(editor);
  ObjEditContext *const objects = Session_get_objects(session);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;

  ObjectsMode_wipe_ghost(editor);

  MapPoint const size = (objects_ref_is_none(obj_ref) || objects_ref_is_cloud(obj_ref) ||
                         objects_ref_is_hill(obj_ref) || objects_ref_is_mask(obj_ref)) ?
    (MapPoint){0,0} : ObjGfxMeshes_get_collision_size(meshes, obj_ref);

  if (size.x == 0 && size.y == 0) {
    ObjectsEdit_plot_line(objects, last_map_pos, map_pos, obj_ref,
      brush_size, &mode_data->change_info, meshes);

    changed_with_msg(editor);
  }
}

static void ObjectsMode_pending_snake(Editor *const editor, MapPoint const map_pos)
{
  size_t const snake = Palette_get_selection(&editor->palette_data);
  if (snake == NULL_DATA_INDEX) {
    return;
  }

  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjSnakes *const snakes_data = &graphics->snakes;
  ObjGfxMeshes *const meshes = &graphics->meshes;

  ObjRef const obj_ref = ObjSnakes_get_value(session, snakes_data,
                             map_pos, snake, false, meshes);

  if (!objects_ref_is_none(obj_ref)) {
    ObjectsMode_set_pending(editor, Pending_Point, obj_ref, NULL, map_pos);
  }
}

static void ObjectsMode_start_snake(Editor *const editor, MapPoint const map_pos, bool const inside)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);

  size_t const snake = Palette_get_selection(&editor->palette_data);
  if (snake == NULL_DATA_INDEX) {
    return;
  }

  ObjEditChanges_init(&mode_data->change_info);

  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjSnakes *const snakes_data = &graphics->snakes;
  ObjGfxMeshes *const meshes = &graphics->meshes;

  ObjSnakes_begin_line(&mode_data->snake_ctx, session,
    snakes_data, map_pos, snake, inside,
    &mode_data->change_info, meshes);

  changed_with_msg(editor);
}

static void ObjectsMode_draw_snake(Editor *const editor, MapPoint const map_pos)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);

  size_t const snake = Palette_get_selection(&editor->palette_data);
  if (snake == NULL_DATA_INDEX) {
    return;
  }

  ObjectsMode_wipe_ghost(editor);

  ObjSnakes_plot_line(&mode_data->snake_ctx, map_pos,
    &mode_data->change_info);

  changed_with_msg(editor);
}

static bool ObjectsMode_start_pending_paste(Editor *const editor, Reader *const reader,
                       int const estimated_size,
                       DataType const data_type, char const *const filename)
{
  NOT_USED(estimated_size);
  NOT_USED(data_type);
  ObjectsModeData *const mode_data = get_mode_data(editor);

  free_pending_paste(mode_data);
  mode_data->pending_paste = ObjTransfer_create();
  if (mode_data->pending_paste == NULL) {
    return false;
  }

  SFError err = read_compressed(ObjTransfer_get_dfile(mode_data->pending_paste),
                                        reader);
  if (err.type == SFErrorType_TransferNot) {
    err = SFERROR(CBWrong);
  }

  if (report_error(err, filename, "")) {
    free_pending_paste(mode_data);
    return false;
  }

  return true;
}

static void ObjectsMode_pending_paste(Editor *const editor, MapPoint const map_pos)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  assert(mode_data->pending_paste);

  MapPoint const t_dims = ObjTransfers_get_dims(mode_data->pending_paste);

  ObjectsMode_set_pending(editor, Pending_Transfer, objects_ref_none(), mode_data->pending_paste,
                          MapPoint_sub(map_pos, MapPoint_div_log2(t_dims, 1)));
}

static bool ObjectsMode_draw_paste(Editor *const editor, MapPoint const map_pos)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  assert(mode_data->pending_paste);

  if (!paste_generic(editor, mode_data->pending_paste, map_pos)) {
    return false;
  }
  free_pending_paste(mode_data);
  return true;
}

static void ObjectsMode_cancel_paste(Editor *const editor)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  if (!mode_data->pending_paste) {
    return;
  }

  ObjectsMode_wipe_ghost(editor);
  free_pending_paste(mode_data);
}

static bool ObjectsMode_can_draw_numbers(Editor *editor, EditWin const *const edit_win)
{
  NOT_USED(editor);
  return EditWin_get_zoom(edit_win) <= 0;
}

static void ObjectsMode_draw_numbers(Editor *const editor,
  Vertex const scr_orig, MapArea const *const redraw_area,
  EditWin const *const edit_win)
{
  assert(ObjectsMode_can_draw_numbers(editor, edit_win));
  int const zoom = EditWin_get_zoom(edit_win);
  PaletteEntry const bg_colour = EditWin_get_bg_colour(edit_win);

  EditSession *const session = Editor_get_session(editor);
  assert(Session_has_data(session, DataType_BaseObjects) ||
         Session_has_data(session, DataType_OverlayObjects));

  if (!Session_has_data(session, DataType_BaseObjects) &&
      !Session_has_data(session, DataType_OverlayObjects))
    return; /* nothing to plot */

  ObjectsModeData *const mode_data = get_mode_data(editor);

  Vertex const font_size = {
    SIGNED_R_SHIFT(6, zoom),
    SIGNED_R_SHIFT(12, zoom)};
  int handle;
  if (!plot_find_font(font_size, &handle)) {
    return;
  }

  /* Calculate which rows and columns to redraw */
  MapArea const scr_area = ObjLayout_scr_area_from_fine(EditWin_get_view(edit_win), redraw_area);

  size_t last_obj = SIZE_MAX; /* impossible */

  PaletteEntry const bg_sel_colour = opposite_col(bg_colour);
  unsigned int const bg_brightness = palette_entry_brightness(bg_colour);
  unsigned int const bg_sel_brightness = palette_entry_brightness(bg_sel_colour);

  Vertex const grid_size = calc_grid_size(zoom);
  Vertex coord = {
    .y = scr_orig.y + (scr_area.min.y * grid_size.y) + (grid_size.y / 2l)
  };

  Vertex const eig = Desktop_get_eigen_factors();
  Vertex const pix = {1 << eig.x, 1 << eig.y};

  PaletteEntry last_bg_colour = 1;
  PaletteEntry last_fg_colour = 1; /* impossible? */

  ObjEditContext const *const objects = Session_get_objects(session);
  ObjEditContext const *const read_obj_ctx = EditWin_get_read_obj_ctx(edit_win);

  char string[4];
  char underline[sizeof(string)] = "";
  size_t last_ulen = 0;
  BBox text_bbox = {0,0,0,0};
  BBox underline_bbox = {0,0,0,0};
  MapAngle const angle = EditWin_get_angle(edit_win);

  for (MapPoint scr_pos = {.y = scr_area.min.y}; scr_pos.y <= scr_area.max.y; scr_pos.y++) {
    coord.x = scr_orig.x + (scr_area.min.x * grid_size.x) + (grid_size.x / 2l);

    for (scr_pos.x = scr_area.min.x; scr_pos.x <= scr_area.max.x; scr_pos.x++) {
      PaletteEntry font_fg_colour, font_bg_colour;
      MapPoint const map_pos = ObjLayout_derotate_scr_coords_to_map(angle, scr_pos);
      ObjRef const obj_ref = ObjectsEdit_read_ref(read_obj_ctx, map_pos);

      bool const is_sel = ObjEditSelection_is_selected(&mode_data->selection, map_pos);
      font_bg_colour = is_sel ? bg_sel_colour : bg_colour;
      font_fg_colour = (is_sel ? bg_sel_brightness : bg_brightness) >
                       MaxBrightness/2 ? PAL_BLACK : PAL_WHITE;

      size_t const this_obj = objects_ref_to_num(obj_ref);
      if (last_obj != this_obj) {
        sprintf(string, "%zu", this_obj);
        plot_get_string_bbox(handle, string, &text_bbox);
        last_obj = this_obj;
      }

      BBox combined_bbox = text_bbox;

      bool is_underlined = false;
      if (objects->triggers && triggers_check_locn(objects->triggers, map_pos)) {
        is_underlined = true;
        size_t const ulen = strlen(string);
        if (ulen != last_ulen) {
          memset(underline, '_', ulen);
          underline[ulen] = '\0';
          last_ulen = ulen;
          plot_get_string_bbox(handle, underline, &underline_bbox);
        }
        BBox_expand_for_area(&combined_bbox, &underline_bbox);
      }

      /* Calculate coordinates at which to plot numbers
         (centred within the corresponding grid location) */
      Vertex const font_coord = {
        .x = coord.x - (text_bbox.xmax / 2),
        .y = coord.y - (text_bbox.ymax / 2)
      };

      /* Use bounding box from Font_ScanString as rubout box for Font_Paint */
      int const rubout_margin = SIGNED_R_SHIFT(2, zoom);
      BBox const rubout = {
        font_coord.x + combined_bbox.xmin - rubout_margin,
        font_coord.y + combined_bbox.ymin - rubout_margin,
        font_coord.x + combined_bbox.xmax - pix.x + rubout_margin,
        font_coord.y + combined_bbox.ymax - pix.y + rubout_margin,
      };

      DEBUG("Painting string '%s' at %d,%d (rubout box %d,%d %d,%d)",
            string, font_coord.x, font_coord.y, rubout.xmin, rubout.ymin,
            rubout.xmax, rubout.ymax);

      /* Only set font colours if different from last map location */
      if (font_bg_colour != last_bg_colour ||
          font_fg_colour != last_fg_colour) {
        plot_set_font_col(handle, font_bg_colour, font_fg_colour);
        last_bg_colour = font_bg_colour;
        last_fg_colour = font_fg_colour;
      }

      /* Paint object number with rub-out box */
      plot_font(handle, string, &rubout, font_coord, false);

      if (is_underlined) {
        plot_font(handle, underline, NULL, font_coord, false);
      }

      coord.x += grid_size.x;
    } /* next scr_pos.x */

    coord.y += grid_size.y;
  } /* next scr_pos.y */

  plot_lose_font(handle);
}

static bool ObjectsMode_can_draw_grid(Editor *editor, EditWin const *const edit_win)
{
  NOT_USED(editor);
  return EditWin_get_zoom(edit_win) <= 2;
}

static void ObjectsMode_draw_grid(Vertex const scr_orig,
  MapArea const *const redraw_area, EditWin const *edit_win)
{
  assert(ObjectsMode_can_draw_grid(EditWin_get_editor(edit_win), edit_win));
  PaletteEntry const colour = EditWin_get_grid_colour(edit_win);
  int const zoom = EditWin_get_zoom(edit_win);

  /* Calculate the size of each grid square (in OS units) */
  Vertex const grid_size = calc_grid_size(zoom);

  /* Calculate which rows and columns to redraw */
  MapArea const scr_area = ObjLayout_scr_area_from_fine(EditWin_get_view(edit_win), redraw_area);

  plot_set_col(colour);

  Vertex const min_os = grid_to_os_coords(scr_orig, scr_area.min, grid_size);

  Vertex line_start = {
    min_os.x,
    SHRT_MIN
  };

  Vertex line_end = {
    min_os.x,
    SHRT_MAX
  };

  for (MapCoord x_grid = scr_area.min.x; x_grid <= scr_area.max.x; x_grid++) {
    plot_move(line_start);
    plot_fg_line(line_end);

    line_start.x += grid_size.x;
    line_end.x += grid_size.x;
  } /* next x_grid */

  line_start.x = SHRT_MIN;
  line_start.y = line_end.y = min_os.y;
  line_end.x = SHRT_MAX;

  for (MapCoord y_grid = scr_area.min.y; y_grid <= scr_area.max.y; y_grid++) {
    plot_move(line_start);
    plot_fg_line(line_end);

    line_start.y += grid_size.y;
    line_end.y += grid_size.y;
  } /* next y_grid */
}

static void delete_core(Editor *const editor, ObjEditContext *const objects,
  ObjEditChanges *const change_info)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;

  mode_data->lock_selection = true; // strictly redundant
  ObjectsEdit_fill_selected(objects, &mode_data->selection, objects_ref_none(),
    change_info, meshes);
  mode_data->lock_selection = false;

  ObjEditSelection_clear(&mode_data->selection);
}

static void delete_selected_trigs(Editor *const editor)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);

  ObjEditChanges_init(&mode_data->change_info);
  ObjEditContext const *const objects = Session_get_objects(session);
  ObjectsEdit_wipe_triggers(objects, &mode_data->selection,
    &mode_data->change_info);

  changed_with_msg(editor);
  Session_redraw_pending(session, false);
}

static void ObjectsMode_paint_selected(Editor *const editor)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);

  ObjRef const obj_ref = get_selected_obj(editor);

  ObjEditChanges_init(&mode_data->change_info);
  ObjEditContext *const objects = Session_get_objects(session);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;

  mode_data->lock_selection = true;
  ObjectsEdit_fill_selected(objects, &mode_data->selection, obj_ref,
    &mode_data->change_info, meshes);
  mode_data->lock_selection = false;

  changed_with_msg(editor);
}

static ObjTransfer *clipboard;

static bool cb_copy_core(Editor *const editor)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  assert(!ObjEditSelection_is_none(&mode_data->selection));

  EditSession *const session = Editor_get_session(editor);
  assert(!clipboard);
  clipboard = ObjTransfers_grab_selection(
    Session_get_objects(session), &mode_data->selection);

  return clipboard != NULL;
}

static void cb_status(Editor *const editor, bool const copy)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);

  size_t const refs_count = ObjEditSelection_size(&mode_data->selection);
  char refs_count_str[16];
  sprintf(refs_count_str, "%ld", refs_count);

  size_t const trig_count = ObjTransfers_get_trigger_count(clipboard);

  if (trig_count > 0) {
    char trig_count_str[16];
    sprintf(trig_count_str, "%zu", trig_count);

    Editor_display_msg(editor, msgs_lookup_subn(copy ? "OStatusCopy2" :
                       "OStatusCut2", 2, refs_count_str, trig_count_str), true);
  } else {
    Editor_display_msg(editor, msgs_lookup_subn(copy ? "OStatusCopy1" :
                       "OStatusCut1", 1, refs_count_str), true);
  }
}

static void clear_selection_and_redraw(Editor *const editor)
{
  /* Deselect all objects on the map */
  ObjectsModeData *const mode_data = get_mode_data(editor);
  ObjEditSelection_clear(&mode_data->selection);
}

static size_t ObjectsMode_num_selected(Editor const *const editor)
{
  assert(editor != NULL);
  ObjectsModeData *const mode_data = get_mode_data(editor);

  return ObjEditSelection_size(&mode_data->selection);
}

static size_t ObjectsMode_max_selected(Editor const *const editor)
{
  NOT_USED(editor);
  assert(Editor_get_edit_mode(editor) == EDITING_MODE_OBJECTS);
  return Obj_Area;
}

static int ObjectsMode_misc_event(Editor *const editor, int event_code)
{
  switch (event_code) {
    case EVENT_DELETE_SEL_TRIG:
      if (!ObjectsMode_trigger_is_selected(editor)) {
        putchar('\a'); /* no map area selected */
      } else {
        delete_selected_trigs(editor);
      }
      return 1; /* claim event */

    default:
      return 0; /* not interested */
  }
}

static bool ObjectsMode_auto_select(Editor *const editor, MapPoint const fine_pos, EditWin *const edit_win)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  ObjEditContext const *const read_obj_ctx = EditWin_get_read_obj_ctx(edit_win);
  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;
  View const *const view = EditWin_get_view(edit_win);

  if (!ObjEditSelection_is_none(&mode_data->selection) ||
      Editor_get_tool(editor) != EDITORTOOL_SELECT) {
    return false; /* already have a selection or not using that tool */
  }

  MapPoint grid_coords;
  ObjRef const obj_ref = get_obj_at_point(meshes, view, read_obj_ctx, fine_pos, &grid_coords);
  if (objects_ref_is_none(obj_ref)) {
    return false;
  }

  ObjEditSelection_select(&mode_data->selection, grid_coords);

  return true;
}

static void ObjectsMode_auto_deselect(Editor *const editor)
{
  clear_selection_and_redraw(editor);
}

static void ObjectsMode_leave(Editor *const editor)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  assert(mode_data != NULL);
  free_dragged(mode_data);
  free_pending_drop(mode_data);
  free_pending_paste(mode_data);

  ObjPropDboxes_destroy(&mode_data->prop_dboxes);

  ObjEditSelection_destroy(&mode_data->selection);
  ObjEditSelection_destroy(&mode_data->occluded);
  ObjEditSelection_destroy(&mode_data->tmp);
  free(mode_data);
}

static void ObjectsMode_resource_change(Editor *const editor, EditorChange const event,
  EditorChangeParams const *const params)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);

  switch (event) {
  case EDITOR_CHANGE_GFX_ALL_RELOADED:
    ObjectsMode_wipe_ghost(editor);
    Palette_reinit(&editor->palette_data);
    break;

  case EDITOR_CHANGE_GFX_SNAKES_RELOADED:
    ObjectsMode_wipe_ghost(editor);
    if (mode_data->palette_type == OBJPALETTE_TYPE_SNAKES) {
      Palette_reinit(&editor->palette_data);
    }
    break;

  case EDITOR_CHANGE_CLOUD_COLOURS:
    if (mode_data->palette_type == OBJPALETTE_TYPE_OBJS) {
      for (size_t cloud_type = Obj_RefMinCloud; cloud_type < Obj_RefMaxCloud; ++cloud_type)
      {
        Palette_redraw_object(&editor->palette_data, cloud_type);
      }
    }
    break;

  case EDITOR_CHANGE_HILL_COLOURS:
    if (mode_data->palette_type == OBJPALETTE_TYPE_OBJS) {
      Palette_redraw_object(&editor->palette_data, Obj_RefHill);
    }
    break;

  case EDITOR_CHANGE_POLYGON_COLOURS:
    if (mode_data->palette_type == OBJPALETTE_TYPE_OBJS) {
      size_t const num_objects = ObjGfxMeshes_get_ground_count(&graphics->meshes);
      for (size_t obj_ref = Obj_RefMinObject; obj_ref < num_objects; ++obj_ref)
      {
        Palette_redraw_object(&editor->palette_data, obj_ref);
      }
    }
    break;

  case EDITOR_CHANGE_OBJ_ALL_REPLACED:
    ObjEditSelection_clear(&mode_data->selection);
    ObjEditSelection_clear(&mode_data->occluded);
    ObjPropDboxes_destroy(&mode_data->prop_dboxes);
    ObjPropDboxes_init(&mode_data->prop_dboxes, editor);
    break;

  case EDITOR_CHANGE_OBJ_PRECHANGE:
    assert(params);
    if (!mode_data->lock_selection) {
      ObjEditSelection_deselect_area(&mode_data->selection,
        &params->obj_prechange.bbox);
    }

    ObjEditSelection_deselect_area(&mode_data->occluded,
      &params->obj_prechange.bbox);

    ObjPropDboxes_update_for_del(&mode_data->prop_dboxes, &params->obj_prechange.bbox);
    break;

  case EDITOR_CHANGE_OBJ_PREMOVE:
    assert(params);
    if (ObjEditSelection_is_selected(&mode_data->selection, params->obj_premove.old_pos))
    {
      ObjEditSelection_deselect(&mode_data->selection,
        params->obj_premove.old_pos);

      ObjEditSelection_select(&mode_data->selection,
        params->obj_premove.new_pos);
    }

    ObjEditSelection_deselect(&mode_data->occluded,
                              params->obj_premove.old_pos);

    ObjPropDboxes_update_for_del(&mode_data->prop_dboxes,
                                 &(MapArea){params->obj_premove.new_pos,
                                            params->obj_premove.new_pos});

    ObjPropDboxes_update_for_move(&mode_data->prop_dboxes, params->obj_premove.old_pos,
                                  params->obj_premove.new_pos);
    break;

  default:
    break;
  }
}

static void ObjectsMode_palette_selection(Editor *const editor, size_t const object)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  char const *msg = "";

  if (object == NULL_DATA_INDEX) {
    return;
  }
  switch (mode_data->palette_type) {
    case OBJPALETTE_TYPE_OBJS:
    {
      FilenamesData const *const filenames = Session_get_filenames(session);
      char const *const graphics_set = filenames_get(filenames, DataType_PolygonMeshes);

      StringBuffer obj_name;
      stringbuffer_init(&obj_name);

      if (!get_objname_from_type(&obj_name, graphics_set, objects_ref_from_num((size_t)object)))
      {
        report_error(SFERROR(NoMem), "", "");
        return;
      }

      msg = msgs_lookup_subn("StatusObSel", 1, stringbuffer_get_pointer(&obj_name));
      stringbuffer_destroy(&obj_name);
      break;
    }

    case OBJPALETTE_TYPE_SNAKES:
    {
      ObjGfx *const graphics = Session_get_graphics(session);
      char snake_name[16];
      ObjSnakes_get_name(&graphics->snakes, object, snake_name, sizeof(snake_name));
      msg = msgs_lookup_subn("StatusSnSel", 1, snake_name);
      break;
    }

    default:
      break;
  }

  Editor_display_msg(editor, msg, true);
}

static void ObjectsMode_select_all(Editor *const editor)
{
  /* Select all objects on the map */
  ObjectsModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  ObjEditContext const *const objects = Session_get_objects(session);

  MapAreaIter iter;
  for (MapPoint p = objects_get_first(&iter);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    ObjRef const obj_ref = ObjectsEdit_read_ref(objects, p);
    if (!objects_ref_is_none(obj_ref)) {
      ObjEditSelection_select(&mode_data->selection, p);
    }
  }
}

static void ObjectsMode_clear_selection(Editor *const editor)
{
  clear_selection_and_redraw(editor);
}

static void ObjectsMode_delete(Editor *const editor)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);

  ObjEditChanges_init(&mode_data->change_info);
  EditSession *const session = Editor_get_session(editor);
  delete_core(editor, Session_get_objects(session), &mode_data->change_info);
  changed_with_msg(editor);
}

static bool ObjectsMode_cut(Editor *editor)
{
  if (!cb_copy_core(editor)) {
    return false;
  }

  cb_status(editor, false);

  EditSession *const session = Editor_get_session(editor);
  delete_core(editor, Session_get_objects(session), NULL);

  return true;
}

static bool ObjectsMode_copy(Editor *editor)
{
  if (!cb_copy_core(editor)) {
    return false;
  }

  cb_status(editor, true);
  return true;
}

static bool ObjectsMode_start_drag_obj(Editor *const editor,
  MapPoint const fine_pos, EditWin *const edit_win)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  ObjEditContext const *const objects = Session_get_objects(session);

  MapArea sel_box;
  if (!ObjEditSelection_get_bounds(&mode_data->selection, &sel_box)) {
    return false;
  }

  /* Although the selection bounds may happen to be relative to the drag start
     position, it is not guaranteed (e.g. click on far left, drag on far right). */
  View const *const view = EditWin_get_view(edit_win);
  MapPoint const map_pos = ObjLayout_map_coords_from_fine(view, fine_pos);

  if (map_pos.x + (Obj_Size / 2) < sel_box.min.x) {
    sel_box.min.x -= Obj_Size;
    sel_box.max.x -= Obj_Size;
  } else if (map_pos.x - (Obj_Size / 2) > sel_box.max.x) {
    sel_box.min.x += Obj_Size;
    sel_box.max.x += Obj_Size;
  }

  if (map_pos.y + (Obj_Size / 2) < sel_box.min.y) {
    sel_box.min.y -= Obj_Size;
    sel_box.max.y -= Obj_Size;
  } else if (map_pos.y - (Obj_Size / 2) > sel_box.max.y) {
    sel_box.min.y += Obj_Size;
    sel_box.max.y += Obj_Size;
  }

  mode_data->drag_start_pos = sel_box.min;

  free_dragged(mode_data);
  mode_data->dragged = ObjTransfers_grab_selection(objects, &mode_data->selection);
  if (!mode_data->dragged) {
    return false;
  }

  MapArea sent_bbox = ObjLayout_map_area_to_centre(EditWin_get_view(edit_win), &sel_box);
  MapArea_translate(&sent_bbox, (MapPoint){-fine_pos.x, -fine_pos.y}, &sent_bbox);

  MapArea shown_bbox = MapArea_make_invalid();
  MapPoint const t_dims = ObjTransfers_get_dims(mode_data->dragged);

  for (MapPoint trans_pos = {0,0}; trans_pos.y < t_dims.y; trans_pos.y++) {
    for (trans_pos.x = 0; trans_pos.x < t_dims.x; trans_pos.x++) {
      ObjRef const obj_ref = ObjTransfers_read_ref(mode_data->dragged, trans_pos);
      if (!objects_ref_is_mask(obj_ref)) {
        MapArea const obj_bbox = EditWin_get_ghost_obj_bbox(edit_win,
                                     MapPoint_add(sel_box.min, trans_pos), obj_ref);
        MapArea_expand_for_area(&shown_bbox, &obj_bbox);
      }
    }
  }

  MapArea_translate(&shown_bbox, (MapPoint){-fine_pos.x, -fine_pos.y}, &shown_bbox);
  return EditWin_start_drag_obj(edit_win, &sent_bbox, &shown_bbox);
}

static bool ObjectsMode_drag_obj_remote(Editor *const editor, struct Writer *const writer,
    DataType const data_type, char const *const filename)
{
  NOT_USED(data_type);
  ObjectsModeData *const mode_data = get_mode_data(editor);

  if (!mode_data->dragged) {
    return false;
  }

  bool success = !report_error(write_compressed(ObjTransfer_get_dfile(mode_data->dragged),
                                 writer), filename, "");

  free_dragged(mode_data);
  return success;
}

static bool ObjectsMode_show_ghost_drop(Editor *const editor,
                                        MapArea const *const bbox,
                                        Editor const *const drag_origin)
{
  bool hide_origin_bbox = true;
  ObjectsModeData *const mode_data = get_mode_data(editor);
  ObjectsModeData *const origin_data = drag_origin ? get_mode_data(drag_origin) : NULL;
  assert(MapArea_is_valid(bbox));

  if (origin_data) {
    // Dragging from a window belonging to this task
    assert(origin_data->dragged);
    assert(!mode_data->uk_drop_pending);

    EditSession *const session = Editor_get_session(editor);
    ObjGfx *const graphics = Session_get_graphics(session);
    ObjGfxMeshes *const meshes = &graphics->meshes;
    ObjEditContext const *const objects = Session_get_objects(session);

    /* If the zoom level mismatches then the origin drag box will be hidden
       automatically but we also don't want to show it unless it accurately
       reflects the dragged objects' outline in the destination graphics set. */
    if (graphics == Session_get_graphics(Editor_get_session(drag_origin))) {
      hide_origin_bbox = false;
    }

    if (mode_data->pending_drop) {
      if (MapArea_compare(&mode_data->drop_bbox, bbox) &&
          mode_data->pending_drop == origin_data->dragged) {
        DEBUGF("Drop pos unchanged\n");
        return hide_origin_bbox;
      }

      free_pending_drop(mode_data);
      Editor_redraw_ghost(editor); // undraw
    }

    Editor_clear_ghost_bbox(editor);

    ObjEditSelection_copy(&mode_data->tmp, &mode_data->occluded);
    ObjEditSelection_clear(&mode_data->occluded);

    if (ObjTransfers_can_plot_to_map(objects, bbox->min, origin_data->dragged, meshes,
                                        &mode_data->occluded)) {
      ObjectsMode_add_ghost_bbox_for_transfer(editor, bbox->min, origin_data->dragged);

      ObjEditSelection_for_each_changed(&mode_data->occluded, &mode_data->tmp,
                                         NULL, occluded_changed, editor);

      mode_data->pending_drop = origin_data->dragged;
      dfile_claim(ObjTransfer_get_dfile(origin_data->dragged));
    } else {
      ObjEditSelection_for_each(&mode_data->tmp, occluded_changed, editor);
    }
  } else {
    // Dragging from a window belonging to another task
    assert(!mode_data->pending_drop);

    if (mode_data->uk_drop_pending) {
      if (MapArea_compare(&mode_data->drop_bbox, bbox)) {
        DEBUGF("Drop pos unchanged\n");
        return hide_origin_bbox;
      }

      Editor_redraw_ghost(editor); // undraw
    }

    ObjectsMode_wipe_ghost(editor);
    Editor_clear_ghost_bbox(editor);
    Editor_add_ghost_unknown_obj(editor, bbox);
    mode_data->uk_drop_pending = true;
  }

  mode_data->drop_bbox = *bbox;

  Editor_redraw_ghost(editor); // draw
  return hide_origin_bbox;
}

static void ObjectsMode_hide_ghost_drop(Editor *const editor)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);

  if (mode_data->pending_drop) {
    ObjEditSelection_for_each(&mode_data->occluded, occluded_changed, editor);
    ObjEditSelection_clear(&mode_data->occluded);
    Editor_redraw_ghost(editor); // undraw
    Editor_clear_ghost_bbox(editor);
    free_pending_drop(mode_data);
  }

  if (mode_data->uk_drop_pending) {
    Editor_redraw_ghost(editor); // undraw
    Editor_clear_ghost_bbox(editor);
    mode_data->uk_drop_pending = false;
  }
}

static bool drag_obj_copy_core(Editor *const editor,
                           MapArea const *const bbox,
                           ObjTransfer *const dropped,
                           ObjEditContext const *const objects)
{
  assert(MapArea_is_valid(bbox));
  ObjectsModeData *const mode_data = get_mode_data(editor);

  EditSession *const session = Editor_get_session(editor);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;

  if (!ObjTransfers_can_plot_to_map(objects, bbox->min, dropped, meshes, NULL)) {
    Editor_display_msg(editor, msgs_lookup("StatusNoPlace"), true);
    return false;
  }

  ObjEditSelection_clear(&mode_data->selection);

  return ObjTransfers_plot_to_map(objects, bbox->min, dropped,
                                  meshes, &mode_data->selection, &mode_data->change_info);
}

static bool ObjectsMode_drag_obj_copy(Editor *const editor,
                                  MapArea const *const bbox,
                                  Editor const *const drag_origin)
{
  ObjectsModeData *const dst_data = get_mode_data(editor);
  ObjectsModeData *const origin_data = get_mode_data(drag_origin);
  EditSession *const session = Editor_get_session(editor);

  ObjEditChanges_init(&dst_data->change_info);

  if (!drag_obj_copy_core(editor, bbox, origin_data->dragged,
                          Session_get_objects(session)))
  {
    return false;
  }

  changed_with_msg(editor);
  free_dragged(origin_data);

  return true;
}

static void ObjectsMode_cancel_drag_obj(Editor *const editor)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  free_dragged(mode_data);
}

static bool ObjectsMode_drag_obj_link(Editor *const editor,
  int const window, int const icon, Editor *const drag_origin)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  ObjectsModeData *const origin_data = get_mode_data(drag_origin);

  MapPoint const dims = ObjTransfers_get_dims(origin_data->dragged);
  if (MapPoint_area(dims) > 1) {
    return false;
  }

  if (!ObjPropDboxes_drag_obj_link(&mode_data->prop_dboxes, window, icon, origin_data->drag_start_pos)) {
    return false;
  }

  free_dragged(origin_data);
  return true;
}

static void gen_premove_msgs(EditSession *const session, ObjectsModeData *const mode_data,
                           MapArea const *const bbox)
{
  assert(mode_data);
  assert(MapArea_is_valid(bbox));

  /* Take into account the direction of the move to avoid issues when part of the
     source data is overwritten by the moved data. */
  ObjTransfer *const transfer = mode_data->dragged;
  MapPoint const dims = ObjTransfers_get_dims(transfer);
  MapPoint dir = {1, 1}, start = {0, 0}, stop = dims;

  if (mode_data->drag_start_pos.x < bbox->min.x) {
    start.x = dims.x - 1;
    stop.x = -1;
    dir.x = -1;
  }

  if (mode_data->drag_start_pos.y < bbox->min.y) {
    start.y = dims.y - 1;
    stop.y = -1;
    dir.y = -1;
  }

  for (MapPoint p = {.x = start.x}; p.x != stop.x; p.x += dir.x) {
    for (p.y = start.y; p.y != stop.y; p.y += dir.y) {
      DEBUGF("%" PRIMapCoord ",%" PRIMapCoord " in source area\n", p.x, p.y);
      ObjRef const obj_ref = ObjTransfers_read_ref(transfer, p);

      if (!objects_ref_is_mask(obj_ref)) {
        Session_object_premove(session, MapPoint_add(mode_data->drag_start_pos, p), MapPoint_add(bbox->min, p));
      }
    }
  }
}

static void ObjectsMode_drag_obj_move(Editor *const editor,
                                  MapArea const *const bbox,
                                  Editor *const drag_origin)
{
  ObjectsModeData *const dst_data = get_mode_data(editor);
  ObjectsModeData *const origin_data = get_mode_data(drag_origin);
  EditSession *const session = Editor_get_session(editor);
  assert(session == Editor_get_session(drag_origin));
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;
  ObjEditContext const no_prechange_cb_ctx = get_no_prechange_cb_ctx(Session_get_objects(session));

  if (!ObjTransfers_can_plot_to_map(&no_prechange_cb_ctx, bbox->min,
                                          origin_data->dragged, meshes, NULL)) {
    Editor_display_msg(editor, msgs_lookup("StatusNoPlace"), true);
    return;
  }

  ObjEditChanges_init(&dst_data->change_info);
  ObjEditChanges_init(&origin_data->change_info);

  // Moves the selection: take care if reordering these calls
  gen_premove_msgs(session, origin_data, bbox);

  // FIXME: single move call?
  ObjTransfers_fill_map(&no_prechange_cb_ctx, origin_data->drag_start_pos, origin_data->dragged,
                        objects_ref_from_num(Obj_RefNone), meshes, &origin_data->change_info);

  ObjEditSelection_clear(&dst_data->selection);
  ObjTransfers_plot_to_map(&no_prechange_cb_ctx, bbox->min, origin_data->dragged,
                           meshes, &dst_data->selection, &dst_data->change_info);

  changed_with_msg(editor);
  if (editor != drag_origin) {
    changed_with_msg(drag_origin);
  }
  free_dragged(origin_data);
}

static bool ObjectsMode_drop(Editor *const editor, MapArea const *const bbox,
                             Reader *const reader, int const estimated_size,
                             DataType const data_type, char const *const filename)
{
  NOT_USED(data_type);
  NOT_USED(estimated_size);
  ObjectsModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);

  ObjTransfer *const dropped = ObjTransfer_create();
  if (dropped == NULL) {
    return false;
  }

  SFError err = read_compressed(ObjTransfer_get_dfile(dropped), reader);
  bool success = !report_error(err, filename, "");
  if (success) {
    ObjEditChanges_init(&mode_data->change_info);

    success = drag_obj_copy_core(editor, bbox, dropped, Session_get_objects(session));
    if (success) {
      changed_with_msg(editor);
    }
  }

  dfile_release(ObjTransfer_get_dfile(dropped));
  return success;
}

static void ObjectsMode_pending_point(Editor *const editor, MapPoint const map_pos)
{
  ObjRef const obj_ref = get_selected_obj(editor);
  ObjectsMode_set_pending(editor, Pending_Point, obj_ref, NULL, map_pos);
}

static void ObjectsMode_pending_line(Editor *const editor, MapPoint const a, MapPoint const b)
{
  assert(Editor_get_tool(editor) == EDITORTOOL_PLOTSHAPES);

  ObjRef const obj_ref = get_selected_obj(editor);
  ObjectsMode_set_pending(editor, Pending_Line, obj_ref, NULL, a, b);
}

static void ObjectsMode_plot_line(Editor *const editor, MapPoint const a, MapPoint const b)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  assert(Editor_get_plot_shape(editor) == PLOTSHAPE_LINE);
  ObjEditContext *const objects = Session_get_objects(session);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;

  ObjEditChanges_init(&mode_data->change_info);

  ObjectsMode_wipe_ghost(editor);

  ObjRef const obj_ref = get_selected_obj(editor);
  ObjectsEdit_plot_line(objects, a, b, obj_ref, 0,
                    &mode_data->change_info, meshes);
  changed_with_msg(editor);
}

static void ObjectsMode_pending_tri(Editor *const editor,
  MapPoint const a, MapPoint const b, MapPoint const c)
{
  assert(Editor_get_tool(editor) == EDITORTOOL_PLOTSHAPES);

  ObjRef const obj_ref = get_selected_obj(editor);
  ObjectsMode_set_pending(editor, Pending_Triangle, obj_ref, NULL, a, b, c);
}

static void ObjectsMode_plot_tri(Editor *const editor,
  MapPoint const a, MapPoint const b, MapPoint const c)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  assert(Editor_get_plot_shape(editor) == PLOTSHAPE_TRIANGLE);
  EditSession *const session = Editor_get_session(editor);
  ObjEditContext *const objects = Session_get_objects(session);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;

  ObjectsMode_wipe_ghost(editor);

  ObjRef const obj_ref = get_selected_obj(editor);
  ObjEditChanges_init(&mode_data->change_info);
  ObjectsEdit_plot_tri(objects, a, b, c, obj_ref,
                   &mode_data->change_info, meshes);
  changed_with_msg(editor);
}

static void ObjectsMode_pending_rect(Editor *const editor, MapPoint const a, MapPoint const b)
{
  assert(Editor_get_tool(editor) == EDITORTOOL_PLOTSHAPES);

  ObjRef const obj_ref = get_selected_obj(editor);
  ObjectsMode_set_pending(editor, Pending_Rectangle, obj_ref, NULL, a, b);
}

static void ObjectsMode_plot_rect(Editor *const editor, MapPoint const a, MapPoint const b)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  assert(Editor_get_plot_shape(editor) == PLOTSHAPE_RECTANGLE);
  EditSession *const session = Editor_get_session(editor);
  ObjEditContext *const objects = Session_get_objects(session);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;

  ObjectsMode_wipe_ghost(editor);

  ObjRef const obj_ref = get_selected_obj(editor);
  ObjEditChanges_init(&mode_data->change_info);
  ObjectsEdit_plot_rect(objects, a, b, obj_ref, &mode_data->change_info, meshes);
  changed_with_msg(editor);
}

static void ObjectsMode_pending_circ(Editor *const editor, MapPoint const a, MapPoint const b)
{
  assert(Editor_get_plot_shape(editor) == PLOTSHAPE_CIRCLE);

  ObjRef const obj_ref = get_selected_obj(editor);
  ObjectsMode_set_pending(editor, Pending_Circle, obj_ref, NULL, a, b);
}

static void ObjectsMode_plot_circ(Editor *const editor, MapPoint const a, MapPoint const b)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  assert(Editor_get_plot_shape(editor) == PLOTSHAPE_CIRCLE);
  EditSession *const session = Editor_get_session(editor);
  ObjEditContext *const objects = Session_get_objects(session);
  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;

  ObjectsMode_wipe_ghost(editor);

  ObjRef const obj_ref = get_selected_obj(editor);
  ObjEditChanges_init(&mode_data->change_info);
  ObjectsEdit_plot_circ(objects, a, MapPoint_dist(a, b),
    obj_ref, &mode_data->change_info, meshes);

  changed_with_msg(editor);
}

static char *ObjectsMode_get_help_msg(Editor const *const editor)
{
  char *msg = NULL; // remove help
  ObjectsModeData *const mode_data = get_mode_data(editor);

  switch (Editor_get_tool(editor)) {
    case EDITORTOOL_BRUSH:
      msg = msgs_lookup("MapObjBrush");
      break;

    case EDITORTOOL_SNAKE:
      msg = msgs_lookup("MapObjSnake");
      break;

    case EDITORTOOL_SELECT:
      msg = msgs_lookup(mode_data->pending_paste ? "MapObjPaste" : "MapObjSelect");
      break;

    case EDITORTOOL_SAMPLER:
      msg = msgs_lookup("MapObjSample");
      break;

    default:
      break;
  }
  return msg;
}

static void ObjectsMode_tool_selected(Editor *const editor)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);

  ObjectsMode_wipe_ghost(editor);

  switch (Editor_get_tool(editor)) {
    case EDITORTOOL_SNAKE:
      /* Configure palette to display snakes */
      if (mode_data->palette_type != OBJPALETTE_TYPE_SNAKES)
      {
        ObjSnakesPalette_register(&editor->palette_data);
        mode_data->palette_type = OBJPALETTE_TYPE_SNAKES;
      }
      break;

    default:
      /* Configure palette to display objects */
      if (mode_data->palette_type != OBJPALETTE_TYPE_OBJS)
      {
        ObjsPalette_register(&editor->palette_data);
        mode_data->palette_type = OBJPALETTE_TYPE_OBJS;
      }
      break;
  }
}

static MapPoint ObjectsMode_map_to_grid_coords(MapPoint const pos, EditWin const *const edit_win)
{
  return ObjLayout_map_coords_from_fine(EditWin_get_view(edit_win), pos);
}

MapArea ObjectsMode_map_to_grid_area(MapArea const *const map_area, EditWin const *const edit_win)
{
  return ObjLayout_map_area_from_fine(EditWin_get_view(edit_win), map_area);
}

static MapPoint ObjectsMode_grid_to_map_coords(MapPoint const pos, EditWin const *const edit_win)
{
  return ObjLayout_map_coords_to_centre(EditWin_get_view(edit_win), pos);
}

/* ----------------- Public functions ---------------- */

bool ObjectsMode_can_enter(Editor *const editor)
{
  EditSession *const session = Editor_get_session(editor);

  return Session_has_data(session, DataType_BaseObjects) ||
         Session_has_data(session, DataType_OverlayObjects);
}

bool ObjectsMode_enter(Editor *const editor)
{
  DEBUG("Entering objects mode");
  assert(ObjectsMode_can_enter(editor));

  ObjectsModeData *const mode_data = malloc(sizeof(ObjectsModeData));
  if (mode_data == NULL) {
    report_error(SFERROR(NoMem), "", "");
    return false;
  }

  *mode_data = (ObjectsModeData){
    .palette_type = OBJPALETTE_TYPE_NONE,
    .pending_shape = Pending_None,
  };

  editor->editingmode_data = mode_data;

  static DataType const type_list[] = {DataType_ObjectsTransfer, DataType_Count};

  static EditModeFuncts const objects_mode_fns = {
    .coord_limit = {Obj_Size, Obj_Size},
    .dragged_data_types = type_list,
    .import_data_types = type_list,
    .export_data_types = type_list,
    .auto_select = ObjectsMode_auto_select,
    .auto_deselect = ObjectsMode_auto_deselect,
    .misc_event = ObjectsMode_misc_event,
    .can_draw_grid = ObjectsMode_can_draw_grid,
    .draw_grid = ObjectsMode_draw_grid,
    .leave = ObjectsMode_leave,

    .resource_change = ObjectsMode_resource_change,
    .palette_selection = ObjectsMode_palette_selection,

    .can_draw_numbers = ObjectsMode_can_draw_numbers,
    .draw_numbers = ObjectsMode_draw_numbers,
    .map_to_grid_coords = ObjectsMode_map_to_grid_coords,
    .map_to_grid_area = ObjectsMode_map_to_grid_area,
    .grid_to_map_coords = ObjectsMode_grid_to_map_coords,
    .num_selected = ObjectsMode_num_selected,
    .max_selected = ObjectsMode_max_selected,
    .can_clip_overlay = ObjectsMode_can_clip_overlay,
    .clip_overlay = ObjectsMode_clip_overlay,
    .can_delete = ObjectsMode_has_selection,
    .can_replace = ObjectsMode_has_selection,
    .can_select_tool = ObjectsMode_can_select_tool,
    .tool_selected = ObjectsMode_tool_selected,
    .select_all = ObjectsMode_select_all,
    .clear_selection = ObjectsMode_clear_selection,
    .delete = ObjectsMode_delete,
    .cut = ObjectsMode_cut,
    .copy = ObjectsMode_copy,
    .can_edit_properties = ObjectsMode_can_edit_properties,
    .edit_properties = ObjectsMode_edit_properties,
    .trigger_is_selected = ObjectsMode_trigger_is_selected,
    .update_title = ObjectsMode_update_title,
    .get_help_msg = ObjectsMode_get_help_msg,

    .sample_obj = ObjectsMode_sample_obj,

    .pending_plot = ObjectsMode_pending_point,

    .pending_line = ObjectsMode_pending_line,
    .plot_line = ObjectsMode_plot_line,

    .pending_rect = ObjectsMode_pending_rect,
    .plot_rect = ObjectsMode_plot_rect,

    .pending_circ = ObjectsMode_pending_circ,
    .plot_circ = ObjectsMode_plot_circ,

    .pending_tri = ObjectsMode_pending_tri,
    .plot_tri = ObjectsMode_plot_tri,

    .cancel_plot = ObjectsMode_cancel_plot,

    .pending_brush = ObjectsMode_pending_brush,
    .start_brush = ObjectsMode_start_brush,
    .draw_brush = ObjectsMode_draw_brush,

    .pending_snake = ObjectsMode_pending_snake,
    .start_snake = ObjectsMode_start_snake,
    .draw_snake = ObjectsMode_draw_snake,

    .pending_flood_fill = ObjectsMode_pending_fill,
    .flood_fill = ObjectsMode_flood_fill,

    .pending_global_replace = ObjectsMode_pending_fill,
    .global_replace = ObjectsMode_global_replace,

    .start_select = ObjectsMode_start_select,
    .start_exclusive_select = ObjectsMode_start_exclusive_select,
    .update_select = ObjectsMode_update_select,
    .cancel_select = ObjectsMode_cancel_select,

    .start_drag_obj = ObjectsMode_start_drag_obj,
    .drag_obj_remote = ObjectsMode_drag_obj_remote,
    .drag_obj_copy = ObjectsMode_drag_obj_copy,
    .drag_obj_move = ObjectsMode_drag_obj_move,
    .drag_obj_link = ObjectsMode_drag_obj_link,
    .cancel_drag_obj = ObjectsMode_cancel_drag_obj,

    .show_ghost_drop = ObjectsMode_show_ghost_drop,
    .hide_ghost_drop = ObjectsMode_hide_ghost_drop,
    .drop = ObjectsMode_drop,

    .edit_properties_at_pos = ObjectsMode_edit_properties_at_pos,

    .start_pending_paste = ObjectsMode_start_pending_paste,
    .pending_paste = ObjectsMode_pending_paste,
    .draw_paste = ObjectsMode_draw_paste,
    .cancel_paste = ObjectsMode_cancel_paste,

    .paint_selected = ObjectsMode_paint_selected,

    .wipe_ghost = ObjectsMode_wipe_ghost,
  };
  editor->mode_functions = &objects_mode_fns;

  ObjPropDboxes_init(&mode_data->prop_dboxes, editor);

  SFError err = ObjEditSelection_init(&mode_data->selection, redraw_selection, editor);
  if (!SFError_fail(err)) {
    err = ObjEditSelection_init(&mode_data->tmp, NULL, NULL);
    if (!SFError_fail(err)) {
      // No redraw callback to avoid flickering of objects still occluded from one frame to the next
      err = ObjEditSelection_init(&mode_data->occluded, NULL, NULL);
      if (!SFError_fail(err)) {
        Editor_display_msg(editor, msgs_lookup("StatusObjMode"), false);
        return true;
      }
      ObjEditSelection_destroy(&mode_data->tmp);
    }
    ObjEditSelection_destroy(&mode_data->selection);
  }
  report_error(err, "", "");
  free(mode_data);
  editor->editingmode_data = NULL;
  return false;
}

void ObjectsMode_free_clipboard(void)
{
  if (clipboard) {
    dfile_release(ObjTransfer_get_dfile(clipboard));
    clipboard = NULL;
  }
}

bool ObjectsMode_write_clipboard(struct Writer *const writer,
  DataType const data_type, char const *const filename)
{
  NOT_USED(data_type);
  return !report_error(write_compressed(ObjTransfer_get_dfile(clipboard), writer), filename, "");
}

int ObjectsMode_estimate_clipboard(DataType const data_type)
{
  NOT_USED(data_type);
  return worst_compressed_size(ObjTransfer_get_dfile(clipboard));
}

bool ObjectsMode_set_properties(Editor *const editor, MapPoint const pos, ObjRef const obj_ref,
  TriggerFullParam const *const fparam, size_t const nitems)
{
  ObjectsModeData *const mode_data = get_mode_data(editor);
  EditSession *const session = Editor_get_session(editor);
  ObjEditContext *const objects = Session_get_objects(session);

  ObjGfx *const graphics = Session_get_graphics(session);
  ObjGfxMeshes *const meshes = &graphics->meshes;

  ObjEditChanges_init(&mode_data->change_info);

  mode_data->lock_selection = true;
  bool success = ObjectsEdit_write_ref_n_triggers(objects, pos, obj_ref, fparam, nitems,
                     &mode_data->change_info, meshes);
  mode_data->lock_selection = false;

  changed_with_msg(editor);
  Session_redraw_pending(session, false);
  return success;
}

void ObjectsMode_redraw_clouds(Editor *const editor)
{
  /* Redraw all clouds on the map */
  EditSession *const session = Editor_get_session(editor);
  ObjEditContext const *const objects = Session_get_objects(session);

  MapAreaIter iter;
  for (MapPoint p = objects_get_first(&iter);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter))
  {
    ObjRef const obj_ref = ObjectsEdit_read_ref(objects, p);
    if (objects_ref_is_cloud(obj_ref)) {
      bool const has_triggers = objects->triggers && triggers_check_locn(objects->triggers, p);
      Editor_redraw_object(editor, p, obj_ref, has_triggers);
    }
  }
}
