/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Errors
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef SFError_h
#define SFError_h

typedef enum {
#define DECLARE_ERROR(ms) SFErrorType_ ## ms,
#include "DeclErrors.h"
#undef DECLARE_ERROR
} SFErrorType;

typedef struct {
  SFErrorType type;
#ifndef NDEBUG
  char const *loc;
#endif
} SFError;

#ifdef NDEBUG
#define SFERROR(t) (SFError){.type = SFErrorType_ ## t}
#else
#include "Debug.h"
#define SFERROR(t) (SFError){.type = SFErrorType_ ## t, .loc = LOCATION}
#endif

static inline bool SFError_fail(SFError const e)
{
  return e.type != SFErrorType_OK;
}

#endif
