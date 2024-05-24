/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map animations editor
 *  Copyright (C) 2001 Christopher Bazley
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
#include <assert.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include "stdlib.h"
#include "stdio.h"
#include <inttypes.h>

#include "flex.h"

#include "Macros.h"
#include "Debug.h"
#include "MsgTrans.h"
#include "err.h"
#include "hourglass.h"
#include "Scheduler.h"
#include "Reader.h"
#include "Writer.h"

#include "utils.h"
#include "MapAnims.h"
#include "MapEdit.h"
#include "MapCoord.h"
#include "MapEditSel.h"
#include "MapEditChg.h"
#include "MapAreaCol.h"
#include "Map.h"
#include "CoarseCoord.h"
#include "IntDict.h"

enum {
  MIN_EXTEND_SIZE = 8,
  ANIMS_BIT_MAP_SIZE = Map_Area/CHAR_BIT * sizeof(char),
  BytesPerAnim = 28,
  BytesPerHdr = 16,
  MapOffsetDivider = 4,
};

struct ConvAnimations
{
  DFile dfile;
  IntDict sa_coords;
  void *bit_map; /* flex anchor for array [32][256], one bit per
map location */
  int steps_since_reset;
};

struct MapAnim
{
  CoarsePoint2d coords;
  int32_t timer_counter;
  unsigned char frame_num;
  MapAnimParam param;
};

bool fixed_last_anims_load = false; /* FIXME: a bit of a hack */

/* ---------------- Private functions ---------------- */

static void update_anims_map(ConvAnimations *const anims, CoarsePoint2d const coords, bool const set)
{
  /* Update bit map of where animations are located */
  assert(anims != NULL);

  const size_t bit_offset = map_coarse_coords_to_index(coords);
  const size_t byte_offset = bit_offset / CHAR_BIT;
  unsigned int const bit_mask = 1u << (bit_offset % CHAR_BIT);
  DEBUG("%s animations map byte %zu, bits %u", set ? "Setting" : "Clearing",
        byte_offset, bit_mask);

  assert(byte_offset < ANIMS_BIT_MAP_SIZE);
  if (set) {
    SET_BITS(((char *)anims->bit_map)[byte_offset], bit_mask);
  } else {
    CLEAR_BITS(((char *)anims->bit_map)[byte_offset], bit_mask);
  }
}

static SFError add_anim(ConvAnimations *const anims, MapData *const write_map,
  const MapAnim *const new_anim)
{
  /* Copy external animation into our array */
  assert(anims != NULL);
  assert(new_anim != NULL);

  if (intdict_count(&anims->sa_coords) >= AnimsMax)
  {
    return SFERROR(NumAnims);
  }

  MapAnim *const anim = malloc(sizeof(*anim));
  if (!anim)
  {
    return SFERROR(NoMem);
  }

  *anim = *new_anim;

  if (!intdict_insert(&anims->sa_coords,
                      map_coarse_coords_to_key(anim->coords), anim, NULL)) {
    free(anim);
    return SFERROR(NoMem);
  }

  update_anims_map(anims, new_anim->coords, true);
  if (write_map) {
//    splat_map_tile(write_map, new_anim);
  }
  return SFERROR(OK);
}

static void delete_anim(ConvAnimations *const anims, MapAnim *const anim)
{
  assert(anim);
  update_anims_map(anims, anim->coords, false);
  free(anim);
}

static void calc_current_frame(ConvAnimations *const anims,
  MapAnim *const anim_templ)
{
  /* Calculate 'current' frame of animation and time interval to next frame
     (relative to the time of the last global reset) */
  assert(anim_templ != NULL);
  assert(anims != NULL);
  int const period = anim_templ->param.period;
  DEBUG("Timer reset value is %d", anim_templ->param.period);

  anim_templ->timer_counter = period -
                              (anims->steps_since_reset % (period + 1));

  anim_templ->frame_num = (anims->steps_since_reset / (period + 1)) % AnimsNFrames;

  DEBUG("Skipping forward by %d (tile: %d, timer: %u)",
        anims->steps_since_reset, anim_templ->frame_num,
        anim_templ->timer_counter);
}

