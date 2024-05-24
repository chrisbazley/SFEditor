/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Objects grid
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef Obj_h
#define Obj_h

#include <stdbool.h>
#include "DFile.h"
#include "ObjData.h"
#include "MapCoord.h"
#include "IntDict.h"

enum {
  Obj_SizeLog2 = 7,
  Obj_Size = 1 << Obj_SizeLog2,
  Obj_AreaLog2 = Obj_SizeLog2 * 2,
  Obj_Area = 1l << Obj_AreaLog2,
  Obj_RefNone = 0,
  Obj_RefMinGun = 1,
  Obj_RefMaxGun = 3,
  Obj_RefMinSAM = 4,
  Obj_RefMaxSAM = 6,
  Obj_RefMinHangar = 7,
  Obj_RefMaxHangar = 8,
  Obj_RefMinObject = 1,
  Obj_RefMaxObject = 99,
  Obj_ObjectCount = Obj_RefMaxObject - Obj_RefMinObject + 1,
  Obj_RefMinCloud = 100,
  Obj_RefMaxCloud = 198,
  Obj_CloudCount = Obj_RefMaxCloud - Obj_RefMinCloud + 1,
  Obj_RefHill = 199,
  Obj_RefMask = 255,
  Obj_NumCloudTints = 4,
  Obj_MinCloudHeight = 16384,
  Obj_CloudHeightStep = 64,
  Obj_MaxCloudHeight = Obj_MinCloudHeight +
                       (Obj_CloudHeightStep * Obj_CloudCount),
};

typedef struct ObjectsData ObjectsData;

void objects_init(void);
ObjectsData *objects_create_base(void);
ObjectsData *objects_create_overlay(void);

ObjectsData *objects_get_shared(char const *filename);
bool objects_share(ObjectsData *objects);
DFile *objects_get_dfile(ObjectsData *objects);

void objects_clip_bbox(MapArea *area);

MapPoint objects_get_first(MapAreaIter *iter);

bool objects_bbox_contains(MapArea const *area, MapPoint const pos);

static inline bool objects_coord_in_range(const MapCoord coord)
{
  return (coord < Obj_Size) && (coord >= 0);
}

static inline MapCoord objects_wrap_coord(const MapCoord coord)
{
  MapCoord const wrapped_coord = (unsigned long)coord % Obj_Size;
  if (wrapped_coord != coord) {
    DEBUGF("Wrap object coordinate %" PRIMapCoord " to %" PRIMapCoord "\n",
           coord, wrapped_coord);
  }
  assert(objects_coord_in_range(wrapped_coord));
  return wrapped_coord;
}

static inline MapPoint objects_wrap_coords(
  MapPoint const pos)
{
  return (MapPoint){objects_wrap_coord(pos.x),
                    objects_wrap_coord(pos.y)};
}

static inline bool objects_coords_compare(MapPoint const a, MapPoint const b)
{
  return MapPoint_compare(objects_wrap_coords(a), objects_wrap_coords(b));
}

static inline bool objects_coords_in_range(MapPoint const pos)
{
  return objects_coord_in_range(pos.x) &&
         objects_coord_in_range(pos.y);
}

static inline size_t objects_coords_to_index(MapPoint const pos)
{
  return (Obj_Size * (size_t)objects_wrap_coord(pos.y)) + (size_t)objects_wrap_coord(pos.x);
}

static inline IntDictKey objects_coords_to_key(MapPoint const pos)
{
  size_t const index = objects_coords_to_index(pos);
  assert(index <= INTDICTKEY_MAX);
  return (IntDictKey)index;
}

typedef struct {
  unsigned char index;
} ObjRef;

static inline size_t objects_ref_to_num(ObjRef const obj_ref)
{
  assert(obj_ref.index <= Obj_RefHill || obj_ref.index == Obj_RefMask);
  return obj_ref.index;
}

static inline ObjRef objects_ref_from_num(size_t const obj_ref)
{
  assert(obj_ref >= Obj_RefNone);
  assert(obj_ref <= Obj_RefHill || obj_ref == Obj_RefMask);
  return (ObjRef){obj_ref};
}

static inline ObjRef objects_ref_none(void)
{
  return (ObjRef){Obj_RefNone};
}

static inline ObjRef objects_ref_object(size_t const normal_type)
{
  assert(normal_type < Obj_ObjectCount);
  return (ObjRef){Obj_RefMinObject + normal_type};
}

static inline ObjRef objects_ref_mask(void)
{
  return (ObjRef){Obj_RefMask};
}

static inline ObjRef objects_ref_cloud(size_t const cloud_type)
{
  assert(cloud_type < Obj_CloudCount);
  return (ObjRef){Obj_RefMinCloud + cloud_type};
}

static inline ObjRef objects_ref_hill(void)
{
  return (ObjRef){Obj_RefHill};
}

static inline bool objects_ref_is_cloud(ObjRef const ref)
{
  return ref.index >= Obj_RefMinCloud && ref.index <= Obj_RefMaxCloud;
}

static inline bool objects_ref_is_hill(ObjRef const ref)
{
  return ref.index == Obj_RefHill;
}

static inline bool objects_ref_is_object(ObjRef const ref)
{
  return ref.index >= Obj_RefMinObject && ref.index <= Obj_RefMaxObject;
}

static inline bool objects_ref_is_gun(ObjRef const ref)
{
  return ref.index >= Obj_RefMinGun && ref.index <= Obj_RefMaxGun;
}

