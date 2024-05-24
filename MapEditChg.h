/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map/animations editing mode change
 *  Copyright (C) 2019 Christopher Bazley
 */

#ifndef MapEditChg_h
#define MapEditChg_h

#include <stdbool.h>

typedef struct MapEditChanges
{
  unsigned long int tiles_changed;
  unsigned long int anims_added;
  unsigned long int anims_changed;
  unsigned long int anims_deleted;
}
MapEditChanges;

void MapEditChanges_init(MapEditChanges *change_info);

static inline bool MapEditChanges_anims_changed(const MapEditChanges *const change_info)
{
  if (!change_info) {
    return false;
  }
  return change_info->anims_added ||
         change_info->anims_changed ||
         change_info->anims_deleted;
}

static inline bool MapEditChanges_anims_increased(const MapEditChanges *const change_info)
{
  if (!change_info) {
    return false;
  }
  return change_info->anims_added > change_info->anims_deleted;
}

static inline bool MapEditChanges_anims_decreased(const MapEditChanges *const change_info)
{
  if (!change_info) {
    return false;
  }
  return change_info->anims_added < change_info->anims_deleted;
}

static inline bool MapEditChanges_map_changed(const MapEditChanges *const change_info)
{
  if (!change_info) {
    return false;
  }
  return change_info->tiles_changed;
}

static inline bool MapEditChanges_is_changed(const MapEditChanges *const change_info)
{
  return MapEditChanges_anims_changed(change_info) ||
         MapEditChanges_map_changed(change_info);
}

static inline void MapEditChanges_change_tiles(MapEditChanges *const change_info,
  unsigned long int const n)
{
  if (change_info) {
    change_info->tiles_changed += n;
  }
}

static inline void MapEditChanges_change_tile(MapEditChanges *const change_info)
{
  MapEditChanges_change_tiles(change_info, 1);
}

static inline void MapEditChanges_change_anim(MapEditChanges *const change_info)
{
  if (change_info) {
    ++change_info->anims_changed;
  }
}

static inline void MapEditChanges_add_anim(MapEditChanges *const change_info)
{
  if (change_info) {
    ++change_info->anims_added;
  }
}

static inline void MapEditChanges_delete_anim(MapEditChanges *const change_info)
{
  if (change_info) {
    ++change_info->anims_deleted;
  }
}

char *MapEditChanges_get_message(const MapEditChanges *change_info);

#endif
