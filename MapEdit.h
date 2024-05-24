/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map/animations editing mode
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef MapEdit_h
#define MapEdit_h

#include <stdbool.h>

#include "scheduler.h"

#include "MapEditChg.h"
#include "MapAnims.h"
#include "MapCoord.h"

typedef struct MapEditContext MapEditContext;

struct MapEditSelection;
struct MapEditChanges;
struct MapTexGroups;
struct MapAreaColData;

void MapEdit_reverse_selected(MapEditContext const *map,
  struct MapEditSelection *selected, struct MapEditChanges *change_info);

void MapEdit_delete_selected(MapEditContext const *map,
  struct MapEditSelection *selected, struct MapEditChanges *change_info);

void MapEdit_fill_selection(MapEditContext const *map,
  struct MapEditSelection *selected, MapRef tile,
  struct MapEditChanges *change_info);

void MapEdit_smooth_selection(MapEditContext const *map,
  struct MapEditSelection *selected, struct MapTexGroups *groups_data,
  struct MapEditChanges *change_info);

void MapEdit_flood_fill(MapEditContext const *map,
  MapRef replace, MapPoint map_pos, struct MapEditChanges *change_info);

void MapEdit_global_replace(MapEditContext const *map, MapRef find,
  MapRef replace, struct MapEditChanges *change_info);

void MapEdit_plot_tri(MapEditContext const *map, MapPoint vertex_A,
  MapPoint vertex_B, MapPoint vertex_C, MapRef tile,
  struct MapEditChanges *change_info);

void MapEdit_plot_rect(MapEditContext const *map, MapPoint vertex_A,
  MapPoint vertex_B, MapRef tile, struct MapEditChanges *change_info);

void MapEdit_plot_circ(MapEditContext const *map, MapPoint centre,
  MapCoord radius, MapRef tile, struct MapEditChanges *change_info);

void MapEdit_plot_line(MapEditContext const *map, MapPoint start,
  MapPoint end, MapRef tile, MapCoord thickness, struct MapEditChanges *change_info);

MapRef MapEdit_read_tile(MapEditContext const *map, MapPoint map_pos);

MapRef MapEdit_read_overlay(MapEditContext const *map, MapPoint map_pos);

void MapEdit_write_tile(MapEditContext const *map, MapPoint map_pos,
  MapRef tile_num, struct MapEditChanges *change_info);

void MapEdit_crop_overlay(MapEditContext const *map,
  struct MapEditChanges *change_info);

bool MapEdit_write_anim(MapEditContext const *map,
  MapPoint map_pos, MapAnimParam param,
  struct MapEditChanges *change_info);

void MapEdit_reset_anims(MapEditContext const *map);

SchedulerTime MapEdit_update_anims(MapEditContext const *map,
  int steps_to_advance, struct MapAreaColData *redraw_map);

size_t MapEdit_count_anims(MapEditContext const *map);

void MapEdit_anims_to_map(MapEditContext const *map,
  struct MapEditChanges *change_info);

void MapEdit_fill_area(MapEditContext const *map,
  MapArea const *area, MapRef tile_num,
  struct MapEditChanges *change_info);

typedef MapRef MapEditReadFn(void *cb_arg, MapPoint map_pos);

void MapEdit_copy_to_area(MapEditContext const *map,
  MapArea const *area, MapEditReadFn *read, void *cb_arg,
  struct MapEditChanges *change_info);

bool MapEdit_check_tile_range(MapEditContext const *map,
  size_t num_tiles);

#endif
