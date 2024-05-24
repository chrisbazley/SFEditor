/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission file
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

#include "Macros.h"
#include "Debug.h"
#include "Reader.h"
#include "Writer.h"

#include "Mission.h"
#include "MissionData.h"
#include "Briefing.h"
#include "Text.h"
#include "Filenames.h"
#include "Ships.h"
#include "Paths.h"
#include "Infos.h"
#include "Triggers.h"
#include "Defenc.h"
#include "Player.h"
#include "FPerf.h"
#include "BPerf.h"
#include "Pyram.h"
#include "Clouds.h"
#include "Utils.h"

/* ---------------- Public functions ---------------- */

enum {
  BytesPerTextOffset = 4,
  BytesPerTitle = 32,
  TextTerm = 255,
  TextOffsetCount = (TargetInfoMax * TargetInfoTextIndex_Count) + BriefingMax,
  TextOffsetMin = (TextOffsetCount * BytesPerTextOffset) + BytesPerTitle,
  TotalTextSize = 3072,
  TotalFileSize = 7060,
  Misc1Padding = 2,
  Misc2Padding = 30,
};

static SFError mission_type_read(MissionData *const mission,
  Reader *const reader)
{
  assert(mission);

  if (reader_fseek(reader, Misc1Padding, SEEK_CUR))
  {
    return SFERROR(BadSeek);
  }

  int const type = reader_fgetc(reader);
  if (type == EOF)
  {
    return SFERROR(ReadFail);
  }
  DEBUGF("Space: %d\n", type);
  if (type < MissionType_Normal && type > MissionType_Cyber)
  {
    return SFERROR(BadMissionType);
  }
  mission->type = type;
  return SFERROR(OK);
}

static SFError read_dock_to_finish(MissionData *const mission,
  Reader *const reader)
{
  int const dock_to_finish = reader_fgetc(reader);
  if (dock_to_finish == EOF)
  {
    return SFERROR(ReadFail);
  }
  DEBUGF("End-docked %d\n", dock_to_finish);
  if (dock_to_finish != 0 && dock_to_finish != 1)
  {
    return SFERROR(BadEndDocked);
  }
  mission->dock_to_finish = dock_to_finish;

  return SFERROR(OK);
}

static void mission_type_write(MissionData const *const mission,
  Writer *const writer)
{
  assert(mission);

  writer_fseek(writer, Misc1Padding, SEEK_CUR);

  assert(mission->type >= MissionType_Normal);
  assert(mission->type <= MissionType_Cyber);
  writer_fputc(mission->type, writer);
}

static void write_dock_to_finish(MissionData const *const mission,
  Writer *const writer)
{
  writer_fputc(mission->dock_to_finish, writer);
}

static SFError misc2_read(MissionData *const mission,
  Reader *const reader)
{
  assert(mission);

  int const scanners_down = reader_fgetc(reader);
  if (scanners_down == EOF)
  {
    return SFERROR(ReadFail);
  }
  if (scanners_down != 0 && scanners_down != 1)
  {
    return SFERROR(BadScannersDown);
  }
  mission->scanners_down = scanners_down;

  int const impervious_map = reader_fgetc(reader);
  if (impervious_map == EOF)
  {
    return SFERROR(ReadFail);
  }
  if (impervious_map != 0 && impervious_map != 1)
  {
    return SFERROR(BadImperviousMap);
  }
  mission->impervious_map = impervious_map;

  if (reader_fseek(reader, Misc2Padding, SEEK_CUR))
  {
    return SFERROR(BadSeek);
  }
  DEBUGF("Finished reading misc2 data at %ld\n", reader_ftell(reader));
  return SFERROR(OK);
}

static void misc2_write(MissionData const *const mission,
  Writer *const writer)
{
  assert(mission);

  writer_fputc(mission->scanners_down, writer);
  writer_fputc(mission->impervious_map, writer);
  writer_fseek(writer, Misc2Padding, SEEK_CUR);
  DEBUGF("Finished writing misc2 data at %ld\n", writer_ftell(writer));
}

