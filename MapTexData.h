/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map textures set data
 *  Copyright (C) 2019 Chris Bazley
 */

#ifndef MapTexData_h
#define MapTexData_h

#include "DfileData.h"
#include "SmoothData.h"
#include "MSnakesData.h"
#include "MTransfersData.h"
#include "MapTexBDat.h"
#include "MapTex.h"

struct MapTex
{
  DFile dfile;
  struct MapTexBitmaps tiles;
  struct MapTexGroups groups;
  struct MapSnakes snakes;
  struct MapTransfers transfers;
};

#endif
