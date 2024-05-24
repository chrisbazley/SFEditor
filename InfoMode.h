/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Info/flightpaths editing mode
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef InfoMode
#define InfoMode

#include <stdbool.h>

#include "Vertex.h"
#include "MapCoord.h"
#include "infos.h"

struct Editor;
struct EditWin;

bool InfoMode_set_properties(struct Editor *editor, TargetInfo *info,
  char const *strings[static TargetInfoTextIndex_Count]);

bool InfoMode_enter(struct Editor *editor);
bool InfoMode_can_enter(struct Editor *editor);

void InfoMode_draw(struct Editor *editor, Vertex scr_orig, MapArea const *redraw_area,
  struct EditWin const *edit_win);

bool InfoMode_write_clipboard(struct Writer *writer,
                              DataType data_type,
                              char const *filename);

int InfoMode_estimate_clipboard(DataType data_type);

void InfoMode_free_clipboard(void);

#endif
