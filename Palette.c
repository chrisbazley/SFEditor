/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Palette window
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

#include "stdlib.h"
#include "stdio.h"
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>

#include "toolbox.h"
#include "window.h"
#include "event.h"
#include "wimp.h"
#include "wimplib.h"

#include "msgtrans.h"
#include "err.h"
#include "Macros.h"
#include "FilePerc.h"
#include "StrExtra.h"
#include "PathTail.h"
#include "WimpExtra.h"
#include "DeIconise.h"
#include "debug.h"
#include "OSVDU.h"
#include "EventExtra.h"
#include "Scheduler.h"
#include "OSReadTime.h"

#include "Utils.h"
#include "Session.h"
#include "Palette.h"
#include "Editor.h"
#include "plot.h"
#include "PaletteData.h"
#include "Desktop.h"
#include "Vertex.h"
#include "EditWin.h"
#include "OurEvents.h"
#include "TransMenu.h"

enum {
  X_BORDER = 4,
  Y_BORDER = 4,
  OBJECT_X_SPACER = 4,
  OBJECT_Y_SPACER = 4,
  NAME_HEIGHT = 40,
  MinWindowExtentX = 320 + (OBJECT_X_SPACER * 2),
  MinWindowExtentY = 256 + NAME_HEIGHT + (OBJECT_Y_SPACER * 2),
};

typedef enum {
  ReformatAction_Default,
  ReformatAction_Force,
  ReformatAction_OnlyIfWidthChanged,
} ReformatAction;

#define PALETTE_KEEP_VISIBLE_AREA 0

/* ---------------- Private functions ---------------- */

static SchedulerTime anim_cb(void *handle, SchedulerTime time_now, const volatile bool *time_up)
{
  PaletteData *const pal_data = handle;
  NOT_USED(time_up);

  if (pal_data->client_functions &&
      pal_data->client_functions->animate) {
    time_now = pal_data->client_functions->animate(pal_data->parent_editor, time_now);
  }

  Palette_redraw_object(pal_data, Palette_get_selection(pal_data));

  return time_now;
}

static int get_preferred_width(PaletteData const *const pal_data)
{
  DEBUG ("Preferred width queried for palette object %p", (void *)pal_data);
  assert(pal_data != NULL);

  int width = 2 * X_BORDER; /* absolute minimum width */

  if (pal_data->client_functions != NULL)
  {
    DEBUG ("Default no. of columns is %d",
           pal_data->client_functions->default_columns);

    width += pal_data->object_size.x *
             pal_data->client_functions->default_columns;
  }

  DEBUG ("Requesting width %d", width);
  return width;
}

static BBox bbox_for_object(PaletteData const *const pal_data, Vertex const grid_pos)
{
  assert(pal_data != NULL);
  assert(grid_pos.x >= 0);
  assert(grid_pos.y >= 0);
  assert(grid_pos.x < pal_data->grid_size.x);
  assert(grid_pos.y < pal_data->grid_size.y);

  Vertex const object_min = Vertex_mul(grid_pos, pal_data->object_size);
  Vertex const object_max = Vertex_add(object_min, pal_data->object_size);
  return (BBox){X_BORDER + object_min.x, -Y_BORDER - object_max.y,
                X_BORDER + object_max.x, -Y_BORDER - object_min.y};
}

static Vertex grid_from_index(PaletteData *const pal_data, size_t const index)
{
  assert(pal_data != NULL);
  assert(index >= 0);
  assert(index <= pal_data->num_indices); /* intentional for deletion of last object */

  Vertex grid_pos = {0,0};
  if (!pal_data->numeric_order &&
      pal_data->client_functions != NULL &&
      pal_data->client_functions->index_to_grid)
  {
    DEBUGF("Calling index-to-grid function for custom layout\n");
    grid_pos = pal_data->client_functions->index_to_grid(pal_data->parent_editor,
               index, (size_t)pal_data->grid_size.x);
  }
  else
  {
    grid_pos = (Vertex){(int)index % pal_data->grid_size.x, (int)index / pal_data->grid_size.x};
  }

  DEBUGF("Object with index %zu is at %d,%d in palette\n",
         index, grid_pos.x, grid_pos.y);

  return grid_pos;
}

static size_t index_to_object(PaletteData const *const pal_data, size_t const index)
{
  assert(pal_data);
  if (index == NULL_DATA_INDEX) {
    return NULL_DATA_INDEX;
  }
  assert(index >= 0);
  assert(index < pal_data->num_indices);

  size_t object = index;

  if (pal_data->client_functions != NULL &&
      pal_data->client_functions->index_to_object != NULL)
  {
    object = pal_data->client_functions->index_to_object(
      pal_data->parent_editor, index);
  }

  DEBUGF("Object is %zu from index %zu, count %zu\n", object, index, pal_data->num_indices);
  return object;
}

static void update_menus(PaletteData *const pal_data)
{
  assert(pal_data);
  if (pal_data->client_functions != NULL &&
      pal_data->client_functions->update_menus != NULL)
  {
    pal_data->client_functions->update_menus(pal_data);
  }
}


static size_t p_object_to_index(PaletteData const *const pal_data, size_t const object)
{
  assert(pal_data);
  assert(object >= 0);

  size_t index = object;

  if (pal_data->client_functions != NULL &&
      pal_data->client_functions->object_to_index != NULL)
  {
    index = pal_data->client_functions->object_to_index(
      pal_data->parent_editor, object);
  }

  DEBUGF("Index is %zu from object %zu, count %zu\n", index, object, pal_data->num_indices);
  assert(index >= 0);
  assert(index < pal_data->num_indices);
  return index;
}

static void select(PaletteData *const pal_data, Vertex const grid_pos, size_t const index,
                   bool scroll, bool redraw, bool const hint)
{
  DEBUGF("Selecting item %zu at %d,%d\n", index, grid_pos.x, grid_pos.y);
  BBox object_bbox = bbox_for_object(pal_data, grid_pos);
  int window_handle;
  if (E(window_get_wimp_handle(0, pal_data->my_object, &window_handle)))
    redraw = scroll = false; /* attempt to recover */

  if (redraw) {
    /*
       Force redraw of both previously and newly selected tiles
       (Have to be a bit careful because xmax and ymax are EXCLUSIVE)
     */
    if (pal_data->sel_index != NULL_DATA_INDEX) {
      DEBUGF("Previous selection is %d,%d\n", pal_data->sel_pos.x, pal_data->sel_pos.y);
      BBox sel_bbox = bbox_for_object(pal_data, pal_data->sel_pos);
      E(window_force_redraw(0, pal_data->my_object, &sel_bbox));
    } else {
      DEBUGF("No previous selection\n");
    }
    E(window_force_redraw(0, pal_data->my_object, &object_bbox));
  }

  if (scroll && object_is_showing(pal_data->my_object)) {
    /* Re-open window with new scroll offset */
    WimpGetWindowStateBlock window_state = {window_handle};

    if (!E(wimp_get_window_state(&window_state)))
    {
      if (object_bbox.ymax > window_state.yscroll)
      {
        window_state.yscroll = object_bbox.ymax;
      }
      else
      {
        int visible_height = window_state.visible_area.ymax -
                             window_state.visible_area.ymin;
        if (object_bbox.ymin < window_state.yscroll - visible_height)
          window_state.yscroll = object_bbox.ymin + visible_height;
      }
      // Parent object may be dead by now, and we don't need it anyway
      E(toolbox_show_object(0, pal_data->my_object,
                 Toolbox_ShowObject_FullSpec, &window_state.visible_area,
                 NULL_ObjectId, NULL_ComponentId));
    }
  }

  pal_data->sel_index = index;
  pal_data->sel_pos = grid_pos;

  if (!hint) {
    return;
  }

  Editor_palette_selection(pal_data->parent_editor, index_to_object(pal_data, index));
}

