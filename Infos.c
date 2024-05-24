/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission target information points
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
#include <inttypes.h>
#include <limits.h>

#include "Debug.h"
#include "Macros.h"
#include "Reader.h"
#include "Writer.h"
#include "IntDict.h"

#include "Utils.h"
#include "SFError.h"
#include "Map.h"
#include "CoarseCoord.h"
#include "Infos.h"
#include "InfosData.h"
#include "Text.h"
#include "TextData.h"

enum {
  TargetInfoAlloc = 16, // impossible because of TextOffsetCount limit
  TargetInfoPadding = 2,
  BytesPerTargetInfo = 4,
  InfoDuration = 5000,
  InfoDelay = 25,
  InfoSpeed = 3,
  InfoTypeLeftMargin = 35,
  InfoTypeTopMargin = 9,
  InfoTypeColour = 251,
  InfoDetailsLeftMargin = 18,
  InfoDetailsTopMargin = 32,
  InfoDetailsColour = 247,
  InfoTypeMaxCols = Text_NumColumns - InfoTypeLeftMargin - 1,
  InfoTypeMaxRows = InfoDetailsTopMargin - InfoTypeTopMargin - 3,
  InfoDetailsMaxCols = Text_NumColumns - InfoDetailsLeftMargin - 1,
  InfoDetailsMaxRows = Text_NumRows - InfoDetailsTopMargin - 1,
};

struct TargetInfo
{
  TargetInfosData *infos;
  CoarsePoint2d coords;
  uint8_t id;
  Text texts[TargetInfoTextIndex_Count];
};

static inline SFError read_target_info_text(TargetInfo *const info,
  Reader *const reader, long int const *const offsets, size_t const i)
{
  assert(info);
  assert(offsets);

  for (TargetInfoTextIndex k = TargetInfoTextIndex_First;
       k < TargetInfoTextIndex_Count;
       ++k) {
    if (reader_fseek(reader, offsets[(i * TargetInfoTextIndex_Count) + k], SEEK_SET))
    {
      return SFERROR(BadSeek);
    }

    SFError err = text_read_block(&info->texts[k], reader);
    if (SFError_fail(err))
    {
      return err;
    }
  }

  return SFERROR(OK);
}

static inline void write_target_info_offset(TargetInfo const *const info,
  Writer *const writer, int *const offset)
{
  assert(info);
  assert(offset);
  for (TargetInfoTextIndex k = TargetInfoTextIndex_First;
       k < TargetInfoTextIndex_Count;
       ++k) {
    text_write_offset(&info->texts[k], writer, offset);
  }
}

static inline void write_target_info_text(TargetInfo const *const info,
  Writer *const writer)
{
  assert(info);
  for (TargetInfoTextIndex k = TargetInfoTextIndex_First;
       k < TargetInfoTextIndex_Count;
       ++k) {
    text_write_block(&info->texts[k], writer);
  }
}

static inline void write_target_info_coords(TargetInfo const *const info,
  Writer *const writer)
{
  assert(info);

  CoarsePoint2d_write(info->coords, writer);

  writer_fseek(writer, TargetInfoPadding, SEEK_CUR);
}

static void free_info(TargetInfo *const info)
{
  assert(info);
  for (TargetInfoTextIndex k = TargetInfoTextIndex_First;
       k < TargetInfoTextIndex_Count;
       ++k) {
    text_destroy(&info->texts[k]);
  }
  info->infos->count--;
  free(info);
}

size_t target_info_delete(TargetInfo *const info)
{
  assert(info);
  TargetInfosData *const target_infos = info->infos;
  assert(target_infos);
  size_t index = 0;
  void *const removed = intdict_remove_value(&target_infos->dict,
                           map_coarse_coords_to_key(info->coords), &index);
  assert(removed == info);
  NOT_USED(removed);
  free_info(info);
  return index;
}

static void destroy_cb(IntDictKey const key, void *const data, void *const arg)
{
  NOT_USED(key);
  NOT_USED(arg);
  free_info(data);
}

void target_infos_init(TargetInfosData *const target_infos)
{
  assert(target_infos);
  *target_infos = (TargetInfosData){.count = 0, .next = 0};
  intdict_init(&target_infos->dict);
}

void target_infos_destroy(TargetInfosData *const target_infos)
{
  assert(target_infos);
  intdict_destroy(&target_infos->dict, destroy_cb, target_infos);
}

