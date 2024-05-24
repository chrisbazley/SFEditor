/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Strategic target information editing functions
 *  Copyright (C) 2023 Christopher Bazley
 */

#ifndef infoedit_h
#define infoedit_h

#include <stdbool.h>
#include "MapCoord.h"
#include "Infos.h"

struct InfoEditChanges;
struct SelectionBitmask;

typedef struct InfoEditContext InfoEditContext;

SFError InfoEdit_add(InfoEditContext const *infos, MapPoint pos,
  char const *const strings[static TargetInfoTextIndex_Count],
  struct InfoEditChanges *change_info, size_t *index);

void InfoEdit_move(InfoEditContext const *infos,
                   MapPoint vec,
                   struct SelectionBitmask *selected,
                   struct InfoEditChanges *change_info);

SFError InfoEdit_set_texts(
  TargetInfo *info,
  char const *const strings[static TargetInfoTextIndex_Count],
  struct InfoEditChanges *change_info);

TargetInfo *InfoEdit_get(InfoEditContext const *infos, size_t index);

void InfoEdit_find_occluded(InfoEditContext const *infos, MapPoint pos,
  struct SelectionBitmask *occluded);

void InfoEdit_delete(InfoEditContext const *infos, struct SelectionBitmask *selected,
  struct InfoEditChanges *change_info);

size_t InfoEdit_count(InfoEditContext const *infos);

typedef struct {
  TargetInfosIter inner;
} InfoEditIter;

size_t InfoEdit_get_first_idx(InfoEditIter *iter,
  InfoEditContext const *infos, MapArea const *map_area);

static inline size_t InfoEditIter_get_next(InfoEditIter *iter)
{
  assert(iter);
  return TargetInfosIter_get_next(&iter->inner);
}

static inline bool InfoEditIter_done(InfoEditIter const *const iter)
{
  assert(iter);
  return TargetInfosIter_done(&iter->inner);
}

static inline void InfoEditIter_del_current(InfoEditIter *const iter)
{
  assert(iter);
  TargetInfosIter_del_current(&iter->inner);
}

#endif
