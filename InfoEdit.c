/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Objects grid and ground_checks editing functions
 *  Copyright (C) 2001 Christopher Bazley
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public Licence as published by
 *  the Free Software Foundation; either version 2 of the Licence, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public Licence for more details.
 *
 *  You should have received a copy of the GNU General Public Licence
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "stdlib.h"
#include <stdbool.h>
#include <assert.h>
#include <limits.h>
#include "toolbox.h"

#include "err.h"
#include "Macros.h"
#include "msgtrans.h"
#include "hourglass.h"
#include "Debug.h"

#include "Infos.h"
#include "InfosData.h"
#include "InfoEdit.h"
#include "InfoEditCtx.h"
#include "InfoEditChg.h"
#include "MapCoord.h"
#include "Map.h"
#include "DrawInfo.h"
#include "DrawTrig.h"
#include "SelBitmask.h"

SFError InfoEdit_add(InfoEditContext const *const infos, MapPoint const pos,
  char const *const strings[static TargetInfoTextIndex_Count],
  InfoEditChanges *const change_info, size_t *const index)
{
  assert(infos);

  size_t index2 = 0;
  SFError err = target_infos_add(infos->data, pos, &index2);
  if (SFError_fail(err)) {
    return err;
  }

  TargetInfo *const info = target_info_from_index(infos->data, index2);

  if (strings) {
    for (TargetInfoTextIndex k = TargetInfoTextIndex_First;
         k < TargetInfoTextIndex_Count;
         ++k) {
      err = target_info_set_text(info, k, strings[k]);
      if (SFError_fail(err)) {
        target_info_delete(info);
        return err;
      }
    }
  }

  InfoEditChanges_add(change_info);
  if (infos->added_cb) {
    infos->added_cb(info, index2, infos->session);
  }

  if (index) {
    *index = index2;
  }
  return err;
}

void InfoEdit_move(InfoEditContext const *const infos, MapPoint const vec,
                   SelectionBitmask *const selected,
                   InfoEditChanges *const change_info)
{
  SelectionBitmask copy;
  SelectionBitmask_init(&copy, SelectionBitmask_size(selected), NULL, NULL);
  SelectionBitmask_copy(&copy, selected); // because moved_cb changes the selection

  SelectionBitmaskIter iter;
  for (size_t index = SelectionBitmaskIter_get_first(&iter, &copy);
       !SelectionBitmaskIter_done(&iter);
       index = SelectionBitmaskIter_get_next(&iter))
  {
    TargetInfo *const info = target_info_from_index(infos->data, index);
    MapPoint const old_pos = target_info_get_pos(info), new_pos = MapPoint_add(old_pos, vec);
    size_t const new_index = target_info_set_pos(info, new_pos);
    if (infos->moved_cb) {
      infos->moved_cb(info, old_pos, index, new_index, infos->session);
    }
    InfoEditChanges_change(change_info);
    SelectionBitmaskIter_move_current(&iter, new_index);
  }
}


SFError InfoEdit_set_texts(
  TargetInfo *const info,
  char const *const strings[static TargetInfoTextIndex_Count],
  InfoEditChanges *const change_info)
{
  SFError err = SFERROR(OK);
  for (TargetInfoTextIndex k = TargetInfoTextIndex_First;
       k < TargetInfoTextIndex_Count;
       ++k) {
    err = target_info_set_text(info, k, strings[k]);
    if (SFError_fail(err)) {
      break;
    }
  }
  if (!SFError_fail(err)) {
    InfoEditChanges_change(change_info);
  }
  return err;
}

TargetInfo *InfoEdit_get(InfoEditContext const *const infos, size_t const index)
{
  assert(infos);
  return target_info_from_index(infos->data, index);
}

void InfoEdit_find_occluded(InfoEditContext const *const infos, MapPoint const pos,
  SelectionBitmask *const occluded)
{
  assert(infos);
  assert(occluded);

  MapPoint const coll_size = {InfoMaxClickDist, InfoMaxClickDist};
  MapArea const my_info_area = {MapPoint_sub(pos, coll_size), MapPoint_add(pos, coll_size)};

  MapArea const overlapping_area = {
    MapPoint_sub(my_info_area.min, coll_size),
    MapPoint_add(my_info_area.max, coll_size)
  };

  InfoEditIter iter;
  for (size_t index = InfoEdit_get_first_idx(&iter, infos, &overlapping_area);
       !InfoEditIter_done(&iter);
       index = InfoEditIter_get_next(&iter))
  {
    MapPoint const p = target_info_get_pos(InfoEdit_get(infos, index));
    MapArea const info_area = {MapPoint_sub(p, coll_size), MapPoint_add(p, coll_size)};

    if (map_overlap(&my_info_area, &info_area)) {
      DEBUGF("Info at %" PRIMapCoord ",%" PRIMapCoord
             " overlaps info at %" PRIMapCoord ",%" PRIMapCoord "\n",
             pos.x, pos.y, p.x, p.y);
      SelectionBitmask_select(occluded, index);
    }
  }
}

void InfoEdit_delete(InfoEditContext const *const infos, SelectionBitmask *const selected,
  InfoEditChanges *const change_info)
{
  assert(infos);
  SelectionBitmask copy = *selected;
  SelectionBitmaskIter iter;
  for (size_t index = SelectionBitmaskIter_get_first(&iter, &copy);
       !SelectionBitmaskIter_done(&iter);
       index = SelectionBitmaskIter_get_next(&iter))
  {
    TargetInfo *const info = target_info_from_index(infos->data, index);
    if (infos->predelete_cb) {
      infos->predelete_cb(info, index, infos->session);
    }
    target_info_delete(info);
    InfoEditChanges_delete(change_info);
    SelectionBitmaskIter_del_current(&iter);
  }
}

size_t InfoEdit_count(InfoEditContext const *const infos)
{
  assert(infos);
  return infos->data ? target_infos_get_count(infos->data) : 0;
}

size_t InfoEdit_get_first_idx(InfoEditIter *const iter,
  InfoEditContext const *const infos, MapArea const *const map_area)
{
  assert(iter);
  assert(infos);
  static TargetInfosData empty;
  TargetInfosData *target_infos = &empty;
  if (infos) {
    target_infos = infos->data;
  } else {
    target_infos_init(&empty);
  }
  return TargetInfosIter_get_first(&iter->inner, target_infos, map_area);
}
