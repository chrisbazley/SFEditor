/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Graphics files dialogue box
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef GraphicsFiles_h
#define GraphicsFiles_h

#include <stdbool.h>
#include "toolbox.h"
#include "Session.h"

extern ObjectId GraphicsFiles_id;

void GraphicsFiles_created(ObjectId window_id);
int GraphicsFiles_colour_selected(EditSession *session, ComponentId parent_component, unsigned int colour);
#endif