static int32_t calc_map_offset(MapPoint const map_pos)
{
  /* Calculate word offset value from map coordinates */
  size_t map_offset = map_coords_to_index(map_pos);
  assert(map_offset <= SIZE_MAX / MapOffsetDivider);
  map_offset *= MapOffsetDivider;
  DEBUG("Word offset for map coords %" PRIMapCoord ",%" PRIMapCoord " is %zu\n",
        map_pos.x, map_pos.y, map_offset);
  assert(map_offset <= INT32_MAX);
  return (int32_t)map_offset;
}

static CoarsePoint2d calc_map_coords(int32_t const map_offset)
{
  /* Calculate map coordinates from word offset value */
  assert(map_offset >= 0);
  assert((map_offset % MapOffsetDivider) == 0);

  int32_t const byte_offset = map_offset / MapOffsetDivider;
  assert(byte_offset < Map_Area);

  CoarsePoint2d const map_pos = {
    .y = byte_offset / Map_Size,
    .x = byte_offset % Map_Size
  };
  DEBUG("Map coords for word offset %" PRId32 " is %d,%d",
        map_offset, map_pos.x, map_pos.y);
  return map_pos;
}

static bool check_wrapped(ConvAnimations *const anims, MapPoint const map_pos)
{
  assert(anims != NULL);
  const size_t bit_offset = map_coords_to_index(map_pos);
  const size_t byte_offset = bit_offset / CHAR_BIT;
  unsigned int const bit_mask = 1u << (bit_offset % CHAR_BIT);
  DEBUG("Checking byte %zu, bits %u", byte_offset, bit_mask);

  assert(byte_offset < ANIMS_BIT_MAP_SIZE);
  if (TEST_BITS(((char *)anims->bit_map)[byte_offset], bit_mask)) {
    DEBUG("Found an animation at %" PRIMapCoord ",%" PRIMapCoord, map_pos.x, map_pos.y);
    return true;
  }
  return false;
}

static void anim_destroy_cb(IntDictKey const key, void *const data, void *const arg)
{
  NOT_USED(key);
  NOT_USED(arg);
  free(data);
}

static void clear_all(ConvAnimations *const anims)
{
  assert(anims);
  memset_flex(&anims->bit_map, 0, ANIMS_BIT_MAP_SIZE);
  intdict_destroy(&anims->sa_coords, anim_destroy_cb, NULL);
  intdict_init(&anims->sa_coords);
}

static SFError read_inner(ConvAnimations *const anims, Reader *const reader)
{
  assert(anims);

  int32_t tmp;
  if (!reader_fread_int32(&tmp, reader))
  {
    return SFERROR(ReadFail);
  }

  if (tmp < 0 || tmp > AnimsMax)
  {
    return SFERROR(BadNumAnims);
  }
  size_t const count = (size_t)tmp;

  if (reader_fseek(reader, BytesPerHdr, SEEK_SET))
  {
    return SFERROR(BadSeek);
  }

  for (size_t a = 0; a < count; ++a)
  {
    int32_t map_offset;
    if (!reader_fread_int32(&map_offset, reader))
    {
      return SFERROR(ReadFail);
    }

    if (map_offset % MapOffsetDivider)
    {
      return SFERROR(BadAnimCoord);
    }

    if (map_offset < 0 || (map_offset / MapOffsetDivider) >= Map_Area)
    {
      return SFERROR(BadAnimCoord);
    }

    MapPoint const coords = map_coords_from_coarse(calc_map_coords(map_offset));
    if (check_wrapped(anims, coords))
    {
      return SFERROR(AnimOverlap);
    }

    int32_t timer_counter;
    if (!reader_fread_int32(&timer_counter, reader))
    {
      return SFERROR(ReadFail);
    }

    uint16_t period;
    if (!reader_fread_uint16(&period, reader))
    {
      return SFERROR(ReadFail);
    }

    if (timer_counter != period)
    {
      return SFERROR(BadAnimTime);
    }

    uint16_t frame_num;
    if (!reader_fread_uint16(&frame_num, reader))
    {
      return SFERROR(ReadFail);
    }

    if (frame_num != 0)
    {
      return SFERROR(BadAnimState);
    }

    MapAnimParam param = {
      .period = period,
    };

    for (size_t i = 0; i < AnimsNFrames; ++i)
    {
      int32_t tile;
      if (!reader_fread_int32(&tile, reader))
      {
        return SFERROR(ReadFail);
      }
      if (tile < 0 || (tile > Map_RefMax && tile != Map_RefMask))
      {
        return SFERROR(BadAnimFrame);
      }
      param.tiles[i] = map_ref_from_num((uint32_t)tile);
    }

    SFError err = MapAnims_add(anims, NULL, coords, param);
    if (SFError_fail(err))
    {
      return err;
    }
  }

  return SFERROR(OK);
}