static size_t index_from_grid(PaletteData *const pal_data, Vertex grid_pos)
{
  /*
     Given a grid location in the palette, find the corresponding object number
     returns NULL_DATA_INDEX if grid location empty
  */
  assert(pal_data != NULL);
  size_t index = NULL_DATA_INDEX;

  if (!pal_data->numeric_order && pal_data->client_functions != NULL &&
      pal_data->client_functions->grid_to_index != NULL)
  {
    DEBUGF("Calling grid-to-index function for custom layout\n");
    index = pal_data->client_functions->grid_to_index(pal_data->parent_editor,
                grid_pos, (size_t)pal_data->grid_size.x);
  }
  else
  {
    index = (size_t)((grid_pos.y * pal_data->grid_size.x) + grid_pos.x);
    if (index >= pal_data->num_indices)
      index = NULL_DATA_INDEX; /* off the end (final row?) */
  }

  if (index == NULL_DATA_INDEX)
  {
    DEBUG("Grid location %d,%d is blank", grid_pos.x, grid_pos.y);
  }
  else
  {
    DEBUG("Item at grid location %d,%d has index %zu", grid_pos.x, grid_pos.y,
          index);
  }

  return index;
}

static bool calcmaxcolumns(PaletteData *const pal_data, int const new_width)
{
  /* Maximum width of palette depends on width of righthand window border
    (vertical scrollbar) and the width of the desktop area.
    Returns 'true' if the maximum width (in columns) has changed. */
  assert(pal_data != NULL);
  DEBUG ("Calculating maximum width of palette %p (object 0x%x)", (void *)pal_data,
         pal_data->my_object);

  int sbar_width = 0;
  get_scrollbar_sizes(&sbar_width, NULL);
  DEBUG("Vertical scrollbar width: %d", sbar_width);

  size_t new_max_columns = (size_t)((new_width - sbar_width - X_BORDER * 2) /
                                     pal_data->object_size.x);
  assert(new_max_columns >= 0);
  size_t columns_limit = 1;

  if (pal_data->client_functions == NULL || pal_data->numeric_order ||
      pal_data->client_functions->get_max_columns == NULL) {
    /* Assume the width may not exceed the number of objects to display */
    columns_limit = pal_data->num_indices;
  } else {
    /* Do not make any assumptions about the layout of the palette */
    columns_limit = pal_data->client_functions->get_max_columns(
                      pal_data->parent_editor);
  }

  if (new_max_columns > columns_limit) {
    new_max_columns = HIGHEST(columns_limit, 1);
  }

  if (new_max_columns != pal_data->max_columns) {
    pal_data->max_columns = new_max_columns;
    DEBUG("New max no. of columns: %zu", pal_data->max_columns);
    return true; /* maximum no. of columns changed */
  }
  return false; /* maximum no. of columns unchanged */
}

static BBox calc_extent(PaletteData *const pal_data)
{
  /* Recalculate the work area extent of a palette window according to the
     size of each item, no. of rows required by the current layout, and
     maximum no. of columns. */
  assert(pal_data != NULL);

  /* Calculate work area extent (maximum width & height of palette) */
  DEBUG("Calculating work area extent for %d rows and up to %zu columns",
        pal_data->grid_size.y, pal_data->max_columns);

  Vertex max_layout_size = Vertex_mul(
    (Vertex){(int)pal_data->max_columns, pal_data->grid_size.y},
    pal_data->object_size);

  max_layout_size.x = HIGHEST(max_layout_size.x, MinWindowExtentX);
  max_layout_size.y = HIGHEST(max_layout_size.y, MinWindowExtentY);

  return (BBox){
    .xmin = 0,
    .ymin = -(Y_BORDER * 2) - max_layout_size.y,
    .xmax = (X_BORDER * 2) + max_layout_size.x,
    .ymax = 0,
  };
}

static void set_extent(PaletteData *const pal_data, BBox const *const visible_area,
  bool const redraw)
{
  /* Set the work area extent of the palette window according to the size of
     each item, no. of rows required by the current layout, and maximum no. of
     columns. */
  assert(pal_data != NULL);
  assert(visible_area);
  ObjectId const pal_id = pal_data->my_object;

  DEBUG ("Setting extent of palette %p (object 0x%x)", (void *)pal_data, pal_id);

  BBox extent = calc_extent(pal_data);

#if PALETTE_KEEP_VISIBLE_AREA
  // Don't shrink extent below visible area to avoid annoyance when contents change
  assert(extent.ymax == 0);
  int const h = BBox_height(visible_area);
  if (extent.ymin > -h) {
    extent.ymin = -h;
  }
  assert(extent.xmin == 0);
  int const w = BBox_width(visible_area);
  if (extent.xmax < w) {
    extent.xmax = w;
  }
#else
  NOT_USED(visible_area);
#endif

  assert(pal_id != NULL_ObjectId);
  E(window_set_extent(0, pal_id, &extent));

  if (redraw)
  {
    DEBUG ("Forcing redraw of whole work area");
    E(window_force_redraw(0, pal_id, &extent));
  }
}

static void redraw_below_pos(PaletteData *const pal_data, Vertex start_pos)
{
  assert(pal_data != NULL);
  DEBUG ("Redrawing palette %p (object 0x%x) below %d,%d", (void *)pal_data,
         pal_data->my_object, start_pos.x, start_pos.y);

  assert(pal_data->my_object != NULL_ObjectId);

  Vertex const start_min = Vertex_mul(start_pos, pal_data->object_size);
  Vertex const layout_size = Vertex_mul(pal_data->grid_size, pal_data->object_size);

  if (start_pos.x != 0) {
    /* Use separate redraw rectangle for less than full width of row */
    DEBUG("Redrawing row %d from column %d", start_pos.y, start_pos.x);
    BBox redraw_box = {
      .xmin = X_BORDER + start_min.x,
      .xmax = X_BORDER + layout_size.x,
      .ymax = -Y_BORDER - start_min.y,
      .ymin = -Y_BORDER - start_min.y - pal_data->object_size.y
    };
    E(window_force_redraw(0, pal_data->my_object, &redraw_box));
    start_pos.y ++;
  }

  if (start_pos.y < pal_data->grid_size.y) {
    /* Redraw all rows below that on which object was inserted/removed */
    DEBUG("Redrawing all rows below %d (inclusive)", start_pos.y);
    BBox redraw_box = {
      .xmin = X_BORDER,
      .xmax = X_BORDER + layout_size.x,
      .ymax = -Y_BORDER - start_min.y,
      .ymin = -Y_BORDER - layout_size.y
    };
    E(window_force_redraw(0, pal_data->my_object, &redraw_box));
  }
}

