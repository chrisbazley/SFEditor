/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Objects grid and triggers editing context
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef ObjEditCtx_h
#define ObjEditCtx_h

#include "MapCoord.h"
#include "Obj.h"
#include "Triggers.h"

struct EditSession;

struct ObjEditContext
{
  struct ObjectsData *base; /* (Map/Mission) */
  struct ObjectsData *overlay; /* (Mission only) */
  TriggersData *triggers; /* (Mission only) */
  void (*prechange_cb)(MapArea const *, struct EditSession *);
  void (*redraw_obj_cb)(MapPoint, ObjRef, ObjRef, ObjRef, bool, struct EditSession *);
  void (*redraw_trig_cb)(MapPoint, ObjRef, TriggerFullParam, struct EditSession *);
  struct EditSession *session;
};

#endif
