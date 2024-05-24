/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  DCS dialogue box
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef DCS_dialogue
#define DCS_dialogue

#include <stdbool.h>
#include "toolbox.h"

struct EditSession;

void DCS_created(ObjectId object);
void DCS_notifysaved(ObjectId savebox_parent, struct EditSession *session);
void DCS_queryunsaved(ObjectId edit_win, int num_files, bool open_parent);

#endif
