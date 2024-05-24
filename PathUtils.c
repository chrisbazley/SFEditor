/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Generally useful file path utilities
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
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "err.h"
#include "debug.h"
#include "strextra.h"
#include "FileUtils.h"

#include "Macros.h"
#include "utils.h"
#include "Config.h"
#include "filepaths.h"
#include "pathutils.h"
#include "filescan.h"
#include "filepaths.h"
#include "filenames.h"
#include "mission.h"
#include "pyram.h"
#include "dfileutils.h"

static bool rename_level_file(char const *const dest_sub_path,
  char const *const src_sub_path, char const *const dir_name, bool const copy)
{
  char const *const write_dir = Config_get_write_dir();
  char *rename_source_path = NULL, *copy_source_path = NULL;
  char *const dest_path = make_file_path_in_subdir(
    write_dir, dir_name, dest_sub_path);

  bool success = false;
  do {
    if (!dest_path || !ensure_path_exists(dest_path)) {
      break;
    }

    if (!copy) {
      /* Check whether we can simply rename file (quicker than copy) */
      rename_source_path = make_file_path_in_subdir(
        write_dir, dir_name, src_sub_path);

      if (!rename_source_path) {
        break;
      }

      if (file_exists(rename_source_path)) {
        DEBUG("Source file is in writable dir - can rename it");
        success = verbose_rename(rename_source_path, dest_path);
        break;
      }
    }

    DEBUG("Copy file from internal to external levels dir");
    copy_source_path = make_file_path_in_dir_on_path(
      LEVELS_PATH, dir_name, src_sub_path);

    if (!copy_source_path) {
      break;
    }

    /* Actually we never move (i.e. delete) files from the internal dir */
    success = verbose_copy(copy_source_path, dest_path, false);
  } while (0);

  free(copy_source_path);
  free(dest_path);
  free(rename_source_path);

  return success;
}

static bool files_exist(DataType const data_types[], char const *const sub_path)
{
  bool exists = false;
  char const *const write_dir = Config_get_write_dir();

  /* Check for existing files on the target paths */
  for (size_t i = 0; !exists && data_types[i] != DataType_Count; ++i) {
    char *const full_path = make_file_path_in_subdir(
      write_dir, data_type_to_sub_dir(data_types[i]), sub_path);

    if (!full_path) {
      break;
    }
    exists = file_exists(full_path);
    free(full_path);
  }

  return exists;
}

static void delete_mission_only(char const *const sub_path)
{
  char *const miss_del_path = make_file_path_in_dir_on_path(
    Config_get_write_dir(), MISSION_DIR, sub_path);

  if (miss_del_path && verbose_remove(miss_del_path)) {
    filescan_directory_updated(filescan_get_emh_type(sub_path));
  }
  free(miss_del_path);
}

static bool rename_mission_only(char const *const source_sub_path,
  char const *const dest_sub_path, Pyramid const pyramid_number,
  int const miss_number, FilenamesData *const old_names)
{
  /* Load mission file for modification */
  char *const miss_read_path = make_file_path_in_dir_on_path(
    LEVELS_PATH, MISSION_DIR, source_sub_path);

  if (!miss_read_path) {
    return false;
  }

  bool success = false;
  MissionData *const mission = mission_create();
  if (!mission)
  {
    report_error(SFERROR(NoMem), miss_read_path, "");
  }
  else
  {
    DFile *const dfile = mission_get_dfile(mission);

    if (!report_error(load_compressed(dfile, miss_read_path), miss_read_path, ""))
    {
      PyramidData *const pyramid = mission_get_pyramid(mission);
      pyramid_set_position(pyramid, pyramid_number, miss_number);

      FilenamesData *const filenames = mission_get_filenames(mission);
      if (old_names) {
        *old_names = *filenames;
      }

      static DataType const data_types[] = {
        DataType_OverlayMap,
        DataType_OverlayObjects,
        DataType_OverlayMapAnimations};

      for (size_t i = 0; success && i < ARRAY_SIZE(data_types); ++i) {
        if (!stricmp(filenames_get(filenames, data_types[i]), source_sub_path)) {
          filenames_set(filenames, data_types[i], dest_sub_path);
        }
      }

      char const *const write_dir = Config_get_write_dir();
      char *const miss_write_path = make_file_path_in_subdir(
        write_dir, MISSION_DIR, dest_sub_path);

      success = miss_write_path &&
                ensure_path_exists(miss_write_path) &&
                !report_error(save_compressed(dfile, miss_write_path), miss_write_path, "") &&
                set_data_type(miss_write_path, DataType_Mission);

      free(miss_write_path);
    }
    dfile_release(dfile);
  }
  free(miss_read_path);
  return success;
}