static bool reformat_visible(PaletteData *const pal_data, BBox const *const visible_area,
                             ReformatAction const action, size_t const change_pos)
{
  /* Reformat window contents to fit given visible area coordinates and clip
     work area Y extent. 'change_pos' is the index of the object at which to
     start redraw if no. of columns unchanged. ReformatAction_OnlyIfWidthChanged
     means only reformat display if no. of columns changed. ReformatAction_Force
     means force reformat of whole display. Returns true if the display was
     re-formatted. */
  assert(pal_data != NULL);
  assert(visible_area->xmin <= visible_area->xmax);
  DEBUGF("Visible area will be xmin:%d xmax:%d (change_pos:%zu)\n", visible_area->xmin, visible_area->xmax,
         change_pos);

  /* Calculate number of columns for this window width */
  size_t new_num_columns = (size_t)(((visible_area->xmax - X_BORDER) - (visible_area->xmin + X_BORDER)) /
                                    pal_data->object_size.x);
  DEBUGF("Calculated no. of columns as %zu\n", new_num_columns);

  /* Some sanity checking */
  if (new_num_columns < 1) {
    new_num_columns = 1;
  }

  if (new_num_columns > pal_data->max_columns) {
    new_num_columns = pal_data->max_columns;
  }

  bool full_reformat = false;
  if (action == ReformatAction_Force) {
    full_reformat = true;
  } else if (new_num_columns != (size_t)pal_data->grid_size.x) {
    full_reformat = true;
  } else if (action == ReformatAction_OnlyIfWidthChanged) {
    return false; /* display not reformatted */
  }

  pal_data->grid_size.x = (int)new_num_columns;

  DEBUG("Reformatting window for width of %d (%zu objects across)", visible_area->xmax - visible_area->xmin,
        new_num_columns);

  /* Predict number of rows in display */
  if (!pal_data->numeric_order && pal_data->client_functions != NULL &&
      pal_data->client_functions->get_num_rows != NULL)
  {
    pal_data->grid_size.y = (int)pal_data->client_functions->get_num_rows(
                               pal_data->parent_editor, new_num_columns);
  }
  else
  {
    pal_data->grid_size.y = (int)((pal_data->num_indices + (size_t)new_num_columns - 1) /
                                  (size_t)new_num_columns);
  }
  if (pal_data->grid_size.y < 1) {
    pal_data->grid_size.y = 1;
  }

  DEBUGF("Predicted no. of rows: %d\n", pal_data->grid_size.y);

  /* If the number of columns has changed then our record of the grid location
  of the selected object will have been invalidated */
  if (full_reformat && pal_data->sel_index != NULL_DATA_INDEX)
  {
    pal_data->sel_pos = grid_from_index(pal_data, pal_data->sel_index);

    DEBUG("Selected object %zu is now at %d,%d", pal_data->sel_index,
          pal_data->sel_pos.x, pal_data->sel_pos.y);
  }

  /* Set appropriate window work area extent
     (and redraw whole window, if layout has changed). */
  set_extent(pal_data, visible_area, full_reformat);

  /* Redraw below the position specified by our caller if layout has not changed. */
  if (!full_reformat) {
    redraw_below_pos(pal_data, grid_from_index(pal_data, change_pos));
  }

  return true; /* display was reformatted */
}

