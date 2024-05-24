/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Goto dialogue box
 *  Copyright (C) 2023 Christopher Bazley
 */

#ifndef Goto_h
#define Goto_h

#include "toolbox.h"

void Goto_created(ObjectId id);
struct EditWin;
void Goto_show(struct EditWin const *edit_win);

#endif
