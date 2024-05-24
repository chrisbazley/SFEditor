/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Code for building menus of files
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

/* "Its not the way that you caress and toy with with my affections..." */
#include "stdlib.h"
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <limits.h>

#include "kernel.h"
#include "toolbox.h"
#include "Menu.h"

#include "err.h"
#include "msgtrans.h"
#include "hourglass.h"
#include "Macros.h"
#include "StrExtra.h"
#include "DirIter.h"
#include "hourglass.h"
#include "stringbuff.h"

#include "filescan.h"
#include "filepaths.h"
#include "Config.h"
#include "utils.h"
#include "SFInit.h"
#include "debug.h"
#include "DataType.h"

struct fs_dir_info {
  DataType data_type;
  filescan_leafname *leaf_names;
  bool rescan_needed;
  int scan_no;
};

static struct fs_dir_info knowledge[FS_LAST] =
{
  [FS_MISSION_E]      = {DataType_Mission,   NULL, true, 1},
  [FS_MISSION_M]      = {DataType_Mission,   NULL, true, 1},
  [FS_MISSION_H]      = {DataType_Mission,   NULL, true, 1},
  [FS_MISSION_U]      = {DataType_Mission,   NULL, true, 1},
  [FS_BASE_SPRSCAPE]  = {DataType_BaseMap,  NULL, true, 1},
  [FS_BASE_FXDOBJ]    = {DataType_BaseObjects,  NULL, true, 1},
  [FS_BASE_ANIMS]     = {DataType_BaseMapAnimations,  NULL, true, 1},
  [FS_SPRITES]        = {DataType_MapTextures,  NULL, true, 1},
  [FS_GRAPHICS]       = {DataType_PolygonMeshes,  NULL, true, 1},
  [FS_HILL]           = {DataType_HillColours,  NULL, true, 1},
  [FS_PALETTE]        = {DataType_PolygonColours,  NULL, true, 1},
  [FS_SKY]            = {DataType_SkyColours,  NULL, true, 1},
  [FS_PLANETS]        = {DataType_SkyImages,  NULL, true, 1},
};

enum {
  ARRAY_EXTEND_FACTOR = 2,
  MIN_EXTEND = 4 /* avoid frequent use of realloc() */
};

typedef struct {
  filescan_leafname *names;
  size_t used;
  size_t len;
} fs_array;

/* ---------------- Private functions ---------------- */

static void fs_array_init(fs_array *const array)
{
  assert(array != NULL);
  *array = (fs_array){
    .names = NULL,
    .used = 0,
    .len = 0,
  };
}

static bool fs_array_push(fs_array *const array, const filescan_leafname *const name)
{
  assert(array != NULL);
  assert(array->used <= array->len);
  assert(name != NULL);

  if (array->used == array->len) {
    /* Create/extend output array */
    size_t const new_len = HIGHEST(MIN_EXTEND, array->len * ARRAY_EXTEND_FACTOR);
    DEBUGF("Extending array from %zu to %zu\n", array->len, new_len);
    void *const new_ptr = realloc(array->names, new_len * sizeof(*array->names));
    if (new_ptr == NULL) {
      report_error(SFERROR(NoMem), "", "");
      return false;
    }

    array->names = new_ptr;
    array->len = new_len;
  }

  assert(array->used < array->len);
  array->names[array->used] = *name;
  DEBUGF("Have written array entry %zu '%s'%s\n", array->used,
         name->leaf_name, name->is_internal ? " (internal)" : "");
  array->used++;
  return true;
}

static bool fs_array_end(fs_array *const array)
{
  assert(array != NULL);
  assert(array->used <= array->len);

  DEBUGF("Marking end of array of %zu names\n", array->used);
  static filescan_leafname const end = {.leaf_name = "", .is_internal = false};
  return fs_array_push(array, &end);
}

static void fs_array_destroy(fs_array *const array)
{
  assert(array != NULL);
  assert(array->used <= array->len);
  free(array->names);
}

#ifdef USE_REPORTER
static void print_list(const filescan_leafname *const filenames)
{
 for (size_t ptr = 0; *filenames[ptr].leaf_name != '\0'; ptr++)
   DEBUG("%s%s", filenames[ptr].leaf_name, filenames[ptr].is_internal ?
   " (internal)" : "");
}
#endif