SFError texts_read(MissionData *const mission,
  Reader *const reader)
{
  assert(mission);

  int32_t tmp = 0;
  if (!reader_fread_int32(&tmp, reader))
  {
    return SFERROR(ReadFail);
  }

  if (tmp < BriefingMin || tmp > TextOffsetCount)
  {
    return SFERROR(BadNumBriefingStrings);
  }

  size_t const btexts = (size_t)tmp;

#ifndef NDEBUG
  static size_t max_btexts;
  max_btexts = HIGHEST(btexts, max_btexts);
  DEBUGF("max_btexts=%zu\n", max_btexts);
#endif

  if (!reader_fread_int32(&tmp, reader))
  {
    return SFERROR(ReadFail);
  }

  /* Hard mission 4 has unused target information text so allow that */
  DEBUGF("%"PRId32" target infos, expected %zu\n", tmp, target_infos_get_count(&mission->target_infos));
  if (tmp < 0 || (size_t)tmp < target_infos_get_count(&mission->target_infos))
  {
    return SFERROR(TooFewTargetInfoStrings);
  }
  size_t const num_infos = (size_t)tmp;

  size_t const ttexts = target_infos_get_text_count(&mission->target_infos);
  if (ttexts > (TextOffsetCount - btexts))
  {
    return SFERROR(TooManyStrings);
  }
  DEBUGF("Finished reading strings header data at %ld\n", reader_ftell(reader));

  /* Read the whole index in the hope of avoiding backward seeks. */
  long int const index_start = reader_ftell(reader);
  if (index_start < 0)
  {
    return SFERROR(BadTell);
  }
  long int offsets[TextOffsetCount];
  for (size_t t = 0; t < btexts + ttexts; ++t)
  {
    int32_t offset;
    if (!reader_fread_int32(&offset, reader))
    {
      return SFERROR(ReadFail);
    }

    if ((offset < TextOffsetMin) || (offset >= (TextOffsetMin + TotalTextSize)) ||
        (offset != WORD_ALIGN(offset)))
    {
      return SFERROR(BadStringOffset);
    }
    offsets[t] = index_start + offset;
  }

  size_t const padding = TextOffsetCount - btexts - ttexts;
  if (reader_fseek(reader, (long)padding * BytesPerTextOffset, SEEK_CUR))
  {
    return SFERROR(BadSeek);
  }
  DEBUGF("Finished reading strings index data at %ld\n", reader_ftell(reader));

  if (reader_fgetc(reader) != TextTerm)
  {
    return SFERROR(BadTitleString);
  }

  if (reader_fseek(reader, BytesPerTitle - 1, SEEK_CUR))
  {
    return SFERROR(BadSeek);
  }
  DEBUGF("Finished reading title string data at %ld\n", reader_ftell(reader));

  SFError err = briefing_read_texts(&mission->briefing,
                                    offsets, btexts, reader);
  if (SFError_fail(err))
  {
    return err;
  }
  DEBUGF("Finished reading briefing strings data at %ld\n", reader_ftell(reader));

  err = target_infos_read_texts(&mission->target_infos,
                                offsets + btexts, num_infos, reader);
  if (SFError_fail(err))
  {
    return err;
  }
  DEBUGF("Finished reading target info strings data at %ld\n", reader_ftell(reader));
  return SFERROR(OK);
}

void texts_write(MissionData *const mission, Writer *const writer)
{
  assert(mission);

  size_t const btexts = briefing_get_text_count(&mission->briefing);
  assert(btexts < INT32_MAX);
  writer_fwrite_int32((int32_t)btexts, writer);

  size_t const tinfo_count = target_infos_get_count(&mission->target_infos);
  assert(tinfo_count < INT32_MAX);
  writer_fwrite_int32((int32_t)tinfo_count, writer);

  size_t const ttexts = target_infos_get_text_count(&mission->target_infos);
  assert(btexts + ttexts <= TextOffsetCount);
  DEBUGF("Finished writing strings header data at %ld\n", writer_ftell(writer));

  int offset = briefing_write_text_offsets(&mission->briefing, writer, TextOffsetMin);
  offset = target_infos_write_text_offsets(&mission->target_infos, writer, offset);
  assert(offset >= TextOffsetMin);
  assert(offset - TextOffsetMin <= TotalTextSize);

  size_t const padding = TextOffsetCount - btexts - ttexts;
  assert(padding >= 0);
  writer_fseek(writer, (long)padding * BytesPerTextOffset, SEEK_CUR);
  DEBUGF("Finished writing string index data at %ld\n", writer_ftell(writer));

  writer_fputc(TextTerm, writer);
  writer_fseek(writer, BytesPerTitle - 1, SEEK_CUR);
  DEBUGF("Finished writing title string data at %ld\n", writer_ftell(writer));

  briefing_write_texts(&mission->briefing, writer);
  DEBUGF("Finished writing briefing strings data at %ld\n", writer_ftell(writer));

  target_infos_write_texts(&mission->target_infos, writer);
  DEBUGF("Finished writing target info strings data at %ld\n", writer_ftell(writer));
}

