/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ships/flightpaths editing mode
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef ShipsMode
#define ShipsMode

#include <stdbool.h>

#include "Vertex.h"
#include "MapCoord.h"

struct Editor;
struct EditWin;

bool ShipsMode_enter(struct Editor *editor);
bool ShipsMode_can_enter(struct Editor *editor);

void ShipsMode_draw(struct Editor *editor, Vertex map_origin,
  MapArea const *redraw_area, struct EditWin *edit_win);

#endif
