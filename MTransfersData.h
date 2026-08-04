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

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

struct MapTransfers
{
  int               count;
  StrDict           dict;
  SprMem            thumbnail_sprites; /* flex anchor for sprite area */
  bool              have_thumbnails;
  _Optional char *directory;
};

#endif
