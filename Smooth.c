/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Smoothing wand implementation
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

#include "stdlib.h"
#include "stdio.h"
#include <assert.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>

#include "flex.h"

#include "err.h"
#include "msgtrans.h"
#include "Macros.h"
#include "filepaths.h"
#include "utils.h"
#include "hourglass.h"
#include "FileUtils.h"
#include "SFError.h"
#include "debug.h"
#include "Smooth.h"
#include "MapEdit.h"
#include "utils.h"
#include "MapCoord.h"
#include "SmoothData.h"

#define UX_STARTSMOOTHMARK "StartGroup"
#define STARTSMOOTHMARK UX_STARTSMOOTHMARK" %zu\n"
#define UX_ENDSMOOTHMARK "EndGroup"
#define ENDSMOOTHMARK UX_ENDSMOOTHMARK"\n"
#define UX_SMOOTHUNDEFMARK "UndefinedGroup"
#define SMOOTHUNDEFMARK UX_SMOOTHUNDEFMARK" %zu\n"
#define UX_SUBGROUP "SubGroup"
#define SUBGROUP UX_SUBGROUP" %zu\n"

enum {
  LineBufferSize = 255,
  InitGroupSize = 8,
  GroupGrowthFactor = 2,
  ChangesMismatch = 0,
  ContinuesMismatch = 1,
  FuzzyMatch = 4,
  PerfectMatch = 5,
  NumAdjacent = 4,
  MinScore = NumAdjacent * ChangesMismatch,
  MaxScore = NumAdjacent * PerfectMatch,
  MinFuzzyScore = NumAdjacent * FuzzyMatch,
  ErrBufferSize = 64,
};

/* array of TileSmoothData, in tile number order */
typedef struct
{
  bool dont_smooth;
  unsigned char main_group; /* if tile is undefined then all fields are 255 */
  unsigned char north_group;
  unsigned char east_group;
  unsigned char south_group;
  unsigned char west_group; /* (group numbers for this tile) */
} TileSmoothData;


/* ---------------- Private functions ---------------- */

static size_t get_group_member(TexGroupRoot *const group, size_t const index)
{
  assert(group != NULL);
  assert(index >= 0);
  assert(index < group->count);
  assert(group->array_anchor != NULL);
  DEBUG_VERBOSEF("get_group_member %p,%zu, count %zu\n", (void *)group, index,
                 group->count);
  assert(index < (size_t)flex_size(&group->array_anchor));

  const unsigned char *const member_array = group->array_anchor;
  return member_array[index];
}

static size_t calc_match_2(const MapTexGroups *const groups_data,
  size_t const main_group, size_t const cand_edge, size_t const ideal_edge)
{
  DEBUGF("Seeking match between candidate edge %zu and ideal %zu (in group %zu)\n",
         cand_edge, ideal_edge, main_group);

  if (cand_edge == UCHAR_MAX || ideal_edge == UCHAR_MAX) {
    DEBUG("Fuzzy match");
    return FuzzyMatch;
  }

  if (ideal_edge == cand_edge) {
    DEBUG("Simple perfect match");
    return PerfectMatch;
  }

  /* the advent of super-groups complicates things somewhat... */
  TexGroupRoot *const tile_groups = groups_data->array;
  TexGroupRoot *const a_group_def = &tile_groups[cand_edge];
  TexGroupRoot *const b_group_def = &tile_groups[ideal_edge];

  if (b_group_def->super) {
    for (size_t b_member = 0; b_member < b_group_def->count; b_member++) {
      assert(get_group_member(b_group_def, b_member) != UCHAR_MAX);
      if (a_group_def->super) {
        for (size_t a_member = 0; a_member < a_group_def->count; a_member++) {
          assert(get_group_member(a_group_def, a_member) != UCHAR_MAX);
          if (get_group_member(b_group_def, b_member) ==
              get_group_member(a_group_def, a_member)) {
            DEBUG("Perfect subgroup match");
            return PerfectMatch; /* perfect match of two subgroups */
          }
        }
      } else {
        if (get_group_member(b_group_def, b_member) == cand_edge) {
          DEBUG("Perfect match with B subgroup");
          return PerfectMatch; /* perfect match with B subgroup */
        }
      }
    }
  } else if (a_group_def->super) {
    for (size_t a_member = 0; a_member < a_group_def->count; a_member++) {
      assert(get_group_member(a_group_def, a_member) != UCHAR_MAX);
      if (get_group_member(a_group_def, a_member) == ideal_edge) {
        DEBUG("Perfect match with A subgroup");
        return PerfectMatch; /* perfect match with A subgroup */
      }
    }
  }

  /* Disfavour change in material type, if it's the wrong material */
  if (main_group != cand_edge) {
    DEBUGF("Mismatching material change\n");
    return ChangesMismatch;
  }

  DEBUGF("Mismatching material continuation\n");
  return ContinuesMismatch;
}

