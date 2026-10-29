/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Polygonal graphics set data
 *  Copyright (C) 2021 Chris Bazley
 */

#ifndef ObjGfxData_h
#define ObjGfxData_h

#include "DFileData.h"
#include "OSnakesData.h"
#include "OTransfersData.h"
#include "ObjGfxMeshData.h"
#include "ObjGfx.h"

struct ObjGfx
{
  DFile dfile;
  struct ObjGfxMeshes meshes;
  union {
    char nil;
    struct ObjSnakes obj;
  } snakes;
  union {
    char nil;
    struct ObjTransfers obj;
  } transfers;
};

#endif
