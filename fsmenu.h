/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Generic code for selection from a menu of files
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef fsmenu_h
#define fsmenu_h

#include <stdbool.h>
#include "toolbox.h"
#include "filescan.h"

/* Build menu from array of leafnames */
ComponentId fsmenu_build(ObjectId menu, filescan_leafname *leaf_names,
  ComponentId *ret_next_free, bool inc_blank, bool add_none,
  bool grey_internal, char const *tick_me);

void fsmenu_grey_internal(ObjectId menu,
  const filescan_leafname *leaf_names, bool inc_blank, bool grey_internal);

#endif
