/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Graphics files configuration
 *  Copyright (C) 2019 Christopher Bazley
 */

#ifndef GfxConfig_h
#define GfxConfig_h

#include <stdbool.h>

#include "FilenamesData.h"
#include "CloudsData.h"

typedef struct
{
  struct FilenamesData filenames;
  struct CloudColData clouds;
}
GfxConfig;

bool GfxConfig_load(GfxConfig *graphics, char const *basemap_filename);
bool GfxConfig_save(const GfxConfig *graphics, char const *basemap_filename);

#endif
