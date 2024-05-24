/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Hard-wired data on polygonal graphics sets
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
#include <string.h>
#include <stdbool.h>
#include "stdio.h"

#include "stringbuff.h"
#include "debug.h"
#include "msgtrans.h"
#include "err.h"
#include "Macros.h"
#include "messtrans.h"

#include "graphicsdata.h"
#include "SFInit.h"
#include "utils.h"
#include "Obj.h"

#include "Ships.h"

enum {
  TokenTailMaxBytes = 16 /* two characters, decimal integer and terminator */
};

static bool get_name_from_type(StringBuffer *const output_string,
  char const *const graphics_set, int const type_prefix, size_t const obj_no)
{
  bool success = false;

  StringBuffer token;
  stringbuffer_init(&token);

  if (stringbuffer_append_all(&token, graphics_set))
  {
    size_t toksize = TokenTailMaxBytes;
    char *const toktail = stringbuffer_prepare_append(&token, &toksize);
    if (toktail)
    {
      int const nchars = sprintf(toktail, "@%c%zu", type_prefix, obj_no);
      assert(nchars >= 0);
      stringbuffer_finish_append(&token, (size_t)nchars);

      stringbuffer_truncate(output_string, 0);
      size_t msgsize = 0;
      if (messagetrans_lookup(&messages, stringbuffer_get_pointer(&token),
                               NULL, 0, &msgsize, 0) == NULL)
      {
        char *const outtail = stringbuffer_prepare_append(output_string, &msgsize);
        if (outtail &&
            messagetrans_lookup(&messages, stringbuffer_get_pointer(&token),
                                 outtail, msgsize, &msgsize, 0) == NULL)
        {
          stringbuffer_finish_append(output_string, msgsize - 1);
          success = true;
        }
      }

      if (!success)
      {
        success = stringbuffer_append_all(output_string,
                                          stringbuffer_get_pointer(&token));
      }
    }
  }

  stringbuffer_destroy(&token);
  return success;
}

bool get_objname_from_type(StringBuffer *const output_string,
  char const *const graphics_set, ObjRef const obj_no)
{
  if (objects_ref_is_cloud(obj_no)) {
    int const height = objects_ref_get_cloud_height(obj_no);

    stringbuffer_truncate(output_string, 0);
    size_t msgsize = 0;

    static char const *const categories[] = {
      "XLCloud", "VLCloud", "LCloud", "MCloud",
      "HCloud", "VHCloud", "XHCloud"
    };
    size_t const cat_size = (Obj_MaxCloudHeight - Obj_MinCloudHeight) /
                            ARRAY_SIZE(categories);
    size_t const height_cat = (size_t)(height - Obj_MinCloudHeight) / cat_size;
    assert(height_cat < ARRAY_SIZE(categories));
    char const *const token = categories[height_cat];

    char id_string[16];
    sprintf(id_string, "%zu", objects_ref_to_num(obj_no) - Obj_RefMinCloud);

    if (messagetrans_lookup(&messages, token,
                             NULL, 0, &msgsize, 1, id_string) == NULL)
    {
      char *const outtail = stringbuffer_prepare_append(output_string, &msgsize);
      if (outtail &&
          messagetrans_lookup(&messages, token,
                              outtail, msgsize, &msgsize, 1, id_string) == NULL)
      {
        stringbuffer_finish_append(output_string, msgsize - 1);
        return true;
      }
    }

    return stringbuffer_append_all(output_string, token);
  } else {
    return get_name_from_type(output_string, graphics_set, 'O', objects_ref_to_num(obj_no));
  }
}

bool get_shipname_from_type(StringBuffer *const output_string,
  char const *const graphics_set, ShipType const ship_no)
{
  return get_name_from_type(output_string, graphics_set, 'S', ship_no);
}

bool build_ships_stringset(StringBuffer *const output_string,
  char const *const graphics_set,
  bool const include_player, bool const include_fighters,
  bool const include_bigships, bool const include_satellite)
{
  /* Build string suitable to pass to stringset_set_available() */
  bool success = true;

  stringbuffer_truncate(output_string, 0);

  StringBuffer ship_name;
  stringbuffer_init(&ship_name);

  if (include_player)
  {
    success = get_shipname_from_type(&ship_name, graphics_set, ShipType_Player);
    if (success)
    {
      success = stringbuffer_append_all(output_string, stringbuffer_get_pointer(&ship_name));
    }
  }

  if (include_fighters)
  {
    for (ShipType i = ShipType_Fighter1; i <= ShipType_Fighter4 && success; i++)
    {
      success = get_shipname_from_type(&ship_name, graphics_set, i);
      if (success)
      {
        success = append_to_csv(output_string, stringbuffer_get_pointer(&ship_name));
      }
    }
  }

  if (include_bigships)
  {
    for (ShipType i = ShipType_Big1; i <= ShipType_Big3 && success; i++)
    {
      success = get_shipname_from_type(&ship_name, graphics_set, i);
      if (success)
      {
        success = append_to_csv(output_string, stringbuffer_get_pointer(&ship_name));
      }
    }
  }

  if (include_satellite && success)
  {
    success = get_shipname_from_type(&ship_name, graphics_set, ShipType_Satellite);
    if (success)
    {
      success = append_to_csv(output_string, stringbuffer_get_pointer(&ship_name));
    }
  }

  stringbuffer_destroy(&ship_name);

  return success;
}

bool build_objs_stringset(StringBuffer *const output_string,
  char const *const graphics_set,
  bool const include_none, bool const include_objects, bool const include_clouds,
  bool const include_hills, bool const include_mask)
{
  /* Build string suitable to pass to stringset_set_available() */
  bool success = true;

  stringbuffer_truncate(output_string, 0);

  StringBuffer obj_name;
  stringbuffer_init(&obj_name);

  if (include_none)
  {
    success = get_objname_from_type(&obj_name, graphics_set, objects_ref_none());
    if (success)
    {
      success = append_to_csv(output_string, stringbuffer_get_pointer(&obj_name));
    }
  }

  if (include_objects)
  {
    for (size_t i = Obj_RefMinObject; i <= Obj_RefMaxObject && success; i++)
    {
      success = get_objname_from_type(&obj_name, graphics_set, objects_ref_from_num(i));
      if (success)
      {
        success = append_to_csv(output_string, stringbuffer_get_pointer(&obj_name));
      }
    }
  }

  if (include_clouds)
  {
    for (size_t i = Obj_RefMinCloud; i <= Obj_RefMaxCloud && success; i++)
    {
      success = get_objname_from_type(&obj_name, graphics_set, objects_ref_from_num(i));
      if (success)
      {
        success = append_to_csv(output_string, stringbuffer_get_pointer(&obj_name));
      }
    }
  }

  if (include_hills && success)
  {
    success = get_objname_from_type(&obj_name, graphics_set, objects_ref_hill());
    if (success)
    {
      success = append_to_csv(output_string, stringbuffer_get_pointer(&obj_name));
    }
  }

  if (include_mask && success)
  {
    success = get_objname_from_type(&obj_name, graphics_set, objects_ref_mask());
    if (success)
    {
      success = append_to_csv(output_string, stringbuffer_get_pointer(&obj_name));
    }
  }

  stringbuffer_destroy(&obj_name);

  return success;
}
