/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Generally useful file path utilities
 *  Copyright (C) 2001 Christopher Bazley
 */


#ifndef pathutils_h
#define pathutils_h

#include <stdbool.h>
#include "pyram.h"
#include "filenamesdata.h"
#include "DFile.h"

bool filepaths_get_mission_filenames(char const *path,
  struct FilenamesData *filenames);

bool filepaths_mission_exists(char const *sub_path);
bool filepaths_map_exists(char const *sub_path);

void filepaths_delete_mission(char const *sub_path);
void filepaths_delete_map(char const *sub_path);

bool filepaths_rename_mission(char const *source_sub_path,
  char const *dest_sub_path, Pyramid pyramid_number, int miss_number,
  bool copy);

bool filepaths_rename_map(char const *source_name,
  char const *dest_name, bool copy);

#endif
