/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Strategic target information editing context
 *  Copyright (C) 2023 Christopher Bazley
 */

#ifndef InfoEditCtx_h
#define InfoEditCtx_h

#include "MapCoord.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

struct EditSession;

typedef void InfoEditAddedFn(struct TargetInfo const *, size_t, struct EditSession *),
             InfoEditPreDeleteFn(struct TargetInfo const *, size_t, struct EditSession *),
             InfoEditMovedFn(struct TargetInfo const *, MapPoint, size_t, size_t, struct EditSession *);

struct InfoEditContext
{
  _Optional struct TargetInfosData *data; /* (Mission only) */
  _Optional InfoEditAddedFn *added_cb;
  _Optional InfoEditPreDeleteFn *predelete_cb;
  _Optional InfoEditMovedFn *moved_cb;
  struct EditSession *session;
};

#endif