static void redraw_loop(PaletteData *const pal_data, WimpRedrawWindowBlock *const block)
{
  assert(pal_data);
  assert(block);
  assert(BBox_is_valid(&block->redraw_area));
  const PaletteClientFuncts *const client_functions =
    pal_data->client_functions;

  if (client_functions != NULL && client_functions->start_redraw != NULL) {
    DEBUGF("Calling client function at start of redraw\n");
    client_functions->start_redraw(pal_data->parent_editor, pal_data->labels);
  }

  /* Find origin in absolute OS coordinates */
  Vertex const wa_origin = {
    block->visible_area.xmin - block->xscroll,
    block->visible_area.ymax - block->yscroll
  };

  Vertex const layout_size = Vertex_mul(
    pal_data->grid_size, pal_data->object_size);

  DEBUGF("wa_origin.x:%d wa_origin.y:%d width:%d, height:%d\n",
         wa_origin.x, wa_origin.y, layout_size.x, layout_size.y);

  int more = 1;
  while (more)
  {
    if (client_functions == NULL)
    {
      DEBUG ("No redraw because palette has no client");
      if (E(wimp_get_rectangle(block, &more)))
        more = 0;
      continue;
    }

    DEBUGF("redraw rectangle: xmin:%d (inc) ymin:%d (inc) xmax:%d (exc) ymax:%d (exc)\n",
      block->redraw_area.xmin - (block->visible_area.xmin - block->xscroll),
      block->redraw_area.ymin - (block->visible_area.ymax - block->yscroll),
      block->redraw_area.xmax - (block->visible_area.xmin - block->xscroll),
      block->redraw_area.ymax - (block->visible_area.ymax - block->yscroll));

    DEBUGF("width:%d height:%d\n",
      block->redraw_area.xmax - block->redraw_area.xmin,
      block->redraw_area.ymax - block->redraw_area.ymin);

    /* Find which rows/columns to redraw... */
    if (block->redraw_area.xmax <= wa_origin.x + X_BORDER ||
        block->redraw_area.xmin >= wa_origin.x + X_BORDER + layout_size.x ||
        block->redraw_area.ymin >= wa_origin.y - Y_BORDER ||
        block->redraw_area.ymax <= wa_origin.y - Y_BORDER - layout_size.y)
    {
      DEBUGF("No intersection with redraw rectangle\n");
      if (E(wimp_get_rectangle(block, &more)))
        more = 0;
      continue; /* redraw rectangle to left/right/above/below grid */
    }

    Vertex const coord_min = (Vertex){block->redraw_area.xmin - wa_origin.x - X_BORDER,
                                      wa_origin.y - Y_BORDER - block->redraw_area.ymax};

    Vertex const coord_max = (Vertex){block->redraw_area.xmax - wa_origin.x - X_BORDER,
                                      wa_origin.y - Y_BORDER - 1 - block->redraw_area.ymin};

    Vertex grid_min = Vertex_div(coord_min, pal_data->object_size);
    Vertex grid_max = Vertex_div(coord_max, pal_data->object_size);

    DEBUGF("redraw rectangle (grid coords): columns %d to %d & rows %d to %d\n",
           grid_min.x, grid_max.x, grid_min.y, grid_max.y);

    if (grid_min.y < 0)
      grid_min.y = 0;

    if (grid_max.y >= pal_data->grid_size.y)
      grid_max.y = pal_data->grid_size.y - 1;

    if (grid_min.x < 0)
      grid_min.x = 0;

    if (grid_max.x >= pal_data->grid_size.x)
      grid_max.x = pal_data->grid_size.x - 1;

    Vertex const image_min = Vertex_mul(grid_min, pal_data->object_size);

    BBox image_bbox, label_bbox;

    image_bbox.ymax = -Y_BORDER - image_min.y -OBJECT_Y_SPACER;
    image_bbox.ymin = image_bbox.ymax - pal_data->object_size.y + (OBJECT_Y_SPACER * 2);

    if (pal_data->labels)
    {
      assert(client_functions != NULL);
      if (!client_functions->overlay_labels)
      {
        image_bbox.ymin += NAME_HEIGHT;
        label_bbox.ymax = image_bbox.ymin - OBJECT_Y_SPACER;
        label_bbox.ymin = label_bbox.ymax - NAME_HEIGHT;
      }
      else
      {
        label_bbox.ymax = image_bbox.ymax;
        label_bbox.ymin = image_bbox.ymin;
      }
    }

    Vertex grid_pos;

    for (grid_pos.y = grid_min.y;
         grid_pos.y <= grid_max.y;
         grid_pos.y++)
    {
      image_bbox.xmin = X_BORDER + image_min.x + OBJECT_X_SPACER;
      assert(client_functions != NULL);
      image_bbox.xmax = image_bbox.xmin + client_functions->object_size.x;

      if (pal_data->labels)
      {
        label_bbox.xmin = image_bbox.xmin;
        label_bbox.xmax = image_bbox.xmax;
      }

      for (grid_pos.x = grid_min.x;
           grid_pos.x <= grid_max.x;
           grid_pos.x++)
      {
        size_t const index = index_from_grid(pal_data, grid_pos);

        if (index == NULL_DATA_INDEX)
        {
          DEBUG ("Premature end of row");
          break; /* assume it is the end of this row */
        }

        if (client_functions->redraw_object != NULL)
        {
          DEBUGF("Calling client function to redraw item %zu (bbox %d,%d,%d,%d)\n",
                 index, image_bbox.xmin, image_bbox.ymin, image_bbox.xmax,
                 image_bbox.ymax);

          client_functions->redraw_object(pal_data->parent_editor, wa_origin,
                                          &image_bbox, index_to_object(pal_data, index),
                                          pal_data->sel_index == index);
        }

        if (pal_data->labels && client_functions->redraw_label != NULL)
        {
          DEBUGF("Calling client function to redraw label %zu (bbox %d,%d,%d,%d)\n",
                 index, label_bbox.xmin, label_bbox.ymin,
                 label_bbox.xmax, label_bbox.ymax);

          client_functions->redraw_label(pal_data->parent_editor, wa_origin,
                                         &label_bbox, index_to_object(pal_data, index),
                                         pal_data->sel_index == index);
        }

        if (client_functions->selected_has_border &&
            pal_data->sel_index == index)
        {
          /* Plot a thick red rectangle around the selected object */
          DEBUGF("Drawing selection rectangle around item %zu\n", index);
          BBox plot_bbox;
          BBox_translate(&image_bbox, wa_origin, &plot_bbox);

          plot_set_wimp_col(WimpColour_Red);

          /* Draw line at bottom of image (thickness is OBJECT_Y_SPACER) */
          Vertex min = {
            plot_bbox.xmin - OBJECT_X_SPACER,
            plot_bbox.ymin - OBJECT_Y_SPACER
          };

          Vertex max = {
            plot_bbox.xmax + OBJECT_X_SPACER - 1,
            plot_bbox.ymin - 1
          };

          plot_fg_rect_2v(min, max);

          /* Draw line at top of image (thickness is OBJECT_Y_SPACER) */
          min.y = plot_bbox.ymax;
          max.y = plot_bbox.ymax + OBJECT_Y_SPACER - 1;

          plot_fg_rect_2v(min, max);

          /* Draw line at left side of image (thickness is OBJECT_X_SPACER) */
          min.y = plot_bbox.ymin;
          max.y = plot_bbox.ymax - 1;
          max.x = plot_bbox.xmin - 1;

          plot_fg_rect_2v(min, max);

          /* Draw line at right side of image (thickness is OBJECT_X_SPACER) */
          min.x = plot_bbox.xmax;
          max.x = plot_bbox.xmax + OBJECT_X_SPACER - 1;

          plot_fg_rect_2v(min, max);
        }

        /* Update the bounding boxes for the next column */
        if (pal_data->labels)
        {
          label_bbox.xmin += pal_data->object_size.x;
          label_bbox.xmax += pal_data->object_size.x;
        }

        image_bbox.xmin += pal_data->object_size.x;
        image_bbox.xmax += pal_data->object_size.x;

      } /* next column */

      /* Update the bounding boxes for the next row */
      if (pal_data->labels)
      {
        label_bbox.ymin -= pal_data->object_size.y;
        label_bbox.ymax -= pal_data->object_size.y;
      }

      image_bbox.ymin -= pal_data->object_size.y;
      image_bbox.ymax -= pal_data->object_size.y;

    } /* next row */

    /* Get next redraw rectangle */
    if (E(wimp_get_rectangle(block, &more))) {
      more = 0;
    }
  }

  if (client_functions != NULL && client_functions->end_redraw != NULL)
  {
    DEBUGF("Calling client function at end of redraw\n");
    client_functions->end_redraw(pal_data->parent_editor, pal_data->labels);
  }
}

static int redraw_window(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Process redraw events */
  NOT_USED(event_code);
  NOT_USED(id_block);
  const WimpRedrawWindowRequestEvent *const wrwre = (WimpRedrawWindowRequestEvent *)event;
  PaletteData *const pal_data = handle;
  WimpRedrawWindowBlock block;
  int more;

  assert(pal_data != NULL);
  DEBUG ("Request to redraw palette %p (object 0x%x)", (void *)pal_data,
         pal_data->my_object);
  assert(pal_data->my_object == id_block->self_id);

  block.window_handle = wrwre->window_handle;
  if (!E(wimp_redraw_window(&block, &more)) && more)
  {
    redraw_loop(pal_data, &block);
  }
  return 1; /* claim event */
}

static int open_window(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(handle);
  WimpOpenWindowRequestEvent *const wowre =
    (WimpOpenWindowRequestEvent *)event;
  assert(wowre != NULL);

  /*reformat_visible(handle, wowre->visible_area.xmin,
  wowre->visible_area.xmax, false);*/

  // Parent object may be dead by now, and we don't need it anyway
  E(toolbox_show_object(0, id_block->self_id,
             Toolbox_ShowObject_FullSpec, &wowre->visible_area,
             NULL_ObjectId, NULL_ComponentId));

  return 1; /* claim event */
}

static int tools_or_mode_changed(PaletteData *const pal_data)
{
  Vertex const desktop_size = Desktop_get_size_os();
  if (!calcmaxcolumns(pal_data, desktop_size.x))
    return 0; /* maximum width unchanged */

#if PALETTE_KEEP_VISIBLE_AREA
  WimpGetWindowStateBlock state;
  ON_ERR_RPT_RTN_V(window_get_wimp_handle(0, pal_data->my_object,
                   &state.window_handle), 0);

  ON_ERR_RPT_RTN_V(wimp_get_window_state(&state), 0);

  /* Set the new maximum width for the window (will shortly be reopened at
     new coordinates by the Wimp, at which point we can reformat the display) */
  set_extent(pal_data, &state.visible_area, false);
#else
  set_extent(pal_data, NULL, false);
#endif

  return 0; /* pass message on to any other handlers */
}

