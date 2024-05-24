/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission action triggers data
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef TriggersData_h
#define TriggersData_h

#include "LinkedList.h"
#include "IntDict.h"

struct TriggersData
{
  size_t count;
  LinkedList list;
  LinkedList delete_list;
  size_t max_losses;
  void *bit_map; /* flex anchor for array [16][128], one bit per
map location */
  IntDict all_triggers;
  IntDict chain_triggers;
};

#endif