static inline bool objects_ref_is_hangar(ObjRef const ref)
{
  return ref.index >= Obj_RefMinHangar && ref.index <= Obj_RefMaxHangar;
}

static inline bool objects_ref_is_sam(ObjRef const ref)
{
  return ref.index >= Obj_RefMinSAM && ref.index <= Obj_RefMaxSAM;
}

static inline bool objects_ref_is_none(ObjRef const ref)
{
  return ref.index == Obj_RefNone;
}

static inline bool objects_ref_is_mask(ObjRef const ref)
{
  return ref.index == Obj_RefMask;
}

static inline bool objects_ref_is_valid(ObjectsData const *const objects,
  ObjRef const ref)
{
  assert(objects);
  return !objects_ref_is_mask(ref) || objects->is_overlay;
}

static inline bool objects_ref_is_defence(ObjRef const ref)
{
  return objects_ref_is_gun(ref) ||
         objects_ref_is_hangar(ref) ||
         objects_ref_is_sam(ref);
}

static inline int objects_ref_get_cloud_tint(ObjRef const ref, MapPoint const map_pos)
{
  assert(objects_ref_is_cloud(ref));
  return (ref.index ^ objects_wrap_coord(map_pos.x)) % Obj_NumCloudTints;
}

static inline int objects_ref_get_cloud_height(ObjRef const ref)
{
  assert(objects_ref_is_cloud(ref));
  return Obj_MinCloudHeight +
         ((int)(ref.index - Obj_RefMinCloud) * Obj_CloudHeightStep);
}

static inline bool objects_ref_is_equal(ObjRef const a, ObjRef const b)
{
  return a.index == b.index;
}

static inline ObjRef objects_get_ref(ObjectsData const *const objects,
  MapPoint const pos)
{
  assert(objects);
  size_t const value = ((unsigned char *)objects->flex)[objects_coords_to_index(pos)];
  DEBUG("Got ref %zu at grid location %" PRIMapCoord ",%" PRIMapCoord,
        value, pos.x, pos.y);
  /* If you're thinking of converting values here, don't! It's more
     efficient to do so when reading/writing the file. */
  ObjRef const obj_ref = objects_ref_from_num(value);
  assert(objects_ref_is_valid(objects, obj_ref));
  return obj_ref;
}

static inline void objects_set_ref(ObjectsData const *const objects,
  MapPoint const pos, ObjRef const ref)
{
  assert(objects);
  assert(objects_ref_is_valid(objects, ref));
  size_t const value = objects_ref_to_num(ref);
  DEBUG("Set ref %zu at grid location %" PRIMapCoord ",%" PRIMapCoord,
        value, pos.x, pos.y);
  ((unsigned char *)objects->flex)[objects_coords_to_index(pos)] = value;
  /* If you're thinking of converting values here, don't! It's more
     efficient to do so when reading/writing the file. */
}

static inline ObjRef objects_update_ref(ObjectsData const *const objects,
  MapPoint const pos, ObjRef const ref)
{
  assert(objects);
  assert(objects_ref_is_valid(objects, ref));
  size_t const index = objects_coords_to_index(pos);

  size_t const current = ((unsigned char *)objects->flex)[index];
  ObjRef const cref = objects_ref_from_num(current);
  assert(objects_ref_is_valid(objects, cref));

  if (!objects_ref_is_equal(cref, ref)) {
    size_t const value = objects_ref_to_num(ref);
    DEBUG("Change ref %zu to %zu at grid location %" PRIMapCoord ",%" PRIMapCoord,
          current, value, pos.x, pos.y);
    ((unsigned char *)objects->flex)[index] = value;
  }
  return cref;
}

#include "CoarseCoord.h"

static inline CoarsePoint2d objects_coords_to_coarse(MapPoint pos)
{
  pos = objects_wrap_coords(pos);
  DEBUGF("%" PRIMapCoord" ,%" PRIMapCoord " to coarse coords\n", pos.x, pos.y);
  return (CoarsePoint2d){pos.x, pos.y};
}

static inline MapPoint objects_coords_from_coarse(CoarsePoint2d const pos)
{
  DEBUGF("from coarse coords %d,%d\n", pos.x, pos.y);
  MapPoint const objects_pos = (MapPoint){pos.x, pos.y};
  assert(objects_coords_in_range(objects_pos));
  return objects_pos;
}

static inline MapCoord objects_coarse_coords_to_index(CoarsePoint2d const pos)
{
  assert(objects_coords_in_range(objects_coords_from_coarse(pos)));
  return (Obj_Size * (MapCoord)pos.y) + pos.x;
}

static inline bool objects_split_area(MapArea const *const area,
                        bool (*const callback)(MapArea const *, void *),
                        void *const cb_arg)
{
  return MapArea_split(area, Obj_SizeLog2, callback, cb_arg);
}

MapPoint objects_coords_in_area(MapPoint pos, MapArea const *area);

bool objects_overlap(MapArea const *a, MapArea const *b);

static inline bool objects_can_place(MapPoint const pos)
{
  // 'clear edge of map' in the game (authentically bugged)
  MapCoord const y = objects_wrap_coord(pos.y);
  if (y == (Obj_Size - 2) || y == 0) {
    return false;
  }
  return true;
}

void objects_area_to_key_range(MapArea const *map_area,
  IntDictKey *min_key, IntDictKey *max_key);

#endif