static int tools_changed_handler(WimpMessage *const message, void *const handle)
{
  NOT_USED(message);
  PaletteData *const pal_data = handle;
  assert(pal_data != NULL);

  return tools_or_mode_changed(pal_data);
}

static int mode_changed_handler(WimpMessage *const message, void *const handle)
{
  /* We need to recalculate the maximum horizontal extent of the
     palette window when the screen mode is changed or new window tool
     sprites are loaded. */
  NOT_USED(message);
  PaletteData *const pal_data = handle;
  assert(pal_data != NULL);

  Desktop_invalidate(); // can't predict order handlers are called
  return tools_or_mode_changed(pal_data);
}

static int mouse_click(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(id_block);
  PaletteData *const pal_data = handle;
  const WimpMouseClickEvent *const mouse_click = (WimpMouseClickEvent *)event;
  WimpGetWindowStateBlock state;

  assert(pal_data != NULL);
  DEBUGF("Mouse click on palette %p (object 0x%x) buttons=%d pos=%d,%d\n",
         (void *)pal_data, pal_data->my_object, mouse_click->buttons,
         mouse_click->mouse_x, mouse_click->mouse_y);

  assert(id_block->self_id == pal_data->my_object);

  if (pal_data->client_functions == NULL)
    return 1; /* palette is empty */

  if (!TEST_BITS(mouse_click->buttons, Wimp_MouseButtonSelect) &&
      (!TEST_BITS(mouse_click->buttons, Wimp_MouseButtonMenu) ||
       !pal_data->client_functions->menu_selects))
    return 1; /* not interested in other mouse buttons */

  /*
     Find row, column from pointer's OS coordinates
   */

  state.window_handle = mouse_click->window_handle;
  ON_ERR_RPT_RTN_V(wimp_get_window_state(&state), 1);

  Vertex const wa_origin = {
    (state.visible_area.xmin - state.xscroll) + X_BORDER,
    (state.visible_area.ymax - state.yscroll) - Y_BORDER
  };

  Vertex const layout_size = Vertex_mul(pal_data->grid_size, pal_data->object_size);

  if (mouse_click->mouse_x < wa_origin.x ||
      mouse_click->mouse_x >= wa_origin.x + layout_size.x ||
      mouse_click->mouse_y >= wa_origin.y ||
      mouse_click->mouse_y < wa_origin.y - layout_size.y)
  {
    DEBUG("mouse_click outside grid");
    return 1;  /* mouse_click to left/right/above/below grid */
  }
  Vertex const click_pos = (Vertex){mouse_click->mouse_x - wa_origin.x,
    (wa_origin.y - 1) - mouse_click->mouse_y};

  Vertex const grid_pos = Vertex_div(click_pos, pal_data->object_size);
  DEBUG("row=%d col=%d\n", grid_pos.y, grid_pos.x);

  size_t const index = index_from_grid(pal_data, grid_pos);
  if (index != NULL_DATA_INDEX &&
      index != pal_data->sel_index) {
    select(pal_data, grid_pos, index, false, true, true);
  }

  return 1; /* claim event */
}

static bool reformat(PaletteData *const pal_data, ReformatAction const action,
                     size_t const change_pos)
{
  /* Reformat contents to fit current window width and clip the work area
     Y extent. 'change_pos' is the index of the object at which to start redraw
     if no. of columns is unchanged. ReformatAction_OnlyIfWidthChanged means only
     reformat display if no. of columns changed. ReformatAction_Force means force
     reformat of whole display. Returns true if the display was re-formatted. */
  assert(pal_data != NULL);
  DEBUG("Reformatting palette %p (object 0x%x) for current window width",
        (void *)pal_data, pal_data->my_object);

  WimpGetWindowStateBlock state;
  ON_ERR_RPT_RTN_V(window_get_wimp_handle(0, pal_data->my_object,
                   &state.window_handle), false);

  ON_ERR_RPT_RTN_V(wimp_get_window_state(&state), false);

  bool const reformatted = reformat_visible(
    pal_data, &state.visible_area, action, change_pos);

  if (reformatted && action != ReformatAction_OnlyIfWidthChanged)
  {
    if (object_is_showing(pal_data->my_object))
    {
      DEBUG("Re-opening palette window");

      // Parent object may be dead by now, and we don't need it anyway
      E(toolbox_show_object(0, pal_data->my_object,
                 Toolbox_ShowObject_FullSpec, &state.visible_area,
                 NULL_ObjectId, NULL_ComponentId));
    }
  }

  return reformatted;
}

static int close_window(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  /* We need to update the session's display flags when the user closes the
     palette window */
  NOT_USED(event_code);
  NOT_USED(id_block);
  WimpCloseWindowRequestEvent *const wcwre =
    (WimpCloseWindowRequestEvent *)event;
  PaletteData *const pal_data = handle;

  E(wimp_close_window(&wcwre->window_handle));
  Editor_pal_was_hidden(pal_data->parent_editor);

  return 1; /* claim event */
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  assert(event_code == Window_AboutToBeShown);
  NOT_USED(event_code);
  NOT_USED(id_block);
  const WindowAboutToBeShownEvent *const atbse =
    (WindowAboutToBeShownEvent *)event;
  PaletteData *const pal_data = handle;

  /* Correctly format the display before the Toolbox opens this window */
  if (atbse->show_type == Toolbox_ShowObject_FullSpec)
    reformat_visible(pal_data, &atbse->info.full_spec.visible_area,
                     ReformatAction_OnlyIfWidthChanged, 0);
  else
    reformat(pal_data, ReformatAction_OnlyIfWidthChanged, 0);

  if (!pal_data->is_showing) {
    if (pal_data->client_functions != NULL &&
        pal_data->client_functions->animate != NULL)
    {
      int now;
      EF(os_read_monotonic_time(&now));
      E(scheduler_register(anim_cb, pal_data, now, SchedulerPriority_Min));
    }
    pal_data->is_showing = true;
  }

  return 1; /* claim event */
}

static int has_been_hidden(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  assert(event_code == Window_HasBeenHidden);
  NOT_USED(event_code);
  NOT_USED(id_block);
  NOT_USED(event);
  PaletteData *const pal_data = handle;

  if (pal_data->is_showing) {
    Editor_pal_was_hidden(pal_data->parent_editor);

    if (pal_data->client_functions != NULL &&
        pal_data->client_functions->animate != NULL)
    {
      scheduler_deregister(anim_cb, pal_data);
    }
    pal_data->is_showing = false;
  }

  return 1; /* claim event */
}

