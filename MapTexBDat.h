/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map texture bitmaps data
 *  Copyright (C) 2019 Chris Bazley
 */

#ifndef MapTexBDat_h
#define MapTexBDat_h

#include "SprMem.h"
#include "MapCoord.h"
#include "MapTexBitm.h"

struct MapTexBitmaps
{
  size_t count;
  bool have_sprites[MapAngle_Count][MapTexSizeLog2+1];
  SprMem sprites[MapAngle_Count][MapTexSizeLog2+1];
  void *avcols_table; /* flex anchor (for x1 zoom level) */
  void *bw_table; /* flex anchor for black/white table,
                     one bit per tile graphic */
  void *bitmaps;
};

#endif
