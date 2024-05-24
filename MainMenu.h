/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Main menu for map window
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef MainMenu_h
#define MainMenu_h

#include <stdbool.h>
#include "toolbox.h"

#include "Session.h"

void MainMenu_created(ObjectId id);
EditSession *MainMenu_get_session(void);
void MainMenu_hide(void);

#endif