bool filepaths_get_mission_filenames(char const *const path,
  FilenamesData *const filenames)
{
  MissionData *const mission = mission_create();
  if (!mission)
  {
    report_error(SFERROR(NoMem), path, "");
    return false;
  }

  DFile *const dfile = mission_get_dfile(mission);
  bool const success = !report_error(load_compressed(dfile, path), path, "");
  if (success)
  {
    *filenames = *mission_get_filenames(mission);
  }
  dfile_release(dfile);
  return success;
}

bool filepaths_mission_exists(char const *const sub_path)
{
  static DataType const data_types[] = {
    DataType_Mission,
    DataType_OverlayMap,
    DataType_OverlayObjects,
    DataType_OverlayMapAnimations,
    DataType_Count};

  return files_exist(data_types, sub_path);
}

bool filepaths_rename_mission(char const *source_sub_path, char const *dest_sub_path,
  Pyramid pyramid_number, int miss_number, bool const copy)
{
  DEBUG("Handling request to %s mission %s as %s", copy ? "copy" : "rename",
  source_sub_path, dest_sub_path);

  assert(stricmp(source_sub_path, dest_sub_path) != 0);

  FilenamesData old_names;
  bool success = rename_mission_only(source_sub_path, dest_sub_path,
    pyramid_number, miss_number, &old_names);

  if (!success) {
    return false;
  }

  static DataType const data_types[] = {
    DataType_OverlayMap,
    DataType_OverlayObjects,
    DataType_OverlayMapAnimations};

  for (size_t i = 0; success && i < ARRAY_SIZE(data_types); ++i) {
    if (stricmp(filenames_get(&old_names, data_types[i]), source_sub_path) != 0) {
      continue; /* e.g. 'Blank' */
    }

    success = rename_level_file(dest_sub_path, source_sub_path, data_type_to_sub_dir(data_types[i]), copy);
    if (!copy && !success) {
      /* Undo moving of files */
      for (size_t j = 0; j < i; ++j) {
        if (stricmp(filenames_get(&old_names, data_types[j]), source_sub_path) != 0) {
          continue; /* e.g. 'Blank' */
        }

        (void)rename_level_file(source_sub_path, dest_sub_path, data_type_to_sub_dir(data_types[j]), false);
      }
    }
  }

  if (copy && !success) {
    /* Delete copied mission file */
    filepaths_delete_mission(dest_sub_path);
  }
  else if (!copy && success) {
    /* Delete original mission file */
    delete_mission_only(source_sub_path);
  }

  return success;
}

void filepaths_delete_mission(char const *const sub_path)
{
  /* We must delete any mission, anims, map overlay and grid overlay files */
  DEBUG("Handling request to delete mission %s", sub_path);
  char const *const root_dir = Config_get_write_dir();

  char *const miss_read_path = make_file_path_in_dir_on_path(LEVELS_PATH,
                                  MISSION_DIR, sub_path);
  if (!miss_read_path) {
    return;
  }

  /* Load mission file */
  struct FilenamesData filenames;
  bool const success = filepaths_get_mission_filenames(miss_read_path, &filenames);
  free(miss_read_path);

  if (!success) {
    return;
  }

  /* Delete only files with canonical names (e.g. omitting 'Blank') */
  static DataType const data_types[] = {
    DataType_OverlayMap,
    DataType_OverlayObjects,
    DataType_OverlayMapAnimations};

  for (size_t i = 0; i < ARRAY_SIZE(data_types); ++i) {
    if (stricmp(filenames_get(&filenames, data_types[i]), sub_path) != 0) {
      continue;
    }

    char *const delete_path = make_file_path_in_subdir(
      root_dir, data_type_to_sub_dir(data_types[i]), sub_path);

    if (!delete_path) {
      break;
    }

    if (file_exists(delete_path)) {
      verbose_remove(delete_path);
    }
    free(delete_path);
  }

  /* Delete main mission file itself */
  delete_mission_only(sub_path);
}