SFError target_infos_add(TargetInfosData *const target_infos,
  MapPoint const pos, size_t *const index)
{
  assert(target_infos);
  assert(target_infos->count >= 0);
  assert(target_infos->count <= TargetInfoMax);

  if (target_infos->count == TargetInfoMax)
  {
    return SFERROR(NumInfos);
  }

  TargetInfo *const info = malloc(sizeof(*info));
  if (info)
  {
    *info = (TargetInfo){.coords = map_coords_to_coarse(pos), .infos = target_infos,
                         .id = target_infos->next++};
    if (!intdict_insert(&target_infos->dict,
                       map_coords_to_key(pos), info, index)) {
      free(info);
      return SFERROR(NoMem);
    }

    for (TargetInfoTextIndex k = TargetInfoTextIndex_First;
         k < TargetInfoTextIndex_Count;
         ++k) {
      text_init(&info->texts[k]);

      if (k == TargetInfoTextIndex_Type) {
        static TextParams const params = {
          .duration = InfoDuration,
          .delay = InfoDelay,
          .speed = InfoSpeed,
          .x_pos = InfoTypeLeftMargin,
          .y_pos = InfoTypeTopMargin * Text_CharHeight,
          .y_clip = Text_NoYClip,
          .repeat = false,
          .colour = InfoTypeColour,
          .cursor_type = CursorType_Block,
        };
        text_set_params(&info->texts[k], &params);
      } else {
        static TextParams const params = {
          .duration = InfoDuration,
          .delay = InfoDelay,
          .speed = InfoSpeed,
          .x_pos = InfoDetailsLeftMargin,
          .y_pos = InfoDetailsTopMargin * Text_CharHeight,
          .y_clip = Text_NoYClip,
          .repeat = false,
          .colour = InfoDetailsColour,
          .cursor_type = CursorType_Block,
        };
        text_set_params(&info->texts[k], &params);
      }

    }
    target_infos->count++;
  }

  return SFERROR(OK);
}

SFError target_info_set_text(TargetInfo *const info,
  TargetInfoTextIndex const index, char const *const string)
{
  assert(info);
  assert(index >= 0);
  assert(index < TargetInfoTextIndex_Count);

  int max_width = 0;
  int const line_count = string_lcount(string, &max_width);
  int const max_num_cols = (index == TargetInfoTextIndex_Type ? InfoTypeMaxCols : InfoDetailsMaxCols);
  int const max_num_rows = (index == TargetInfoTextIndex_Type ? InfoTypeMaxRows : InfoDetailsMaxRows);

  if (max_width > max_num_cols) {
    return SFERROR(TooManyBriefingColumns);
  }

  if (line_count > max_num_rows) {
    return SFERROR(TooManyBriefingLines);
  }

  return text_set_string(&info->texts[index], string);
}

size_t target_info_set_pos(TargetInfo *const info, MapPoint const pos)
{
  assert(info);
  TargetInfosData *const target_infos = info->infos;

  size_t old_index;
  bool const removed = intdict_remove_specific(&target_infos->dict,
                             map_coarse_coords_to_key(info->coords),
                             info,
                             &old_index);
  assert(removed);
  NOT_USED(removed);

  info->coords = map_coords_to_coarse(pos);

  size_t new_index;
  bool const inserted = intdict_insert(&target_infos->dict,
                                 map_coords_to_key(pos), info, &new_index);
  assert(inserted);
  NOT_USED(inserted);
  return new_index;
}

char const *target_info_get_text(TargetInfo const *const info,
  TargetInfoTextIndex const index)
{
  assert(info);
  assert(index >= 0);
  assert(index < TargetInfoTextIndex_Count);
  return text_get_string(&info->texts[index]);
}

MapPoint target_info_get_pos(TargetInfo const *const info)
{
  assert(info);
  return map_coords_from_coarse(info->coords);
}

int target_info_get_id(TargetInfo const *const info)
{
  assert(info);
  return info->id;
}

SFError target_infos_read_pad(TargetInfosData *const target_infos,
  Reader *const reader)
{
  SFError err = target_infos_read(target_infos, reader);
  if (SFError_fail(err)) {
    return err;
  }
  assert(TargetInfoAlloc <= LONG_MAX);
  long int const padding = TargetInfoAlloc - (long)target_infos->count;
  if (reader_fseek(reader, padding * BytesPerTargetInfo, SEEK_CUR))
  {
    return SFERROR(BadSeek);
  }
  DEBUGF("Finished reading target info data at %ld\n", reader_ftell(reader));
  return SFERROR(OK);
}

SFError target_infos_read(TargetInfosData *const target_infos,
  Reader *const reader)
{
  assert(target_infos);

  int32_t num_target_infos = 0;
  if (!reader_fread_int32(&num_target_infos, reader))
  {
    return SFERROR(ReadFail);
  }
  DEBUGF("num_target_infos=%"PRId32"\n", num_target_infos);

  if (num_target_infos < 0 || num_target_infos > TargetInfoMax)
  {
    return SFERROR(BadNumTargetInfo);
  }

#ifndef NDEBUG
  static int32_t max_target_infos;
  max_target_infos = HIGHEST(max_target_infos, num_target_infos);
  DEBUGF("max_target_infos=%"PRId32"\n", max_target_infos);
#endif

  for (int32_t j = 0; j < num_target_infos; ++j)
  {
    DEBUGF("Reading target info %" PRId32 " data at %ld\n", j, reader_ftell(reader));
    CoarsePoint2d coords = {0};
    if (!CoarsePoint2d_read(&coords, reader))
    {
      return SFERROR(ReadFail);
    }

    SFError err = target_infos_add(target_infos, map_coords_from_coarse(coords), NULL);
    if (SFError_fail(err)) {
      return err;
    }

    if (reader_fseek(reader, TargetInfoPadding, SEEK_CUR))
    {
      return SFERROR(BadSeek);
    }
  }
  return SFERROR(OK);
}

