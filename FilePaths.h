/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Generally useful file path components
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef filepaths_h
#define filepaths_h

#include <stddef.h>
#include <stdbool.h>

#include "Platform.h"
#include "Pyram.h"
#include "DataType.h"
#include "filescan.h"

#define DIR_SEP "."
#define NUM_SEP "_"

/* Paths in choices dir (Choices:SFeditor or !SFeditor.Choices plus
   !SFeditor.Defaults) */
#define CHOICES_READ_PATH "SFeditorChoices:"
#define CHOICES_WRITE_PATH "<SFeditorChoices$Write>" DIR_SEP
#define CHOICES_DEFAULTS_PATH "<SFeditor$Dir>" DIR_SEP "Defaults" DIR_SEP

/* The following paths are relative to a choices directory */
#define MAPGFX_DIR "MapGfx"
#define TILEGROUPS_DIR "TileGroups"
#define TILESNAKES_DIR "TileSnakes"
#define OBJSNAKES_DIR "ObjSnakes"
#define CONFIG_FILE "Config"

/* Fixed paths to Landscapes directories */
#define LEVELS_PATH "SFeditorLevels:"
#define FIXED_GAME_DIR "<Star3000$Dir>" DIR_SEP "Landscapes"

/* The following paths are relative to a Landscapes directory */
#define SKY_DIR "Sky"
#define PLANETS_DIR "Planets"
#define BASE_DIR "Base"
#define LEVEL_DIR "Level"
#define MAP_SUBDIR "SprScape"
#define GRID_SUBDIR "FxdObj"
#define ANIMS_SUBDIR "Animations"
#define MAPTILES_DIR "Sprites"
#define POLYGFX_DIR "Graphics"
#define HILLCOL_DIR "Hill"
#define PALETTE_DIR "Palette"
#define MISSION_DIR "Missions"

#define E_DIR "E"
#define M_DIR "M"
#define H_DIR "H"
#define U_DIR "U"

#define E_FILE_PREFIX "E" NUM_SEP
#define M_FILE_PREFIX "M" NUM_SEP
#define H_FILE_PREFIX "H" NUM_SEP

#define BASEMAP_DIR BASE_DIR DIR_SEP MAP_SUBDIR
#define BASEGRID_DIR BASE_DIR DIR_SEP GRID_SUBDIR
#define BASEANIMS_DIR BASE_DIR DIR_SEP ANIMS_SUBDIR

#define E_PATH E_DIR DIR_SEP
#define M_PATH M_DIR DIR_SEP
#define H_PATH H_DIR DIR_SEP
#define U_PATH U_DIR DIR_SEP

#define MISSION_E_DIR MISSION_DIR DIR_SEP E_DIR
#define MISSION_M_DIR MISSION_DIR DIR_SEP M_DIR
#define MISSION_H_DIR MISSION_DIR DIR_SEP H_DIR
#define MISSION_U_DIR MISSION_DIR DIR_SEP U_DIR

#define LEVELMAP_DIR LEVEL_DIR DIR_SEP MAP_SUBDIR
#define LEVELGRID_DIR LEVEL_DIR DIR_SEP GRID_SUBDIR
#define LEVELANIMS_DIR LEVEL_DIR DIR_SEP ANIMS_SUBDIR

/* Graphics configuration file for an unknown map */
#define UNKNOWN_FILE "Unknown"

/* Empty/default data */
#define BLANK_FILE "Blank"

/* Not all ancillary mission files are mandatory */
#define NO_FILE "X"

void get_mission_file_name(Filename buffer, Pyramid p,
  int mission, char const *user_name);

char const *data_type_to_sub_dir(DataType data_type);

int data_type_to_file_type(DataType data_type);
DataType file_type_to_data_type(int file_type, char const *filename);
bool data_type_allow_none(DataType data_type);

#endif