static void set_labels_flag(PaletteData *const pal_data, bool const show_labels)
{
  DEBUG("%s labels for palette object %p", show_labels ? "Enable" :
        "Disable", (void *)pal_data);
  assert(pal_data != NULL);

  if (show_labels != pal_data->labels)
  {
    pal_data->labels = show_labels;
    update_menus(pal_data);

    /* Recalculate the total height of each object
       (including spacing and label, if any) */
    pal_data->object_size.y = OBJECT_Y_SPACER * 2;
    if (pal_data->client_functions != NULL)
    {
      pal_data->object_size.y += pal_data->client_functions->object_size.y;

      /* Allow extra room for the labels, if enabled and not overlaid on icons
       */
      if (!pal_data->client_functions->overlay_labels && show_labels)
      {
        DEBUG ("Allowing extra room for labels");
        pal_data->object_size.y += NAME_HEIGHT;
      }
    }
    DEBUG ("New height of each item is %d", pal_data->object_size.y);

    if (pal_data->client_functions != NULL &&
        pal_data->client_functions->overlay_labels)
    {
      static BBox extent = {SHRT_MIN, SHRT_MIN, SHRT_MAX, SHRT_MAX};
      E(window_force_redraw(0, pal_data->my_object, &extent));
    }
    else
    {
      reformat(pal_data, ReformatAction_Force, 0);
    }
  }
}

static void set_ordered_flag(PaletteData *const pal_data, bool const numeric_order)
{
  DEBUG("%s ordered layout for palette object %p", numeric_order ? "Enable" :
        "Disable", (void *)pal_data);
  assert(pal_data != NULL);

  if (numeric_order != pal_data->numeric_order)
  {
    pal_data->numeric_order = numeric_order;
    update_menus(pal_data);

    Vertex const desktop_size = Desktop_get_size_os();
    calcmaxcolumns(pal_data, desktop_size.x);
    reformat(pal_data, ReformatAction_Force, 0);
  }
}

static int user_event(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event);
  assert(id_block);
  PaletteData *const pal_data = handle;
  assert(pal_data);

  /* Careful - handler is called for unclaimed toolbox events on any object */
  if (id_block->self_id != pal_data->my_object &&
      id_block->ancestor_id != pal_data->my_object)
    return 0; /* event not for us - pass it on */

  /* Handle hotkey/menu selection events */

  if (pal_data->client_functions != NULL) {
    switch (event_code) {
    case EVENT_PALETTE_DELETE:
      if (pal_data->client_functions->delete != NULL &&
          pal_data->sel_index != NULL_DATA_INDEX) {
        pal_data->client_functions->delete(
          pal_data->parent_editor, index_to_object(pal_data, pal_data->sel_index));
      }
      return 1; /* claim event */

    case EVENT_PALETTE_DELETE_ALL:
      if (pal_data->client_functions->delete_all) {
        pal_data->client_functions->delete_all(pal_data->parent_editor);
      }
      return 1; /* claim event */

    case EVENT_PALETTE_RELOAD:
      if (pal_data->client_functions->reload) {
        pal_data->client_functions->reload(pal_data->parent_editor);
      }
      return 1; /* claim event */

    case EVENT_PALETTE_EDIT:
      if (pal_data->client_functions->edit) {
        pal_data->client_functions->edit(pal_data->parent_editor);
      }
      return 1; /* claim event */

    case EVENT_PALETTE_TOGGLE_LABELS:
      set_labels_flag(pal_data, !pal_data->labels);
      return 1; /* claim event */

    case EVENT_PALETTE_TOGGLE_ORDER:
      set_ordered_flag(pal_data, !pal_data->numeric_order);
      return 1; /* claim event */

    default:
      return 0;
    }
  }
  return 1; /* claim event */
}

static void setselrowcol(PaletteData *const pal_data)
{
  assert(pal_data != NULL);
  DEBUG ("Recalculating position of selected item in palette %p (object 0x%x)",
         (void *)pal_data, pal_data->my_object);

  pal_data->sel_pos.y = (int)pal_data->sel_index / pal_data->grid_size.x;
  pal_data->sel_pos.x = (int)pal_data->sel_index % pal_data->grid_size.x;

  DEBUG ("Selected item %zu is now at %d,%d", pal_data->sel_index,
         pal_data->sel_pos.x, pal_data->sel_pos.y);
}

static void reset_vars(PaletteData *const pal_data)
{
  assert(pal_data != NULL);

  pal_data->num_indices = 0;
  pal_data->grid_size = (Vertex){0, 0};
  pal_data->object_size = (Vertex){OBJECT_X_SPACER * 2, OBJECT_Y_SPACER * 2};
  pal_data->sel_pos = (Vertex){0, 0};
  pal_data->sel_index = NULL_DATA_INDEX;
  pal_data->client_functions = NULL;
}

static bool update_title(PaletteData *const pal_data)
{
  char *new_title;

  assert(pal_data != NULL);
  DEBUG ("Updating title of palette %p (object 0x%x)", (void *)pal_data,
         pal_data->my_object);

  assert(pal_data->parent_editor != NULL);
  new_title = msgs_lookup_subn(pal_data->client_functions == NULL ? "PalTitleN" :
              pal_data->client_functions->title_msg, 1,
              pathtail(Session_get_filename(Palette_get_session(pal_data)), 1));

  assert(pal_data->my_object != NULL_ObjectId);
  return !E(window_set_title(0, pal_data->my_object, new_title));
}

static void do_finalise(PaletteData *const pal_data, bool const reinit)
{
  /* Tell the previous client (if any) to tidy up any state associated
     with this palette */
  assert(pal_data != NULL);
  DEBUG ("Finalising client of palette %p (object 0x%x)", (void *)pal_data,
         pal_data->my_object);

  if (pal_data->client_functions == NULL) {
    DEBUG ("Palette has no client");
    return; /* no client registered */
  }

  DEBUG ("Finalising client %p ('%s')", (void *)pal_data->client_functions,
         pal_data->client_functions->title_msg);

  if (pal_data->is_showing) {
    if (pal_data->client_functions->animate != NULL) {
      scheduler_deregister(anim_cb, pal_data);
    }
  }

  if (pal_data->client_functions->finalise != NULL) {
    DEBUG ("Calling client finalisation function");
    pal_data->client_functions->finalise(pal_data, pal_data->parent_editor, reinit);
  } else if (!reinit) {
    /* Our default action is to detach any menu */
    ObjectId menu_id;
    if (!E(window_get_menu(0, pal_data->my_object, &menu_id)) &&
        menu_id != NULL_ObjectId)
    {
      DEBUG ("Detaching palette menu 0x%x", menu_id);
      E(window_set_menu(0, pal_data->my_object, NULL_ObjectId));
    } else {
      DEBUG ("Palette currently has no menu");
    }
  }

  /* Forget the client */
  pal_data->client_functions = NULL;
}

