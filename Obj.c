/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Objects grid file
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

#include "Obj.h"
#include "ObjData.h"
#include "utils.h"

enum {
  PREALLOC_SIZE = 4096,
};

static StrDict file_dict;

static SFError objects_read_cb(DFile const *const dfile, Reader *const reader)
{
  assert(dfile);
  assert(reader);
  ObjectsData *const obj = CONTAINER_OF(dfile, ObjectsData, dfile);
  SFError err = SFERROR(OK);

  MapAreaIter iter;
  for (MapPoint p = objects_get_first(&iter);
       !MapAreaIter_done(&iter);
       p = MapAreaIter_get_next(&iter)) {

    int byte = reader_fgetc(reader);
    if (byte == EOF) {
      err = SFERROR(ReadFail);
      break;
    }

    if (byte < 0 || (byte > Obj_RefHill && byte != Obj_RefMask)) {
      DEBUGF("Bad object ref %d at %" PRIMapCoord ",%" PRIMapCoord "\n", byte, p.x, p.y);
      return SFERROR(BadObjRef);
    }

    ObjRef obj_ref = objects_ref_from_num((size_t)byte);
    if (!objects_ref_is_valid(obj, obj_ref)) {
      DEBUGF("Invalid object ref %d at %" PRIMapCoord ",%" PRIMapCoord "\n", byte, p.x, p.y);
      return SFERROR(BadObjRef);
    }

    if (!objects_ref_is_none(obj_ref) && !objects_ref_is_mask(obj_ref) && !objects_can_place(p)) {
      /* Too common to be able to report this as an error.
         Instead, clear the object like the game does. */
      DEBUGF("Object %d at bad position %" PRIMapCoord ",%" PRIMapCoord "\n", byte, p.x, p.y);
      obj_ref = obj->is_overlay ? objects_ref_mask() : objects_ref_none();
    }

    objects_set_ref(obj, p, obj_ref);
  }

  return check_trunc_or_ext(reader, err);
}

static long int objects_get_min_size_cb(DFile const *const dfile)
{
  NOT_USED(dfile);
  return Obj_Area;
}

static void objects_destroy_cb(DFile const *const dfile)
{
  assert(dfile);
  ObjectsData *const obj = CONTAINER_OF(dfile, ObjectsData, dfile);
  flex_free(&obj->flex);
  dfile_destroy(&obj->dfile);
  free(obj);
}

static void objects_write_cb(DFile const *const dfile, Writer *const writer)
{
  assert(dfile);
  assert(writer);
  ObjectsData *const obj = CONTAINER_OF(dfile, ObjectsData, dfile);
  DEBUGF("Writing obj data %p wrapping dfile %p\n",
         (void*)obj, (void*)dfile);

  nobudge_register(PREALLOC_SIZE);
  writer_fwrite(obj->flex, Obj_Area, 1, writer);
  nobudge_deregister();
}

static void objects_cleanup(void)
{
  strdict_destroy(&file_dict, NULL, NULL);
}

void objects_init(void)
{
  strdict_init(&file_dict);
  atexit(objects_cleanup);
}

DFile *objects_get_dfile(ObjectsData *const obj)
{
  assert(obj);
  return &obj->dfile;
}

static ObjectsData *objects_create(bool const is_overlay)
{
  ObjectsData *obj = malloc(sizeof(*obj));
  if (obj)
  {
    *obj = (ObjectsData){.is_overlay = is_overlay};

    dfile_init(&obj->dfile, objects_read_cb, objects_write_cb,
               objects_get_min_size_cb,
               objects_destroy_cb);

    if (!flex_alloc(&obj->flex, Obj_Area))
    {
      free(obj);
      obj = NULL;
    }
  }
  return obj;
}

ObjectsData *objects_create_base(void)
{
  return objects_create(false);
}

ObjectsData *objects_create_overlay(void)
{
  return objects_create(true);
}

