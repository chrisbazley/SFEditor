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
#include "stdlib.h"
#include "strb.h"

#include "msgtrans.h"
#include "Debug.h"

#include "ObjEditChg.h"

#ifdef USE_OPTIONAL
#include "Optional.h"
#endif

#define TOKEN_STEM "OStatus"

void ObjEditChanges_init(ObjEditChanges *const change_info)
{
  assert(change_info != NULL);
  *change_info = (ObjEditChanges){0,0,0,0};
}

_Optional char *ObjEditChanges_get_message(const ObjEditChanges *const change_info)
{
  STRB_DECL_FIXED(refs_changed_str, 23);
  STRB_DECL_FIXED(triggers_added_str, 23);
  STRB_DECL_FIXED(triggers_changed_str, 23);
  STRB_DECL_FIXED(triggers_deleted_str, 23);
  STRB_DECL_FIXED(token, sizeof(TOKEN_STEM "OACK") - 1);
  char const *sub[4];

  if (!ObjEditChanges_is_changed(change_info)) {
    DEBUG("No changes to report to user");
    return NULL;
  }

  if (strb_printf(token, TOKEN_STEM "%s%s%s%s",
                  change_info->refs_changed ? "O" : "",
                  change_info->triggers_added ? "A" : "",
                  change_info->triggers_changed ? "C" : "",
                  change_info->triggers_deleted ? "K" : "")) {
    abort();
  }

  size_t p = 0;

  if (change_info->refs_changed) {
    if (strb_printf(refs_changed_str, "%lu",
                    change_info->refs_changed)) {
      abort();
    }
    sub[p++] = strb_cptr(refs_changed_str);
  } else {
    sub[p++] = "";
  }

  if (change_info->triggers_added) {
    if (strb_printf(triggers_added_str, "%lu",
                    change_info->triggers_added)) {
      abort();
    }
    sub[p++] = strb_cptr(triggers_added_str);
  } else {
    sub[p++] = "";
  }

  if (change_info->triggers_changed) {
    if (strb_printf(triggers_changed_str, "%lu",
                    change_info->triggers_changed)) {
      abort();
    }
    sub[p++] = strb_cptr(triggers_changed_str);
  } else {
    sub[p++] = "";
  }

  if (change_info->triggers_deleted) {
    if (strb_printf(triggers_deleted_str, "%lu",
                    change_info->triggers_deleted)) {
      abort();
    }
    sub[p++] = strb_cptr(triggers_deleted_str);
  } else {
    sub[p++] = "";
  }

  return msgs_lookup_subn(strb_cptr(token), p, sub[0], sub[1], sub[2], sub[3]);
}
