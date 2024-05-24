/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Dialogue box for selection of background colour
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef BackCol_h
#define BackCol_h

#include "toolbox.h"

struct EditWin;

void BackCol_created(ObjectId menu_id);
void BackCol_show(struct EditWin const *edit_win);

#endif
