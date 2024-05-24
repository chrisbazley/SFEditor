/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Big ships performance dialogue box
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef BPerfDbox_h
#define BPerfDbox_h

#include <stdbool.h>
#include "toolbox.h"

#include "Ships.h"
#include "BPerf.h"

struct EditSession;

typedef struct {
  ObjectId my_object;
  struct EditSession *session;
  ShipType ship_type;
} BPerfDboxData;

bool BPerfDbox_init(BPerfDboxData *performance_data, struct EditSession *session,
                      ShipType ship_type);
void BPerfDbox_show(BPerfDboxData *performance_data);
void BPerfDbox_update_title(BPerfDboxData *performance_data);
void BPerfDbox_destroy(BPerfDboxData *performance_data);

#endif
