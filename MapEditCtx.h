/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map/animations editing mode context
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef MapEditCtx_h
#define MapEditCtx_h

struct EditSession;
struct MapArea;

struct MapEditContext
{
  struct MapData *base; /* (Map/Mission) */
  struct MapData *overlay; /* (Mission only) */
  struct ConvAnimations *anims; /* (Mission only) */
  void (*prechange_cb)(struct MapArea const *, struct EditSession *);
  void (*redraw_cb)(struct MapArea const *, struct EditSession *);
  struct EditSession *session;
};

#endif
