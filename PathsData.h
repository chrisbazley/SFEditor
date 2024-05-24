/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission flightpaths data
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef PathsData_h
#define PathsData_h

#include "LinkedList.h"

typedef enum {
  PathsDataState_PreWrite,
  PathsDataState_Write,
} PathsDataState;

struct PathsData {
  PathsDataState state;
  size_t count;
  LinkedList list;
};

#endif