static bool do_init(PaletteData *const pal_data,
  const PaletteClientFuncts *client_functions, bool const reinit)
{
  assert(pal_data != NULL);
  DEBUG ("Initialising new client of palette %p (object 0x%x)", (void *)pal_data,
         pal_data->my_object);

  reset_vars(pal_data);

  if (client_functions != NULL)
  {
    Vertex default_selected = {0, 0};
    if (client_functions->initialise != NULL)
    {
      DEBUG ("Calling client initialisation function");

      if (!client_functions->initialise(pal_data, pal_data->parent_editor,
                                        &pal_data->num_indices, reinit))
      {
        DEBUG ("Client of palette failed to initialise");
        return false; /* failure */
      }
    }

    pal_data->client_functions = client_functions;

    pal_data->object_size = Vertex_add(client_functions->object_size,
      (Vertex){ OBJECT_X_SPACER * 2, OBJECT_Y_SPACER * 2 });

    /* Allow extra room for the labels, if enabled and not overlaid on icons */
    if (!client_functions->overlay_labels && pal_data->labels)
    {
      DEBUG ("Allowing extra room for labels");
      pal_data->object_size.y += NAME_HEIGHT;
    }
    DEBUG ("Dimensions of each item are %d,%d", pal_data->object_size.x,
           pal_data->object_size.y);

    Vertex const desktop_size = Desktop_get_size_os();
    calcmaxcolumns(pal_data, desktop_size.x);
    reformat(pal_data, ReformatAction_Force, 0);

    size_t index = index_from_grid(pal_data, default_selected);
    if (index != NULL_DATA_INDEX)
      select(pal_data, default_selected, index, true, false,
                      false);


    if (pal_data->is_showing) {
      if (pal_data->client_functions->animate) {
        int now;
        EF(os_read_monotonic_time(&now));
        E(scheduler_register(anim_cb, pal_data, now, SchedulerPriority_Min));
      }
    }
  }

  if (!update_title(pal_data))
  {
    do_finalise(pal_data, false);
    return false; /* failure */
  }

  return true; /* success */
}

/* ---------------- Public functions ---------------- */

bool Palette_init(PaletteData *const pal_data, Editor *parent_editor)
{
  DEBUG("Creating Palette for editor %p", (void *)parent_editor);

  *pal_data = (PaletteData){
    .mode_change = false,
    .tools_change = false,
    .numeric_order = false,
    .labels = true,
    .parent_editor = parent_editor,
  };

  reset_vars(pal_data);

  if (E(toolbox_create_object(0, "Palette", &pal_data->my_object)))
  {
    DEBUG ("Failed to create Palette object");
    return false;
  }
  DEBUG("Palette object id is 0x%x", pal_data->my_object);

  bool success = !E(toolbox_set_client_handle(0, pal_data->my_object, pal_data));

  static const struct {
    int event_code;
    WimpEventHandler *handler;
  } wimp_handlers[] = {
    {Wimp_ERedrawWindow, redraw_window},
    {Wimp_EMouseClick, mouse_click},
    {Wimp_EOpenWindow, open_window},
    {Wimp_ECloseWindow, close_window},
  };

  for (size_t i = 0; success && (i < ARRAY_SIZE(wimp_handlers)); ++i)
  {
    success = !E(event_register_wimp_handler(
                  pal_data->my_object, wimp_handlers[i].event_code,
                  wimp_handlers[i].handler, pal_data));
  }

  if (success)
  {
    success = !E(event_register_message_handler(
                  Wimp_MModeChange, mode_changed_handler, pal_data));
    pal_data->mode_change = success;
  }

  if (success)
  {
    success = !E(event_register_message_handler(
                  Wimp_MToolsChanged, tools_changed_handler, pal_data));
    pal_data->tools_change = success;
  }

  if (success)
  {
    success = !E(event_register_toolbox_handler(-1, -1, user_event, pal_data));
    pal_data->user_event = success;
  }

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } tbox_handlers[] = {
    {Window_AboutToBeShown, about_to_be_shown},
    {Window_HasBeenHidden, has_been_hidden},
  };

  for (size_t i = 0; success && (i < ARRAY_SIZE(tbox_handlers)); ++i)
  {
    success = !E(event_register_toolbox_handler(
                  pal_data->my_object, tbox_handlers[i].event_code,
                  tbox_handlers[i].handler, pal_data));
  }

  if (!success)
  {
    Palette_destroy(pal_data);
  }

  return success;
}

bool Palette_is_showing(PaletteData const *const pal_data)
{
  assert(pal_data != NULL);
  return pal_data->is_showing;
}

EditSession *Palette_get_session(PaletteData const *const pal_data)
{
  /* This function compensates for the inconvenience to child objects of having
     made ourselves ancestor in place of the main edit_win window. */
  assert(pal_data != NULL);
  return Editor_get_session(pal_data->parent_editor);
}

void Palette_set_menu(PaletteData *const pal_data, ObjectId const menu_id)
{
  assert(pal_data != NULL);
  DEBUG("Setting palette menu 0x%x", menu_id);
  E(window_set_menu(0, pal_data->my_object, menu_id));
}

void Palette_show(PaletteData *const pal_data, EditWin *const edit_win)
{
  assert(pal_data != NULL);
  EditWin_show_window_aligned_right(edit_win, pal_data->my_object, get_preferred_width(pal_data));
}

void Palette_hide(PaletteData *const pal_data)
{
  DEBUG("Hiding palette");
  assert(pal_data != NULL);
  E(DeIconise_hide_object(0, pal_data->my_object));
}

void Palette_reveal(PaletteData *const pal_data)
{
  DEBUG("Bringing palette to front");
  assert(pal_data != NULL);
  E(DeIconise_show_object(0, pal_data->my_object,
    Toolbox_ShowObject_Default, NULL, NULL_ObjectId, NULL_ComponentId));
}

void Palette_destroy(PaletteData *const pal_data)
{
  assert(pal_data != NULL);
  DEBUG ("Palette %p (object 0x%x) was deleted", (void *)pal_data, pal_data->my_object);

  do_finalise(pal_data, false);

  if (pal_data->user_event)
    E(event_deregister_toolbox_handler(-1, -1, user_event, pal_data));

  if (pal_data->mode_change)
    E(event_deregister_message_handler(Wimp_MModeChange,
               mode_changed_handler, pal_data));

  if (pal_data->tools_change)
    E(event_deregister_message_handler(Wimp_MToolsChanged,
               tools_changed_handler, pal_data));

  E(remove_event_handlers_delete(pal_data->my_object));
}

void Palette_object_moved(PaletteData *const pal_data, size_t const old_object, size_t const new_object)
{
  DEBUG ("Palette object %p notified that item moved from %zu to %zu",
         (void *)pal_data, old_object, new_object);
  assert(pal_data != NULL);

  size_t const old_index = p_object_to_index(pal_data, old_object);
  size_t const new_index = p_object_to_index(pal_data, new_object);

  if (pal_data->sel_index != NULL_DATA_INDEX) {
    /* Adjust the index of the selected object, according to whether it was
       before or after the object that was moved. */
    if (pal_data->sel_index == old_index)
    {
      pal_data->sel_index = new_index;
    }
    else
    {
      if (old_index < pal_data->sel_index)
        pal_data->sel_index --;

      if (new_index <= pal_data->sel_index)
        pal_data->sel_index ++;
    }
    setselrowcol(pal_data); /* find new grid location */
  }

  Vertex start_pos = grid_from_index(pal_data, old_index);
  Vertex const new_pos = grid_from_index(pal_data, new_index);
  if (new_pos.y < start_pos.y)
  {
    start_pos.y = new_pos.y;
    start_pos.x = new_pos.x;
  }
  else if (new_pos.y == start_pos.y && new_pos.x < start_pos.x)
  {
    start_pos.x = new_pos.x;
  }

  redraw_below_pos(pal_data, start_pos);
}

