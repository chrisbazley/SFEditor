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
#include "stdlib.h"
#include "strb.h"

#include "msgtrans.h"
#include "Debug.h"

#include "InfoEditChg.h"

#ifdef USE_OPTIONAL
#include "Optional.h"
#endif

#define TOKEN_STEM "IStatus"

void InfoEditChanges_init(InfoEditChanges *const change_info)
{
  assert(change_info != NULL);
  *change_info = (InfoEditChanges){0,0,0};
}

_Optional char *InfoEditChanges_get_message(const InfoEditChanges *const change_info)
{
  STRB_DECL_FIXED(infos_added_str, 23);
  STRB_DECL_FIXED(infos_changed_str, 23);
  STRB_DECL_FIXED(infos_deleted_str, 23);
  STRB_DECL_FIXED(token, sizeof(TOKEN_STEM "ACK") - 1);
  char const *sub[3];

  if (!InfoEditChanges_is_changed(change_info)) {
    DEBUG("No changes to report to user");
    return NULL;
  }

  if (strb_printf(token, TOKEN_STEM "%s%s%s",
                  change_info->infos_added ? "A" : "",
                  change_info->infos_changed ? "C" : "",
                  change_info->infos_deleted ? "K" : "")) {
    abort();
  }

  size_t p = 0;

  if (change_info->infos_added) {
    if (strb_printf(infos_added_str, "%lu",
                    change_info->infos_added)) {
      abort();
    }
    sub[p++] = strb_cptr(infos_added_str);
  } else {
    sub[p++] = "";
  }

  if (change_info->infos_changed) {
    if (strb_printf(infos_changed_str, "%lu",
                    change_info->infos_changed)) {
      abort();
    }
    sub[p++] = strb_cptr(infos_changed_str);
  } else {
    sub[p++] = "";
  }

  if (change_info->infos_deleted) {
    if (strb_printf(infos_deleted_str, "%lu",
                    change_info->infos_deleted)) {
      abort();
    }
    sub[p++] = strb_cptr(infos_deleted_str);
  } else {
    sub[p++] = "";
  }

  return msgs_lookup_subn(strb_cptr(token), p, sub[0], sub[1], sub[2]);
}
