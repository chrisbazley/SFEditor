/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map/animations editing mode
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef MapMode_h
#define MapMode_h

#include <stdbool.h>

#include "Vertex.h"
#include "MapCoord.h"
#include "MapAnims.h"
#include "DataType.h"

struct Editor;
struct EditWin;
struct Writer;

bool MapMode_set_properties(struct Editor *editor, MapPoint pos, MapAnimParam anim);

struct EditorTool MapMode_get_current_tool(struct Editor const *editor);

bool MapMode_enter(struct Editor *editor);
bool MapMode_can_enter(struct Editor *editor);

void MapMode_draw(struct Editor *editor, Vertex map_origin,
  MapArea const *redraw_area, struct EditWin *edit_win);

bool MapMode_write_clipboard(struct Writer *writer,
                             DataType data_type, char const *const filename);

int MapMode_estimate_clipboard(DataType data_type);

void MapMode_free_clipboard(void);

#endif
