/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground texture map layout
 *  Copyright (C) 2022 Christopher Bazley
 */

#ifndef MapLayout_h
#define MapLayout_h

#include "MapCoord.h"

struct View;

/* Map coordinates to screen coordinates. */
MapPoint MapLayout_rotate_map_coords_to_scr(MapAngle angle, MapPoint pos);
MapPoint MapLayout_map_coords_to_fine(struct View const *view, MapPoint pos);
MapPoint MapLayout_map_coords_to_centre(struct View const *view, MapPoint pos);
MapArea MapLayout_rotate_map_area_to_scr(MapAngle angle, MapArea const *area);
MapArea MapLayout_map_area_to_fine(struct View const *view, MapArea const *area);
MapArea MapLayout_map_area_to_centre(struct View const *view, MapArea const *area);

/* Screen coordinates to map coordinates */
MapPoint MapLayout_derotate_scr_coords_to_map(MapAngle angle, MapPoint pos);
MapPoint MapLayout_map_coords_from_fine(struct View const *view, MapPoint pos);
MapPoint MapLayout_map_coords_up_from_fine(struct View const *view, MapPoint pos);
MapPoint MapLayout_scr_coords_from_fine(struct View const *view, MapPoint pos);
MapArea MapLayout_map_area_from_fine(struct View const *view, MapArea const *area);
MapArea MapLayout_scr_area_from_fine(struct View const *view, MapArea const *area);
MapArea MapLayout_map_area_inside_from_fine(struct View const *view, MapArea const *area);

#endif
