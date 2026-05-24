/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Various menus of files
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef filesmenus_h
#define filesmenus_h

#include <stdbool.h>
#include "toolbox.h"
#include "filescan.h"

void tilesetmenu_created(ObjectId const id);
void polysetmenu_created(ObjectId const id);
void coloursmenu_created(ObjectId const id);
void hillcolmenu_created(ObjectId const id);
void basefxdmenu_created(ObjectId const id);
void basesprmenu_created(ObjectId const id);
void skymenu_created(ObjectId const id);
void planetsmenu_created(ObjectId const id);

/* Build menu from array of leafnames */
ComponentId filesmenus_build(ObjectId menu, filescan_leafname *leaf_names,
  ComponentId *ret_next_free, bool inc_blank, bool add_none,
  bool grey_internal, char const *tick_me);

void filesmenus_grey_internal(ObjectId menu,
  const filescan_leafname *leaf_names, bool inc_blank, bool grey_internal);

#endif
