/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map transfers data
 *  Copyright (C) 2019 Chris Bazley
 */

#ifndef MTransfersData_h
#define MTransfersData_h

#include <stdbool.h>
#include "SprMem.h"

#include "StrDict.h"

struct MapTransfers
{
  size_t            count;
  StrDict           dict;
  SprMem            thumbnail_sprites; /* flex anchor for sprite area */
  bool              have_thumbnails;
  char *directory;
};

#endif
