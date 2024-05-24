/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Generally useful file path components
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

#include <stddef.h>
#include <stdbool.h>
#include "stdio.h"
#include <string.h>

#include "SFformats.h" /* get Fednet filetype */
#include "debug.h"
#include "macros.h"
#include "filepaths.h"
#include "pyram.h"
#include "strextra.h"
#include "pathtail.h"
#include "DataType.h"

void get_mission_file_name(Filename buffer, Pyramid const p,
  int const mission, char const *const user_name)
{
  switch (p) {
    case Pyramid_Easy:
      sprintf(buffer, E_PATH E_FILE_PREFIX "%02d", mission);
      break;

    case Pyramid_Medium:
      sprintf(buffer, M_PATH M_FILE_PREFIX "%02d", mission);
      break;

    case Pyramid_Hard:
      sprintf(buffer, H_PATH H_FILE_PREFIX "%02d", mission);
      break;

    case Pyramid_User:
      sprintf(buffer, U_PATH "%s", user_name);
      break;

    default:
      assert("Bad pyramid" == NULL);
      break;
  }
}

char const *data_type_to_sub_dir(DataType const data_type)
{
  static char const *data_type_to_sub_dir[DataType_Count] =
  {
    [DataType_HillColours] = HILLCOL_DIR,
    [DataType_PolygonColours] = PALETTE_DIR,
    [DataType_PolygonMeshes] = POLYGFX_DIR,
    [DataType_BaseMap] = BASEMAP_DIR,
    [DataType_OverlayMap] = LEVELMAP_DIR,
    [DataType_BaseObjects] = BASEGRID_DIR,
    [DataType_OverlayObjects] = LEVELGRID_DIR,
    [DataType_SkyColours] = SKY_DIR,
    [DataType_Mission] = MISSION_DIR,
    [DataType_SkyImages] = PLANETS_DIR,
    [DataType_MapTextures] = MAPTILES_DIR,
    [DataType_BaseMapAnimations] = BASEANIMS_DIR,
    [DataType_OverlayMapAnimations] = LEVELANIMS_DIR,
    [DataType_MapTransfer] = "",
    [DataType_ObjectsTransfer] = "",
    [DataType_InfosTransfer] = "",
  };
  assert(data_type >= 0);
  assert(data_type < ARRAY_SIZE(data_type_to_sub_dir));
  return data_type_to_sub_dir[data_type];
}

static int const data_type_to_ftype[DataType_Count] =
{
  [DataType_HillColours] = FileType_Fednet,
  [DataType_PolygonColours] = FileType_Fednet,
  [DataType_PolygonMeshes] = FileType_SFObjGfx,
  [DataType_BaseMap] = FileType_SFBasMap,
  [DataType_OverlayMap] = FileType_SFOvrMap,
  [DataType_BaseObjects] = FileType_SFBasObj,
  [DataType_OverlayObjects] = FileType_SFOvrObj,
  [DataType_SkyColours] = FileType_SFSkyCol,
  [DataType_Mission] = FileType_SFMissn,
  [DataType_SkyImages] = FileType_SFSkyPic,
  [DataType_MapTextures] = FileType_SFMapGfx,
  [DataType_BaseMapAnimations] = FileType_SFMapAni,
  [DataType_OverlayMapAnimations] = FileType_SFMapAni,
  [DataType_MapTransfer] = FileType_Fednet,
  [DataType_ObjectsTransfer] = FileType_Fednet,
  [DataType_InfosTransfer] = FileType_Fednet,
};

DataType file_type_to_data_type(int const file_type, char const *const filename)
{
  DataType data_type = DataType_Count;

  if (file_type == FileType_SFMapAni) {
    /* More tricky to work out whether session type should be map or mission
       in case of animations (same file type) */
    if (strnicmp(pathtail(filename, 3), BASE_DIR, strlen(BASE_DIR)) == 0) {
      data_type = DataType_BaseMapAnimations;
    } else {
      data_type = DataType_OverlayMapAnimations;
    }
  } else {
    for (data_type = DataType_First; data_type < DataType_Count; ++data_type) {
      assert(data_type_to_ftype[data_type]);
      if (file_type == data_type_to_ftype[data_type]) {
        break;
      }
    }
  }

  return data_type;
}

int data_type_to_file_type(DataType const data_type)
{
  assert(data_type >= 0);
  assert(data_type < ARRAY_SIZE(data_type_to_ftype));
  int const ftype = data_type_to_ftype[data_type];
  DEBUGF("Data type %d to file type 0x%x\n", data_type, ftype);
  return ftype;
}

bool data_type_allow_none(DataType const data_type)
{
  bool allow_none;

  /* Same list of exceptions as is hard-wired in the game */
  switch (data_type)
  {
    case DataType_HillColours:
    case DataType_SkyImages:
    case DataType_SkyColours:
      allow_none = true;
      break;
    default:
      allow_none = false;
      break;
  }

  return allow_none;
}
