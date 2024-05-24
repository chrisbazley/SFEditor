/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Wand configuration
 *  Copyright (C) 2021 Christopher Bazley
 */

#ifndef ConfigWand_h
#define ConfigWand_h

#include "toolbox.h"
#include "Session.h"

void ConfigWand_created(ObjectId window_id);
void ConfigWand_show_at_ptr(EditWin const *edit_win);

#endif
