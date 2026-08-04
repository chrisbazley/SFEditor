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

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

struct EditSession;


typedef void ObjEditPreChangeFn(MapArea const *, struct EditSession *),
             ObjEditRedrawnObjFn(MapPoint, ObjRef, ObjRef, ObjRef, bool, struct EditSession *),
             ObjEditRedrawTrigFn(MapPoint, ObjRef, TriggerFullParam, struct EditSession *);

struct ObjEditContext
{
  _Optional struct ObjectsData *base, /* (Map/Mission) */
                               *overlay; /* (Mission only) */
  _Optional TriggersData *triggers; /* (Mission only) */
  _Optional ObjEditPreChangeFn *prechange_cb;
  _Optional ObjEditRedrawnObjFn *redraw_obj_cb;
  _Optional ObjEditRedrawTrigFn *redraw_trig_cb;
  struct EditSession *session;
};

#endif
