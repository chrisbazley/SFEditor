/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Objects grid layout
 *  Copyright (C) 2022 Christopher Bazley
 */

#ifndef ObjLayout_h
#define ObjLayout_h

#include "MapCoord.h"

struct View;

/* Map coordinates to screen coordinates. */
MapPoint ObjLayout_rotate_map_coords_to_scr(MapAngle angle, MapPoint pos);
MapPoint ObjLayout_rotate_hill_pos(MapAngle angle, MapPoint pos);
MapPoint ObjLayout_map_coords_to_fine(struct View const *view, MapPoint pos);
MapPoint ObjLayout_map_coords_to_centre(struct View const *view, MapPoint pos);
MapArea ObjLayout_rotate_map_area_to_scr(MapAngle angle, MapArea const *area);
MapArea ObjLayout_map_area_to_fine(struct View const *view, MapArea const *area);
MapArea ObjLayout_map_area_to_centre(struct View const *view, MapArea const *area);

/* Screen coordinates to map coordinates */
MapPoint ObjLayout_derotate_scr_coords_to_map(MapAngle angle, MapPoint pos);
MapPoint ObjLayout_map_coords_from_fine(struct View const *view, MapPoint pos);
MapPoint ObjLayout_scr_coords_from_fine(struct View const *view, MapPoint pos);
MapArea ObjLayout_map_area_from_fine(struct View const *view, MapArea const *area);
MapArea ObjLayout_scr_area_from_fine(struct View const *view, MapArea const *area);

#endif
