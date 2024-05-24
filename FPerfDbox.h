/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Fighter performance dialogue box
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef FPerfDbox_h
#define FPerfDbox_h

#include <stdbool.h>
#include "toolbox.h"

#include "Ships.h"
#include "FPerf.h"
#include "FilenamesData.h"

struct EditSession;

typedef struct {
  ObjectId my_object;
  struct EditSession *session;
  ShipType ship_type;
  Filename graphics_set;
} FPerfDboxData;

bool FPerfDbox_init(FPerfDboxData *performance_data, struct EditSession *session, ShipType ship_type);
void FPerfDbox_show(FPerfDboxData *performance_data);
void FPerfDbox_update_title(FPerfDboxData *performance_data);
void FPerfDbox_destroy(FPerfDboxData *performance_data);

#endif