static size_t calc_match(const MapTexGroups *const groups_data,
  const TileSmoothData *const cand, const TileSmoothData *const ideal)
{
  size_t score = 0;

  size_t const main_group = cand->main_group;
  assert(main_group == ideal->main_group);
  score += calc_match_2(groups_data, main_group, cand->north_group, ideal->north_group);
  score += calc_match_2(groups_data, main_group, cand->east_group, ideal->east_group);
  score += calc_match_2(groups_data, main_group, cand->south_group, ideal->south_group);
  score += calc_match_2(groups_data, main_group, cand->west_group, ideal->west_group);

  DEBUG("Score is %zu for templates %d,%d,%d,%d and %d,%d,%d,%d", score,
        cand->north_group, cand->east_group, cand->south_group, cand->west_group,
        ideal->north_group, ideal->east_group, ideal->south_group, ideal->west_group);

  assert(score >= MinScore);
  assert(score <= MaxScore);
  return score;
}

static void init_group(TexGroupRoot *const tile_group)
{
  assert(tile_group != NULL);
  tile_group->array_anchor = NULL;
  tile_group->count = 0;
  tile_group->super = false;
}

static bool add_group_member(TexGroupRoot *const group, size_t const new_member)
{
  assert(group != NULL);
  assert(new_member >= 0);
  assert(new_member < UCHAR_MAX);

  /*
    Create or extend the flexblock holding this group's members
  */
  bool success = true;

  if (group->count == 0) {
    success = flex_alloc(&group->array_anchor, InitGroupSize);
  } else {
    size_t const size = (size_t)flex_size(&group->array_anchor);
    if (group->count >= size) {
      success = flex_extend(&group->array_anchor, (int)size * GroupGrowthFactor);
    }
  }

  if (success) {
    /* Add tile number to array of group members */
    DEBUG("Adding member %zu to group %p at index %zu", new_member, (void *)group,
          group->count);

    unsigned char *const member_array = group->array_anchor;
    member_array[group->count++] = new_member;
  }

  return success;
}

static void set_tile_smooth_data(MapTexGroups *const groups_data, MapRef const tile,
  bool const dont_smooth, size_t const main,
  size_t const n, size_t const e, size_t const s, size_t const w)
{
  assert(groups_data != NULL);
  assert(groups_data->smooth_anchor != NULL);
  size_t const index = map_ref_to_num(tile);
  assert(index * sizeof(TileSmoothData) < (size_t)flex_size(&groups_data->smooth_anchor));
  assert(main >= 0);
  assert(main == UCHAR_MAX || main < groups_data->count);
  assert(n >= 0);
  assert(n == UCHAR_MAX || n < groups_data->count);
  assert(e >= 0);
  assert(e == UCHAR_MAX || e < groups_data->count);
  assert(s >= 0);
  assert(s == UCHAR_MAX || s < groups_data->count);
  assert(w >= 0);
  assert(w == UCHAR_MAX || w < groups_data->count);

  ((TileSmoothData *)groups_data->smooth_anchor)[index] = (TileSmoothData){
    .dont_smooth = dont_smooth,
    .main_group = main,
    .north_group = n,
    .east_group = e,
    .south_group = s,
    .west_group = w
  };
}