size_t target_infos_get_count(TargetInfosData const *const target_infos)
{
  assert(target_infos);
  assert(target_infos->count >= 0);
  assert(target_infos->count <= TargetInfoMax);
  return target_infos->count;
}

size_t target_infos_get_text_count(TargetInfosData const *const target_infos)
{
  return target_infos_get_count(target_infos) * TargetInfoTextIndex_Count;
}

TargetInfo *target_info_from_index(TargetInfosData const *const target_infos,
  size_t const index)
{
  return intdict_get_value_at(&target_infos->dict, index);
}

void target_infos_write_pad(TargetInfosData *const target_infos, Writer *const writer)
{
  target_infos_write(target_infos, writer);
  if (writer_ferror(writer)) {
    return;
  }

  size_t const padding = TargetInfoAlloc - target_infos->count;
  writer_fseek(writer, (long)padding * BytesPerTargetInfo, SEEK_CUR);
  DEBUGF("Finished writing target info data at %ld\n", writer_ftell(writer));
}

void target_infos_write(TargetInfosData *const target_infos, Writer *const writer)
{
  assert(target_infos);
  assert(target_infos->count >= 0);
  assert(target_infos->count <= TargetInfoMax);
  writer_fwrite_int32((int32_t)target_infos->count, writer);

  IntDictVIter iter;
  for (TargetInfo *info = intdictviter_all_init(&iter, &target_infos->dict);
       info != NULL;
       info = intdictviter_advance(&iter)) {
    write_target_info_coords(info, writer);
    if (writer_ferror(writer))
    {
      return;
    }
  }
}

int target_infos_write_text_offsets(TargetInfosData *const target_infos,
  Writer *const writer, int offset)
{
  assert(target_infos);

  IntDictVIter iter;
  for (TargetInfo *info = intdictviter_all_init(&iter, &target_infos->dict);
       info != NULL;
       info = intdictviter_advance(&iter)) {
    write_target_info_offset(info, writer, &offset);
    if (writer_ferror(writer))
    {
      break;
    }
  }

  return offset;
}

void target_infos_write_texts(TargetInfosData *const target_infos, Writer *const writer)
{
  assert(target_infos);

  IntDictVIter iter;
  for (TargetInfo *info = intdictviter_all_init(&iter, &target_infos->dict);
       info != NULL;
       info = intdictviter_advance(&iter)) {
    write_target_info_text(info, writer);
    if (writer_ferror(writer))
    {
      return;
    }
  }
}

SFError target_infos_read_texts(TargetInfosData *const target_infos,
                                long int const *const offsets, size_t const count,
                                Reader *const reader)
{
  assert(target_infos);
  size_t i = 0;
  SFError err = SFERROR(OK);

  IntDictVIter iter;
  for (TargetInfo *info = intdictviter_all_init(&iter, &target_infos->dict);
       info != NULL;
       info = intdictviter_advance(&iter)) {
    assert(i <= count);
    if (i == count)
    {
      break;
    }

    err = read_target_info_text(info, reader, offsets, i++);
    if (SFError_fail(err))
    {
      break;
    }
  }

  return err;
}

static size_t iter_loop_core(TargetInfosIter *const iter)
{
  assert(iter);

  for (; iter->next_index < iter->end; iter->next_index++) {
    TargetInfo *const info = intdict_get_value_at(iter->dict, iter->next_index);

    MapPoint const coords = map_coords_from_coarse(info->coords);
    if (map_bbox_contains(&iter->map_area, coords)) {
      DEBUGF("Getting target info %zu at coordinates %d,%d\n",
             iter->next_index, info->coords.x, info->coords.y);
      return iter->next_index++;
    }
  }

  assert(!iter->done);
  iter->done = true;
  return 0;
}

size_t TargetInfosIter_get_first(TargetInfosIter *const iter,
  TargetInfosData *const infos, MapArea const *const map_area)
{
  assert(iter != NULL);
  assert(infos != NULL);
  assert(MapArea_is_valid(map_area));

  MapCoord min_key, max_key;
  map_area_to_key_range(map_area, &min_key, &max_key);

  *iter = (TargetInfosIter){
    .map_area = *map_area,
    .dict = &infos->dict,
    .next_index = intdict_bisect_left(&infos->dict, min_key),
    .end = intdict_bisect_right(&infos->dict, max_key),
  };

  return iter_loop_core(iter);
}

size_t TargetInfosIter_get_next(TargetInfosIter *const iter)
{
  assert(iter != NULL);
  assert(!iter->done);
  assert(MapArea_is_valid(&iter->map_area));
  return iter_loop_core(iter);
}

void TargetInfosIter_del_current(TargetInfosIter *const iter)
{
  assert(iter != NULL);
  assert(iter->next_index > 0);
  assert(iter->end > 0);
  assert(!iter->done);
  assert(MapArea_is_valid(&iter->map_area));

  --iter->end;
  size_t const index = --iter->next_index;
  free_info(intdict_get_value_at(iter->dict, index));
  intdict_remove_at(iter->dict, index);
}
