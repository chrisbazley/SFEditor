/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Strategic target information change
 *  Copyright (C) 2022 Christopher Bazley
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

#include "InfoEditChg.h"

#define TOKEN_STEM "IStatus"

void InfoEditChanges_init(InfoEditChanges *const change_info)
{
  assert(change_info != NULL);
  *change_info = (InfoEditChanges){0,0,0};
}

char *InfoEditChanges_get_message(const InfoEditChanges *const change_info)
{
  char infos_added_str[12], infos_changed_str[12],
       infos_deleted_str[12], token[sizeof(TOKEN_STEM "ACK")];
  char const *sub[3];

  if (!InfoEditChanges_is_changed(change_info)) {
    DEBUG("No changes to report to user");
    return NULL;
  }

  sprintf(token, TOKEN_STEM "%s%s%s",
           change_info->infos_added ? "A" : "",
           change_info->infos_changed ? "C" : "",
           change_info->infos_deleted ? "K" : "");

  size_t p = 0;

  if (change_info->infos_added) {
    sprintf(infos_added_str, "%lu",
             change_info->infos_added);
    sub[p++] = infos_added_str;
  } else {
    sub[p++] = NULL;
  }

  if (change_info->infos_changed) {
    sprintf(infos_changed_str, "%lu",
             change_info->infos_changed);
    sub[p++] = infos_changed_str;
  } else {
    sub[p++] = NULL;
  }

  if (change_info->infos_deleted) {
    sprintf(infos_deleted_str, "%lu",
             change_info->infos_deleted);
    sub[p++] = infos_deleted_str;
  } else {
    sub[p++] = NULL;
  }

  return msgs_lookup_subn(token, p, sub[0], sub[1], sub[2]);
}