static void init_tile_smooth_data(MapTexGroups *const groups_data, MapRef const tile)
{
  /* If there is no UndefinedGroup specified then it is legitimate for
     undefined tiles to remain in the TileSmoothData array. That is why we
     mark ALL fields. */
  set_tile_smooth_data(groups_data, tile, true, UCHAR_MAX,
    UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX);
}

static void init_smooth_data(MapTexGroups *const groups_data, size_t const ntiles)
{
  assert(groups_data);
  groups_data->ntiles = ntiles;

  for (size_t tile_number = 0; tile_number < ntiles; tile_number++) {
    init_tile_smooth_data(groups_data, map_ref_from_num(tile_number));
  }
}

static inline TileSmoothData get_tile_smooth_data(
  MapTexGroups *const groups_data, MapRef const tile)
{
  assert(groups_data != NULL);
  assert(groups_data->smooth_anchor != NULL);

  size_t const index = map_ref_to_num(tile);
  if (index < groups_data->ntiles)
  {
    assert(index * sizeof(TileSmoothData) < (size_t)flex_size(&groups_data->smooth_anchor));
    return ((TileSmoothData *)groups_data->smooth_anchor)[index];
  }
  else
  {
    return (TileSmoothData){
      .dont_smooth = true,
      .main_group = UCHAR_MAX,
      .north_group = UCHAR_MAX,
      .east_group = UCHAR_MAX,
      .south_group = UCHAR_MAX,
      .west_group = UCHAR_MAX};
  }
}

static size_t count_groups_in_file(FILE *const file)
{
  assert(file != NULL);
  assert(!ferror(file));

  size_t num_groups = 0;
  char read_line[LineBufferSize];

  while (read_line_comm(read_line, sizeof(read_line), file, NULL) != NULL) {
    /* KISS - no syntax checking etc on first pass */
    int num_inputs = 0;
    size_t group;
    if (strncmp(read_line, UX_STARTSMOOTHMARK,
                sizeof(UX_STARTSMOOTHMARK) - 1) == 0) {
      /* extract group number */
      num_inputs = sscanf(read_line, STARTSMOOTHMARK, &group);
    } else {
      if (strncmp(read_line, UX_SMOOTHUNDEFMARK,
                  sizeof(UX_SMOOTHUNDEFMARK) - 1) == 0) {
        /* extract group number */
        num_inputs = sscanf(read_line, SMOOTHUNDEFMARK, &group);
      }
    }
    if (num_inputs == 1 && group < UCHAR_MAX && group >= num_groups) {
      num_groups = group + 1;
    }
  }
  DEBUG("No. of groups found on first pass:%zu", num_groups);
  return num_groups;
}

static TexGroupRoot *alloc_groups(size_t const ngroups)
{
  assert(ngroups > 0);
  TexGroupRoot *const roots_array = calloc(ngroups, sizeof(*roots_array));
  if (roots_array != NULL) {
    /* For each group, set the flex anchor to NULL and the membership to 0 */
    for (size_t i = 0; i < ngroups; ++i) {
      init_group(&roots_array[i]);
    }
  }
  return roots_array;
}

static bool add_undef_to_group(MapTexGroups *const groups_data,
  size_t const undef_group, size_t const ntiles)
{
  assert(groups_data != NULL);
  assert(undef_group >= 0);
  assert(undef_group < UCHAR_MAX);

  /*
    Append undefined tiles to specified group
  */
  TexGroupRoot *const pgroup = &groups_data->array[undef_group];

  for (size_t tile_number = 0; tile_number < ntiles; tile_number++) {
    TileSmoothData const smooth_data =
      get_tile_smooth_data(groups_data, map_ref_from_num(tile_number));

    if (smooth_data.main_group != UCHAR_MAX)
      continue;

    /* Found an undefined tile */
    set_tile_smooth_data(groups_data, map_ref_from_num(tile_number), smooth_data.dont_smooth,
      undef_group, undef_group, undef_group, undef_group, undef_group);

    /* Append to list of group members */
    DEBUG("Adding undefined tile %zu to group %zu", tile_number, undef_group);
    if (!add_group_member(pgroup, tile_number)) {
      return false;
    }
  } /* next tile_number */

  return true;
}