static SFError init_all(MissionData *const mission)
{
  assert(mission);
#ifdef FORTIFY
  Fortify_CheckAllMemory();
#endif

  SFError err = triggers_init(&mission->triggers);
  if (SFError_fail(err)) {
    return err;
  }
  target_infos_init(&mission->target_infos);
  ships_init(&mission->ships);
  paths_init(&mission->paths);
  briefing_init(&mission->briefing);

#ifdef FORTIFY
  Fortify_CheckAllMemory();
#endif
  return SFERROR(OK);
}

static void destroy_all(MissionData *const mission)
{
  assert(mission);
  triggers_destroy(&mission->triggers);
  target_infos_destroy(&mission->target_infos);
  ships_destroy(&mission->ships);
  paths_destroy(&mission->paths);
  briefing_destroy(&mission->briefing);
}

static SFError read_inner(MissionData *const mission,
  Reader *const reader)
{
  SFError err = player_read(&mission->player, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  err = triggers_read_max_losses(&mission->triggers, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  err = mission_type_read(mission, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  err = clouds_read(&mission->clouds, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  err = read_dock_to_finish(mission, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  err = player_read_docked(&mission->player, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  err = triggers_read_pad(&mission->triggers, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  err = target_infos_read_pad(&mission->target_infos, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  int32_t timer = 0;
  if (!reader_fread_int32(&timer, reader))
  {
    return SFERROR(ReadFail);
  }

  if (timer < 0)
  {
    return SFERROR(BadMissionTimer);
  }
  mission->time_limit = timer;
  DEBUGF("Finished reading mission timer data at %ld\n", reader_ftell(reader));

  err = defences_read(&mission->defences, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  err = pyramid_read(&mission->pyramid, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  err = misc2_read(mission, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  err = fighter_perform_read(&mission->fighter_perform, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  err = big_perform_read(&mission->big_perform, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  err = ships_read_pad(&mission->ships, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  err = paths_read_pad(&mission->paths, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  err = ships_post_read(&mission->ships, &mission->paths);
  if (SFError_fail(err))
  {
    return err;
  }

  err = player_post_read(&mission->player, &mission->ships);
  if (SFError_fail(err))
  {
    return err;
  }

  err = filenames_read(&mission->filenames, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  return texts_read(mission, reader);
}

static SFError mission_read_cb(DFile const *const dfile, Reader *const reader)
{
  assert(dfile);
  assert(reader);
  MissionData *const mission = CONTAINER_OF(dfile, MissionData, dfile);
  DEBUGF("Reading mission data %p wrapping dfile %p\n",
         (void*)mission, (void*)dfile);

  destroy_all(mission);
  SFError err = init_all(mission);
  if (SFError_fail(err)) {
    return err;
  }

  err = check_trunc_or_ext(reader, read_inner(mission, reader));
  assert(reader_ftell(reader) <= TotalFileSize);

  if (err.type == SFErrorType_TooLong && reader_ftell(reader) <= TotalFileSize)
  {
    /* Don't have to read the whole file */
    err = SFERROR(OK);
  }

  return err;
}

static long int mission_get_min_size_cb(DFile const *const dfile)
{
  NOT_USED(dfile);
  return TotalFileSize;
}

static void mission_destroy_cb(DFile const *const dfile)
{
  assert(dfile);
  MissionData *const mission = CONTAINER_OF(dfile, MissionData, dfile);
  destroy_all(mission);
  dfile_destroy(&mission->dfile);
  free(mission);
}

static void mission_write_cb(DFile const *const dfile, Writer *const writer)
{
  assert(dfile);
  assert(writer);
  MissionData *const mission = CONTAINER_OF(dfile, MissionData, dfile);
  DEBUGF("Writing mission data %p wrapping dfile %p\n",
         (void*)mission, (void*)dfile);

  ships_pre_write(&mission->ships);
  paths_pre_write(&mission->paths);

  player_write(&mission->player, writer);
  triggers_write_max_losses(&mission->triggers, writer);
  mission_type_write(mission, writer);
  clouds_write(&mission->clouds, writer);
  write_dock_to_finish(mission, writer);
  player_write_docked(&mission->player, writer);
  triggers_write_pad(&mission->triggers, writer);
  target_infos_write_pad(&mission->target_infos, writer);
  writer_fwrite_int32(mission->time_limit, writer);
  DEBUGF("Finished writing mission timer data at %ld\n", writer_ftell(writer));
  defences_write(&mission->defences, writer);
  pyramid_write(&mission->pyramid, writer);
  misc2_write(mission, writer);
  fighter_perform_write(&mission->fighter_perform, writer);
  big_perform_write(&mission->big_perform, writer);
  ships_write_pad(&mission->ships, writer);
  paths_write_pad(&mission->paths, writer);
  filenames_write(&mission->filenames, writer);

  texts_write(mission, writer);

  assert(writer_ftell(writer) <= TotalFileSize);
}

MissionType mission_get_type(MissionData const *const mission)
{
  assert(mission);
  return mission->type;
}

void mission_set_type(MissionData *const mission, MissionType const type)
{
  assert(mission);
  assert(type >= MissionType_Normal);
  assert(type <= MissionType_Cyber);
  mission->type = type;
}

bool mission_get_dock_to_finish(MissionData const *const mission)
{
  assert(mission);
  return mission->dock_to_finish;
}

void mission_set_dock_to_finish(MissionData *const mission,
  bool const dock_to_finish)
{
  assert(mission);
  mission->dock_to_finish = dock_to_finish;
}

bool mission_get_scanners_down(MissionData const *const mission)
{
  assert(mission);
  return mission->scanners_down;
}

void mission_set_scanners_down(MissionData *const mission,
  bool const scanners_down)
{
  assert(mission);
  mission->scanners_down = scanners_down;
}

bool mission_get_impervious_map(MissionData const *const mission)
{
  assert(mission);
  return mission->impervious_map;
}

void mission_set_impervious_map(MissionData *const mission,
  bool const impervious_map)
{
  assert(mission);
  mission->impervious_map = impervious_map;
}

int mission_get_time_limit(MissionData const *const mission)
{
  assert(mission);
  assert(mission->time_limit >= 0);
  return mission->time_limit;
}

void mission_set_time_limit(MissionData *const mission, int const time_limit)
{
  assert(mission);
  assert(time_limit > 0);
  mission->time_limit = time_limit;
}

void mission_disable_time_limit(MissionData *const mission)
{
  assert(mission);
  mission->time_limit = 0;
}

bool mission_time_limit_is_disabled(MissionData const *const mission)
{
  assert(mission);
  assert(mission->time_limit >= 0);
  return mission->time_limit == 0;
}

PyramidData *mission_get_pyramid(MissionData *const mission)
{
  assert(mission);
  return &mission->pyramid;
}

PlayerData *mission_get_player(MissionData *const mission)
{
  assert(mission);
  return &mission->player;
}

DefencesData *mission_get_defences(MissionData *const mission)
{
  assert(mission);
  return &mission->defences;
}

TriggersData *mission_get_triggers(MissionData *const mission)
{
  assert(mission);
  return &mission->triggers;
}

TargetInfosData *mission_get_target_infos(MissionData *const mission)
{
  assert(mission);
  return &mission->target_infos;
}

FighterPerformData *mission_get_fighter_perform(MissionData *const mission)
{
  assert(mission);
  return &mission->fighter_perform;
}

BigPerformData *mission_get_big_perform(MissionData *const mission)
{
  assert(mission);
  return &mission->big_perform;
}

PathsData *mission_get_paths(MissionData *const mission)
{
  assert(mission);
  return &mission->paths;
}

ShipsData *mission_get_ships(MissionData *const mission)
{
  assert(mission);
  return &mission->ships;
}

FilenamesData *mission_get_filenames(MissionData *const mission)
{
  assert(mission);
  return &mission->filenames;
}

BriefingData *mission_get_briefing(MissionData *const mission)
{
  assert(mission);
  return &mission->briefing;
}

CloudColData *mission_get_cloud_colours(MissionData *const mission)
{
  assert(mission);
  return &mission->clouds;
}

DFile *mission_get_dfile(MissionData *const mission)
{
  assert(mission);
  return &mission->dfile;
}

MissionData *mission_create(void)
{
  MissionData *const mission = malloc(sizeof(*mission));
  if (mission)
  {
    *mission = (MissionData){0};

    dfile_init(&mission->dfile, mission_read_cb, mission_write_cb,
               mission_get_min_size_cb, mission_destroy_cb);

    SFError err = init_all(mission);
    if (SFError_fail(err)) {
      free(mission);
      return NULL;
    }
  }
  return mission;
}
