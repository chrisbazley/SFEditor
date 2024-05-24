/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Menu for selection of editing mode
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef ModeMenu_h
#define ModeMenu_h

#include "toolbox.h"

struct EditWin;

void ModeMenu_created(ObjectId menu_id);
void ModeMenu_show_at_ptr(struct EditWin const *edit_win);

#endif
