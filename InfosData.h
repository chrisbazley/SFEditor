/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission target information points data
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef InfosData_h
#define InfosData_h

#include "IntDict.h"

struct TargetInfosData
{
  size_t count;
  uint8_t next;
  IntDict dict;
};

#endif
