/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Status bar of main editing window
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
#include "stdio.h"
#include <string.h>
#include <assert.h>
#include "stdlib.h"

#include "toolbox.h"
#include "gadgets.h"
#include "window.h"

#include "err.h"
#include "msgtrans.h"
#include "Macros.h"
#include "Debug.h"

#include "EditWin.h"
#include "statusbar.h"
#include "MapCoord.h"

enum {
  LEFT_BORDER = 8,
  TOP_BORDER = 8,
  COORDS_WIDTH = 216,
  NUMSEL_WIDTH = 108,
  ZOOM_WIDTH = 76,
  ANGLE_WIDTH = 44,
  MODE_WIDTH = 144,
  SPACER = 4,
  HEIGHT = 68,
  MIN_HINT_WIDTH = 32,
};

/* --------------------- Gadgets -------------------- */

enum {
  STATUSBAR_MODE   = 0x13,
  STATUSBAR_COORDS = 0x11,
  STATUSBAR_HINT   = 0x14,
  STATUSBAR_ZOOM   = 0x15,
  STATUSBAR_ANGLE  = 0x16,
};

/* ---------------- Public functions ---------------- */

void StatusBar_init(StatusBarData *const statusbar_data, ObjectId const id)
{
  DEBUG("Initializing status bar object 0x%x", id);
  assert(statusbar_data != NULL);
  assert(id != NULL_ObjectId);

  *statusbar_data = (StatusBarData){
    .my_object = id,
    .field_width = -1,
    .window_width = -1, /* force re-format of status bar */
  };
}

int StatusBar_get_height(void)
{
  return HEIGHT;
}

void StatusBar_show_mode(StatusBarData *const statusbar_data, char const *const mode)
{
  assert(statusbar_data != NULL);
  DEBUG("Updating mode display on status bar %d (now %s)",
        statusbar_data->my_object, mode);

  E(displayfield_set_value(0, statusbar_data->my_object, STATUSBAR_MODE, mode));
}

void StatusBar_show_hint(StatusBarData *const statusbar_data, char const *const hint)
{
  assert(statusbar_data != NULL);
  DEBUG("Updating hint on status bar %d (now %s)",
        statusbar_data->my_object, hint);

  /* First read current value and only update if new value is different */
  char buf[128];
  E(displayfield_get_value(0, statusbar_data->my_object, STATUSBAR_HINT,
                                    buf, sizeof(buf), NULL));

  if (strcmp(buf, hint) != 0) {
    buf[0] = '\0';
    strncat(buf, hint, sizeof(buf)-1);
    E(displayfield_set_value(0, statusbar_data->my_object,
                                      STATUSBAR_HINT, buf));

    /* Set help too in case the full message isn't visible */
    E(gadget_set_help_message(0, statusbar_data->my_object,
                                      STATUSBAR_HINT, buf));
  }
}

void StatusBar_show_zoom(StatusBarData *const statusbar_data, int const zoom_factor)
{
  assert(statusbar_data != NULL);
  char token[32];
  DEBUG("Updating zoom display on status bar %d (now %d)",
        statusbar_data->my_object, zoom_factor);

  sprintf(token, "Zoom%d", zoom_factor);
  E(displayfield_set_value(0, statusbar_data->my_object, STATUSBAR_ZOOM,
                           msgs_lookup(token)));
}

void StatusBar_show_angle(StatusBarData *const statusbar_data, MapAngle const angle)
{
  assert(statusbar_data != NULL);
  char token[32];
  DEBUG("Updating angle display on status bar %d (now %d)",
        statusbar_data->my_object, angle);

  sprintf(token, "Angle%d", angle);
  E(displayfield_set_value(0, statusbar_data->my_object, STATUSBAR_ANGLE,
                           msgs_lookup(token)));
}