void Palette_redraw_object(PaletteData *const pal_data, size_t object)
{
  assert(pal_data != NULL);
  DEBUG ("Redrawing item %zu in palette %p (object 0x%x)", object, (void *)pal_data, pal_data->my_object);
  assert(pal_data->client_functions != NULL);
  size_t const index = p_object_to_index(pal_data, object);

  // FIXME: only draw inside
  Vertex const grid_min = grid_from_index(pal_data, index);
  Vertex const object_min = Vertex_mul(grid_min, pal_data->object_size);

  int window_handle;
  if (E(window_get_wimp_handle(0, pal_data->my_object, &window_handle)))
  {
    return;
  }

  WimpRedrawWindowBlock block = {
    .window_handle = window_handle
  };
  block.visible_area.xmin = X_BORDER + object_min.x + OBJECT_X_SPACER;
  block.visible_area.xmax = block.visible_area.xmin + pal_data->client_functions->object_size.x;
  block.visible_area.ymax = -Y_BORDER - object_min.y -OBJECT_Y_SPACER;
  block.visible_area.ymin = block.visible_area.ymax - pal_data->client_functions->object_size.y;

  int more;
  if (!E(wimp_update_window(&block, &more)) && more) {
    redraw_loop(pal_data, &block);
  }
}

void Palette_redraw_name(PaletteData *const pal_data, size_t const object)
{
  assert(pal_data != NULL);
  if (!pal_data->labels)
  {
    return;
  }

  size_t const index = p_object_to_index(pal_data, object);
  DEBUG ("Redrawing label %zu in palette %p (object 0x%x)", index, (void *)pal_data, pal_data->my_object);
  if (pal_data->client_functions->overlay_labels)
  {
    Palette_redraw_object(pal_data, index);
  }
  else
  {
    BBox redraw_box = bbox_for_object(pal_data, grid_from_index(pal_data, index));
    redraw_box.ymax = redraw_box.ymin + NAME_HEIGHT;
    E(window_force_redraw(0, pal_data->my_object, &redraw_box));
  }
}

void Palette_object_deleted(PaletteData *const pal_data, size_t const object)
{
  /* Notification that an object (at position 'index') has been deleted, so we
     must reformat our display. Call with index == NULL_DATA_INDEX if all
     objects have been deleted simultaneously. */
  assert(pal_data != NULL);
  DEBUG ("Palette object %p notified that item %zu was deleted", (void *)pal_data,
         object);
  size_t const index = p_object_to_index(pal_data, object);

  if (pal_data->num_indices == 0)
    return; /* nothing to do! */

  if (index == NULL_DATA_INDEX)
    pal_data->num_indices = 0; /* all objects were deleted */
  else
    pal_data->num_indices --; /* one object was deleted */

  if (pal_data->max_columns > pal_data->num_indices)
    pal_data->max_columns = HIGHEST(pal_data->num_indices, 1);

  /* Prevent reformat_visible from recalculating the grid coordinates of
     the selected object in its old position... */
  size_t const old_sel_index = pal_data->sel_index;
  pal_data->sel_index = NULL_DATA_INDEX;

  /* Reformat the display and redraw it below the index of the deleted object */
  reformat(pal_data, ReformatAction_Default, index == NULL_DATA_INDEX ? 0 : index);

  if (index == NULL_DATA_INDEX || pal_data->num_indices == 0)
  {
    pal_data->sel_index = NULL_DATA_INDEX;
  }
  else if (index < old_sel_index)
  {
    /* Compensate for removal of an earlier object */
    pal_data->sel_index = old_sel_index - 1;
    setselrowcol(pal_data); /* find new grid location */
  }
  else if (index == old_sel_index)
  {
    /* If we have deleted the selected object then the old object index
       may now be off the end of the list */
    if (index >= pal_data->num_indices - 1)
    {
      pal_data->sel_index = pal_data->num_indices - 1;
      setselrowcol(pal_data); /* find grid location of selected */
    }
  }
  update_menus(pal_data);
}

void Palette_object_added(PaletteData *const pal_data, size_t const object)
{
  /* Notification that an object is being added so that we can reformat our
     display. */
  DEBUG ("Palette object %p notified that item was added at %zu", (void *)pal_data,
         object);
  assert(pal_data != NULL);
  size_t const index = p_object_to_index(pal_data, object);

  pal_data->num_indices ++;
  assert(pal_data->num_indices > 0);

  if (index <= pal_data->sel_index) {
    /* Bump up selection (object added prior to it) */
    pal_data->sel_index++;
  }

  if (pal_data->max_columns < pal_data->num_indices) {
    /* May be possible to expand maximum width if desktop wide enough */
    Vertex const desktop_size = Desktop_get_size_os();
    calcmaxcolumns(pal_data, desktop_size.x);
  }
  reformat(pal_data, ReformatAction_Default, index);
}

void Palette_update_title(PaletteData *const pal_data)
{
  assert(pal_data != NULL);
  update_title(pal_data);
}

void Palette_reinit(PaletteData *const pal_data)
{
  DEBUG("Palette object %p notified of new tiles set", (void *)pal_data);
  assert(pal_data != NULL);

  /* Finalise and then reinitialise the client of this palette, so that the no.
     of objects is re-evaluated, new thumbnail sprites are generated and the
     palette layout is reformatted. */
  const PaletteClientFuncts *const client_functions = pal_data->client_functions;
  do_finalise(pal_data, true);
  do_init(pal_data, client_functions, true);
}

bool Palette_get_labels_flag(PaletteData const *const pal_data)
{
  DEBUG("Labels enable state queried for palette object %p", (void *)pal_data);
  assert(pal_data != NULL);

  DEBUG ("Labels are %senabled", pal_data->labels ? "" : "not ");
  return pal_data->labels;
}

bool Palette_get_ordered_flag(PaletteData const *const pal_data)
{
  DEBUG("Item order queried for palette object %p", (void *)pal_data);
  assert(pal_data != NULL);

  DEBUG ("Items are %sin order", pal_data->numeric_order ? "" : "not ");
  return pal_data->numeric_order;
}

bool Palette_register_client(PaletteData *const pal_data,
                             const PaletteClientFuncts *client_functions)
{
  DEBUG ("Registering client %p ('%s') with palette object %p",
         (void *)client_functions, client_functions->title_msg,
         (void *)pal_data);
  assert(pal_data != NULL);

  do_finalise(pal_data, false);

  return do_init(pal_data, client_functions, false);
}

size_t Palette_get_selection(PaletteData const *const pal_data)
{
  DEBUG ("Selected item queried for palette object %p", (void *)pal_data);
  assert(pal_data != NULL);

  size_t const sel_index = index_to_object(pal_data, pal_data->sel_index);
  DEBUG ("Selected item is %zu", sel_index);
  return sel_index;
}

void Palette_set_selection(PaletteData *const pal_data, size_t const object)
{
  DEBUG ("About to select item %zu in palette object %p", object, (void *)pal_data);
  assert(pal_data != NULL);

  size_t const index = p_object_to_index(pal_data, object);
  if (index != pal_data->sel_index) {
    select(pal_data, grid_from_index(pal_data, index), index, true, true, true);
  }
}
