/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Objects editing mode change
 *  Copyright (C) 2021 Christopher Bazley
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

#include "ObjEditChg.h"

#define TOKEN_STEM "OStatus"

void ObjEditChanges_init(ObjEditChanges *const change_info)
{
  assert(change_info != NULL);
  *change_info = (ObjEditChanges){0,0,0,0};
}

char *ObjEditChanges_get_message(const ObjEditChanges *const change_info)
{
  char refs_changed_str[12], triggers_added_str[12], triggers_changed_str[12],
       triggers_deleted_str[12], token[sizeof(TOKEN_STEM "OACK")];
  char const *sub[4];

  if (!ObjEditChanges_is_changed(change_info)) {
    DEBUG("No changes to report to user");
    return NULL;
  }

  sprintf(token, TOKEN_STEM "%s%s%s%s",
           change_info->refs_changed ? "O" : "",
           change_info->triggers_added ? "A" : "",
           change_info->triggers_changed ? "C" : "",
           change_info->triggers_deleted ? "K" : "");

  size_t p = 0;

  if (change_info->refs_changed) {
    sprintf(refs_changed_str, "%lu",
             change_info->refs_changed);
    sub[p++] = refs_changed_str;
  } else {
    sub[p++] = NULL;
  }

  if (change_info->triggers_added) {
    sprintf(triggers_added_str, "%lu",
             change_info->triggers_added);
    sub[p++] = triggers_added_str;
  } else {
    sub[p++] = NULL;
  }

  if (change_info->triggers_changed) {
    sprintf(triggers_changed_str, "%lu",
             change_info->triggers_changed);
    sub[p++] = triggers_changed_str;
  } else {
    sub[p++] = NULL;
  }

  if (change_info->triggers_deleted) {
    sprintf(triggers_deleted_str, "%lu",
             change_info->triggers_deleted);
    sub[p++] = triggers_deleted_str;
  } else {
    sub[p++] = NULL;
  }

  return msgs_lookup_subn(token, p, sub[0], sub[1], sub[2], sub[3]);
}
