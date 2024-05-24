/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Hard-wired data on polygonal graphics sets
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef graphicsdata_h
#define graphicsdata_h

#include <stdbool.h>

#include "stringbuff.h"
#include "ships.h"
#include "obj.h"

bool get_shipname_from_type(StringBuffer *output_string,
  char const *graphics_set, ShipType shipnum);

bool get_objname_from_type(StringBuffer *output_string,
  char const *graphics_set, ObjRef obj_no);

bool build_ships_stringset(StringBuffer *output_string,
  char const *graphics_set,
  bool include_player, bool include_fighters,
  bool include_bigships, bool include_satellite);

bool build_objs_stringset(StringBuffer *output_string,
  char const *graphics_set,
  bool include_none, bool include_objects, bool include_clouds,
  bool include_hills, bool include_mask);

#endif
