/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Objects grid layout
 *  Copyright (C) 2022 Christopher Bazley
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

#include "Macros.h"
#include "debug.h"

#include "Obj.h"
#include "Hill.h"
#include "View.h"
#include "MapCoord.h"
#include "ObjLayout.h"

enum {
  MAP_GRID_SIZE_LOG2 =  MAP_COORDS_LIMIT_LOG2 - Obj_SizeLog2,
  MAP_GRID_SIZE = 1l << MAP_GRID_SIZE_LOG2, // 0x80 0000
};

static MapArea swap_area_limits_for_rot(MapAngle const angle, MapArea const *const area)
{
  /* Just ensure the correct order of minimum and maximum coordinates. */
  assert(area);

  switch (angle) {
  case MapAngle_Count:
  case MapAngle_North:
    break;
  case MapAngle_East:
    return (MapArea){.min = {area->min.x, /*swap*/ area->max.y}, .max = {area->max.x, /*swap*/ area->min.y}};
  case MapAngle_South:
    return (MapArea){.min = /*swap*/ area->max, .max = /*swap*/ area->min};
  case MapAngle_West:
    return (MapArea){.min = {/*swap*/ area->max.x, area->min.y}, .max = {/*swap*/ area->min.x, area->max.y}};
  }
  return *area;
}

MapPoint ObjLayout_rotate_map_coords_to_scr(MapAngle const angle, MapPoint const pos)
{
  /* Map coordinates to screen coordinates, biased towards coordinate origin at zero.
     Input and output are both coarse (objects grid) coordinates. */
  switch (angle) {
  case MapAngle_Count:
  case MapAngle_North:
    break;
  case MapAngle_East:
    return (MapPoint){Obj_Size - 1 - pos.y, pos.x};
  case MapAngle_South:
    return (MapPoint){Obj_Size - 1 - pos.x, Obj_Size - 1 - pos.y};
  case MapAngle_West:
    return (MapPoint){pos.y, Obj_Size - 1 - pos.x};
  }
  return pos;
}

static MapPoint ObjLayout_rotate_map_coords_to_scr_for_fine(MapAngle const angle, MapPoint const pos)
{
  /* Map coordinates to screen coordinates, assuming coordinate origin is also rotated.
     Input and output are both coarse (objects grid) coordinates. */
  switch (angle) {
  case MapAngle_Count:
  case MapAngle_North:
    break;
  case MapAngle_East:
    return (MapPoint){Obj_Size - pos.y, pos.x};
  case MapAngle_South:
    return (MapPoint){Obj_Size - pos.x, Obj_Size - pos.y};
  case MapAngle_West:
    return (MapPoint){pos.y, Obj_Size - pos.x};
  }
  return pos;
}

MapPoint ObjLayout_derotate_scr_coords_to_map(MapAngle const angle, MapPoint const pos)
{
  /* Screen coordinates to map coordinates.
     Input and output are both coarse (objects grid) coordinates. */
  switch (angle) {
  case MapAngle_Count:
  case MapAngle_North:
    break;
  case MapAngle_East:
    return (MapPoint){pos.y, Obj_Size - 1 - pos.x};
  case MapAngle_South:
    return (MapPoint){Obj_Size - 1 - pos.x, Obj_Size - 1 - pos.y};
  case MapAngle_West:
    return (MapPoint){Obj_Size - 1 - pos.y, pos.x};
  }
  return pos;
}

MapPoint ObjLayout_map_coords_to_fine(View const *const view, MapPoint const pos)
{
  /* Calculate the corner of the grid location closest to the grid's origin in fine screen coordinates.
     Input is coarse (objects grid) coordinates, output is fine screen coordinates. */
  MapPoint const rot_pos = ObjLayout_rotate_map_coords_to_scr_for_fine(view->config.angle, pos);
  MapPoint const fine_coords = MapPoint_mul_log2(rot_pos, MAP_GRID_SIZE_LOG2);
  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, pos.x, pos.y, fine_coords.x, fine_coords.y);

  return fine_coords;
}

MapPoint ObjLayout_map_coords_to_centre(View const *const view, MapPoint const pos)
{
  /* Calculate the centre of the grid location in fine screen coordinates.
     Input is coarse (objects grid) coordinates, output is fine screen coordinates. */
  MapPoint const rot_pos = ObjLayout_rotate_map_coords_to_scr(view->config.angle, pos);
  static MapPoint const offset = {MAP_GRID_SIZE / 2, MAP_GRID_SIZE / 2};
  MapPoint const fine_coords = MapPoint_add(offset, MapPoint_mul_log2(rot_pos, MAP_GRID_SIZE_LOG2));

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, pos.x, pos.y, fine_coords.x, fine_coords.y);

  return fine_coords;
}

