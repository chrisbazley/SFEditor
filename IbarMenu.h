/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Root iconbar menu
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef IbarMenu_h
#define IbarMenu_h

#include <stdbool.h>
#include "toolbox.h"

extern ObjectId IbarMenu_id;

void IbarMenu_created(ObjectId id);
void IbarMenu_dosubmenuaction(ComponentId menu_entry,
  const char *file_path, bool map);
bool IbarMenu_grey_intern_files(ComponentId menu_entry);
char const *IbarMenu_get_sub_menu_title(void);

#endif
