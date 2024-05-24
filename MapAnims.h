/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map animations handling
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef MapAnims_h
#define MapAnims_h

#include <stdbool.h>
#include <stdint.h>

#include "Scheduler.h"

#include "SFError.h"
#include "Map.h"
#include "MapCoord.h"
#include "DFile.h"
#include "IntDict.h"

struct MapEditSelection;
struct MapEditChanges;
struct MapAreaColData;

enum {
  AnimsNFrames = 4,
  AnimsMax = 176, /* increased from 128 in release 2.02 of the game */
};

typedef struct ConvAnimations ConvAnimations;
typedef struct MapAnim MapAnim;

typedef struct {
  int32_t period;
  MapRef tiles[AnimsNFrames];
} MapAnimParam;

ConvAnimations *MapAnims_create(void);
DFile *MapAnims_get_dfile(ConvAnimations *anims);

extern bool fixed_last_anims_load; /* a bit of a hack */

SFError MapAnims_add(ConvAnimations *anims, MapData *write_map,
  MapPoint map_pos, MapAnimParam param);

bool MapAnims_get(ConvAnimations *anims, MapPoint map_pos,
  MapAnimParam *param);

bool MapAnims_check_locn(ConvAnimations *anims, MapPoint map_pos);

void MapAnims_wipe_locn(ConvAnimations *anims, MapPoint map_pos,
  struct MapEditChanges *change_info);

void MapAnims_wipe_bbox(ConvAnimations *anims, MapArea const *area,
  struct MapEditChanges *change_info);

typedef struct
{
  ConvAnimations *anims;
  IntDictVIter viter;
  MapAnim *anim;
  MapArea map_area;
  bool done;
}
MapAnimsIter;

MapPoint MapAnimsIter_get_first(MapAnimsIter *iter, ConvAnimations *anims,
  MapArea const *map_area, MapAnimParam *param);

MapPoint MapAnimsIter_get_next(MapAnimsIter *iter, MapAnimParam *param);
void MapAnimsIter_del_current(MapAnimsIter *iter);
void MapAnimsIter_replace_current(MapAnimsIter const *iter, MapAnimParam param);
MapRef MapAnimsIter_get_current(MapAnimsIter const *iter);

static inline bool MapAnimsIter_done(MapAnimsIter const *iter)
{
  assert(iter);
  assert(iter->anims);
  return iter->done;
}

void MapAnims_reset(ConvAnimations *anims);

SchedulerTime MapAnims_update(ConvAnimations *anims,
  MapData *write_map, int steps_to_advance, struct MapAreaColData *redraw_map);

size_t MapAnims_count(ConvAnimations const *const anims);

#endif
