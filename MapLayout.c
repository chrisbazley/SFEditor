/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground texture map layout
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

#include "Map.h"
#include "View.h"
#include "MapCoord.h"
#include "MapLayout.h"

enum {
  MAP_TILE_SIZE_LOG2 = MAP_COORDS_LIMIT_LOG2 - Map_SizeLog2,
  MAP_TILE_SIZE = 1l << MAP_TILE_SIZE_LOG2,
};

MapPoint MapLayout_rotate_map_coords_to_scr(MapAngle const angle, MapPoint const pos)
{
  /* Map coordinates to screen coordinates, biased towards coordinate origin at zero.
     Input and output are both coarse (tiles grid) coordinates. */
  switch (angle) {
  case MapAngle_Count:
  case MapAngle_North:
    break;
  case MapAngle_East:
    return (MapPoint){Map_Size - 1 - pos.y, pos.x};
  case MapAngle_South:
    return (MapPoint){Map_Size - 1 - pos.x, Map_Size - 1 - pos.y};
  case MapAngle_West:
    return (MapPoint){pos.y, Map_Size - 1 - pos.x};
  }
  return pos;
}

static MapPoint MapLayout_rotate_map_coords_to_scr_for_fine(MapAngle const angle, MapPoint const pos)
{
  /* Map coordinates to screen coordinates, assuming coordinate origin is also rotated.
     Input and output are both coarse (tiles grid) coordinates. */
  switch (angle) {
  case MapAngle_Count:
  case MapAngle_North:
    break;
  case MapAngle_East:
    return (MapPoint){Map_Size - pos.y, pos.x};
  case MapAngle_South:
    return (MapPoint){Map_Size - pos.x, Map_Size - pos.y};
  case MapAngle_West:
    return (MapPoint){pos.y, Map_Size - pos.x};
  }
  return pos;
}

MapPoint MapLayout_derotate_scr_coords_to_map(MapAngle const angle, MapPoint const pos)
{
  /* Screen coordinates to map coordinates */
  switch (angle) {
  case MapAngle_Count:
  case MapAngle_North:
    break;
  case MapAngle_East:
    return (MapPoint){pos.y, Map_Size - 1 - pos.x};
  case MapAngle_South:
    return (MapPoint){Map_Size - 1 - pos.x, Map_Size - 1 - pos.y};
  case MapAngle_West:
    return (MapPoint){Map_Size - 1 - pos.y, pos.x};
  }
  return pos;
}

static MapArea swap_area_limits_for_rot(MapAngle const angle, MapArea const *const area)
{
  /* Just ensure the correct order of minimum and maximum coordinates. */
  assert(area);

  switch (angle) {
  case MapAngle_Count:
  case MapAngle_North:
    break;
  case MapAngle_East:
    return (MapArea){.min = {area->min.x, area->max.y}, .max = {area->max.x, area->min.y}};
  case MapAngle_South:
    return (MapArea){.min = area->max, .max = area->min};
  case MapAngle_West:
    return (MapArea){.min = {area->max.x, area->min.y}, .max = {area->min.x, area->max.y}};
  }
  return *area;
}

MapPoint MapLayout_map_coords_to_fine(View const *const view, MapPoint const pos)
{
  MapPoint const rot_pos = MapLayout_rotate_map_coords_to_scr_for_fine(view->config.angle, pos);
  MapPoint const fine_coords = MapPoint_mul_log2(rot_pos, MAP_TILE_SIZE_LOG2);

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, pos.x, pos.y, fine_coords.x, fine_coords.y);

  return fine_coords;
}

MapPoint MapLayout_map_coords_to_centre(View const *const view, MapPoint const pos)
{
  MapPoint const rot_pos = MapLayout_rotate_map_coords_to_scr(view->config.angle, pos);
  static MapPoint const offset = {MAP_TILE_SIZE / 2, MAP_TILE_SIZE / 2};
  MapPoint const fine_coords = MapPoint_add(offset, MapPoint_mul_log2(rot_pos, MAP_TILE_SIZE_LOG2));

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, pos.x, pos.y, fine_coords.x, fine_coords.y);

  return fine_coords;
}

MapPoint MapLayout_scr_coords_from_fine(View const *const view, MapPoint const pos)
{
  /*  Convert generic map coordinates to tiles grid location
  (2^19 = 524288 units per ground map texel) */
  NOT_USED(view); // FIXME - dimetric?
  MapPoint const coarse_coords = MapPoint_div_log2(pos, MAP_TILE_SIZE_LOG2);

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, pos.x, pos.y, coarse_coords.x, coarse_coords.y);

  return coarse_coords;
}

