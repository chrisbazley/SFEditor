/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef Map_h
#define Map_h

#include <assert.h>
#include <stdbool.h>

#include "DFile.h"
#include "MapData.h"
#include "MapCoord.h"
#include "CoarseCoord.h"
#include "IntDict.h"

enum {
  Map_SizeLog2 = 8,
  Map_Size = 1 << Map_SizeLog2,
  Map_AreaLog2 = Map_SizeLog2 * 2,
  Map_Area = 1l << Map_AreaLog2,
  Map_RefMax = 191, // game only allocates space for 192 bitmaps
  Map_RefMask = 255,
};

typedef struct MapData MapData;

void map_init(void);
MapData *map_create_base(void);
MapData *map_create_overlay(void);
MapData *map_get_shared(char const *filename);
bool map_share(MapData *map);
DFile *map_get_dfile(MapData *map);

void map_clip_bbox(MapArea *area);

MapPoint map_get_first(MapAreaIter *iter);

bool map_bbox_contains(MapArea const *area, MapPoint const pos);

static inline bool map_coord_in_range(const MapCoord x)
{
  return (x < Map_Size) && (x >= 0);
}

static inline MapCoord map_wrap_coord(const MapCoord coord)
{
  MapCoord const wrapped_coord = (unsigned long)coord % Map_Size;
  if (wrapped_coord != coord) {
    DEBUGF("Wrap map coordinate %" PRIMapCoord " to %" PRIMapCoord "\n",
           coord, wrapped_coord);
  }
  assert(map_coord_in_range(wrapped_coord));
  return wrapped_coord;
}

static inline MapPoint map_wrap_coords(MapPoint const pos)
{
  return (MapPoint){map_wrap_coord(pos.x), map_wrap_coord(pos.y)};
}

static inline bool map_coords_compare(MapPoint const a, MapPoint const b)
{
  return MapPoint_compare(map_wrap_coords(a), map_wrap_coords(b));
}

static inline bool map_coords_in_range(MapPoint const pos)
{
  return map_coord_in_range(pos.x) &&
         map_coord_in_range(pos.y);
}

static inline size_t map_coords_to_index(MapPoint const pos)
{
  return (Map_Size * (size_t)map_wrap_coord(pos.y)) + (size_t)map_wrap_coord(pos.x);
}

static inline IntDictKey map_coords_to_key(MapPoint const pos)
{
  size_t const index = map_coords_to_index(pos);
  assert(index <= INTDICTKEY_MAX);
  return (IntDictKey)index;
}

typedef struct {
  unsigned char index;
} MapRef;

static inline size_t map_ref_to_num(MapRef const tile)
{
  assert(tile.index <= Map_RefMax || tile.index == Map_RefMask);
  return tile.index;
}

static inline MapRef map_ref_from_num(size_t const tile)
{
  assert(tile >= 0);
  assert(tile <= Map_RefMax || tile == Map_RefMask);
  return (MapRef){tile};
}

static inline MapRef map_ref_mask(void)
{
  return (MapRef){Map_RefMask};
}

static inline bool map_ref_is_texture(MapRef const tile)
{
  return tile.index <= Map_RefMax;
}

static inline bool map_ref_is_mask(MapRef const tile)
{
  return tile.index == Map_RefMask;
}

static inline bool map_ref_is_valid(MapData const *const map, MapRef const tile)
{
  assert(map);
  return !map_ref_is_mask(tile) || map->is_overlay;
}

static inline bool map_ref_is_equal(MapRef const a, MapRef const b)
{
  return a.index == b.index;
}

static inline MapRef map_get_tile(MapData const *const map,
  MapPoint const pos)
{
  assert(map);
  size_t const value = ((unsigned char *)map->flex)[map_coords_to_index(pos)];
  /* If you're thinking of converting values here, don't! It's more
     efficient to do so when reading/writing the file. */
  MapRef const tile = map_ref_from_num(value);
  assert(map_ref_is_valid(map, tile));
  return tile;
}

static inline void map_set_tile(MapData const *const map,
  MapPoint const pos, MapRef const tile)
{
  assert(map);
  assert(map_ref_is_valid(map, tile));
  size_t const value = map_ref_to_num(tile);
  ((unsigned char *)map->flex)[map_coords_to_index(pos)] = value;
  /* If you're thinking of converting values here, don't! It's more
     efficient to do so when reading/writing the file. */
}

static inline MapRef map_update_tile(MapData const *const map,
  MapPoint const pos, MapRef const tile)
{
  assert(map);
  assert(map_ref_is_valid(map, tile));
  size_t const index = map_coords_to_index(pos);

  size_t const current = ((unsigned char *)map->flex)[index];
  MapRef const ctile = map_ref_from_num(current);
  assert(map_ref_is_valid(map, ctile));

  if (!map_ref_is_equal(ctile, tile)) {
    size_t const value = map_ref_to_num(tile);
    DEBUG("Changing tile %zu to %zu at grid location %" PRIMapCoord ",%" PRIMapCoord,
          current, value, pos.x, pos.y);
    ((unsigned char *)map->flex)[index] = value;
  }
  return ctile;
}

#include "CoarseCoord.h"

static inline CoarsePoint2d map_coords_to_coarse(MapPoint const pos)
{
  return (CoarsePoint2d){map_wrap_coord(pos.x), map_wrap_coord(pos.y)};
}

static inline MapPoint map_coords_from_coarse(CoarsePoint2d const pos)
{
  MapPoint const map_pos = (MapPoint){pos.x, pos.y};
  assert(map_coords_in_range(map_pos));
  return map_pos;
}

static inline size_t map_coarse_coords_to_index(CoarsePoint2d const pos)
{
  assert(map_coords_in_range(map_coords_from_coarse(pos)));
  return (Map_Size * (size_t)pos.y) + (size_t)pos.x;
}

static inline IntDictKey map_coarse_coords_to_key(CoarsePoint2d const pos)
{
  size_t const index = map_coarse_coords_to_index(pos);
  assert(index <= INTDICTKEY_MAX);
  return (IntDictKey)index;
}

static inline bool map_split_area(MapArea const *const area,
                        bool (*const callback)(MapArea const *, void *),
                        void *const cb_arg)
{
  return MapArea_split(area, Map_SizeLog2, callback, cb_arg);
}

MapPoint map_coords_in_area(MapPoint pos, MapArea const *area);

bool map_overlap(MapArea const *a, MapArea const *b);

void map_area_to_key_range(MapArea const *map_area,
  IntDictKey *min_key, IntDictKey *max_key);

#endif
