/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map/animations editing mode change
 *  Copyright (C) 2019 Christopher Bazley
 */

#ifndef MapEditChg_h
#define MapEditChg_h

#include <stdbool.h>

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

typedef struct MapEditChanges
{
  unsigned long int tiles_changed, anims_added, anims_changed, anims_deleted;
}
MapEditChanges;

void MapEditChanges_init(MapEditChanges *change_info);

static inline bool MapEditChanges_anims_changed(_Optional const MapEditChanges *const change_info)
{
  if (!change_info) {
    return false;
  }
  return change_info->anims_added ||
         change_info->anims_changed ||
         change_info->anims_deleted;
}

static inline bool MapEditChanges_anims_increased(_Optional const MapEditChanges *const change_info)
{
  if (!change_info) {
    return false;
  }
  return change_info->anims_added > change_info->anims_deleted;
}

static inline bool MapEditChanges_anims_decreased(_Optional const MapEditChanges *const change_info)
{
  if (!change_info) {
    return false;
  }
  return change_info->anims_added < change_info->anims_deleted;
}

static inline bool MapEditChanges_map_changed(_Optional const MapEditChanges *const change_info)
{
  if (!change_info) {
    return false;
  }
  return change_info->tiles_changed;
}

static inline bool MapEditChanges_is_changed(_Optional const MapEditChanges *const change_info)
{
  return MapEditChanges_anims_changed(change_info) ||
         MapEditChanges_map_changed(change_info);
}

static inline void MapEditChanges_change_tiles(_Optional MapEditChanges *const change_info,
  unsigned long int const n)
{
  if (change_info) {
    change_info->tiles_changed += n;
  }
}

static inline void MapEditChanges_change_tile(_Optional MapEditChanges *const change_info)
{
  MapEditChanges_change_tiles(change_info, 1);
}

static inline void MapEditChanges_change_anim(_Optional MapEditChanges *const change_info)
{
  if (change_info) {
    ++change_info->anims_changed;
  }
}

static inline void MapEditChanges_add_anim(_Optional MapEditChanges *const change_info)
{
  if (change_info) {
    ++change_info->anims_added;
  }
}

static inline void MapEditChanges_delete_anim(_Optional MapEditChanges *const change_info)
{
  if (change_info) {
    ++change_info->anims_deleted;
  }
}

_Optional char *MapEditChanges_get_message(const MapEditChanges *change_info);

#endif