MapPoint MapLayout_map_coords_from_fine(View const *const view, MapPoint const pos)
{
  /*  Convert generic map coordinates to tiles grid location
  (2^19 = 524288 units per ground map texel) */
  MapPoint coarse_coords = MapLayout_scr_coords_from_fine(view, pos);
  coarse_coords = MapLayout_derotate_scr_coords_to_map(view->config.angle, coarse_coords);

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, pos.x, pos.y, coarse_coords.x, coarse_coords.y);

  return coarse_coords;
}

MapPoint MapLayout_map_coords_up_from_fine(View const *const view, MapPoint const pos)
{
  /*  Convert generic map coordinates to tiles grid location
  (2^19 = 524288 units per ground map texel) */
  MapPoint coarse_coords = MapPoint_div_up_log2(pos, MAP_TILE_SIZE_LOG2);
  coarse_coords = MapLayout_derotate_scr_coords_to_map(view->config.angle, coarse_coords);

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, pos.x, pos.y, coarse_coords.x, coarse_coords.y);

  return coarse_coords;
}

MapPoint MapLayout_map_coords_down_from_fine(View const *const view, MapPoint const pos)
{
  /*  Convert generic map coordinates to tiles grid location
  (2^19 = 524288 units per ground map texel) */
  MapPoint coarse_coords = MapPoint_div_log2(pos, MAP_TILE_SIZE_LOG2);
  coarse_coords = MapPoint_sub(coarse_coords, (MapPoint){1,1});
  coarse_coords = MapLayout_derotate_scr_coords_to_map(view->config.angle, coarse_coords);

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, pos.x, pos.y, coarse_coords.x, coarse_coords.y);

  return coarse_coords;
}


MapArea MapLayout_map_area_from_fine(View const *const view, MapArea const *const area)
{
  MapArea out = {
    .min = MapLayout_map_coords_from_fine(view, area->min),
    .max = MapLayout_map_coords_from_fine(view, area->max)
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

MapArea MapLayout_scr_area_from_fine(View const *const view, MapArea const *const area)
{
  MapArea const out = {
    .min = MapLayout_scr_coords_from_fine(view, area->min),
    .max = MapLayout_scr_coords_from_fine(view, area->max)
  };

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, area->min.x, area->min.y, area->max.x, area->max.y,
        out.min.x, out.min.y, out.max.x, out.max.y);

  assert(MapArea_is_valid(&out));
  return out;
}

MapArea MapLayout_map_area_inside_from_fine(View const *const view, MapArea const *const area)
{
  MapArea out = {
    .min = MapLayout_map_coords_up_from_fine(view, area->min),
    .max = MapLayout_map_coords_down_from_fine(view, area->max)
  };
  out = swap_area_limits_for_rot(view->config.angle, &out);

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, area->min.x, area->min.y, area->max.x, area->max.y,
        out.min.x, out.min.y, out.max.x, out.max.y);

  return out; // may be an invalid bounding box
}

MapArea MapLayout_map_area_to_fine(View const *const view, MapArea const *const area)
{
  MapArea rot_area = {area->min, MapPoint_add(area->max, (MapPoint){1,1})};
  rot_area = swap_area_limits_for_rot(view->config.angle, &rot_area);
  MapArea const out = {
    .min = MapLayout_map_coords_to_fine(view, rot_area.min),
    .max = MapLayout_map_coords_to_fine(view, rot_area.max)
  };

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, area->min.x, area->min.y, area->max.x, area->max.y,
        out.min.x, out.min.y, out.max.x, out.max.y);

  assert(MapArea_is_valid(&out));
  return out;
}

MapArea MapLayout_map_area_to_centre(View const *const view, MapArea const *const area)
{
  MapArea const rot_area = swap_area_limits_for_rot(view->config.angle, area);
  MapArea const out = {
    .min = MapLayout_map_coords_to_centre(view, rot_area.min),
    .max = MapLayout_map_coords_to_centre(view, rot_area.max),
  };

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, area->min.x, area->min.y, area->max.x, area->max.y,
        out.min.x, out.min.y, out.max.x, out.max.y);

  assert(MapArea_is_valid(&out));
  return out;
}

MapArea MapLayout_rotate_map_area_to_scr(MapAngle const angle, MapArea const *const area)
{
  MapArea const rot_area = swap_area_limits_for_rot(angle, area);
  MapArea const out = {
    .min = MapLayout_rotate_map_coords_to_scr(angle, rot_area.min),
    .max = MapLayout_rotate_map_coords_to_scr(angle, rot_area.max)
  };

  DEBUGF("%s IN {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "} OUT {%" PRIMapCoord ",%" PRIMapCoord
        " %" PRIMapCoord ",%" PRIMapCoord "}\n",
        __func__, area->min.x, area->min.y, area->max.x, area->max.y,
        out.min.x, out.min.y, out.max.x, out.max.y);

  assert(MapArea_is_valid(&out));
  return out;
}