static int fs_file_type(filescan_type const directory)
{
  return data_type_to_file_type(filescan_get_data_type(directory));
}

static bool fs_not_empty(char const *const s, filescan_type const directory)
{
  /* Returns true if the directory exists and contains one or more files of the
     specified type */

  if (!file_exists(s)) {
    return false;
  }

  int const file_type = fs_file_type(directory);
  DEBUG("Checking for existence of files of type %x in %s", file_type, s);

  bool found_object = false;
  DirIterator *iter = NULL;
  for (const _kernel_oserror *e = diriterator_make(&iter, 0, s, NULL);
       !E(e);
       e = diriterator_advance(iter)) {

    DirIteratorObjectInfo info;
    int const object_type = diriterator_get_object_info(iter, &info);
    if (object_type == ObjectType_NotFound) {
      break;
    }

    if (info.file_type != file_type) {
      continue;
    }

    /* Check that filename within length limit */
    filescan_leafname tmp;
    if (diriterator_get_object_leaf_name(iter, tmp.leaf_name,
        sizeof(tmp.leaf_name)) >= sizeof(tmp.leaf_name)) {
      DEBUGF("%s exceeds the character limit\n", tmp.leaf_name);
      continue;
    }

    found_object = true;
    break;
  }
  diriterator_destroy(iter);

  return found_object;
}

static filescan_leafname *fs_dir(char const *const s,
  filescan_type const directory, bool const internal)
{
  /* Scan directory and build array of 11-character leafnames */
  fs_array scan_results;
  fs_array_init(&scan_results);

  bool success = false;
  if (!file_exists(s)) {
    success = true;
  } else {
    int const file_type = fs_file_type(directory);

    hourglass_on();
    DirIterator *iter = NULL;
    for (const _kernel_oserror *e = diriterator_make(&iter, 0, s, NULL);
         !E(e);
         e = diriterator_advance(iter)) {

      DirIteratorObjectInfo info;
      int const object_type = diriterator_get_object_info(iter, &info);
      if (object_type == ObjectType_NotFound) {
        success = true;
        break;
      }

      /* Check that file is of correct type */
      if (info.file_type == file_type) {

        /* Check that filename within length limit */
        filescan_leafname tmp = {.is_internal = internal};

        if (diriterator_get_object_leaf_name(iter, tmp.leaf_name, sizeof(tmp.leaf_name)) >=
            sizeof(tmp.leaf_name)) {
          DEBUGF("%s exceeds the character limit\n", tmp.leaf_name);
          continue;
        }

        if (!fs_array_push(&scan_results, &tmp)) {
          report_error(SFERROR(NoMem), "", "");
          break;
        }
      }
    }
    diriterator_destroy(iter);
    hourglass_off();
  }

  if (success && !fs_array_end(&scan_results)) {
    report_error(SFERROR(NoMem), "", "");
    success = false;
  }

  if (!success) {
    fs_array_destroy(&scan_results);
  }

  return success ? scan_results.names : NULL; /* return finished array */
}

static filescan_leafname *fs_scanlevelspath(filescan_type const directory)
{
  /* Scan the currently configured levels paths, and combine */
  filescan_leafname *combined = NULL;
  StringBuffer full_path;
  stringbuffer_init(&full_path);

  /* construct FULL path for internal game directory to scan */
  filescan_leafname *intern_files = NULL;
  char const *const relative_scanpath = filescan_get_directory(directory);
  char *const intern_path = make_file_path_in_dir(Config_get_read_dir(), relative_scanpath);
  if (intern_path)
  {
    intern_files = fs_dir(intern_path, directory, true);
    free(intern_path);
  }

  /* do we have an external levels directory configured? */
  if (!Config_get_use_extern_levels_dir() || intern_files == NULL)
  {
    /* Return pointer to results from internal game directory: */
    combined = intern_files;
  }
  else
  {
    filescan_leafname *extern_files = NULL;
    char *const extern_path = make_file_path_in_dir(Config_get_extern_levels_dir(),
      relative_scanpath);

    if (extern_path)
    {
      extern_files = fs_dir(extern_path, directory, false);
      free(extern_path);

      /* Combine the internal and external scan results together: */
      combined = filescan_combine_filenames(extern_files, intern_files);
    }
    free(extern_files);
    free(intern_files);
  }

  /* Return pointer to combined results */
  return combined;
}