static SFError read_from_file(FILE *const file,
  MapTexGroups *const groups_data, size_t *const undef_group,
  size_t const ntiles, char *const err_buf)
{
  assert(file != NULL);
  assert(!ferror(file));
  assert(groups_data != NULL);
  assert(err_buf);

  *err_buf = '\0';

  char read_line[LineBufferSize];
  bool block = false;
  int line = 0;
  size_t group_num = 0;
  TexGroupRoot *pgroup = NULL;
  size_t const ngroups = groups_data->count;

  while (read_line_comm(read_line, sizeof(read_line), file, &line) != NULL)
  {
    if (strncmp(read_line, UX_STARTSMOOTHMARK,
                sizeof(UX_STARTSMOOTHMARK) - 1) == 0)
    {
      if (block) {
        /* syntax error - already in block */
        sprintf(err_buf, "%d", line);
        return SFERROR(Unexp);
      }

      /*
        Start of group - extract group number
      */
      size_t group;
      int const num_inputs = sscanf(read_line, STARTSMOOTHMARK, &group);
      if (num_inputs != 1) {
        /* Report syntax error and line number */
        sprintf(err_buf, "%d", line);
        return SFERROR(Mistake);
      }

      if (group < 0 || group >= ngroups) {
        /* Report group no. out of range and line number */
        sprintf(err_buf, "%d", line);
        return SFERROR(GroupRange);
      }
      group_num = group;
      pgroup = &groups_data->array[group_num];
      block = true;

      continue; /* read next line */
    }

    if (strcmp(read_line, ENDSMOOTHMARK) == 0) {
      if (!block) {
        /* syntax error - not in block */
        sprintf(err_buf, "%d", line);
        return SFERROR(Unexp);
      }
      /*
        End of group definition
      */
      block = false;

      continue; /* read next line */
    }

    if (strncmp(read_line, UX_SMOOTHUNDEFMARK,
                sizeof(UX_SMOOTHUNDEFMARK) - 1) == 0) {
      if (block) {
        /* syntax error - in block */
        sprintf(err_buf, "%d", line);
        return SFERROR(Unexp);
      }

      /*
        Group for undefined tiles  - extract group number
      */
      size_t group;
      int const num_inputs = sscanf(read_line, SMOOTHUNDEFMARK, &group);
      if (num_inputs != 1) {
        /* Report syntax error and line number */
        sprintf(err_buf, "%d", line);
        return SFERROR(Mistake);
      }

      if (group < 0 || group >= ngroups) {
        /* Report group no. out of range and line number */
        sprintf(err_buf, "%d", line);
        return SFERROR(GroupRange);
      }
      *undef_group = group;

      continue; /* read next line */
    }

    if (strncmp(read_line, UX_SUBGROUP, sizeof(UX_SUBGROUP) - 1) == 0) {
      if (!block) {
        /* Error - not inside group definition */
        sprintf(err_buf, "%d", line);
        return SFERROR(Unexp);
      }

      if (pgroup->count > 0 && !pgroup->super) {
        /* Error - not a super group */
        sprintf(err_buf, "%d", line);
        return SFERROR(MixMem);
      }

      /*
        Subgroup definition - extract group number
      */
      size_t group;
      int const num_inputs = sscanf(read_line, SUBGROUP, &group);
      if (num_inputs != 1) {
        /* Report syntax error and line number */
        sprintf(err_buf, "%d", line);
        return SFERROR(Mistake);
      }

      if (group < 0 || group >= ngroups) {
        /* Report group no. out of range and line number */
        sprintf(err_buf, "%d", line);
        return SFERROR(GroupRange);
      }

      if (!add_group_member(pgroup, group)) {
        sprintf(err_buf, "%d", line);
        return SFERROR(NoMem);
      }
      pgroup->super = true;
      continue; /* read next line */
    }

    if (!block) {
      /* unknown non-comment text outside block */
      sprintf(err_buf, "%d", line);
      return SFERROR(Mistake);
    }

    /*
      Now we expect a tile definition in the form
      Texnum: May smooth?, N group, E group, S group, W group
    */

    int no_smooth;
    size_t tile, n, e, s, w; /* N.B. can't read directly into chars */
    int const num_inputs = sscanf(read_line, "%zu:%d,%zu,%zu,%zu,%zu", &tile,
                     &no_smooth, &n, &e, &s, &w);
    if (num_inputs != 6 || no_smooth > 1) {
      /* Report syntax error and line number */
      sprintf(err_buf, "%d", line);
      return SFERROR(Mistake);
    }

    if (pgroup->count > 0 && pgroup->super) {
      /* syntax error - tile definitions not allowed in a super group */
      sprintf(err_buf, "%d", line);
      return SFERROR(MixMem);
    }

    if ((n != UCHAR_MAX && n >= ngroups) ||
        (e != UCHAR_MAX && e >= ngroups) ||
        (s != UCHAR_MAX && s >= ngroups) ||
        (w != UCHAR_MAX && w >= ngroups)) {
      /* Report group no. out of range and line number */
      sprintf(err_buf, "%d", line);
      return SFERROR(GroupRange);
    }

    if (tile < 0 || tile >= ntiles) {
      /* Tile number out of range */
      sprintf(err_buf, "%d", line);
      return SFERROR(NumRange);
    }

    if (!add_group_member(pgroup, tile)) {
      sprintf(err_buf, "%d", line);
      return SFERROR(NoMem);
    }
    pgroup->super = false;

    /* Enter full smoothing data into TileSmoothData array */
    set_tile_smooth_data(groups_data, map_ref_from_num(tile), no_smooth != 0, group_num,
      n, e, s, w);
  } /* endwhile */

  if (block) {
    /* syntax error - no end of block before EOF */
    strcpy(err_buf, ENDSMOOTHMARK);
    return SFERROR(EOF);
  }

  return SFERROR(OK);
}

