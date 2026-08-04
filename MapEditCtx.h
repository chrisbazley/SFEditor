#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map/animations editing mode context
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef MapEditCtx_h
#define MapEditCtx_h

struct EditSession;
struct MapArea;

typedef void MapEditPreChangeFn(struct MapArea const *, struct EditSession *),
             MapEditRedrawFn(struct MapArea const *, struct EditSession *);

struct MapEditContext
{
  _Optional struct MapData *base, /* (Map/Mission) */
                           *overlay; /* (Mission only) */
  _Optional struct ConvAnimations *anims; /* (Mission only) */
  _Optional MapEditPreChangeFn *prechange_cb;
  _Optional MapEditRedrawFn *redraw_cb;
  struct EditSession *session;
};

#endif
