/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Polygonal graphics set data
 *  Copyright (C) 2021 Chris Bazley
 */

#ifndef ObjGfxData_h
#define ObjGfxData_h

#include "DfileData.h"
#include "OSnakesData.h"
#include "OTransfersData.h"
#include "ObjGfxMeshData.h"
#include "ObjGfx.h"

struct ObjGfx
{
  DFile dfile;
  struct ObjGfxMeshes meshes;
  struct ObjSnakes snakes;
  struct ObjTransfers transfers;
};

#endif