/* ----------------- Public functions ---------------- */

void MapTexGroups_edit(char const *const tiles_set)
{
  edit_file(TILEGROUPS_DIR, tiles_set);
}

void MapTexGroups_init(MapTexGroups *const groups_data)
{
  assert(groups_data);
  *groups_data = (MapTexGroups){.count = 0, .ntiles = 0};
}

void MapTexGroups_load(MapTexGroups *const groups_data, char const *tiles_set, size_t const ntiles)
{
  char *const full_path = make_file_path_in_dir(
                          CHOICES_READ_PATH TILEGROUPS_DIR, tiles_set);
  if (!full_path) {
    return;
  }

  MapTexGroups_free(groups_data);
  MapTexGroups_init(groups_data);

  char err_buf[ErrBufferSize] = "";
  SFError err = SFERROR(OK);

  hourglass_on();
  if (file_exists(full_path)) {
    /*
      Make table for quick look-up of smoothing data for a given tile
    */
    if (!flex_alloc(&groups_data->smooth_anchor,
                    (int)(ntiles * sizeof(TileSmoothData)))) {
      err = SFERROR(NoMem);
    } else {
      /*
        Mark all tiles with magic (reserved) group number
      */
      init_smooth_data(groups_data, ntiles);

      size_t undef_group = SIZE_MAX; /* default is to append undefined tiles to no group */

      DEBUG("Opening tile groups file '%s'", full_path);
      FILE *const file = fopen(full_path, "r");
      if (file == NULL) {
        err = SFERROR(OpenInFail);
      } else {
        /*
          First pass over file is to establish the maximum group number
          (Cannot extend the pointers array using realloc() since the flex anchors
          would be be invalidated)
        */
        groups_data->count = count_groups_in_file(file);
        if (groups_data->count > 0) {
          groups_data->array = alloc_groups(groups_data->count);
          if (groups_data->array == NULL) {
            err = SFERROR(NoMem);
          } else {
            /*
              Second pass over file is to actually read the smoothing data
            */
            fseek(file, 0, SEEK_SET); /* back to beginning of file */
            err = read_from_file(file, groups_data, &undef_group, ntiles, err_buf);
          }
        } /* endif (groups_data->count > 0) */
      }
      fclose(file);

      if (!SFError_fail(err) && undef_group != SIZE_MAX) {
        if (!add_undef_to_group(groups_data, undef_group, ntiles)) {
          err = SFERROR(NoMem);
        }
      }
    }
  }
  hourglass_off();

  report_error(err, full_path, err_buf);
  free(full_path);
}