static void fs_cleanup(void)
{
  for (filescan_type directory = FS_FIRST; directory < FS_LAST; ++directory)
  {
    free(knowledge[directory].leaf_names);
  }
}

/* ---------------- Public functions ---------------- */

void filescan_init(void)
{
  atexit(fs_cleanup);
}

char const *filescan_get_emh_path(filescan_type const directory)
{
  char const *sub_dir = "";
  switch (directory) {
    case FS_MISSION_E:
      sub_dir = E_PATH;
      break;
    case FS_MISSION_M:
      sub_dir = M_PATH;
      break;
    case FS_MISSION_H:
      sub_dir = H_PATH;
      break;
    case FS_MISSION_U:
      sub_dir = U_PATH;
      break;
    default:
      assert("Not EMH" == NULL);
      break;
  }
  return sub_dir;
}

DataType filescan_get_data_type(filescan_type const directory)
{
  assert(directory >= 0);
  assert(directory < ARRAY_SIZE(knowledge));
  return knowledge[directory].data_type;
}

char const *filescan_get_directory(filescan_type const directory)
{
  char const *sub_dir = "";

  switch (directory)
  {
    case FS_MISSION_E:
      sub_dir = MISSION_E_DIR;
      break;
    case FS_MISSION_M:
      sub_dir = MISSION_M_DIR;
      break;
    case FS_MISSION_H:
      sub_dir = MISSION_H_DIR;
      break;
    case FS_MISSION_U:
      sub_dir = MISSION_U_DIR;
      break;
    default:
      sub_dir = data_type_to_sub_dir(filescan_get_data_type(directory));
      break;
  }

  DEBUG("Path to directory %d is '%s'", directory, sub_dir);
  return sub_dir;
}

bool filescan_dir_not_empty(filescan_type const directory)
{
  bool not_empty = false;

  if (!Config_get_lazydirscan() || knowledge[directory].rescan_needed)
  {
    /* Rather than scanning the entire directory we simply check for the
    presence of one or more files */
    char const *const sub_dir = filescan_get_directory(directory);

    /* construct FULL path for internal game directory to scan */
    char *const intern_path = make_file_path_in_dir(Config_get_read_dir(), sub_dir);
    if (intern_path)
    {
      /* Scan internal game directory */
      not_empty = fs_not_empty(intern_path, directory);
      free(intern_path);

      /* do we have an external levels directory configured? */
      if (!not_empty && Config_get_use_extern_levels_dir())
      {
        /* construct FULL path for external game directory to scan */
        char *const extern_path = make_file_path_in_dir(Config_get_extern_levels_dir(), sub_dir);
        if (extern_path)
        {
          /* Scan external game directory */
          not_empty = fs_not_empty(extern_path, directory);
          free(extern_path);
        }
      }
    }
  }
  else
  {
    /* Check cached catalogue of directory contents */
    not_empty = *knowledge[directory].leaf_names[0].leaf_name != '\0';
  }
  return not_empty;
}

filescan_leafname *filescan_get_leaf_names(filescan_type const directory, int *const vsn)
{
  assert(directory >= 0);
  assert(directory < ARRAY_SIZE(knowledge));

  DEBUG("Filescan received request for catalogue of directory %d", directory);

  /* Scan directory? */
  if (!Config_get_lazydirscan() || knowledge[directory].rescan_needed)
  {
    DEBUG("filescan_get_leaf_names about to scan directory %d", directory);
    filescan_leafname *const newfiles = fs_scanlevelspath(directory);

    if (newfiles != NULL)
    {
      free(knowledge[directory].leaf_names);
      knowledge[directory].leaf_names = newfiles;

      knowledge[directory].rescan_needed = false;

      if (knowledge[directory].scan_no < INT_MAX)
      {
        knowledge[directory].scan_no++;
      }
      else
      {
        knowledge[directory].scan_no = 1;
      }
    }
    DEBUG("Rescan count for directory %d is now %d", directory,
    knowledge[directory].scan_no);
  }

  if (vsn != NULL)
  {
    *vsn = knowledge[directory].scan_no;
  }

  DEBUG("Filescan returning leafnames array %p with version %d",
        (void *)knowledge[directory].leaf_names, knowledge[directory].scan_no);

  return knowledge[directory].leaf_names;
}

