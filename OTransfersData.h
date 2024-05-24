/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground object transfers data
 *  Copyright (C) 2021 Chris Bazley
 */

#ifndef OTransfersData_h
#define OTransfersData_h

#include "StrDict.h"

struct ObjTransfers
{
  int               count;
  StrDict           dict; /* root of linked list of transfers */
  char *directory;
};

#endif