void filepaths_delete_map(char const *const sub_path)
{
  /* We must delete any base map, grid or animations files */
  DEBUG("Handling request to delete base map %s", sub_path);
  char const *const root_dir = Config_get_write_dir();

  static filescan_type const dirs[] = {
    FS_BASE_SPRSCAPE, FS_BASE_FXDOBJ, FS_BASE_ANIMS};

  for (size_t i = 0; i < ARRAY_SIZE(dirs); ++i) {
    char *const delete_path = make_file_path_in_subdir(
      root_dir, filescan_get_directory(dirs[i]), sub_path);
    if (!delete_path) {
      break;
    }
    if (file_exists(delete_path) && verbose_remove(delete_path)) {
      filescan_directory_updated(dirs[i]);
    }
    free(delete_path);
  }
}

bool filepaths_map_exists(char const *const sub_path)
{
  static DataType const data_types[] = {
    DataType_BaseMap, DataType_BaseObjects, DataType_BaseMapAnimations, DataType_Count};

  return files_exist(data_types, sub_path);
}

static bool rename_map_file(char const *source_name, char const *dest_name,
  char const *const dir_name, bool const copy)
{
  char const *const write_dir = Config_get_write_dir();
  char *rename_source_path = NULL, *copy_source_path = NULL,*blank_path = NULL,
       *dest_read = NULL;
  char *const dest_path = make_file_path_in_subdir(write_dir, dir_name, dest_name);

  bool success = false;
  do {
    if (!dest_path || !ensure_path_exists(dest_path)) {
      break;
    }

    if (!copy) {
      rename_source_path = make_file_path_in_subdir(
                             write_dir, dir_name, source_name);
      if (!rename_source_path) {
        break;
      }
      if (file_exists(rename_source_path)) {
        /* Source file is in writable dir, can simply rename it
        (much quicker than copy) */
        DEBUG("Can simply rename source file");
        success = verbose_rename(rename_source_path, dest_path);
        break;
      }
    }

    copy_source_path = make_file_path_in_dir_on_path(
                         LEVELS_PATH, dir_name, source_name);
    if (!copy_source_path) {
      break;
    }

    if (file_exists(copy_source_path)) {
      /* Copy file from internal to external levels dir */
      DEBUG("Must copy source file to external levels dir");
      success = verbose_copy(copy_source_path, dest_path, false);
      break;
    }

    /* No source file exists for this type of map data */
    dest_read = make_file_path_in_dir_on_path(
      LEVELS_PATH, dir_name, dest_name);

    if (!dest_read || !file_exists(dest_read)) {
      break;
    }

    /* Irrelevant file exists on destination path */
    DEBUG("An irrelevant file exists on dest path");

    if (file_exists(dest_path)) {
      /* Delete irrelevant file from writable dir */
      DEBUG("Will delete irrelevant file");
      success = verbose_remove(dest_path);
      break;
    }

    /* Must override irrelevant file with blank (cannot delete
    file from internal levels dir */
    DEBUG("Will override irrelevant file");
    blank_path = make_file_path_in_dir_on_path(
      LEVELS_PATH,
      stricmp(dir_name, BASEANIMS_DIR) == 0 ? LEVELANIMS_DIR : dir_name,
      BLANK_FILE);

    if (!blank_path) {
      break;
    }
    success = verbose_copy(blank_path, dest_path, false);
  } while (0);

  free(blank_path);
  free(dest_read);
  free(copy_source_path);
  free(rename_source_path);
  free(dest_path);

  return success;
}

bool filepaths_rename_map(char const *source_name, char const *dest_name, bool const copy)
{
  DEBUG("Handling request to %s map %s as %s", copy ? "copy" : "rename",
  source_name, dest_name);

  static filescan_type const dirs[] = {
    FS_BASE_SPRSCAPE, FS_BASE_FXDOBJ, FS_BASE_ANIMS};
  bool success = true;

  for (size_t i = 0; success && i < ARRAY_SIZE(dirs); ++i) {
    success = rename_map_file(source_name, dest_name,
                  filescan_get_directory(dirs[i]), copy);
    if (success) {
      filescan_directory_updated(dirs[i]);
    }
  }
  return success;

  return true; /* success */
}
