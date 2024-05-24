/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map file
 *  Copyright (C) 2020 Christopher Bazley
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

#include "stdlib.h"
#include "Flex.h"

#include "Debug.h"
#include "Macros.h"
#include "Reader.h"
#include "Writer.h"
#include "NoBudge.h"
#include "StrExtra.h"
#include "StrDict.h"

#include "Map.h"
#include "MapData.h"
#include "utils.h"

enum {
  PREALLOC_SIZE = 4096,
};

static StrDict file_dict;

static SFError map_read_cb(DFile const *const dfile, Reader *const reader)
{
  assert(dfile);
  assert(reader);
  MapData *const map = CONTAINER_OF(dfile, MapData, dfile);
  SFError err = SFERROR(OK);

  MapAreaIter iter;
  for (MapPoint p = map_get_first(&iter);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter)) {
    int const byte = reader_fgetc(reader);
    if (byte == EOF) {
      err = SFERROR(ReadFail);
      break;
    }

    if (byte < 0 || (byte > Map_RefMax && byte != Map_RefMask)) {
      DEBUGF("Bad tile ref %d at %" PRIMapCoord ",%" PRIMapCoord "\n", byte, p.x, p.y);
      return SFERROR(BadTileRef);
    }

    MapRef const tile = map_ref_from_num((size_t)byte);
    if (!map_ref_is_valid(map, tile)) {
      DEBUGF("Invalid tile ref %d at %" PRIMapCoord ",%" PRIMapCoord "\n", byte, p.x, p.y);
      return SFERROR(BadTileRef);
    }

    map_set_tile(map, p, tile);
  }

  return check_trunc_or_ext(reader, err);
}

static long int map_get_min_size_cb(DFile const *const dfile)
{
  NOT_USED(dfile);
  return Map_Area;
}

static void map_destroy_cb(DFile const *const dfile)
{
  assert(dfile);
  MapData *const map = CONTAINER_OF(dfile, MapData, dfile);
  flex_free(&map->flex);
  dfile_destroy(&map->dfile);
  free(map);
}

static void map_write_cb(DFile const *const dfile, Writer *const writer)
{
  assert(dfile);
  assert(writer);
  MapData *const map = CONTAINER_OF(dfile, MapData, dfile);
  DEBUGF("Writing map data %p wrapping dfile %p\n",
         (void*)map, (void*)dfile);

  nobudge_register(PREALLOC_SIZE);
  writer_fwrite(map->flex, Map_Area, 1, writer);
  nobudge_deregister();
}

static void map_cleanup(void)
{
  strdict_destroy(&file_dict, NULL, NULL);
}

void map_init(void)
{
  strdict_init(&file_dict);
  atexit(map_cleanup);
}

DFile *map_get_dfile(MapData *const map)
{
  assert(map);
  return &map->dfile;
}

static MapData *map_create(bool const is_overlay)
{
  MapData *map = malloc(sizeof(*map));
  if (map)
  {
    *map = (MapData){.is_overlay = is_overlay};

    dfile_init(&map->dfile, map_read_cb, map_write_cb,
               map_get_min_size_cb, map_destroy_cb);

    if (!flex_alloc(&map->flex, Map_Area))
    {
      free(map);
      map = NULL;
    }
  }
  DEBUGF("Created map data %p\n", (void *)map);
  return map;
}

MapData *map_create_overlay(void)
{
  return map_create(true);
}

MapData *map_create_base(void)
{
  return map_create(false);
}

bool map_share(MapData *const map)
{
  assert(map);
  return dfile_set_shared(&map->dfile, &file_dict);
}

MapData *map_get_shared(char const *const filename)
{
  DFile *const dfile = dfile_find_shared(&file_dict, filename);
  return dfile ? CONTAINER_OF(dfile, MapData, dfile) : NULL;
}

void map_clip_bbox(MapArea *const area)
{
  assert(MapArea_is_valid(area));
  DEBUG("Will clip bounding box %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord, area->min.x, area->min.y,
        area->max.x, area->max.y);


  if (area->min.x < 0)
    area->min.x = 0;

  if (area->min.y < 0)
    area->min.y = 0;

  if (area->max.x >= Map_Size)
    area->max.x = Map_Size - 1;

  if (area->max.y >= Map_Size)
    area->max.y = Map_Size - 1;

  DEBUG("Clipped bounding box is %" PRIMapCoord ",%" PRIMapCoord
        ",%" PRIMapCoord ",%" PRIMapCoord, area->min.x, area->min.y,
        area->max.x, area->max.y);
}

MapPoint map_get_first(MapAreaIter *const iter)
{
  assert(iter);
  static MapArea const whole = {
    {0,0}, {Map_Size - 1, Map_Size - 1}
  };
  return MapAreaIter_get_first(iter, &whole);
}

static bool bbox_contains_cb(MapArea const *const area, void *const arg)
{
  MapPoint const *const pos = arg;
  return MapArea_contains(area, *pos);
}

bool map_bbox_contains(MapArea const *const area, MapPoint const pos)
{
  MapPoint wrapped = map_wrap_coords(pos);
  bool const contains = map_split_area(area, bbox_contains_cb, &wrapped);

  DEBUG_VERBOSEF("%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord
         " %s %" PRIMapCoord ",%" PRIMapCoord "\n",
        area->min.x, area->min.y, area->max.x, area->max.y,
        contains ? "contains" : "doesn't contain",
        pos.x, pos.y);

  return contains;
}

MapPoint map_coords_in_area(MapPoint const pos, MapArea const *const area)
{
  assert(map_bbox_contains(area, pos));

  MapPoint min = area->min;

  if (pos.x < area->min.x) {
    min.x -= Map_Size;
  } else if (pos.x > area->max.x) {
    min.x = map_wrap_coord(min.x);
  }

  if (pos.y < area->min.y) {
    min.y -= Map_Size;
  } else if (pos.y > area->max.y) {
    min.y = map_wrap_coord(min.y);
  }

  assert(pos.x >= min.x);
  assert(pos.y >= min.y);
  return MapPoint_sub(pos, min);
}

static bool bbox_overlap_split_a_cb(MapArea const *const split_a, void *const arg)
{
  MapArea const *const split_b = arg;
  return MapArea_overlaps(split_b, split_a);
}

static bool bbox_overlap_split_b_cb(MapArea const *const split_b, void *const arg)
{
  MapArea const *const a = arg;
  return map_split_area(a, bbox_overlap_split_a_cb, (void *)split_b);
}

bool map_overlap(MapArea const *const a, MapArea const *const b)
{
  return map_split_area(b, bbox_overlap_split_b_cb, (void *)a);
}

void map_area_to_key_range(MapArea const *const map_area,
  IntDictKey *const min_key, IntDictKey *const max_key)
{
  assert(MapArea_is_valid(map_area));
  assert(min_key);
  assert(max_key);

  MapArea unwrapped_area = *map_area;

  if (map_area->max.x - map_area->min.x >= Map_Size - 1 ||
      map_wrap_coord(map_area->max.x) < map_wrap_coord(map_area->min.x)) {
    unwrapped_area.min.x = 0;
    unwrapped_area.max.x = Map_Size - 1;
  }

  if (map_area->max.y - map_area->min.y >= Map_Size - 1 ||
      map_wrap_coord(map_area->max.y) < map_wrap_coord(map_area->min.y)) {
    unwrapped_area.min.y = 0;
    unwrapped_area.max.y = Map_Size - 1;
  }
  *min_key = map_coords_to_key(unwrapped_area.min);
  *max_key = map_coords_to_key(unwrapped_area.max);
  assert(*min_key <= *max_key);
}