size_t MapTexGroups_get_count(MapTexGroups const *const groups_data)
{
  assert(groups_data != NULL);
  assert(groups_data->count >= 0);
  DEBUGF("There are %zu texture groups\n", groups_data->count);
  return groups_data->count;
}

size_t MapTexGroups_get_num_group_members(MapTexGroups *const groups_data, size_t const group)
{
  assert(groups_data != NULL);
  assert(group >= 0);
  assert(group < groups_data->count);
  const TexGroupRoot *const pgroup = &groups_data->array[group];
  size_t const n = pgroup->super ? 0 : pgroup->count;
  DEBUGF("There are %zu members of texture group %zu\n", n, group);
  return n;
}

MapRef MapTexGroups_get_group_member(MapTexGroups *const groups_data, size_t const group,
  size_t const index)
{
  assert(groups_data != NULL);
  assert(group >= 0);
  assert(group < groups_data->count);
  TexGroupRoot *const pgroup = &groups_data->array[group];
  size_t const tile = get_group_member(pgroup, index);
  DEBUGF("Member %zu of texture group %zu is tile %zu\n", index, group, tile);
  return map_ref_from_num(tile);
}

size_t MapTexGroups_get_group_of_tile(MapTexGroups *const groups_data, MapRef const tile)
{
  assert(groups_data != NULL);
  size_t const index = map_ref_to_num(tile);
  assert(index >= 0);
  assert(index * sizeof(TileSmoothData) < (size_t)flex_size(&groups_data->smooth_anchor));

  size_t const group = ((TileSmoothData *)groups_data->smooth_anchor)[index].main_group;
  DEBUGF("Tile %zu is a member of texture group %zu\n", index, group);
  return group;
}

void MapTexGroups_free(MapTexGroups *const groups_data)
{
  assert(groups_data != NULL);
  assert(groups_data->count >= 0);

  if (groups_data->array != NULL) {
    TexGroupRoot *tile_groups = groups_data->array;
    for (size_t i = 0; i < groups_data->count; i++) {
      if (tile_groups[i].array_anchor != NULL)
        flex_free(&tile_groups[i].array_anchor);
    }
    free(tile_groups);
  }
  groups_data->count = 0;

  if (groups_data->smooth_anchor != NULL) {
    flex_free(&groups_data->smooth_anchor);
  }
}