void filescan_directory_updated(filescan_type const directory)
{
  assert(directory >= 0);
  assert(directory < ARRAY_SIZE(knowledge));
  DEBUG("filescan notified that directory %d updated", directory);
  knowledge[directory].rescan_needed = true;
}

filescan_type filescan_get_emh_type(char const *const filename)
{
  static filescan_type const mission_dirs[] = {
    FS_MISSION_E, FS_MISSION_M, FS_MISSION_H, FS_MISSION_U};

  for (size_t i = 0; i < ARRAY_SIZE(mission_dirs); ++i)
  {
    filescan_type const dir = mission_dirs[i];
    char const *const emh_path = filescan_get_emh_path(dir);

    if (strnicmp(filename, emh_path, strlen(emh_path)) == 0) {
      return dir;
    }
  }

  return FS_LAST;
}

filescan_leafname *filescan_combine_filenames(filescan_leafname *filenames_A, filescan_leafname *filenames_B)
{
  /* Combine two arrays of leafnames into one */
  DEBUGF("Filescan about to combine leafname arrays %p and %p\n",
    (void *)filenames_A, (void *)filenames_B);

  fs_array combined_results;
  fs_array_init(&combined_results);

  size_t A_pos = 0, B_pos = 0;
  assert(filenames_A != NULL);
  assert(filenames_B != NULL);
  bool A_finished = (*filenames_A[A_pos].leaf_name == '\0'),
       B_finished = (*filenames_B[B_pos].leaf_name == '\0');

  /* loop until we reach the end of BOTH input arrays */
  bool success = true;
  for (size_t output_pos = 0;
       success && (!A_finished || !B_finished);
       ++output_pos)
  {
    DEBUG_VERBOSEF("A: %s B: %s\n",filenames_A[A_pos].leaf_name,
                   filenames_B[B_pos].leaf_name);

    /* Where we have both A and B waiting, compare them alphabetically */
    int A_compared_B = 0;
    if (!A_finished && !B_finished)
    {
      A_compared_B = stricmp(filenames_A[A_pos].leaf_name,
                             filenames_B[B_pos].leaf_name);
    }

    filescan_leafname tmp;
    if (!A_finished && (B_finished || A_compared_B <= 0))
    {
      /* It is arbitrarily decided that where A equals B,
         we will insert A rather than B into the array */
      DEBUG_VERBOSEF("inserting A name into combined array\n");
      tmp = filenames_A[A_pos];

      if (!B_finished && A_compared_B == 0)
      {
        /* 'Internal' flag should be set if either file is internal */
        if (filenames_B[B_pos].is_internal)
        {
          tmp.is_internal = true;
        }

        /* If A and B are equal then we do not insert B, just skip it.
           This is to avoid duplication, since A has already been inserted */
        DEBUG_VERBOSEF("Ignoring B name (duplicates A name)\n");
        B_finished = (*filenames_B[++B_pos].leaf_name == '\0');
      }

      DEBUG_VERBOSEF("Advancing to next A name\n");
      A_finished = (*filenames_A[++A_pos].leaf_name == '\0');
    }
    else
    {
      /* A is finished, or (B not finished and A>B)
         Insert current name B into array */
      DEBUG_VERBOSEF("Inserting B name into combined array\n");
      tmp = filenames_B[B_pos];

      DEBUG_VERBOSEF("Advancing to next B name\n");
      B_finished = (*filenames_B[++B_pos].leaf_name == '\0');
    }

    success = fs_array_push(&combined_results, &tmp);
  }

  if (success && !fs_array_end(&combined_results))
  {
    success = false;
  }

  if (success)
  {
#ifdef USE_REPORTER
    print_list(combined_results.names);
#endif
  }
  else
  {
    report_error(SFERROR(NoMem), "", "");
    fs_array_destroy(&combined_results);
  }

  return success ? combined_results.names : NULL;
}
