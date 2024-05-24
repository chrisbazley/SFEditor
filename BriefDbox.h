/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission briefing dialogue box
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef BriefDbox_h
#define BriefDbox_h

#include <stdbool.h>
#include "toolbox.h"

struct EditSession;

typedef struct {
  ObjectId my_object;
  struct EditSession *session;
} BriefDboxData;

bool BriefDbox_init(BriefDboxData *briefing_data,
                    struct EditSession *session);

void BriefDbox_update_title(BriefDboxData *briefing_data);

void BriefDbox_show(BriefDboxData *briefing_data);

void BriefDbox_destroy(BriefDboxData *briefing_data);

#endif
