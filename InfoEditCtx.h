/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Strategic target information editing context
 *  Copyright (C) 2023 Christopher Bazley
 */

#ifndef InfoEditCtx_h
#define InfoEditCtx_h

#include "MapCoord.h"

struct EditSession;

struct InfoEditContext
{
  struct TargetInfosData *data; /* (Mission only) */
  void (*added_cb)(struct TargetInfo const *, size_t, struct EditSession *);
  void (*predelete_cb)(struct TargetInfo const *, size_t, struct EditSession *);
  void (*moved_cb)(struct TargetInfo const *, MapPoint, size_t, size_t, struct EditSession *);
  struct EditSession *session;
};

#endif
