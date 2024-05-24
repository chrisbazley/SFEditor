/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map edit_win orientation menu
 *  Copyright (C) 2022 Christopher Bazley
 */

#ifndef OrientMenu_h
#define OrientMenu_h

#include "toolbox.h"
#include "EditWin.h"

struct EditWin;

void OrientMenu_created(ObjectId menu_id);
void OrientMenu_show(struct EditWin const *edit_win);
void OrientMenu_show_at_ptr(struct EditWin const *edit_win);

#endif