void StatusBar_show_pos(StatusBarData *const statusbar_data,
  bool const out, MapPoint const map_pos)
{
  assert(statusbar_data != NULL);
  char string[24] = "";

  assert(statusbar_data->field_width >= 0);
  if (out) {
    DEBUGF("Updating coordinates on status bar %d (out of window)\n",
      statusbar_data->my_object);

    assert((statusbar_data->field_width * 2) + 1 < (int)sizeof(string));
    strncat(string, "----------", (size_t)statusbar_data->field_width);
    strcat(string, ",");
    strncat(string, "----------", (size_t)statusbar_data->field_width);
  } else {
    DEBUGF("Updating coordinates on status bar %d (now %"PRIMapCoord",%"PRIMapCoord")\n",
      statusbar_data->my_object, map_pos.x, map_pos.y);

    sprintf(string, "%*.*"PRIMapCoord",%*.*"PRIMapCoord,
             statusbar_data->field_width, statusbar_data->field_width, map_pos.x,
             statusbar_data->field_width, statusbar_data->field_width, map_pos.y);
  }

  E(displayfield_set_value(0, statusbar_data->my_object, STATUSBAR_COORDS,
                                    string));
}

void StatusBar_reformat(StatusBarData *const statusbar_data,
  int const window_width, int const field_width)
{
  assert(statusbar_data != NULL);
  /* May call with -ve window_width or field_width to keep existing value */

  DEBUG("Reformatting status bar %d for window width %d & coords width %d",
        statusbar_data->my_object, window_width, field_width);

  BBox new_bbox = {
    .ymax = -TOP_BORDER,
    .ymin = -HEIGHT + TOP_BORDER
  };

  if (statusbar_data->field_width != field_width && field_width >= 0)
  {
    DEBUG("Complete reformat (coords width changed)");
    statusbar_data->field_width = field_width;

    new_bbox.xmin = LEFT_BORDER;
    new_bbox.xmax = new_bbox.xmin + (statusbar_data->field_width * 2 + 3) * 16;
    E(gadget_move_gadget(0, statusbar_data->my_object,
                                  STATUSBAR_COORDS, &new_bbox));

    new_bbox.xmin = new_bbox.xmax + SPACER;
    new_bbox.xmax = new_bbox.xmin + ZOOM_WIDTH;
    E(gadget_move_gadget(0, statusbar_data->my_object,
                                  STATUSBAR_ZOOM, &new_bbox));

    new_bbox.xmin = new_bbox.xmax + SPACER;
    new_bbox.xmax = new_bbox.xmin + ANGLE_WIDTH;
    E(gadget_move_gadget(0, statusbar_data->my_object,
                                  STATUSBAR_ANGLE, &new_bbox));

    new_bbox.xmin = new_bbox.xmax + SPACER;
    new_bbox.xmax = new_bbox.xmin + MODE_WIDTH;
    E(gadget_move_gadget(0, statusbar_data->my_object,
                                  STATUSBAR_MODE, &new_bbox));

    new_bbox.xmin = new_bbox.xmax + SPACER;
  } else {
    if (window_width == statusbar_data->window_width ||
        statusbar_data->field_width < 0)
      return;

    DEBUG("Adjusting extent of hint only");
    new_bbox.xmin = LEFT_BORDER + (statusbar_data->field_width * 2 + 3) * 16 +
                    SPACER + ZOOM_WIDTH + SPACER + ANGLE_WIDTH + SPACER +
                    MODE_WIDTH + SPACER;
  }
  if (window_width >= 0)
    statusbar_data->window_width = window_width;

  new_bbox.xmax = HIGHEST(statusbar_data->window_width - SPACER,
                          new_bbox.xmin + MIN_HINT_WIDTH);
  E(gadget_move_gadget(0, statusbar_data->my_object,
                                STATUSBAR_HINT, &new_bbox));
}

void StatusBar_show(StatusBarData *const statusbar_data, ObjectId const parent_id)
{
  E(toolbox_show_object(0, statusbar_data->my_object,
                                 Toolbox_ShowObject_Default, NULL,
                                 parent_id,
                                 NULL_ComponentId));
}

void StatusBar_hide(StatusBarData *const statusbar_data)
{
  E(toolbox_hide_object(0, statusbar_data->my_object));
}
