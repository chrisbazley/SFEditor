/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Code for building menus of files
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef filescan_h
#define filescan_h

#include <stdbool.h>

#include "StringBuff.h"
#include "DataType.h"
#include "FilenamesData.h"

typedef enum {
  FS_FIRST,
  FS_MISSION_E = FS_FIRST,
  FS_MISSION_M,
  FS_MISSION_H,
  FS_MISSION_U,
  FS_BASE_SPRSCAPE,
  FS_BASE_FXDOBJ,
  FS_BASE_ANIMS,
  FS_SPRITES,
  FS_GRAPHICS,
  FS_HILL,
  FS_PALETTE,
  FS_SKY,
  FS_PLANETS,
  FS_LAST,
} filescan_type;

typedef struct
{
  bool        is_internal;
  Filename    leaf_name;
} filescan_leafname;

void filescan_init(void);

char const *filescan_get_emh_path(filescan_type directory);
const char *filescan_get_directory(filescan_type directory);
bool filescan_dir_not_empty(filescan_type directory);

filescan_leafname *filescan_get_leaf_names(filescan_type directory, int *vsn);
void filescan_directory_updated(filescan_type directory);
filescan_type filescan_get_emh_type(char const *filename);

/* combine two arrays of leafnames into one */
filescan_leafname *filescan_combine_filenames(filescan_leafname *filenames_A, filescan_leafname *filenames_B);

DataType filescan_get_data_type(filescan_type directory);

#endif