MapPoint ObjLayout_scr_coords_from_fine(View const *const view, MapPoint pos)
{
  /* Convert generic map coordinates to objects grid location
  (2^19 = 524288 units per ground map texel) */
  NOT_USED(view); // FIXME - dimetric?
  MapPoint const coarse_coords = MapPoint_div_log2(pos, MAP_GRID_SIZE_LOG2);

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, pos.x, pos.y, coarse_coords.x, coarse_coords.y);

  return coarse_coords;
}

MapPoint ObjLayout_map_coords_from_fine(View const *const view, MapPoint pos)
{
  /* Convert generic map coordinates to objects grid location
  (2^19 = 524288 units per ground map texel) */
  MapPoint coarse_coords = ObjLayout_scr_coords_from_fine(view, pos);
  coarse_coords = ObjLayout_derotate_scr_coords_to_map(view->config.angle, coarse_coords);

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, pos.x, pos.y, coarse_coords.x, coarse_coords.y);

  return coarse_coords;
}

MapArea ObjLayout_map_area_from_fine(View const *const view, MapArea const *const area)
{
  /* Input is fine screen coordinates, output is coarse (objects grid) coordinates. */
  MapArea out = {
    .min = ObjLayout_map_coords_from_fine(view, area->min),
    .max = ObjLayout_map_coords_from_fine(view, area->max)
  };
  out = swap_area_limits_for_rot(view->config.angle, &out);

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, area->min.x, area->min.y, area->max.x, area->max.y,
        out.min.x, out.min.y, out.max.x, out.max.y);

  assert(MapArea_is_valid(&out));
  return out;
}

MapArea ObjLayout_scr_area_from_fine(View const *const view, MapArea const *const area)
{
  /* Input is fine screen coordinates, output is coarse screen coordinates. */
  MapArea const out = {
    .min = ObjLayout_scr_coords_from_fine(view, area->min),
    .max = ObjLayout_scr_coords_from_fine(view, area->max)
  };

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, area->min.x, area->min.y, area->max.x, area->max.y,
        out.min.x, out.min.y, out.max.x, out.max.y);

  assert(MapArea_is_valid(&out));
  return out;
}

MapArea ObjLayout_map_area_to_fine(View const *const view, MapArea const *const area)
{
  /* Calculate the outside edge of a coarse (objects grid) area in fine screen coordinates. */
  assert(area);
  assert(view);
  MapArea rot_area = {area->min, MapPoint_add(area->max, (MapPoint){1,1})};
  rot_area = swap_area_limits_for_rot(view->config.angle, &rot_area);
  MapArea const out = {
    .min = ObjLayout_map_coords_to_fine(view, rot_area.min),
    .max = ObjLayout_map_coords_to_fine(view, rot_area.max)
  };

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, area->min.x, area->min.y, area->max.x, area->max.y,
        out.min.x, out.min.y, out.max.x, out.max.y);

  assert(MapArea_is_valid(&out));
  return out;
}

MapArea ObjLayout_map_area_to_centre(View const *const view, MapArea const *const area)
{
  /* Calculate the centreline edge of a coarse (objects grid) area in fine screen coordinates. */
  MapArea const rot_area = swap_area_limits_for_rot(view->config.angle, area);
  MapArea const out = {
    .min = ObjLayout_map_coords_to_centre(view, rot_area.min),
    .max = ObjLayout_map_coords_to_centre(view, rot_area.max),
  };

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, area->min.x, area->min.y, area->max.x, area->max.y,
        out.min.x, out.min.y, out.max.x, out.max.y);

  assert(MapArea_is_valid(&out));
  return out;
}

MapArea ObjLayout_rotate_map_area_to_scr(MapAngle const angle, MapArea const *const area)
{
  /* Input and output are both coarse (objects grid) coordinates */
  MapArea const rot_area = swap_area_limits_for_rot(angle, area);
  MapArea const out = {
    .min = ObjLayout_rotate_map_coords_to_scr(angle, rot_area.min),
    .max = ObjLayout_rotate_map_coords_to_scr(angle, rot_area.max)
  };

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, area->min.x, area->min.y, area->max.x, area->max.y,
        out.min.x, out.min.y, out.max.x, out.max.y);

  assert(MapArea_is_valid(&out));
  return out;
}
