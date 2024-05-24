/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map data
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef MapData_h
#define MapData_h

#include "DFileData.h"

struct MapData {
  DFile dfile;
  void *flex;
  bool is_overlay;
};

#endif
