/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map textures set data
 *  Copyright (C) 2019 Chris Bazley
 */

#ifndef MapTexData_h
#define MapTexData_h

#include "DFileData.h"
#include "SmoothData.h"
#include "MSnakesData.h"
#include "MTransfersData.h"
#include "MapTexBDat.h"
#include "MapTex.h"

struct MapTex
{
  DFile dfile;
  union {
    char nil;
    struct MapTexBitmaps obj;
  } tiles;
  union {
    char nil;
    struct MapTexGroups obj;
  } groups;
  union {
    char nil;
    struct MapSnakes obj;
  } snakes;
  union {
    char nil;
    struct MapTransfers obj;
  } transfers;
};

#endif
