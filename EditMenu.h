/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Standard edit menu
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef editmenu_h
#define editmenu_h

#include "toolbox.h"
#include "Editor.h"
#include "EditWin.h"

void EditMenu_created(ObjectId id);
void EditMenu_update(Editor const *editor);
void EditMenu_show_at_ptr(EditWin const *edit_win);

#endif