static SFError MapAnims_read_cb(DFile const *const dfile, Reader *const reader)
{
  assert(dfile);
  ConvAnimations *const anims = CONTAINER_OF(dfile, ConvAnimations, dfile);
  DEBUGF("Reading anims data %p wrapping dfile %p\n",
         (void*)anims, (void*)dfile);

  clear_all(anims);
  return check_trunc_or_ext(reader, read_inner(anims, reader));
}

size_t MapAnims_count(ConvAnimations const *const anims)
{
  assert(anims != NULL);
  size_t const count = intdict_count(&anims->sa_coords);
  assert(count <= AnimsMax);
  return count;
}

static long int MapAnims_get_min_size_cb(DFile const *const dfile)
{
  assert(dfile);
  ConvAnimations *const anims = CONTAINER_OF(dfile, ConvAnimations, dfile);
  return BytesPerHdr + ((long int)MapAnims_count(anims) * BytesPerAnim);
}

static void MapAnims_destroy_cb(DFile const *const dfile)
{
  assert(dfile);
  ConvAnimations *const anims = CONTAINER_OF(dfile, ConvAnimations, dfile);

  if (anims->bit_map != NULL)
  {
    flex_free(&anims->bit_map);
  }

  intdict_destroy(&anims->sa_coords, anim_destroy_cb, NULL);
  dfile_destroy(&anims->dfile);
  free(anims);
}

static void MapAnims_write_cb(DFile const *const dfile, Writer *const writer)
{
  assert(dfile);
  assert(writer);
  ConvAnimations *const anims = CONTAINER_OF(dfile, ConvAnimations, dfile);
  DEBUGF("Writing anims data %p wrapping dfile %p\n",
         (void*)anims, (void*)dfile);

  writer_fwrite_int32((int32_t)MapAnims_count(anims), writer);

  writer_fseek(writer, BytesPerHdr, SEEK_SET);

  IntDictVIter iter;
  for (MapAnim *anim = intdictviter_all_init(&iter, &anims->sa_coords);
       anim != NULL;
       anim = intdictviter_advance(&iter)) {
    assert(anim);
    writer_fwrite_int32(calc_map_offset(map_coords_from_coarse(anim->coords)), writer);
    writer_fwrite_int32(anim->param.period, writer);
    writer_fwrite_uint16(anim->param.period, writer);
    writer_fwrite_uint16(0, writer);

    for (size_t i = 0; i < AnimsNFrames; ++i)
    {
      writer_fwrite_uint32(map_ref_to_num(anim->param.tiles[i]), writer);
    }
  }
}

/* ---------------- Public functions ---------------- */

ConvAnimations *MapAnims_create(void)
{
  ConvAnimations *const anims = malloc(sizeof(*anims));
  if (anims)
  {
    *anims = (ConvAnimations){{0}};
    intdict_init(&anims->sa_coords);

    if (!flex_alloc(&anims->bit_map, ANIMS_BIT_MAP_SIZE)) {
      free(anims);
      return NULL;
    }

    memset_flex(&anims->bit_map, 0, ANIMS_BIT_MAP_SIZE);

    dfile_init(&anims->dfile, MapAnims_read_cb, MapAnims_write_cb,
               MapAnims_get_min_size_cb, MapAnims_destroy_cb);
  }
  return anims;
}

