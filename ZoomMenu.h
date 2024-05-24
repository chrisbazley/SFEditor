/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map edit_win zoom menu
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef ZoomMenu_h
#define ZoomMenu_h

#include "toolbox.h"

struct EditWin;

void ZoomMenu_created(ObjectId menu_id);
void ZoomMenu_show(struct EditWin const *edit_win);
void ZoomMenu_show_at_ptr(struct EditWin const *edit_win);

#endif