bool objects_share(ObjectsData *const obj)
{
  assert(obj);
  return dfile_set_shared(&obj->dfile, &file_dict);
}

ObjectsData *objects_get_shared(char const *const filename)
{
  DFile *const dfile = dfile_find_shared(&file_dict, filename);
  return dfile ? CONTAINER_OF(dfile, ObjectsData, dfile) : NULL;
}

void objects_clip_bbox(MapArea *const area)
{
  assert(MapArea_is_valid(area));
  DEBUG("Will clip bounding box %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord, area->min.x, area->min.y,
        area->max.x, area->max.y);


  if (area->min.x < 0)
    area->min.x = 0;

  if (area->min.y < 0)
    area->min.y = 0;

  if (area->max.x >= Obj_Size)
    area->max.x = Obj_Size - 1;

  if (area->max.y >= Obj_Size)
    area->max.y = Obj_Size - 1;

  DEBUG("Clipped bounding box is %" PRIMapCoord ",%" PRIMapCoord
        ",%" PRIMapCoord ",%" PRIMapCoord, area->min.x, area->min.y,
        area->max.x, area->max.y);
}

MapPoint objects_get_first(MapAreaIter *const iter)
{
  assert(iter);
  static MapArea const whole = {
    {0,0}, {Obj_Size - 1, Obj_Size - 1}
  };
  return MapAreaIter_get_first(iter, &whole);
}

static bool bbox_contains_cb(MapArea const *const area, void *const arg)
{
  MapPoint const *const pos = arg;
  return MapArea_contains(area, *pos);
}

bool objects_bbox_contains(MapArea const *const area, MapPoint const pos)
{
  MapPoint wrapped = objects_wrap_coords(pos);
  bool const contains = objects_split_area(area, bbox_contains_cb, &wrapped);

  DEBUG_VERBOSEF("%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord
         " %s %" PRIMapCoord ",%" PRIMapCoord "\n",
        area->min.x, area->min.y, area->max.x, area->max.y,
        contains ? "contains" : "doesn't contain",
        pos.x, pos.y);

  return contains;
}

MapPoint objects_coords_in_area(MapPoint const pos, MapArea const *const area)
{
  assert(objects_bbox_contains(area, pos));

  MapPoint min = area->min;

  if (pos.x < area->min.x) {
    min.x -= Obj_Size;
  } else if (pos.x > area->max.x) {
    min.x = objects_wrap_coord(min.x);
  }

  if (pos.y < area->min.y) {
    min.y -= Obj_Size;
  } else if (pos.y > area->max.y) {
    min.y = objects_wrap_coord(min.y);
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
  return objects_split_area(a, bbox_overlap_split_a_cb, (void *)split_b);
}

bool objects_overlap(MapArea const *const a, MapArea const *const b)
{
  return objects_split_area(b, bbox_overlap_split_b_cb, (void *)a);
}

void objects_area_to_key_range(MapArea const *const map_area,
  IntDictKey *const min_key, IntDictKey *const max_key)
{
  assert(MapArea_is_valid(map_area));
  assert(min_key);
  assert(max_key);

  MapArea unwrapped_area = *map_area;

  if (map_area->max.x - map_area->min.x >= Obj_Size - 1 ||
      objects_wrap_coord(map_area->max.x) < objects_wrap_coord(map_area->min.x)) {
    unwrapped_area.min.x = 0;
    unwrapped_area.max.x = Obj_Size - 1;
  }

  if (map_area->max.y - map_area->min.y >= Obj_Size - 1 ||
      objects_wrap_coord(map_area->max.y) < objects_wrap_coord(map_area->min.y)) {
    unwrapped_area.min.y = 0;
    unwrapped_area.max.y = Obj_Size - 1;
  }
  *min_key = objects_coords_to_key(unwrapped_area.min);
  *max_key = objects_coords_to_key(unwrapped_area.max);
  assert(*min_key <= *max_key);
}