DFile *MapAnims_get_dfile(ConvAnimations *const anims)
{
  assert(anims);
  return &anims->dfile;
}

SFError MapAnims_add(ConvAnimations *const anims, MapData *const write_map,
  MapPoint const map_pos, MapAnimParam const param)
{
  MapAnim anim_templ = {
    .coords = map_coords_to_coarse(map_pos),
    .param = param,
  };
  calc_current_frame(anims, &anim_templ);
  return add_anim(anims, write_map, &anim_templ);
}

bool MapAnims_check_locn(ConvAnimations *const anims, MapPoint const map_pos)
{
  return check_wrapped(anims, map_wrap_coords(map_pos));
}

bool MapAnims_get(ConvAnimations *anims, MapPoint const map_pos,
  MapAnimParam *const param)
{
  if (!MapAnims_check_locn(anims, map_pos)) {
    return false;
  }

  MapArea const bounds = {map_pos, map_pos};
  MapAnimsIter iter;
  MapAnimsIter_get_first(&iter, anims, &bounds, param);
  return !MapAnimsIter_done(&iter);
}

static MapPoint iter_loop_core(MapAnimsIter *const iter, MapAnimParam *const param)
{
  assert(iter);
  for (; iter->anim != NULL; iter->anim = intdictviter_advance(&iter->viter)) {
    MapAnim *const anim = iter->anim;

    MapPoint const coords = map_coords_from_coarse(anim->coords);
    if (!map_bbox_contains(&iter->map_area, coords)) {
      continue;
    }

    DEBUGF("Getting animation with period %u at coordinates %d,%d\n",
           anim->param.period, anim->coords.x, anim->coords.y);

    if (param) {
      *param = anim->param;
    }

    return map_coords_from_coarse(anim->coords);
  }

  assert(!iter->done);
  iter->done = true;
  return (MapPoint){-1, -1};
}

MapPoint MapAnimsIter_get_first(MapAnimsIter *const iter,
  ConvAnimations *const anims, MapArea const *const map_area,
  MapAnimParam *const param)
{
  assert(iter != NULL);
  assert(anims != NULL);
  assert(MapArea_is_valid(map_area));

  MapCoord min_key, max_key;
  map_area_to_key_range(map_area, &min_key, &max_key);

  *iter = (MapAnimsIter){
    .map_area = *map_area,
    .anims = anims,
  };

  iter->anim = intdictviter_init(&iter->viter, &anims->sa_coords, min_key, max_key);
  return iter_loop_core(iter, param);
}

MapPoint MapAnimsIter_get_next(MapAnimsIter *const iter, MapAnimParam *const param)
{
  assert(iter != NULL);
  assert(!iter->done);
  assert(MapArea_is_valid(&iter->map_area));
  iter->anim = intdictviter_advance(&iter->viter);
  return iter_loop_core(iter, param);
}

void MapAnimsIter_del_current(MapAnimsIter *const iter)
{
  assert(iter != NULL);
  assert(!iter->done);
  assert(iter->anim != NULL);
  assert(MapArea_is_valid(&iter->map_area));

  intdictviter_remove(&iter->viter);
  delete_anim(iter->anims, iter->anim);
  iter->anim = NULL;
}

void MapAnimsIter_replace_current(MapAnimsIter const *const iter, MapAnimParam const param)
{
  assert(iter != NULL);
  assert(iter->anim != NULL);
  assert(MapArea_is_valid(&iter->map_area));

  iter->anim->param = param;
}

