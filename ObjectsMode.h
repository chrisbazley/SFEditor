/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Objects/triggers editing mode
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef ObjectsMode
#define ObjectsMode

#include <stdbool.h>

#include "Vertex.h"
#include "MapCoord.h"
#include "DataType.h"
#include "Triggers.h"

struct Editor;
struct EditWin;
struct Writer;
struct ObjGfxMeshes;
struct ObjTransfer;

bool ObjectsMode_set_properties(struct Editor *editor, MapPoint pos, ObjRef obj_ref, TriggerFullParam const *trig, size_t nitems);

bool ObjectsMode_enter(struct Editor *editor);
bool ObjectsMode_can_enter(struct Editor *editor);

void ObjectsMode_draw(struct Editor *editor, Vertex map_origin,
  MapArea const *redraw_area, struct EditWin const *const edit_win);

bool ObjectsMode_write_clipboard(struct Writer *writer,
                                 DataType data_type,
                                 char const *filename);

int ObjectsMode_estimate_clipboard(DataType data_type);

void ObjectsMode_free_clipboard(void);

void ObjectsMode_redraw_clouds(struct Editor *editor);

#endif
