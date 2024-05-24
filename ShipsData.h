/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission ships data
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef ShipsData_h
#define ShipsData_h

#include "LinkedList.h"

typedef enum {
  ShipsDataState_PostRead,
  ShipsDataState_PreWrite,
  ShipsDataState_Write,
} ShipsDataState;

struct ShipsData {
  ShipsDataState state;
  size_t count;
  LinkedList list;
};

#endif
