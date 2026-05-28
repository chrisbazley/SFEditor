/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Smoothing wand implementation
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef Smooth_h
#define Smooth_h

#include "MapCoord.h"
#include "MapEdit.h"
#include "MapEditChg.h"

typedef struct MapTexGroups MapTexGroups;

void MapTexGroups_init(MapTexGroups *groups_data);

void MapTexGroups_smooth(MapEditContext const *map,
  MapTexGroups *groups_data, MapPoint map_pos,
  MapEditChanges *change_info);

void MapTexGroups_edit(const char *tiles_set);

void MapTexGroups_load(MapTexGroups *groups_data,
  const char *tiles_set, int ntiles);

int MapTexGroups_get_count(MapTexGroups const *groups_data);

int MapTexGroups_get_group_of_tile(MapTexGroups *groups_data,
  MapRef tile);

int MapTexGroups_get_num_group_members(MapTexGroups *groups_data, int group);

MapRef MapTexGroups_get_group_member(MapTexGroups *groups_data,
                                     int group, int index);

void MapTexGroups_free(MapTexGroups *groups_data);

#endif
