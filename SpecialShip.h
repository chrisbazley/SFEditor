/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Special ship dialogue box
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef SpecialShip_h
#define SpecialShip_h

#include <stdbool.h>
#include "toolbox.h"
#include "FilenamesData.h"

typedef struct {
  ObjectId my_object;
  struct EditSession *session;
  Filename polygonal_objects_set;
} SpecialShipData;

bool SpecialShip_init(SpecialShipData *special_ship_data, struct EditSession *session);
void SpecialShip_show(SpecialShipData *special_ship_data);
void SpecialShip_update_title(SpecialShipData *special_ship_data);
void SpecialShip_destroy(SpecialShipData *special_ship_data);

#endif