void MapTexGroups_smooth(MapEditContext const *const map,
  MapTexGroups *const groups_data, MapPoint const map_pos,
  MapEditChanges *const change_info)
{
  DEBUG("Will attempt to smooth tile at %" PRIMapCoord ",%" PRIMapCoord,
        map_pos.x, map_pos.y);

  assert(groups_data != NULL);
  if (groups_data->smooth_anchor == NULL || groups_data->array == NULL)
    return; /* can do nothing without smoothing data! */

  MapRef const Ctile = MapEdit_read_tile(map, map_pos);
  if (map_ref_is_mask(Ctile)) {
    DEBUG("no tile at this location");
    return; /* cannot smooth non-tile */
  }

  TileSmoothData const our_tile = get_tile_smooth_data(groups_data, Ctile);
  if (our_tile.main_group == UCHAR_MAX) {
    DEBUG("tile %zu is member of no group", map_ref_to_num(Ctile));
    return; /* can do nothing if tile undefined */
  }

  if (our_tile.dont_smooth) {
    DEBUG("tile %zu cannot be smoothed", map_ref_to_num(Ctile));
    return; /* some tiles are locked against change */
  }

  DEBUG("tile:%zu (group %d)", map_ref_to_num(Ctile), our_tile.main_group);

  DEBUG("tile's edges - N:%d E:%d S:%d W:%d", our_tile.north_group,
        our_tile.east_group, our_tile.south_group, our_tile.west_group);

  MapPoint const north = {map_pos.x, map_pos.y + 1};
  MapRef const n_tile = MapEdit_read_tile(map, north);

  MapPoint const east = {map_pos.x + 1, map_pos.y};
  MapRef const e_tile = MapEdit_read_tile(map, east);

  MapPoint const south = {map_pos.x, map_pos.y - 1};
  MapRef const s_tile = MapEdit_read_tile(map, south);

  MapPoint const west = {map_pos.x - 1, map_pos.y};
  MapRef const w_tile = MapEdit_read_tile(map, west);

  TileSmoothData const ideal_tile = {
    .main_group = our_tile.main_group,
    .north_group = map_ref_is_mask(n_tile) ? UCHAR_MAX :
                   get_tile_smooth_data(groups_data, n_tile).south_group,
    .east_group = map_ref_is_mask(e_tile) ? UCHAR_MAX :
                  get_tile_smooth_data(groups_data, e_tile).west_group,
    .south_group = map_ref_is_mask(s_tile) ? UCHAR_MAX :
                   get_tile_smooth_data(groups_data, s_tile).north_group,
    .west_group = map_ref_is_mask(w_tile) ? UCHAR_MAX :
                  get_tile_smooth_data(groups_data, w_tile).east_group
  };

  DEBUG("adjacent edges - N:%d E:%d S:%d W:%d", ideal_tile.north_group,
        ideal_tile.east_group, ideal_tile.south_group, ideal_tile.west_group);

  size_t const current_score = calc_match(groups_data, &our_tile, &ideal_tile);

  /* Bail if the current tile is already perfect */
  if (current_score >= MaxScore) {
    DEBUG("tile is OK - nothing to do");
    return; /* nothing to do */
  }
  DEBUG("Current tile scores %zu", current_score);
  /*
     Search for a replacement tile (within the same group)
     that fits better with the surrounding tiles
  */
  TexGroupRoot *const centre_group = &groups_data->array[
                                       ideal_tile.main_group];
  MapRef *const best_tiles = malloc(sizeof(*best_tiles) * (size_t)centre_group->count);
  if (!best_tiles) {
    report_error(SFERROR(NoMem), "", "");
    return;
  }

  size_t num_found = 0;
  size_t best_score = current_score;

  if (centre_group == NULL)
    return; /* don't know about this group! */

  assert(!centre_group->super);
  for (size_t member = 0; member < (size_t)centre_group->count; member++)
  {
    MapRef const member_tile = map_ref_from_num(get_group_member(centre_group, member));
    TileSmoothData const member_data =
      get_tile_smooth_data(groups_data, member_tile);

    //if (member_data.dont_smooth) {
    //  DEBUG("May not use tile %zu as substitute", member_tile);
    //  continue; /* may not use this one as substitute */
    //}
    size_t const score = calc_match(groups_data, &member_data,
                                 &ideal_tile);
    //if (score < MinFuzzyScore) {
    //  /* This tile is not a suitable replacement because at least one
    //     edge does not match AT ALL! */
    //  DEBUG("Discounting tile %zu (rubbish score %d)", member_tile,
    //        score);
    //  continue;
    //}

    if (score <= current_score) {
      DEBUG("Discounting tile %zu (no better score %zu)",
            map_ref_to_num(member_tile),
            score);
      continue;
    }

    if (score == best_score) {
      /* Add another tile to the set of possible replacements
         (it is just as suitable as those already found) */
      best_tiles[num_found++] = member_tile;
      DEBUG("Adding tile %zu to list of %zu with score %zu",
            map_ref_to_num(member_tile), num_found, best_score);
      continue;
    }

    if (score > best_score) {
      /* This tile is more suitable than the accumulated list of
         possible replacements - wipe the list and start afresh. */
      num_found = 0;
      best_tiles[num_found++] = member_tile;
      best_score = score;
      DEBUG("New best fit is tile %zu (scores %zu)",
            map_ref_to_num(member_tile), best_score);
    }
  } /* next member */

  if (num_found > 0) {
    /* Use best replacement we find */
    MapRef random_tile = best_tiles[0 /*rand() % num_found*/];
    DEBUG("Replacing with tile %zu (of %zu possibilities)",
          map_ref_to_num(random_tile), num_found);

    MapEdit_write_tile(map, map_pos, random_tile, change_info);
  } else {
    DEBUG("No suitable replacement found");
  }
  free(best_tiles);
}
