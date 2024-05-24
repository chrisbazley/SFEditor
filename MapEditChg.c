/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Map/animations editing mode change
 *  Copyright (C) 2019 Christopher Bazley
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

#include "stdio.h"

#include "msgtrans.h"
#include "debug.h"

#include "MapEditChg.h"

#define TOKEN_STEM "MStatus"

void MapEditChanges_init(MapEditChanges *const change_info)
{
  assert(change_info != NULL);
  *change_info = (MapEditChanges){0,0,0,0};
}

char *MapEditChanges_get_message(const MapEditChanges *const change_info)
{
  char tiles_changed_str[12], anims_added_str[12], anims_changed_str[12],
       anims_deleted_str[12], token[sizeof(TOKEN_STEM "TACK")];
  char const *sub[4];

  if (!MapEditChanges_is_changed(change_info)) {
    DEBUG("No changes to report to user");
    return NULL;
  }

  sprintf(token, TOKEN_STEM "%s%s%s%s",
           change_info->tiles_changed ? "T" : "",
           change_info->anims_added ? "A" : "",
           change_info->anims_changed ? "C" : "",
           change_info->anims_deleted ? "K" : "");

  size_t p = 0;

  if (change_info->tiles_changed) {
    sprintf(tiles_changed_str, "%lu",
             change_info->tiles_changed);
    sub[p++] = tiles_changed_str;
  } else {
    sub[p++] = NULL;
  }

  if (change_info->anims_added) {
    sprintf(anims_added_str, "%lu",
             change_info->anims_added);
    sub[p++] = anims_added_str;
  } else {
    sub[p++] = NULL;
  }

  if (change_info->anims_changed) {
    sprintf(anims_changed_str, "%lu",
             change_info->anims_changed);
    sub[p++] = anims_changed_str;
  } else {
    sub[p++] = NULL;
  }

  if (change_info->anims_deleted) {
    sprintf(anims_deleted_str, "%lu",
             change_info->anims_deleted);
    sub[p++] = anims_deleted_str;
  } else {
    sub[p++] = NULL;
  }

  return msgs_lookup_subn(token, p, sub[0], sub[1], sub[2], sub[3]);
}