MapRef MapAnimsIter_get_current(MapAnimsIter const *const iter)
{
  assert(iter != NULL);
  assert(iter->anim != NULL);
  assert(MapArea_is_valid(&iter->map_area));

  /* Find the map tile for the current frame of this animation */
  MapAnim const *const anim = iter->anim;
  int const current_frame = anim->frame_num;
  MapRef anim_tile = map_ref_mask();

  for (int frame = AnimsNFrames + current_frame; frame > current_frame; frame--)
  {
    int const wrapped_frame = frame % AnimsNFrames;
    anim_tile = anim->param.tiles[wrapped_frame];
    if (!map_ref_is_mask(anim_tile))
    {
      DEBUG("Initial frame is %d (tile %zu)", wrapped_frame, map_ref_to_num(anim_tile));
      break;
    }
  }

  if (map_ref_is_mask(anim_tile))
  {
    DEBUG("Animation is blank!");
  }
  return anim_tile;
}

void MapAnims_reset(ConvAnimations *const anims)
{
  DEBUG("Resetting all animations");
  assert(anims != NULL);

  anims->steps_since_reset = 0;

  IntDictVIter iter;
  for (MapAnim *anim = intdictviter_all_init(&iter, &anims->sa_coords);
       anim != NULL;
       anim = intdictviter_advance(&iter)) {
    assert(anim);
    /* Reset animation state to defaults (i.e. as in save file) */
    anim->frame_num = 0;
    anim->timer_counter = anim->param.period;
    DEBUG("Reset timer of animation at %d,%d to %d", anim->coords.x,
          anim->coords.y, anim->param.period);
  }
}

SchedulerTime MapAnims_update(ConvAnimations *const anims,
  MapData *const write_map, int const steps_to_advance, MapAreaColData *const redraw_map)
{
  SchedulerTime earliest_next_frame = SchedulerTime_Max;

  assert(anims != NULL);
  assert(write_map != NULL);

  IntDictVIter iter;
  for (MapAnim *anim = intdictviter_all_init(&iter, &anims->sa_coords);
       anim != NULL;
       anim = intdictviter_advance(&iter)) {
    assert(anim);
    int frame_num = anim->frame_num;
    int32_t const period = anim->param.period;
    int32_t timer_counter = anim->timer_counter;

    // Try to find a non-mask previous frame
    MapRef old_tile = anim->param.tiles[frame_num];

    for (int prev_frame = frame_num; map_ref_is_mask(old_tile); ) {
      prev_frame = (AnimsNFrames + prev_frame - 1) % AnimsNFrames;
      if (prev_frame == frame_num) {
        break;
      }
      assert(prev_frame >= 0);
      assert(prev_frame < AnimsNFrames);
      old_tile = anim->param.tiles[prev_frame];
    }

    MapRef new_tile = old_tile;
    for (int step = 0; step < steps_to_advance; step++) {
      if (--timer_counter >= 0) {
        continue; /* countdown to next frame not yet expired */
      }

      /* Reset counter to next */
      timer_counter = period;

      /* Advance to next frame of animation */
      frame_num = (frame_num + 1) % AnimsNFrames;
      assert(frame_num >= 0);
      assert(frame_num < AnimsNFrames);
      MapRef const next_tile = anim->param.tiles[frame_num];
      if (!map_ref_is_mask(next_tile)) {
        new_tile = next_tile;
      }
    }

    anim->frame_num = frame_num;
    anim->timer_counter = timer_counter;

    DEBUG("Advanced animation at %d,%d to frame %d",
          anim->coords.x, anim->coords.y, frame_num);

    // Don't write mask values to the map because that's nonsense
    if (!map_ref_is_mask(new_tile)) {
      MapPoint const pos = map_coords_from_coarse(anim->coords);
      if (!map_ref_is_equal(map_update_tile(write_map, pos, new_tile), new_tile))
      {
        if (redraw_map != NULL) {
          MapAreaCol_add(redraw_map, &(MapArea){pos, pos});
        }
      }
    }

    /* Keep track of the earliest time when next update due */
    if (timer_counter < earliest_next_frame)
    {
      earliest_next_frame = timer_counter;
    }
  }

  anims->steps_since_reset += steps_to_advance;
  DEBUG("%d frames since last reset", anims->steps_since_reset);
  DEBUG("Counter with least time has %d", earliest_next_frame);

  return earliest_next_frame;
}
