/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Objects grid data
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef ObjData_h
#define ObjData_h

#include "DFileData.h"

struct ObjectsData {
  DFile dfile;
  void *flex;
  bool is_overlay;
};

#endif
